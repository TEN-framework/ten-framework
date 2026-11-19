import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[2]))

# The extension imports ten_ai_base, which needs names from the TEN runtime.
# Inside a built TEN app those come from the system package; in a bare test
# environment fall back to a minimal stub so the vendor logic stays testable.
# In real CI (where ten_runtime is importable) this does nothing.
try:
    import ten_runtime  # noqa: F401
except ModuleNotFoundError:
    import importlib.abc
    import importlib.util
    import types

    class _Placeholder:
        def __init__(self, *args, **kwargs):
            pass

        def __call__(self, *args, **kwargs):
            return _Placeholder()

        def __getattr__(self, name):
            return _Placeholder()

        @classmethod
        def create(cls, *args, **kwargs):
            return cls()

    class _StubLoader(importlib.abc.Loader):
        def create_module(self, spec):
            module = types.ModuleType(spec.name)
            module.__path__ = []  # behave as a package
            return module

        def exec_module(self, module):
            module.__getattr__ = lambda name: _Placeholder

    class _StubFinder(importlib.abc.MetaPathFinder):
        def find_spec(self, fullname, path=None, target=None):
            if fullname == "ten_runtime" or fullname.startswith("ten_runtime."):
                return importlib.util.spec_from_loader(fullname, _StubLoader())
            return None

    sys.meta_path.insert(0, _StubFinder())
