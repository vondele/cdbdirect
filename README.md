# cdbdirect

`cdbdirect` is a proof of concept implementation to directly probe a local copy of a snapshot of the Chess Cloud Database (cdb),
which is also [accessible online](https://www.chessdb.cn/queryc_en/).

Even though API access to cdb is easily possible (see e.g. [cdbexplore](https://github.com/vondele/cdbexplore/) or [cdblib](https://github.com/robertnurnberg/cdblib/) for tools that do so), local access can be significantly faster.

## Usage

The PoC code just probes the local copy of cdb and prints the ranked moves with their scores for all fens
in a specific file (currently hard-coded to `caissa_sorted_100000.epd` available at [caissatrack](https://github.com/robertnurnberg/caissatrack)) :

```
./cdbdirect
```

## Building

Once prerequisites are available building is as simple as

```
make 
```

## Prerequisites

### A cdb database dump

A suitably prepared dump of the database. 

These dumps are large (e.g. 200GB - >1TB) and will take several hours to download.

* An smaller, old dump, useful for 'quick' testing:
```
wget -c ftp://chessdb:chessdb@ftp.chessdb.cn/pub/chessdb/chess-20211203.tar
tar -xvf chess-20211203.tar
```
* A recent dump containing > 40B chess positions:
```
TODO
```

Note that the file `cdbdirect.cpp` contains the hard-coded path to the DB `kDBPath`, which should be adjusted as needed.

### Compiled rocksdb fork

On Ubuntu, install the needed prerequisites:

```
sudo apt-get install libgoogle-perftools-dev libtbb-dev libaio-dev liblz4-dev libsnappy libsnappy-dev zlib1g-dev libbz2-dev libgflags-dev
```

Clone the needed repos, including terakdb, and build

```
git clone https://github.com/noobpwnftw/ssdb.git
cd ssdb/deps
git clone https://github.com/noobpwnftw/terarkdb.git
cd ..
make -j
```

Ensure the Makefile variable `TERARKDBROOT` points to the full path of `ssdb/deps/terarkdb`


