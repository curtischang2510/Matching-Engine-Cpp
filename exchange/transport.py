from abc import ABC, abstractmethod
import socket
from types import TracebackType
from typing import Optional, Type

from logger.logger import log_latency


class Transport(ABC):
    @abstractmethod
    def connect(self) -> None:
        pass

    @abstractmethod
    def send(self, payload: bytes) -> None:
        pass

    @abstractmethod
    def close(self) -> None:
        pass

    @abstractmethod
    def __enter__(self) -> "Transport":
        pass

    @abstractmethod
    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_value: Optional[BaseException],
        traceback: Optional[TracebackType],
    ) -> bool:
        pass

class TcpTransport(Transport):
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.connect((self.host, self.port))

    @log_latency("send_tcp", 10)
    def send(self, payload: bytes):
        if self.sock is None:
            raise RuntimeError("transport is not connected")
        self.sock.sendall(payload)

    def close(self):
        if self.sock is None:
            return
        self.sock.close()
        self.sock = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_value: Optional[BaseException],
        traceback: Optional[TracebackType],
    ) -> bool:
        self.close()
        return False


class UdpTransport(Transport):
    # One send() == one UDP datagram == one MoldUDP packet. The caller must pass
    # an already-built packet; this transport does no framing of its own (UDP
    # preserves datagram boundaries, unlike TCP's byte stream, so there is no
    # length prefix to add here).
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # UDP has no handshake; connect() just fixes the default destination so
        # send() works (and lets the OS surface ICMP "port unreachable" as an
        # error on a later send if the receiver isn't listening). No TCP_NODELAY
        # here — that is a TCP-only (Nagle) option.
        self.sock.connect((self.host, self.port))

    @log_latency("send_udp", 10)
    def send(self, payload: bytes):
        if self.sock is None:
            raise RuntimeError("transport is not connected")
        # send(), not sendall(): a datagram is sent whole or not at all, so
        # there is no partial-write loop the way TCP needs. Keep each payload
        # under the path MTU (~1472 bytes on Ethernet) to avoid IP fragmentation.
        self.sock.send(payload)

    def close(self):
        if self.sock is None:
            return
        self.sock.close()
        self.sock = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_value: Optional[BaseException],
        traceback: Optional[TracebackType],
    ) -> bool:
        self.close()
        return False
