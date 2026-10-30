from dataclasses import dataclass


@dataclass(frozen=True)
class OrderInput:
    symbol: str
    side: str
    price: float
    quantity: int
