#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast, TypeAlias

from .error import TenError, TenErrorCode

T = TypeVar("T", bound="Value")


class ValueType(IntEnum):
    """Enum representing the different types a Value can hold."""

    INVALID = 0
    NULL = 1
    BOOL = 2
    INT = 3
    FLOAT = 4
    STRING = 5
    BYTES = 6
    ARRAY = 7
    OBJECT = 8
    JSON_STRING = 9


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
    def from_buf(cls: type[T], value: bytes) -> T:
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

    def get_bool(self) -> tuple[bool, TenError | None]:
        if self._type != ValueType.BOOL:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not a boolean, got {self._type.name}",
            )
            return (False, error)
        return (cast(bool, self._data), None)

    def get_int(self) -> tuple[int, TenError | None]:
        if self._type != ValueType.INT:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not an integer, got {self._type.name}",
            )
            return (0, error)
        return (cast(int, self._data), None)

    def get_float(self) -> tuple[float, TenError | None]:
        if self._type != ValueType.FLOAT:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not a float, got {self._type.name}",
            )
            return (0.0, error)
        return (cast(float, self._data), None)

    def get_string(self) -> tuple[str, TenError | None]:
        if self._type != ValueType.STRING:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not a string, got {self._type.name}",
            )
            return ("", error)
        return (cast(str, self._data), None)

    def get_buf(self) -> tuple[bytes, TenError | None]:
        if self._type != ValueType.BYTES:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not bytes, got {self._type.name}",
            )
            return (b"", error)
        return (cast(bytes, self._data), None)

    def get_array(self) -> tuple[list["Value"], TenError | None]:
        if self._type != ValueType.ARRAY:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not an array, got {self._type.name}",
            )
            return ([], error)
        return (cast(list["Value"], self._data), None)

    def get_object(self) -> tuple[dict[str, "Value"], TenError | None]:
        if self._type != ValueType.OBJECT:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not an object, got {self._type.name}",
            )
            return ({}, error)
        return (cast(dict[str, "Value"], self._data), None)

    def get_json_string(self) -> tuple[str, TenError | None]:
        if self._type != ValueType.JSON_STRING:
            error = TenError.create(
                TenErrorCode.ErrorCodeInvalidType,
                f"Value is not a JSON string, got {self._type.name}",
            )
            return ("", error)
        return (cast(str, self._data), None)
