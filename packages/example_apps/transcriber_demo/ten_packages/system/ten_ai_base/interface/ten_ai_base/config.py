#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import builtins
import json

from typing import TypeVar, Type, List
from ten_runtime import AsyncTenEnv, TenEnv
from dataclasses import dataclass, fields


T = TypeVar("T", bound="BaseConfig")


@dataclass
class BaseConfig:
    """
    Base class for implementing configuration.
    Extra configuration fields can be added in inherited class.
    """

    @classmethod
    def create(cls: Type[T], ten_env: TenEnv) -> T:
        c = cls()
        c._init(ten_env)
        return c

    @classmethod
    async def create_async(cls: Type[T], ten_env: AsyncTenEnv) -> T:
        c = cls()
        await c._init_async(ten_env)
        return c

    def update(self, config: dict):
        for field in fields(self):
            try:
                val = config.get(field.name)
                if val:
                    setattr(self, field.name, val)
            except Exception as e:
                pass

    def _init(self, ten_env: TenEnv):
        """
        Get property from ten_env to initialize the dataclass config.
        """
        for field in fields(self):
            try:
                match field.type:
                    case builtins.str:
                        val, err = ten_env.get_property_string(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        if val:
                            setattr(self, field.name, val)
                    case builtins.int:
                        val, err = ten_env.get_property_int(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case builtins.bool:
                        val, err = ten_env.get_property_bool(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case builtins.float:
                        val, err = ten_env.get_property_float(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case _:
                        val, err = ten_env.get_property_to_json(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, json.loads(val))
            except Exception:
                pass

    async def _init_async(self, ten_env: AsyncTenEnv):
        """
        Get property from ten_env to initialize the dataclass config.
        """
        for field in fields(self):
            try:
                match field.type:
                    case builtins.str:
                        val, err = await ten_env.get_property_string(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        if val:
                            setattr(self, field.name, val)
                    case builtins.int:
                        val, err = await ten_env.get_property_int(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case builtins.bool:
                        val, err = await ten_env.get_property_bool(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case builtins.float:
                        val, err = await ten_env.get_property_float(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        setattr(self, field.name, val)
                    case _:
                        val, err = await ten_env.get_property_to_json(field.name)
                        if err:
                            raise RuntimeError(
                                f"Failed to  get property {field.name}: {err}"
                            )
                        if val:
                            setattr(self, field.name, json.loads(val))
            except Exception:
                pass
