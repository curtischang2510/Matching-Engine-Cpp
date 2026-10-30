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
