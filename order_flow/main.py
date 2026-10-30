# This is a sample Python script.
import logging
from pathlib import Path


from encoder import ProtobufOrderEncoder
from market_data_feeder import MarketDataFeeder
from message_source import CsvTradeSource
from transport import TcpTransport
from logger.setup import set_async_logging

DEFAULT_PATH = Path("data/all_trades_2020-03-01.csv")
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8001
LINE_LIMIT = 100_000
LOG_PATH = Path("logs/feeder.log")

def main():
    listener = set_async_logging(LOG_PATH, logging.DEBUG)
    listener.start()
    logger = logging.getLogger("market_data_feeder")
    try:
        logger.info("feeder starting host=%s port=%d limit=%s", DEFAULT_HOST, DEFAULT_PORT, LINE_LIMIT)

        source = CsvTradeSource(DEFAULT_PATH)
        encoder = ProtobufOrderEncoder()
        transport = TcpTransport(DEFAULT_HOST, DEFAULT_PORT)
        feeder = MarketDataFeeder(source, encoder, transport)
        feeder.run(limit=LINE_LIMIT)

        logger.info("feeder stopped")
    finally:
        listener.stop()

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()

# See PyCharm help at https://www.jetbrains.com/help/pycharm/
