# Order Matching Engine

A C++23 exchange-style matching engine built to explore how order book design affects throughput. It accepts limit orders and cancellations, matches by price and arrival order, and maintains a separate book for each symbol.

## Architecture

A producer routes commands into SPSC queues. Each worker owns its books, so matching a symbol requires no shared book lock. The diagram shows the sharded design; the current profiling workload routes all commands to one shard for a controlled comparison.

```mermaid
flowchart LR
    source(("Disk /<br/>Network")) --> parser{"Order Parser"}

    subgraph core0["Core 0"]
        direction LR
        queue0["SPSC Queue"] --> engine0["Matching Engine"]
        engine0 -.-> memory0["Preallocated<br/>Data Structures"]
    end

    subgraph core1["Core 1"]
        direction LR
        queue1["SPSC Queue"] --> engine1["Matching Engine"]
        engine1 -.-> memory1["Preallocated<br/>Data Structures"]
    end

    parser -->|"AAPL, MSFT"| queue0
    parser -->|"AMD, NVDA"| queue1

    classDef input fill:#F1F5F9,stroke:#64748B,color:#0F172A,stroke-width:2px
    classDef queue fill:#DBEAFE,stroke:#2563EB,color:#1E3A8A,stroke-width:2px
    classDef engine fill:#EDE9FE,stroke:#7C3AED,color:#4C1D95,stroke-width:2px
    classDef memory fill:#DCFCE7,stroke:#16A34A,color:#14532D,stroke-width:2px

    class source,parser input
    class queue0,queue1 queue
    class engine0,engine1 engine
    class memory0,memory1 memory

    style core0 fill:#F8FAFC,stroke:#94A3B8,stroke-width:2px
    style core1 fill:#F8FAFC,stroke:#94A3B8,stroke-width:2px
```

## Measuring performance

V3, V4, and V5 preserve the same matching interface while changing the underlying book. Deterministic workloads make the versions comparable, and separate executables make it possible to profile each one. The benchmark measures the complete producer-to-worker path; a direct-book benchmark helps isolate matching and cancellation costs.

The [workload description](test-data/GENERATED_WORKLOAD.md) records the command sequence and timing boundaries. [Benchmark notes](V5/benchmarks/EXPERIMENT.md) contain the comparison method and results.
