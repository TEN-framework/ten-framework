"""
Utility functions and classes.
"""

from typing import Optional, cast
from collections import abc

# pylint: disable=too-many-ancestors


class Slice:
    """
    Wrapper for Python's slice that computes index ranges to slice sequences.

    Currently, the range is recomputed each time. It is probably also possible
    to combine the involved slices into one.
    """

    def __init__(self, slc: slice, rec: Optional["Slice"] = None):
        self._slc = slc
        self._rec = rec

    def rng(self, size):
        """
        Return a range providing indices to access a sequence of length size.
        """
        return (
            range(*self._slc.indices(size))
            if self._rec is None
            else self._rec.rng(size)[self._slc]
        )


class SlicedSequence(abc.Sequence):
    """
    Helper to slice sequences.
    """

    def __init__(self, seq: abc.Sequence, slc: Slice):
        self._seq = seq
        self._slc = slc
        self._len = -1
        self._lst = None

    @property
    def _rng(self):
        size = len(self._seq)
        if size != self._len:
            self._lst = self._slc.rng(size)
            self._len = size
        return self._lst

    def __len__(self) -> int:
        return len(self._rng)

    def __iter__(self):
        for idx in self._rng:
            yield self._seq[idx]

    def __getitem__(self, slc):
        if isinstance(slc, slice):
            return SlicedSequence(self._seq, Slice(slc, self._slc))
        return self._seq[self._rng[slc]]

    def __str__(self):
        return str(list(self))

    def __repr__(self):
        return repr(list(self))


class SlicedMutableSequence(SlicedSequence, abc.MutableSequence):
    """
    Helper to slice sequences.
    """

    def __init__(self, seq: abc.MutableSequence, slc: Slice):
        super().__init__(seq, slc)

    @property
    def _mut_seq(self):
        return cast(abc.MutableSequence, self._seq)

    def __setitem__(self, index, ast):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        self._mut_seq[self._rng[index]] = ast

    def __delitem__(self, index):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        del self._mut_seq[self._rng[index]]

    def insert(self, index, value):
        self._mut_seq[self._rng[index]] = value
