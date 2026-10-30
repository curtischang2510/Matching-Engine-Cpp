from typing import Optional


class SentMessageHistory:
    def __init__(self):
        self._by_seq: dict[int, bytes] = {}

    def record(self, sequence: int, message: bytes) -> None:
        self._by_seq[sequence] = message

    def get(self, sequence: int) -> Optional[bytes]:
        return self._by_seq.get(sequence)
