import logging
from pathlib import Path

from ITCH.encoder import encode_itch_message
from itch_line_source import messages_from_file
from logger.setup import set_async_logging
from MoldUDP.feed_publisher import MulticastFeedPublisher
from MoldUDP.history import SentMessageHistory
from MoldUDP.mold_udp import MoldUdpPacketBuilder
from MoldUDP.retransmission import RetransmissionResponder

ITCH_INPUT_PATH = Path("feed.itch")
LOG_PATH = Path("logs/exchange.log")

MCAST_GROUP = "239.1.1.1"
MCAST_PORT = 9002
MCAST_INTERFACE = "127.0.0.1"
REQUEST_HOST = "127.0.0.1"
REQUEST_PORT = 9001


def main():
    listener = set_async_logging(LOG_PATH, logging.DEBUG)
    listener.start()
    # name must match the logger configured in set_async_logging()
    logger = logging.getLogger("market_data_feeder")

    history = SentMessageHistory()
    responder = RetransmissionResponder(REQUEST_HOST, REQUEST_PORT, history)
    responder.start()
    logger.info("responder listening on %s:%d", REQUEST_HOST, REQUEST_PORT)

    publisher = MulticastFeedPublisher(MCAST_GROUP, MCAST_PORT, MCAST_INTERFACE)

    sequence = 1  # one message per packet -> +1 each time
    try:
        for message in messages_from_file(ITCH_INPUT_PATH):
            encoded = encode_itch_message(message)
            history.record(sequence, encoded)
            publisher.send(MoldUdpPacketBuilder(sequence).add(encoded).build())
            sequence += 1

        logger.info("feed complete, sent %d messages", sequence - 1)
        input("feed done - serving retransmissions; press Enter to shut down\n")
    finally:
        publisher.close()
        responder.stop()
        responder.join(timeout=2)
        listener.stop()


if __name__ == "__main__":
    main()
