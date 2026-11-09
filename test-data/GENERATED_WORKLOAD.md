# Generated matching-engine workload

This describes `--generated 15000000` in `V4/main.cpp`, which is the runner
shared by executables 4.0, 4.1, 5.0, 5.1, 5.2, and 5.4. Each executable generates the
same command sequence. It creates commands as it sends them to the SPSC queue;
it does not read a file or retain all 15 million commands in memory.

## Command sequence

There are two phases, each with 1,500 batches of 5,000 commands:

| Phase | Commands in each batch | Commands in phase |
| --- | --- | ---: |
| 1 | 4,999 BID adds of quantity 1, then one ASK add of quantity 4,999 that fills them | 7,500,000 |
| 2 | 4,500 BID adds of quantity 1, then 499 cancels of live bids, then one ASK add of quantity 4,001 that fills the rest | 7,500,000 |

Total: **15,000,000 commands = 14,251,500 adds + 748,500 cancels**. The
first cancel is command **7,504,501** (one-based). The maximum number of
resting orders in one symbol's book during this workload is **4,999**, because
the final ask clears each batch before that book is used again.

- Six symbols rotate by batch: AAPL, MSFT, NVDA, GOOG, TSLA, AMZN. Each gets
  250 batches per phase. All six books are constructed before timing.
- Every order is priced at **50,000 ticks ($50)**, inside the dense price
  window. BID quantities are 1. The final ASK has the exact quantity needed
  to clear the batch. No other prices, sides, or quantities are sampled.
- Add IDs start at 1 and increase by one, including the final ASK in each
  batch. A cancellation carries the ID of an existing BID; it gets no new ID.

## What is random?

Only the **positions of the 499 canceled BIDs within a phase-2 batch** are
shuffled. The program shuffles offsets 0 through 4,499 once with
`std::mt19937` seed **42**, then uses the first 499 offsets in every phase-2
batch. Thus the same relative positions are canceled in every batch, and no
order is canceled twice. This is deterministic for repeated runs with the
same build and C++ standard library. The exact shuffled order can differ
across standard-library implementations.

There is **no Gaussian distribution** in this workload. The separate
`test-data/generate_profile_data.py` script creates a randomized CSV using
uniform discrete choices for prices, tickers, sides, and quantities, but
`--generated` does not call it or read its output.

## What is measured?

The pipeline has one producer, one worker, and two shards; all commands go
to shard 0. The producer spins when its SPSC queue is full. The printed
`seconds` starts at the worker's first successful pop and ends after it drains
the queue. It includes queue waits, routing, dispatch, and book work. Book
construction and thread creation happen before this timer. `wall_seconds`
starts after book construction and worker readiness; unlike worker `seconds`,
it also includes the producer's setup before the first pop.

`V5/benchmarks/delayed_cancel_bench.cpp` applies the same two-phase command
pattern directly to six books. Its timer excludes book construction and the
offset shuffle. It has no producer, queue, routing, or file I/O. Its ADD
messages use ticker `TEST`; the book operation sequence and per-book ID
sequence match the pipeline pattern.

The comparison script `V5/benchmarks/run_delayed_cancel_bench.py` runs versions
4.0, 5.0, 5.1, 5.2, and 5.4 in both modes. It shuffles the **order of benchmark cases**
with Python seed 42 each round; this does not change the commands within a
case. Version 4.1 can run the same pipeline workload directly:

```sh
./V4/matching_engine_v4_1 --generated 15000000
```

For pipeline runs, the script checks the command totals, first-cancel position,
and empty final book. The direct benchmark checks the quantity after each
cancellation batch and that each batch clears. Timings still vary with CPU
scheduling and system load, so compare
multiple ordinary runs and report the median. This workload specifically
stresses cancellations deep in a single price level; it does not exercise
out-of-window tree prices or model typical market order flow.
