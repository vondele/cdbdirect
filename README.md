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
...
-------------------------------------------------------------
Probing: rn1q1rk1/pbppnpbp/1p2p1p1/6B1/2BPP3/2N2N2/PPP2PPP/R2Q1RK1 w - -
    d1d2 : 133
    d4d5 : 70
    h2h4 : 62
    e4e5 : 41
    g5f4 : 26
    f1e1 : 20
    d1e2 : 18
    g5h4 : 18
    c4d3 : 17
    h2h3 : 17
    c4b3 : 17
    d1c1 : 16
    a2a4 : 15
    a1b1 : 10
    g5e3 : 7
    a2a3 : 2
    c4e2 : 2
    g5d2 : 1
    g5c1 : 1
    c4b5 : -10
    b2b4 : -13
    d1d3 : -13
    g1h1 : -18
    g5e7 : -18
    b2b3 : -37
    c4d5 : -377
    Distance to startpos equal or less than 14
Required time: 36.279 (17.794) microsec.
-------------------------------------------------------------
Probing: rnb1kbnr/pp2qpp1/2pp3p/4p3/3P4/2PBPN2/PP1N1PPP/R1BQK2R w KQkq -
Fen not found in DB!
Required time: 25.659 (17.453) microsec.
-------------------------------------------------------------
...
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


