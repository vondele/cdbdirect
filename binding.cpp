#include "cdbdirect.h"
#include <mutex>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;

class CDB {
public:
  CDB(const std::string &path, std::optional<size_t> threads) {
    m_threads = threads.value_or(
        std::max((unsigned int)1, std::thread::hardware_concurrency()));
    m_handle = cdbdirect_initialize(path);
    if (!m_handle)
      throw std::runtime_error("Init failed: " + path);
  }

  ~CDB() {
    if (m_handle)
      cdbdirect_finalize(m_handle);
  }

  uint64_t size() const { return cdbdirect_size(m_handle); }
  auto get(const std::string &fen) { return cdbdirect_get(m_handle, fen); }

  void apply(py::function callback) {
    // We use a pointer to avoid GIL issues during lambda capture/copy
    py::function *callback_ptr = &callback;

    // This mutex guarantees SERIALIZED access to the Python callback
    // from the multiple C++ worker threads.
    std::mutex python_mutex;

    auto cpp_callback =
        [&](const std::string &fen,
            const std::vector<std::pair<std::string, int>> &entries) -> bool {
      // 1. Wait for our turn to use the Python interpreter
      std::lock_guard<std::mutex> lock(python_mutex);

      // 2. Acquire GIL
      py::gil_scoped_acquire acquire;

      try {
        // 3. Execute (py::cast handles the deep copy safely)
        return (*callback_ptr)(fen, py::cast(entries)).cast<bool>();
      } catch (py::error_already_set &e) {
        return false;
      }
    };

    py::gil_scoped_release release;
    cdbdirect_apply(m_handle, m_threads, cpp_callback);
  }

private:
  std::uintptr_t m_handle;
  size_t m_threads;
};

PYBIND11_MODULE(cdbdirect, m) {
  py::class_<CDB>(m, "CDB")
      .def(py::init<const std::string &, std::optional<size_t>>(),
           py::arg("path"), py::arg("threads") = py::none())
      .def("size", &CDB::size)
      .def("get", &CDB::get)
      .def("apply", &CDB::apply);
}
