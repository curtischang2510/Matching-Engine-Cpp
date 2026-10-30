import csv
from decimal import Decimal, InvalidOperation
from pathlib import Path

from order_input import OrderInput


QUANTITY_SCALE = Decimal("100000000")
REQUIRED_COLUMNS = {"symbol", "side", "price", "amount"}


class FileMessageSource:
    def __init__(self, path: Path):
        self.path = path

    def messages(self):
        with open(self.path, "r") as f:
            for line in f:
                yield line.strip()


class CsvTradeSource:
    def __init__(self, path: Path):
        self.path = path

    def messages(self):
        with self.path.open("r", newline="", encoding="utf-8") as file:
            reader = csv.DictReader(file)
            self._validate_columns(reader.fieldnames)

            for row_number, row in enumerate(reader, start=2):
                try:
                    yield OrderInput(
                        symbol=row["symbol"],
                        side=self._parse_side(row["side"]),
                        price=float(row["price"]),
                        quantity=self._parse_quantity(row["amount"]),
                    )
                except (InvalidOperation, KeyError, TypeError, ValueError) as error:
                    raise ValueError(f"invalid trade at CSV row {row_number}: {error}") from error

    @staticmethod
    def _validate_columns(fieldnames) -> None:
        if fieldnames is None:
            raise ValueError("CSV file has no header")

        missing = REQUIRED_COLUMNS.difference(fieldnames)
        if missing:
            raise ValueError(f"CSV file is missing columns: {sorted(missing)}")

    @staticmethod
    def _parse_side(side: str) -> str:
        normalized = side.strip().lower()
        if normalized == "buy":
            return "BID"
        if normalized == "sell":
            return "ASK"
        raise ValueError(f"unknown side: {side}")

    @staticmethod
    def _parse_quantity(amount: str) -> int:
        quantity = int(Decimal(amount) * QUANTITY_SCALE)
        if quantity <= 0:
            raise ValueError("amount must be positive after scaling")
        return quantity
