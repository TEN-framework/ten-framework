"""
Functions and classes related to configuration.

Examples
--------
The following example shows how to modify the configuration to enumerate all
models:

```python
>>> from clingo.control import Control
>>>
>>> ctl = Control()
>>> ctl.configuration.keys
['tester', 'solve', 'asp', 'solver', 'configuration', 'share',
 'learn_explicit', 'sat_prepro', 'stats', 'parse_ext', 'parse_maxsat']
>>> ctl.configuration.solve.keys
['solve_limit', 'parallel_mode', 'global_restarts', 'distribute',
 'integrate', 'enum_mode', 'project', 'models', 'opt_mode']
>>> ctl.configuration.solve.description("models")
'Compute at most %A models (0 for all)\\n'
>>> ctl.configuration.solve.models = 0
>>> ctl.add("base", [], "1 {a; b}.")
>>> ctl.ground([("base", [])])
>>> print(ctl.solve(on_model=print))
b
a
a b
SAT
```
"""

from typing import List, Optional, Union

from ._internal import _c_call, _ffi, _handle_error, _lib, _to_str

__all__ = ["Configuration"]


class Configuration:
    """
    Allows for changing the configuration of the underlying solver.

    Options are organized hierarchically. To change and inspect an option use:

        config.group.subgroup.option = "value"
        value = config.group.subgroup.option

    There are also arrays of option groups that can be accessed using integer
    indices:

        config.group.subgroup[0].option = "value1"
        config.group.subgroup[1].option = "value2"

    To list the subgroups of an option group, use the `Configuration.keys`
    member. Array option groups, like solver, can be iterated. Furthermore,
    there are meta options having key `configuration`. Assigning a meta option
    sets a number of related options.  To get further information about an
    option or option group, use `Configuration.description`.

    Notes
    -----
    The value of an option is always a string and any value assigned to an
    option is automatically converted into a string.

    See Also
    --------
    clingo.control.Control.configuration
    """

    def __init__(self, rep, key):
        # Note: we have to bypass __setattr__ to avoid infinite recursion
        super().__setattr__("_rep", rep)
        super().__setattr__("_key", key)

    @property
    def _type(self) -> int:
        return _c_call(
            "clingo_configuration_type_bitset_t",
            _lib.clingo_configuration_type,
            self._rep,
            self._key,
        )

    @property
    def is_array(self) -> bool:
        """
        This property is true if the configuration option is an array.
        """
        return bool(self._type & _lib.clingo_configuration_type_array)

    def _get_subkey(self, name: str) -> Optional[int]:
        if self._type & _lib.clingo_configuration_type_map:
            if _c_call(
                "bool",
                _lib.clingo_configuration_map_has_subkey,
                self._rep,
                self._key,
                name.encode(),
            ):
                return _c_call(
                    "clingo_id_t",
                    _lib.clingo_configuration_map_at,
                    self._rep,
                    self._key,
                    name.encode(),
                )
        return None

    def __len__(self):
        if self._type & _lib.clingo_configuration_type_array:
            return _c_call(
                "size_t", _lib.clingo_configuration_array_size, self._rep, self._key
            )
        return 0

    def __getitem__(self, idx: int) -> "Configuration":
        if idx < 0 or idx >= len(self):
            raise IndexError("invalid index")

        key = _c_call(
            "clingo_id_t", _lib.clingo_configuration_array_at, self._rep, self._key, idx
        )
        return Configuration(self._rep, key)

    def __getattr__(self, name: str) -> Union[None, str, "Configuration"]:
        key = self._get_subkey(name)
        if key is None:
            raise AttributeError(f"no attribute: {name}")

        type_ = _c_call(
            "clingo_configuration_type_bitset_t",
            _lib.clingo_configuration_type,
            self._rep,
            key,
        )

        if type_ & _lib.clingo_configuration_type_value:
            if not _c_call(
                "bool", _lib.clingo_configuration_value_is_assigned, self._rep, key
            ):
                return None

            size = _c_call(
                "size_t", _lib.clingo_configuration_value_get_size, self._rep, key
            )

            c_val = _ffi.new("char[]", size)
            _handle_error(
                _lib.clingo_configuration_value_get(self._rep, key, c_val, size)
            )
            return _to_str(c_val)

        return Configuration(self._rep, key)

    def __setattr__(self, name: str, val) -> None:
        key = self._get_subkey(name)
        if key is None:
            super().__setattr__(name, val)
        else:
            _handle_error(
                _lib.clingo_configuration_value_set(self._rep, key, str(val).encode())
            )

    def description(self, name: str) -> str:
        """
        Get a description for an option or option group.

        Parameters
        ----------
        name
            The name of the option.
        """
        key = self._get_subkey(name)
        if key is None:
            raise RuntimeError(f"unknown option {name}")
        return _to_str(
            _c_call("char*", _lib.clingo_configuration_description, self._rep, key)
        )

    @property
    def keys(self) -> Optional[List[str]]:
        """
        The list of names of sub-option groups or options.

        The list is `None` if the current object is not an option group.
        """
        ret = None
        if self._type & _lib.clingo_configuration_type_map:
            ret = []
            for i in range(
                _c_call(
                    "size_t", _lib.clingo_configuration_map_size, self._rep, self._key
                )
            ):
                name = _c_call(
                    "char*",
                    _lib.clingo_configuration_map_subkey_name,
                    self._rep,
                    self._key,
                    i,
                )
                ret.append(_to_str(name))
        return ret
