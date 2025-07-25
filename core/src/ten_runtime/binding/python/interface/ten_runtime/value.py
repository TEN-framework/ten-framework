#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast, TypeAlias

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


ValueDataType: TypeAlias = (
    bool | int | float | str | bytes | list["Value"] | dict[str, "Value"]
)


class Value:
    """
    A flexible value container that can hold different types of data.
    """

    _type: ValueType
    _data: ValueDataType

    def __init__(self, value_type: ValueType, value_data: ValueDataType):
        self._type = value_type
        self._data = value_data

    @classmethod
    def from_bool(cls: type[T], value: bool) -> T:
        return cls(ValueType.BOOL, value)

    @classmethod
    def from_int(cls: type[T], value: int) -> T:
        return cls(ValueType.INT, value)

    @classmethod
    def from_float(cls: type[T], value: float) -> T:
        return cls(ValueType.FLOAT, value)

    @classmethod
    def from_string(cls: type[T], value: str) -> T:
        return cls(ValueType.STRING, value)

    @classmethod
    def from_bytes(cls: type[T], value: bytes) -> T:
        return cls(ValueType.BYTES, value)

    @classmethod
    def from_array(cls: type[T], value: list["Value"]) -> T:
        return cls(ValueType.ARRAY, value)

    @classmethod
    def from_object(cls: type[T], value: dict[str, "Value"]) -> T:
        return cls(ValueType.OBJECT, value)

    @classmethod
    def from_json_string(cls: type[T], value: str) -> T:
        return cls(ValueType.JSON_STRING, value)

    def get_type(self) -> ValueType:
        return self._type

    def get_bool(self) -> bool:
        if self._type != ValueType.BOOL:
            raise TypeError(f"Value is not a boolean, got {self._type.name}")
        return cast(bool, self._data)

    def get_int(self) -> int:
        if self._type != ValueType.INT:
            raise TypeError(f"Value is not an integer, got {self._type.name}")
        return cast(int, self._data)

    def get_float(self) -> float:
        if self._type != ValueType.FLOAT:
            raise TypeError(f"Value is not a float, got {self._type.name}")
        return cast(float, self._data)

    def get_string(self) -> str:
        if self._type != ValueType.STRING:
            raise TypeError(f"Value is not a string, got {self._type.name}")
        return cast(str, self._data)

    def get_bytes(self) -> bytes:
        if self._type != ValueType.BYTES:
            raise TypeError(f"Value is not bytes, got {self._type.name}")
        return cast(bytes, self._data)

    def get_array(self) -> list["Value"]:
        if self._type != ValueType.ARRAY:
            raise TypeError(f"Value is not an array, got {self._type.name}")
        return cast(list["Value"], self._data)

    def get_object(self) -> dict[str, "Value"]:
        if self._type != ValueType.OBJECT:
            raise TypeError(f"Value is not an object, got {self._type.name}")
        return cast(dict[str, "Value"], self._data)

    def get_json_string(self) -> str:
        if self._type != ValueType.JSON_STRING:
            raise TypeError(
                f"Value is not a JSON string, got {self._type.name}"
            )
        return cast(str, self._data)
