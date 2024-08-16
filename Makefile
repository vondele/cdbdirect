# Needs to point to the root directory of git tree containing a compiled version of a specific fork of rocksDB (see README.md)
TERARKDBROOT = /home/vondele/chess/noob/terarkdb

SRC = main.cpp fen2cdb.cpp cdbdirect.cpp
HEADERS = fen2cdb.h cdbdirect.h
EXE = cdbdirect

CXX = g++
CXXFLAGS = -O3 -I$(TERARKDBROOT)/output/include -I$(TERARKDBROOT)/third-party/terark-zip/src -I$(TERARKDBROOT)/include
LDFLAGS = -L$(TERARKDBROOT)/output/lib
LIBS = -lterarkdb -lterark-zip-r -lboost_fiber -lboost_context -ltcmalloc -pthread -lgcc -lrt -ldl -ltbb -laio -lgomp -lsnappy -llz4 -lz -lbz2

all: $(EXE)

$(EXE): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(EXE) $(SRC) $(LDFLAGS) $(LIBS)

format:
	clang-format -i $(SRC) $(HEADERS)

clean:
	rm -f cdbdirect
