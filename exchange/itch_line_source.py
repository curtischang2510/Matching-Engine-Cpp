from typing import Iterator

from ITCH.model import (
    AddOrderMessage,
    ItchMessage,
    OrderCancelMessage,
    OrderDeleteMessage,
    OrderExecutedMessage,
    Side,
)


def parse_itch_line(line: str) -> ItchMessage:
    parts = line.strip().split(",")
    message_type = parts[0]

    # Tuple unpacking raises ValueError on the wrong field count -> loud failure,
    # which is what you want for a malformed feed line.
    if message_type == "A":
        _, locate, ts, order_id, side, shares, stock, price = parts
        return AddOrderMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            side=Side(side),                 # "B" -> Side.BUY, "S" -> Side.SELL
            shares=int(shares),
            stock=stock,
            price=int(price),                # already in ticks
        )
    if message_type == "E":
        _, locate, ts, order_id, executed, match = parts
        return OrderExecutedMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            executed_shares=int(executed),
            match_number=int(match),
        )
    if message_type == "X":
        _, locate, ts, order_id, canceled = parts
        return OrderCancelMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
            canceled_shares=int(canceled),
        )
    if message_type == "D":
        _, locate, ts, order_id = parts
        return OrderDeleteMessage(
            stock_locate=int(locate),
            timestamp_ns=int(ts),
            order_id=int(order_id),
        )
    raise ValueError(f"unknown ITCH message type: {message_type!r}")


def messages_from_file(path) -> Iterator[ItchMessage]:
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):    # allow blanks + comments
                continue
            yield parse_itch_line(line)