"""Auxiliary definitions used in type annotations.
"""

# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from typing import Any

# The typing_extensions module is necessary for type annotations that are
# checked with mypy. It is only used for type annotations or to define
# things that are themselves only used for type annotations. It is not
# available on a default Python installation. Therefore, try loading
# what we need from it for the sake of mypy (which depends on, or comes
# with, typing_extensions), and if not define substitutes that lack the
# static type information but are good enough at runtime.
try:
    from typing_extensions import Protocol #pylint: disable=import-error
except ImportError:
    class Protocol: #type: ignore
        #pylint: disable=too-few-public-methods
        pass

class Writable(Protocol):
    """Abstract class for typing hints."""
    # pylint: disable=no-self-use,too-few-public-methods,unused-argument
    def write(self, text: str) -> Any:
        ...
