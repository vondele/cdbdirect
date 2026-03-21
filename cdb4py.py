import os
import time
import cdbdirect

# 1. Setup Path
default_path = "/mnt/ssd/chess-20251115/data"
db_path = os.environ.get("CHESSDB_PATH", default_path)

db = cdbdirect.CDB(db_path)
print(f"Database contains {db.size()} entries.")

print("Startpos: ", db.get("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"))

count = 0
limit = 100000


# the interface is such that only one c++ thread will call this function at a time.
def process(fen, entries):
    global count, limit

    # Check if we already hit the limit on another thread
    if count >= limit:
        return False

    count += 1
    if False:
        print(f"[{count}] Processing FEN: {fen}")
        for move, score in entries:
            print(f"  -> {move}: {score}")

    return limit > count


# 3. Run
print(f"Starting batch process (limit: {limit})...")
start_time = time.time()
db.apply(process)
end_time = time.time()
print(
    f"Batch process completed in {end_time - start_time:.2f} seconds. Speed {limit / (end_time - start_time):.2f} entries/sec."
)
print("Done.")
