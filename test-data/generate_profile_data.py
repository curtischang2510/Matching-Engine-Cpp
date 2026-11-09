#!/usr/bin/env python3
"""Generate reproducible order flow for profiling matching-engine versions."""

import argparse
import random
from pathlib import Path


TICKERS = ("AAPL", "MSFT", "NVDA", "GOOG", "TSLA", "AMZN")


def generate(count: int, output: Path, seed: int) -> None:
    rng = random.Random(seed)
    issued_ids: list[int] = []
    next_id = 1
    output.parent.mkdir(parents=True, exist_ok=True)

    with output.open("w") as file:
        for _ in range(count):
            if issued_ids and (len(issued_ids) >= 100_000 or rng.random() < 0.3):
                index = rng.randrange(len(issued_ids))
                file.write(f"CANCEL,{issued_ids[index]}\n")
                issued_ids[index] = issued_ids[-1]
                issued_ids.pop()
                continue

            ticker = rng.choice(TICKERS)
            side = rng.choice(("BID", "ASK"))
            cents = rng.randint(4_900, 5_100)  # $49–$51, inside V4's $40–$60 window.
            quantity = rng.randint(1, 100)
            file.write(f"ADD,{next_id},{ticker},{side},{cents // 100}.{cents % 100:02d},{quantity}\n")
            issued_ids.append(next_id)
            next_id += 1

    print(f"wrote {count} messages to {output} (seed={seed})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("count", type=int, help="number of messages to write")
    parser.add_argument("output", type=Path, help="output file")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.count <= 0:
        parser.error("count must be positive")
    generate(args.count, args.output, args.seed)


if __name__ == "__main__":
    main()
