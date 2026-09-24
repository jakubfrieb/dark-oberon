"""UDF error hierarchy."""

from __future__ import annotations


class UDFError(Exception):
    """Base class for all UDF errors."""


class UDFParseError(UDFError):
    """Raised when a UDF file contains invalid JSON."""

    def __init__(self, message: str, line: int, column: int) -> None:
        super().__init__(message)
        self.message = message
        self.line = line
        self.column = column

    def __str__(self) -> str:
        return f"{self.message} (line {self.line}, column {self.column})"


class UDFVersionError(UDFError):
    """Raised when a UDF file specifies an unsupported format_version."""

    def __init__(self, version: int) -> None:
        super().__init__(f"Unsupported format_version: {version}")
        self.version = version


class UDFValidationError(UDFError):
    """Raised when required slots are missing from a UDF file."""

    def __init__(self, missing_slots: list[str]) -> None:
        super().__init__(f"Missing required slots: {missing_slots}")
        self.missing_slots = missing_slots


class UDFEncodeError(UDFError):
    """Raised when the encoder encounters an input problem."""

    def __init__(
        self,
        message: str,
        slot_name: str | None = None,
        file_path: str | None = None,
    ) -> None:
        super().__init__(message)
        self.message = message
        self.slot_name = slot_name
        self.file_path = file_path


class UDFDecodeError(UDFError):
    """Raised when the decoder encounters an output problem."""

    def __init__(self, message: str) -> None:
        super().__init__(message)
        self.message = message


class UDFIOError(UDFError):
    """Raised on file I/O failure."""

    def __init__(self, message: str, path: str) -> None:
        super().__init__(f"{message}: {path}")
        self.message = message
        self.path = path
