# cdbdirect

`cdbdirect` is a proof of concept implementation to directly probe a local copy of a snapshot of the Chess Cloud Database (cdb),
which is also [accessible online](https://www.chessdb.cn/queryc_en/).

Even though API access to cdb is easily possible (see e.g. [cdbexplore](https://github.com/vondele/cdbexplore/) or [cdblib](https://github.com/robertnurnberg/cdblib/) for tools that do so), local access can be significantly faster.

## Usage

### Binaries

The single threaded PoC code just probes the local copy of cdb and prints the ranked moves with
their scores for all fens in a specific file. Note that fens should have
`-` for the ep if no legal ep move is possible (including pinned pawns), and that
move counters are ignored. By default the file is assumed to be
`caissa_sorted_100000.epd` available at
[caissatrack](https://github.com/robertnurnberg/caissatrack)) :

```bash
./cdbdirect
```

sample output:

```txt
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

The threaded version of the PoC code looks for fens that are unknown to cdb in
a specific file, and writes them to `unknown.epd` :

```bash
./cdbdirect_threaded grob_popular_T60t7_cdb.epd
```

sample output:

```txt
Loading: grob_popular_T60t7_cdb.epd
Opened DB with 44262943988 stored positions.
Probing 35754929 fens with 32 threads.
known fens:   35754622
unknown fens: 307
scored moves: 183986252
Required probing time:         74.5051 sec.
Required time per fen: 2.08377 microsec.
```

### Interface

The interface to probe has been kept very simple, with only 4 functions exposed by `cdbdirect.h`

```c++
std::uintptr_t cdbdirect_initialize(const std::string &path);
std::uint64_t cdbdirect_size(std::uintptr_t handle);
std::uintptr_t cdbdirect_finalize(std::uintptr_t handle);
std::vector<std::pair<std::string, int>> cdbdirect_get(std::uintptr_t handle,
                                                       const std::string &fen);
```

See the `Makefile` for how a tool can link to the `libcdbdirect.a` library.

## Building

Once prerequisites are available building is as simple as

```bash
make -j
```

## Prerequisites

### A cdb database dump

A suitably prepared dump of the database.

These dumps are large (~1TB, >50B positions) and might take several hours to download.

* Dumps can be obtained from [Hugging Face](https://huggingface.co/datasets/robertnurnberg/chessdbcn).

```bash
hf download --repo-type dataset robertnurnberg/chessdbcn --local-dir . --cache-dir . --include "chess-20250608/**"
```

* Alternatively, and slower, at the chessdb source:

```bash
wget -c -r -nH --cut-dirs=2 --no-parent --reject="index.html*" -e robots=off ftp://chessdb:chessdb@ftp.chessdb.cn/pub/chessdb/chess-20251115
```

* Or faster, with rclone:

```bash
rclone copy chessdb:/pub/chessdb/chess-20251115 ./chess-20251115  --transfers=10 --checkers=20 --multi-thread-streams=4 --multi-thread-chunk-size=128M --multi-thread-cutoff=1   --no-traverse     --exclude "index.html*"     --progress
```

After creating a config for the remote chessdb:

```txt
    Type: ftp
    Host: ftp.chessdb.cn
    User: chessdb
    Pass: chessdb
    Explicit TLS: false

```

Note: for an older version of the DB, to be able to handle the database the user
should be able to open a sufficiently large number of files (more than the
typical default of 1024), increase that limit e.g. `ulimit -n 102400` for each
shell manually or adjust the defaults (e.g. `/etc/security/limits.conf`,
`/etc/systemd/system.conf`, and/or `/etc/systemd/user.conf`).

### Compiled rocksdb fork

On Ubuntu, install the needed prerequisites:

```bash
sudo apt-get install libboost-fiber-dev libtbb-dev autoconf cmake build-essential curl git
```

Clone noobpwnftw's terakdb repo and build it

```bash
git clone --depth 1 https://github.com/noobpwnftw/terarkdb.git
cd terarkdb
./build.sh
```

After building, ensure this repo's Makefile variables `TERARKDBROOT` and `CHESSDB_PATH` point to `/the/full/path/of/terarkdb/`
and the path of the DB dump, respectively.
