import functools
import logging
import time
from collections.abc import Callable
from typing import ParamSpec, TypeVar

P = ParamSpec("P")
R = TypeVar("R")


def log_latency(name: str, sample_rate: int = 1) -> Callable[[Callable[P, R]], Callable[P, R]]:
    if sample_rate <= 0:
        raise ValueError("sample_rate must be greater than 0")

    logger = logging.getLogger("market_data_feeder.latency")
    call_count = 0

    def decorator(func: Callable[P, R]) -> Callable[P, R]:
        nonlocal call_count

        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> R:
            nonlocal call_count

            start_ns = time.perf_counter_ns()
            try:
                return func(*args, **kwargs)
            finally:
                elapsed_ns = time.perf_counter_ns() - start_ns
                call_count += 1

                if call_count % sample_rate == 0:
                    logger.debug("latency name=%s elapsed_ns=%d", name, elapsed_ns)

        return wrapper

    return decorator