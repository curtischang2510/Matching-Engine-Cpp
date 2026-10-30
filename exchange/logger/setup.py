import logging
import queue
from logging.handlers import QueueHandler, QueueListener
from pathlib import Path


def set_async_logging(log_path: Path, level: int = logging.INFO) -> QueueListener:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_queue: queue.SimpleQueue[logging.LogRecord] = queue.SimpleQueue()
    queue_handler = QueueHandler(log_queue)

    file_handler = logging.FileHandler(log_path)
    file_handler.setLevel(level)
    file_handler.setFormatter(
        logging.Formatter(
            "%(asctime)s %(levelname)s %(name)s %(message)s"
        )
    )

    logger = logging.getLogger("market_data_feeder")
    logger.setLevel(level)
    logger.handlers.clear()
    logger.addHandler(queue_handler)
    logger.propagate = False

    return QueueListener(log_queue, file_handler, respect_handler_level=True)
