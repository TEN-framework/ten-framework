#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from build.scripts import cmd_exec


def deleteFile(f):
    if os.path.isfile(f):
        os.remove(f)


def split_objs(ar, tmp_file, objs):
    command = '"{0}" qc {1} {2}'.format(ar, tmp_file, " ".join(objs))
    if sys.platform == "win32" and len(command) >= 8191:
        split_objs(ar, tmp_file, objs[: len(objs) / 2])
        split_objs(ar, tmp_file, objs[len(objs) / 2 :])  # noqa
    else:
        cmd_exec.get_cmd_output(command)


def combine(argv):
    if len(argv) < 3:
        print("Wrong argument")
        print(argv)
        sys.exit(-1)
    ar = argv[0]
    output = argv[1]
    rspfile = argv[2][1:]
    out_path = os.path.dirname(output)
    tmp_file = output + ".tmp.a"
    mri_file = output + ".mri"
    if len(out_path) != 0 and not os.path.exists(out_path):
        os.makedirs(out_path)
    if not os.path.exists(rspfile):
        sys.exit(-1)
    if os.path.exists(tmp_file):
        deleteFile(tmp_file)
    if os.path.exists(mri_file):
        deleteFile(mri_file)
    f = open(rspfile, "r", encoding="utf-8")
    contents = f.read()
    f.close()
    all_files = contents.split("\n")
    objs = [o for o in all_files if o.endswith(".o")]
    archs = [o for o in all_files if o.endswith(".a")]

    # combine all .o
    split_objs(ar, tmp_file, objs)
    # then combine all .a
    fi = open(mri_file, "w", encoding="utf-8")
    fi.write("create {}\n".format(output))
    if len(objs) != 0:
        fi.write("addlib {}\n".format(tmp_file))
    for a in archs:
        fi.write("addlib {}\n".format(a))
    fi.write("save\n")
    fi.write("end\n")
    fi.close()
    cmd_exec.get_cmd_output('"{0}" -M <{1}'.format(ar, mri_file))
    deleteFile(mri_file)
    deleteFile(tmp_file)
    sys.exit(0)


if __name__ == "__main__":
    combine(sys.argv[1:])
