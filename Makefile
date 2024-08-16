# Needs to point to the root directory of git tree containing a compiled version of a specific fork of rocksDB (see README.md)
TERARKDBROOT = /home/vondele/chess/noob/terarkdb

SRC1 = main.cpp
SRC2 = main_threaded.cpp
SRC = fen2cdb.cpp cdbdirect.cpp
HEADERS = fen2cdb.h cdbdirect.h external/threadpool.hpp

EXE1 = cdbdirect
EXE2 = cdbdirect_threaded

CXX = g++
CXXFLAGS = -O3 -I$(TERARKDBROOT)/output/include -I$(TERARKDBROOT)/third-party/terark-zip/src -I$(TERARKDBROOT)/include
LDFLAGS = -L$(TERARKDBROOT)/output/lib
LIBS = -lterarkdb -lterark-zip-r -lboost_fiber -lboost_context -ltcmalloc -pthread -lgcc -lrt -ldl -ltbb -laio -lgomp -lsnappy -llz4 -lz -lbz2

all: $(EXE1) $(EXE2)

$(EXE1): $(SRC1) $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE1) $(SRC1) $(SRC) $(LDFLAGS) $(LIBS)

$(EXE2): $(SRC2) $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE2) $(SRC2) $(SRC) $(LDFLAGS) $(LIBS)

format:
	clang-format -i $(SRC1) $(SRC2) $(SRC) $(HEADERS)

clean:
	rm -f $(EXE1) $(EXE2)
