import socket
import struct
import threading

from MoldUDP.history import SentMessageHistory
from MoldUDP.mold_udp import MoldUdpPacketBuilder

REQUEST_FORMAT = ">QH"  # sequence(8) + count(2) big-endian
REQUEST_SIZE = struct.calcsize(REQUEST_FORMAT)


def build_request(sequence: int, count: int) -> bytes:
    return struct.pack(REQUEST_FORMAT, sequence, count)


def parse_request(data: bytes) -> tuple[int, int]:
    if len(data) != REQUEST_SIZE:
        raise ValueError(
            f"invalid request size: {len(data)} bytes, expected {REQUEST_SIZE}"
        )
    return struct.unpack(REQUEST_FORMAT, data)


class RetransmissionResponder(threading.Thread):
    def __init__(
        self, request_host: str, request_port: int, history: SentMessageHistory
    ):
        super().__init__(name="retransmission-responder", daemon=True)
        self._history = history
        self._running = True

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((request_host, request_port))

    def run(self) -> None:
        while self._running:
            try:
                data, addr = self._sock.recvfrom(65535)  # blocks; fine, own thread
            except OSError:
                break  # stop() closed the socket underneath us
            try:
                sequence, count = parse_request(data)
            except ValueError:
                continue  # malformed request: drop it, keep serving

            reply = MoldUdpPacketBuilder(sequence)  # reply starts at requested seq
            for i in range(count):
                message = self._history.get(sequence + i)
                if message is not None:
                    reply.add(message)

            # Reply to the request's SOURCE address — the (ip, port) the client
            # sent from. The client waits on that same socket for the reply.
            self._sock.sendto(reply.build(), addr)

    def stop(self) -> None:
        self._running = False
        self._sock.close()  # unblocks recvfrom -> OSError -> run() exits
