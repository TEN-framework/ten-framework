#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast

T = TypeVar("T", bound="Value")


class ValueType(IntEnum):
    """Enum representing the different types a Value can hold."""

    INVALID = 0
    BOOL = 1
    INT = 2
    FLOAT = 3
    STRING = 4
    BYTES = 5
    ARRAY = 6
    OBJECT = 7
    JSON_STRING = 8


class Value:
    """
    A flexible value container that can hold different types of data.

    This class provides a unified interface for handling various data types
    efficiently across the Python-C boundary while maintaining Python's
    native type system.
    """

    _type: ValueType
    _data: bool | int | float | str | bytes | list["Value"] | dict[str, "Value"]

    def __init__(
        self,
        value_type: ValueType,
        data: (
            bool
            | int
            | float
            | str
            | bytes
            | list["Value"]
            | dict[str, "Value"]
        ),
    ):
        """
        Initialize a Value with the specified type and data.

        Args:
            value_type: The type of the value
            data: The actual data to store
        """
        self._type = value_type
        self._data = data

    def get_type(self) -> ValueType:
        """Get the type of this Value."""
        return self._type

    def get_data(
        self,
    ) -> bool | int | float | str | bytes | list["Value"] | dict[str, "Value"]:
        """Get the underlying data of this Value."""
        return self._data

    @classmethod
    def create_bool(cls: type[T], value: bool) -> T:
        """Create a boolean Value."""
        return cls(ValueType.BOOL, value)

    @classmethod
    def create_int(cls: type[T], value: int) -> T:
        """Create an integer Value."""
        return cls(ValueType.INT, value)

    @classmethod
    def create_float(cls: type[T], value: float) -> T:
        """Create a float Value."""
        return cls(ValueType.FLOAT, value)

    @classmethod
    def create_string(cls: type[T], value: str) -> T:
        """Create a string Value."""
        return cls(ValueType.STRING, value)

    @classmethod
    def create_bytes(cls: type[T], value: bytes) -> T:
        """Create a bytes Value."""
        return cls(ValueType.BYTES, value)

    @classmethod
    def create_array(cls: type[T], value: list["Value"]) -> T:
        """Create an array Value."""
        return cls(ValueType.ARRAY, value)

    @classmethod
    def create_object(cls: type[T], value: dict[str, "Value"]) -> T:
        """Create an object Value."""
        return cls(ValueType.OBJECT, value)

    @classmethod
    def create_json_string(cls: type[T], value: str) -> T:
        """Create a JSON string Value."""
        return cls(ValueType.JSON_STRING, value)

    def is_bool(self) -> bool:
        """Check if this is a boolean Value."""
        return self._type == ValueType.BOOL

    def is_int(self) -> bool:
        """Check if this is an integer Value."""
        return self._type == ValueType.INT

    def is_float(self) -> bool:
        """Check if this is a float Value."""
        return self._type == ValueType.FLOAT

    def is_number(self) -> bool:
        """Check if this is a numeric Value (int or float)."""
        return self._type in (ValueType.INT, ValueType.FLOAT)

    def is_string(self) -> bool:
        """Check if this is a string Value."""
        return self._type == ValueType.STRING

    def is_bytes(self) -> bool:
        """Check if this is a bytes Value."""
        return self._type == ValueType.BYTES

    def is_array(self) -> bool:
        """Check if this is an array Value."""
        return self._type == ValueType.ARRAY

    def is_object(self) -> bool:
        """Check if this is an object Value."""
        return self._type == ValueType.OBJECT

    def is_json_string(self) -> bool:
        """Check if this is a JSON string Value."""
        return self._type == ValueType.JSON_STRING

    def get_bool(self) -> bool:
        """Get the boolean value. Raises TypeError if not a boolean."""
        if not self.is_bool():
            raise TypeError(f"Value is not a boolean, got {self._type.name}")
        return cast(bool, self._data)

    def get_int(self) -> int:
        """Get the integer value. Raises TypeError if not an integer."""
        if not self.is_int():
            raise TypeError(f"Value is not an integer, got {self._type.name}")
        return cast(int, self._data)

    def get_float(self) -> float:
        """Get the float value. Raises TypeError if not a float."""
        if not self.is_float():
            raise TypeError(f"Value is not a float, got {self._type.name}")
        return cast(float, self._data)

    def get_string(self) -> str:
        """Get the string value. Raises TypeError if not a string."""
        if not self.is_string():
            raise TypeError(f"Value is not a string, got {self._type.name}")
        return cast(str, self._data)

    def get_bytes(self) -> bytes:
        """Get the bytes value. Raises TypeError if not bytes."""
        if not self.is_bytes():
            raise TypeError(f"Value is not bytes, got {self._type.name}")
        return cast(bytes, self._data)

    def get_array(self) -> list["Value"]:
        """Get the array value. Raises TypeError if not an array."""
        if not self.is_array():
            raise TypeError(f"Value is not an array, got {self._type.name}")
        return cast(list["Value"], self._data)

    def get_object(self) -> dict[str, "Value"]:
        """Get the object value. Raises TypeError if not an object."""
        if not self.is_object():
            raise TypeError(f"Value is not an object, got {self._type.name}")
        return cast(dict[str, "Value"], self._data)

    def get_json_string(self) -> str:
        """Get the JSON string value. Raises TypeError if not a JSON string."""
        if not self.is_json_string():
            raise TypeError(
                f"Value is not a JSON string, got {self._type.name}"
            )
        return cast(str, self._data)
