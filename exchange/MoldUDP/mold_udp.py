import struct


class MoldUdpPacketBuilder:
    def __init__(self, sequence_number: int):
        self._sequence_number = sequence_number
        self._messages: list[bytes] = []

    def add(self, encoded_message: bytes) -> "MoldUdpPacketBuilder":
        self._messages.append(encoded_message)
        return self

    def build(self) -> bytes:
        header = struct.pack(">QH", self._sequence_number, len(self._messages))
        body = b"".join(struct.pack(">H", len(m)) + m for m in self._messages)
        return header + body
