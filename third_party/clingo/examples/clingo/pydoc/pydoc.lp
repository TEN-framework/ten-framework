#!/usr/bin/env clingo
#script (python)
import clingo, clingo.ast, pydoc, subprocess

print("Generating documentation for clingo version {}.".format(clingo.__version__))
for m in [clingo, clingo.ast]:
    pydoc.writedoc(m)
    subprocess.call(["sed", "-i", 
        "-e", r"s/\<ffc8d8\>/88ff99/g", 
        "-e", r"s/\<ee77aa\>/22bb33/g",
        "-e", r's/<a href=".">index<\/a>.*<\/font>/<a href="\/">\&laquo;Potassco<\/a><\/font>/',
        "-e", r's/<a href="__builtin__.html#object">[^<]*object<\/a>/object/g',
        "-e", r's/{0}.html#/#/g',
        "{0}.html".format(m.__name__)])

#end.
