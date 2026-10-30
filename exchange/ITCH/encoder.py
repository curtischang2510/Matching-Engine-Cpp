import struct
from functools import singledispatch

from ITCH.model import (
    AddOrderMessage,
    ItchHeader,
    ItchMessage,
    OrderCancelMessage,
    OrderDeleteMessage,
    OrderExecutedMessage,
)

def _pack_header(message_type: bytes, msg: ItchHeader) -> bytes:
    if msg.stock_locate >= (1 << 16):
        raise ValueError("stock locate does not fit 16 bit")
    if msg.timestamp_ns >= (1 << 48):
        raise ValueError("timestamp does not fit 48 bit")
    fused = (msg.stock_locate << 48) | msg.timestamp_ns
    return message_type + struct.pack(">Q", fused)


def _pack_stock(stock: str) -> bytes:
    return f"{stock:<8.8}".encode("ascii")


@singledispatch
def encode_itch_message(message: ItchMessage) -> bytes:
    raise NotImplementedError(f"no encoder registered for {type(message).__name__}")


@encode_itch_message.register
def _(message: AddOrderMessage) -> bytes:
    return (
        _pack_header(b"A", message)
        + struct.pack(">Q", message.order_id)
        + message.side.value.encode("ascii")
        + struct.pack(">I", message.shares)
        + _pack_stock(message.stock)
        + struct.pack(">I", message.price)  # price already in ticks, no scaling
    )


@encode_itch_message.register
def _(message: OrderExecutedMessage) -> bytes:
    return _pack_header(b"E", message) + struct.pack(
        ">QIQ", message.order_id, message.executed_shares, message.match_number
    )


@encode_itch_message.register
def _(message: OrderCancelMessage) -> bytes:
    return _pack_header(b"X", message) + struct.pack(
        ">QI", message.order_id, message.canceled_shares
    )


@encode_itch_message.register
def _(message: OrderDeleteMessage) -> bytes:
    return _pack_header(b"D", message) + struct.pack(">Q", message.order_id)
