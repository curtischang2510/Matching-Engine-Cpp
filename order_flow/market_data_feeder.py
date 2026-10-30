from itertools import islice
import logging
from transport import Transport
from typing import Optional
from encoder import ProtobufOrderEncoder
from message_source import CsvTradeSource

class MarketDataFeeder:
    def __init__(self, source: CsvTradeSource, encoder: ProtobufOrderEncoder, transport: Transport):
        self.source = source
        self.encoder = encoder
        self.transport = transport
        self.logger = logging.getLogger("market_data_feeder")

    def run(self, limit: Optional[int] = None) -> None:
        messages = self.source.messages()
        if limit is not None:
            messages = islice(messages, limit)
        with self.transport as transport:
            for message in messages:
                payload = self.encoder.encode(message)
                transport.send(payload)
        self.logger.info("sent packets limit=%s", limit)
