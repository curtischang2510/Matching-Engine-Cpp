# V5 cancellation experiment — 2026-11-09

## Question

Does replacing each price level's deque with a linked list and an order-ID
lookup improve cancellation throughput when many orders rest at one price?

The three books differ only in order storage and cancellation lookup:

The intrusive implementation was named 5.1 when these measurements were
recorded. It is now named 5.4; only its version label changed.
The current 5.1 uses a Boost flat hash map, and 5.2 starts as a copy of that
version. Neither is included in the historical results below.

| Version | Resting orders at one price | Cancel lookup/removal |
| --- | --- | --- |
| 4.0 | `std::deque` | ID → price, then linear search and erase |
| 5.0 | `std::list` | ID → list iterator, then erase |
| 5.4 | Intrusive doubly linked list | ID → map-owned node, then unlink |

V4.1 was a copy of 4.0 when these results were recorded, so this comparison
uses 4.0 as its deque baseline. V4.1 has since changed.

## Machine and build

- Mac Studio, Apple M5 Max, 18 CPU cores, 36 GB RAM; macOS 27.0.1; arm64.
- Apple clang 21.0.0; C++23; CMake `RelWithDebInfo` with
  `-O2 -g -DNDEBUG -fno-omit-frame-pointer`.
- One producer and one worker. All six ticker books are created before timing;
  all commands go to shard 0. Shard 1 receives none. Threads are not pinned.
- The original run used `V5/matching_engine_v5.dSYM` (UUID
  `ED9C7599-8D06-3090-87AF-DB1A4B4F99B5`). Current builds have one executable per version.

## Input, identical for every version

The exact sequence, deterministic shuffle, and timing boundaries are
documented in `test-data/GENERATED_WORKLOAD.md`.

The default `--generated 15000000` mode generates **15,000,000 commands in memory** as it
submits them. It does not read a CSV or pre-generate all commands. There are
14,251,500 adds and 748,500 cancels (4.99% cancels). IDs increase for adds;
cancel targets are distinct live IDs selected from a shuffled list with seed
42. The six tickers rotate by batch. All orders use 50,000 ticks ($50), inside
the preallocated dense window.

Each half contains 1,500 batches of 5,000 commands:

| Phase | Commands per batch | First cancel |
| --- | --- | --- |
| First 7.5M | 4,999 bids of quantity 1, then one ask fills all 4,999 | None |
| Second 7.5M | 4,500 bids of quantity 1, 499 valid shuffled cancels, then one ask fills the remaining 4,001 | Command 7,504,501 |

The sweep at the end of each batch bounds live memory while ensuring every
cancel in the second phase targets a deep, live price level. This is an
intentional cancellation stress case. It does not exercise tree prices or
represent typical market order flow.

## What the two timings mean

- **Pipeline:** `matching_engine_vX_Y --generated 15000000`. `seconds` is
  measured on the worker from its first successful SPSC pop until the queue
  drains. It includes queue transfers, ticker lookup, dispatch, book operations,
  and any wait for the producer. It excludes book construction and thread
  creation. The producer may busy-wait when the queue is full.
- **Direct:** `delayed_cancel_bench VERSION`. The timer starts after six book
  constructions and the cancellation shuffle. It measures direct `apply()`
  calls, loop control, and per-batch L1 checks; there is no producer, SPSC
  queue, file I/O, or thread creation.

The script checks 15M messages, 14,251,500 adds, 748,500 cancels, the first
cancel position, and empty final L1 for each pipeline run. The direct benchmark
also checks after each cancellation batch that exactly 4,001 bids remain, then
checks that the sweep empties the book. `order_book_list_test` passed as a
separate behavioral regression check.

## Raw results

Five sequential runs per case, with case order shuffled each round. Times are
seconds; lower is better. These are ordinary runs, not Samply timings.

| Run | Pipeline 4.0 | Pipeline 5.0 | Pipeline 5.4 | Direct 4.0 | Direct 5.0 | Direct 5.4 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 2.138690 | 1.338080 | 1.220490 | 1.193610 | 0.415560 | 0.295636 |
| 2 | 2.123770 | 1.406960 | 1.224030 | 1.189360 | 0.430291 | 0.299887 |
| 3 | 2.142990 | 1.363160 | 1.228370 | 1.183750 | 0.421626 | 0.302072 |
| 4 | 2.141880 | 1.367500 | 1.217700 | 1.164620 | 0.453257 | 0.308808 |
| 5 | 2.127280 | 1.364340 | 1.232770 | 1.179520 | 0.483314 | 0.320252 |
| **Median** | **2.138690** | **1.364340** | **1.224030** | **1.183750** | **0.430291** | **0.302072** |

| Version | Pipeline commands/s | Pipeline speedup | Direct commands/s | Direct speedup |
| --- | ---: | ---: | ---: | ---: |
| 4.0 | 7.01M | 1.00× | 12.67M | 1.00× |
| 5.0 | 10.99M | 1.57× | 34.86M | 2.75× |
| 5.4 | **12.25M** | **1.75×** | **49.66M** | **3.92×** |

The difference between direct and pipeline speedups shows that queue,
routing, and command-production costs limit the end-to-end benefit. An older
random workload used 7.45M cancels and shallow levels; its time must not be
compared with this workload as if the book alone changed.

## Reproduce and profile

From the repository root:

```sh
python3 V5/benchmarks/run_delayed_cancel_bench.py --runs 5
./V5/matching_engine_v5_4 --generated 15000000
samply record --rate 10000 ./V5/matching_engine_v5_4 --generated 15000000
```

The Python script configures and builds the three pipeline executables and direct benchmark before running all
three versions. Use ordinary runs for throughput numbers; Samply adds profiling
overhead and is for locating hot code. The direct benchmark executable is at
`build-instruments/V5/delayed_cancel_bench`, with a matching dSYM beside it.
