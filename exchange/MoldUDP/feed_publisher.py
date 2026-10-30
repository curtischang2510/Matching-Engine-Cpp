import socket


class MulticastFeedPublisher:
    def __init__(self, group: str, port: int, interface: str = "127.0.0.1"):
        self._destination = (group, port)
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        self._sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(interface))

    def send(self, packet: bytes):
        self._sock.sendto(packet, self._destination)

    def close(self):
        self._sock.close()
