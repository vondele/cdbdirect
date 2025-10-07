# Needs to point to the root directory of git tree containing a compiled version of a specific fork of rocksDB (see README.md)
TERARKDBROOT = /home/vondele/chess/noob/terarkdb

# Needs to point to the path of the cdb dump
CHESSDB_PATH = /mnt/ssd/chess-20250608/data

# example executables
EXE1 = cdbdirect
EXE2 = cdbdirect_threaded
EXE3 = cdbdirect_apply
EXESRC1 = main.cpp
EXESRC2 = main_threaded.cpp
EXESRC3 = main_apply.cpp


# library to be used by the exe and other applications
LIBTARGET = libcdbdirect.a
LIBHEADER = cdbdirect.h

# sources and headers to build the library
LIBSRC = fen2cdb.cpp cdbdirect.cpp
LIBOBJ = $(patsubst %.cpp, %.o, $(LIBSRC))
HEADERS = $(LIBHEADER) fen2cdb.h external/threadpool.hpp

# tools
CXX = g++
CXXFLAGS = -O3 -g -Wall -march=native -fomit-frame-pointer -flto=auto
CXXFLAGS += -DCHESSDB_PATH=\"$(CHESSDB_PATH)\"
AR = ar
ARFLAGS = rcs

# includes and flags to be build the lib
INCFLAGS = -I$(TERARKDBROOT)/output/include -I$(TERARKDBROOT)/third-party/terark-zip/src -I$(TERARKDBROOT)/include
LDFLAGS = -L$(TERARKDBROOT)/output/lib
LIBS = -lterarkdb -lterark-zip-r -lboost_fiber -lboost_context -ljemalloc -pthread -lgcc -lrt -ldl -ltbb -lgomp -lsnappy -llz4 -lz -lbz2 -latomic

.PHONY = all lib clean format

all: $(EXE1) $(EXE2) $(EXE3) lib

lib: $(LIBTARGET)

$(EXE1): $(EXESRC1) $(LIBTARGET) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE1) $(EXESRC1) $(LIBTARGET) $(LDFLAGS) $(LIBS)

$(EXE2): $(EXESRC2) $(LIBTARGET) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE2) $(EXESRC2) $(LIBTARGET) $(LDFLAGS) $(LIBS)

$(EXE3): $(EXESRC3) $(LIBTARGET) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE3) $(EXESRC3) $(LIBTARGET) $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(LIBTARGET): $(LIBOBJ) $(HEADERS)
	$(AR) $(ARFLAGS) $(LIBTARGET) $(LIBOBJ)

format:
	clang-format -i $(EXESRC1) $(EXESRC2) $(EXESRC3) $(LIBSRC) $(HEADERS) $(LIBHEADER)

clean:
	rm -f $(EXE1) $(EXE2) $(EXE3) $(LIBTARGET) $(LIBOBJ)
