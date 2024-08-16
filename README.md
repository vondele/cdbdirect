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

sample output:

```
-------------------------------------------------------------
Probing: rnbqkb1r/p1pp2pp/1p3n2/5p2/2P5/2NP4/PP3PPP/R1BQKBNR w KQkq -
    f1e2 : 96
    g1f3 : 79
    c1f4 : 63
    h2h3 : 62
    c1e3 : 60
    g1e2 : 56
    a1b1 : 56
    g1h3 : 51
    d3d4 : 46
    c1g5 : 42
    d1e2 : 26
    h2h4 : 24
    a2a4 : 20
    c1d2 : 19
    d1f3 : 4
    a2a3 : 1
    d1d2 : 0
    g2g3 : -56
    Distance to startpos equal or less than 10
Required time: 112.503 microsec.
-------------------------------------------------------------
Probing: rn1qkbnr/ppp1p1pp/4b3/3p1p2/8/5N2/PPPP1PPP/RNBQKB1R b KQkq -
    g8f6 : 139
    Distance to startpos equal or less than 7
Required time: 125.588 microsec.
-------------------------------------------------------------
Probing: r1bqkb1r/ppp2ppp/4pn2/8/3p4/1B1P1Q2/PPP2PPP/RNB1K2R w KQkq -
Fen not found in DB!
Required time: 95.791 microsec.
-------------------------------------------------------------

```

## Building

Once prerequisites are available building is as simple as

```
make 
```

## Prerequisites

### A cdb database dump

A suitably prepared dump of the database. 

These dumps are large (>1TB) and will take several hours to download.

* The following dump contains >40B chess positions
```
wget -c -r -nH --cut-dirs=2 --no-parent --reject="index.html*" -e robots=off ftp://chessdb:chessdb@ftp.chessdb.cn/pub/chessdb/chess-20240814
```

Note that the file `main.cpp` contains the hard-coded path to the DB `cdbdirect_initialize("/mnt/ssd/chess-20211203/data")`, which should be adjusted as needed.

### Compiled rocksdb fork

On Ubuntu, install the needed prerequisites:

```
sudo apt-get install libgoogle-perftools-dev libtbb-dev libaio-dev liblz4-dev libsnappy-dev zlib1g-dev libbz2-dev libgflags-dev
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


