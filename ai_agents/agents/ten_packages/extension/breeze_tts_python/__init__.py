# Register the addon when TEN loads this package. Unit tests import the
# modules flat, in which case there is no package context and nothing to do.
if __package__:
    from . import addon  # noqa: F401
