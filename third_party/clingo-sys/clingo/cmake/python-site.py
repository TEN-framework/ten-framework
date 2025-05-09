import sys
from argparse import ArgumentParser

if sys.version_info >= (3, 11):
    NEW_STYLE = True
    from sysconfig import (
        get_config_var,
        get_config_vars,
        get_path,
        get_preferred_scheme,
    )
else:
    NEW_STYLE = False
    from distutils.sysconfig import get_config_vars, get_python_lib
    from site import USER_SITE

parser = ArgumentParser()
if sys.version_info >= (3, 7):
    subparser = parser.add_subparsers(required=True, dest="action")
else:
    subparser = parser.add_subparsers(dest="action")

prefix_parser = subparser.add_parser("target")
prefix_group = prefix_parser.add_mutually_exclusive_group()
prefix_group.add_argument("--user", action="store_true", help="get user prefix")
prefix_group.add_argument("--prefix", type=str, help="prepend prefix")

prefix_parser = subparser.add_parser("suffix")

result = parser.parse_args()

if NEW_STYLE:
    if result.action == "target":
        if result.user:
            scheme = get_preferred_scheme("user")
        else:
            scheme = get_preferred_scheme("prefix")
        if result.prefix is not None:
            cvars = get_config_vars()
            cvars["base"] = result.prefix
            cvars["platbase"] = result.prefix
            platlib = get_path("platlib", scheme, cvars)
        else:
            platlib = get_path("platlib", scheme)
        print(platlib)
    elif result.action == "suffix":
        print(get_config_var("EXT_SUFFIX"))
else:
    # TODO: remove once python 3.10 is eol
    if result.action == "target":
        if result.user:
            print(USER_SITE)
        else:
            print(get_python_lib(True, False, result.prefix))
    elif result.action == "suffix":
        SO, SOABI, EXT_SUFFIX = get_config_vars("SO", "SOABI", "EXT_SUFFIX")
        if EXT_SUFFIX is not None:
            ext = EXT_SUFFIX
        elif SOABI is not None:
            ext = "".join(".", SOABI, SO)
        else:
            ext = SO
        print(ext)
