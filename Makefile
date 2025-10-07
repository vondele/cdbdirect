# Needs to point to the root directory of git tree containing a compiled version of a specific fork of rocksDB (see README.md)
TERARKDBROOT = /home/vondele/chess/noob/terarkdb

# Needs to point to the path of the cdb dump
CHESSDB_PATH = /mnt/ssd/chess-20250608/data

# example executables
EXE1 = cdbdirect
EXE1 = explore
EXE2 = cdbdirect_threaded
EXESRC1 = main.cpp
EXESRC1 = explore.cpp
EXESRC2 = main_threaded.cpp

# library to be used by the exe and other applications
LIBTARGET = libcdbdirect.a
LIBHEADER = cdbdirect.h

# sources and headers to build the library
LIBSRC = fen2cdb.cpp cdbdirect.cpp
LIBOBJ = $(patsubst %.cpp, %.o, $(LIBSRC))
HEADERS = $(LIBHEADER) fen2cdb.h external/threadpool.hpp

# tools
CXX = g++
CXXFLAGS = -O3 -g -Wall -march=native -fno-omit-frame-pointer -fno-inline
CXXFLAGS += -DCHESSDB_PATH=\"$(CHESSDB_PATH)\"
AR = ar
ARFLAGS = rcs

# includes and flags to be build the lib
INCFLAGS = -I$(TERARKDBROOT)/output/include -I$(TERARKDBROOT)/third-party/terark-zip/src -I$(TERARKDBROOT)/include
LDFLAGS = -L$(TERARKDBROOT)/output/lib -flto=auto
LIBS = -lterarkdb -lterark-zip-r -lboost_fiber -lboost_context -ljemalloc -pthread -lgcc -lrt -ldl -ltbb -lgomp -lsnappy -llz4 -lz -lbz2 -latomic

.PHONY = all lib clean format

all: $(EXE1) $(EXE2) lib

lib: $(LIBTARGET)

$(EXE1): $(EXESRC1) $(LIBTARGET) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -o $(EXE1) $(EXESRC1) $(LIBTARGET) $(LDFLAGS) $(LIBS)

$(EXE2): $(EXESRC2) $(LIBTARGET) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -o $(EXE2) $(EXESRC2) $(LIBTARGET) $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(LIBTARGET): $(LIBOBJ) $(HEADERS)
	$(AR) $(ARFLAGS) $(LIBTARGET) $(LIBOBJ)

format:
	clang-format -i $(EXESRC1) $(EXESRC2) $(LIBSRC) $(HEADERS) $(LIBHEADER)

clean:
	rm -f $(EXE1) $(EXE2) $(LIBTARGET) $(LIBOBJ)
