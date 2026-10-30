from dataclasses import dataclass
from enum import Enum


class Side(Enum):
    BUY = "B"
    SELL = "S"


@dataclass(frozen=True)
class ItchHeader:
    stock_locate: int
    timestamp_ns: int


@dataclass(frozen=True)
class AddOrderMessage(ItchHeader):
    order_id: int  # 8-byte order reference number
    side: Side
    shares: int  # 4-byte
    stock: str  # 8-byte alpha, space-padded on the wire
    price: int  # 4-byte Price(4): integer in ticks (dollars * 10000)


@dataclass(frozen=True)
class OrderExecutedMessage(ItchHeader):
    order_id: int
    executed_shares: int  # 4-byte
    match_number: int  # 8-byte day-unique execution id (not used)


@dataclass(frozen=True)
class OrderCancelMessage(ItchHeader):
    order_id: int
    canceled_shares: int  # 4-byte


@dataclass(frozen=True)
class OrderDeleteMessage(ItchHeader):
    order_id: int


ItchMessage = (
    AddOrderMessage | OrderExecutedMessage | OrderCancelMessage | OrderDeleteMessage
)
