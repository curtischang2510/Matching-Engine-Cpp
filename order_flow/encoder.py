import struct

from generated import order_pb2
from logger.logger import log_latency
from order_input import OrderInput


class NewlineTextEncoder:
    def encode(self, message: str) -> bytes:
        return (message + "\n").encode("ascii")


class ProtobufOrderEncoder:
    @log_latency("protobuf_encode", sample_rate=10)
    def encode(self, message: OrderInput) -> bytes:
        order = self._create_order(message)
        payload = order.SerializeToString()
        return struct.pack("!I", len(payload)) + payload

    def _create_order(self, message: OrderInput) -> order_pb2.Order:
        order = order_pb2.Order()
        order.stock_ticker = message.symbol
        order.side = self._parse_side(message.side)
        order.price = message.price
        order.quantity = message.quantity

        if order.quantity <= 0:
            raise ValueError("quantity must be positive")

        return order

    @staticmethod
    def _parse_side(side_text: str) -> int:
        if side_text == "BID":
            return order_pb2.BID

        if side_text == "ASK":
            return order_pb2.ASK

        raise ValueError(f"unknown side: {side_text}")
