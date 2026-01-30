#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess
import io
import re
import sys
import os.path

class Option:
    def __init__(self, long_opt, short_opt):
        self.long_opt = long_opt
        self.short_opt = short_opt

def get_all_options(cmd):
    opt_pattern = re.compile(r'  (?:(-.), )?(--[^\s\[=]+)(\[)?')
    proc = subprocess.Popen([cmd, "--help"], stdout=subprocess.PIPE)
    stdoutdata, _ = proc.communicate()
    cur_option = None
    opts = {}
    for line in io.StringIO(stdoutdata.decode('utf-8')):
        match = opt_pattern.match(line)
        if not match:
            continue
        long_opt = match.group(2)
        short_opt = match.group(1)
        opts[long_opt] = Option(long_opt, short_opt)

    return opts

def output_case(out, name, opts):
    out.write('''\
_{name}()
{{
    local cur prev split=false
    COMPREPLY=()
    COMP_WORDBREAKS=${{COMP_WORDBREAKS//=}}

    cmd=${{COMP_WORDS[0]}}
    _get_comp_words_by_ref cur prev
'''.format(name=name))

    # Complete option name.
    out.write('''\
    case $cur in
        -*)
            COMPREPLY=( $( compgen -W '\
''')
    for opt in opts.values():
        out.write(opt.long_opt)
        out.write(' ')

    out.write('''\
' -- "$cur" ) )
            ;;
''')
    # If no option found for completion then complete with files.
    out.write('''\
        *)
            _filedir
            return 0
    esac
    return 0
}}
complete -F _{name} {name}
'''.format(name=name))

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Generates bash_completion using `/path/to/cmd --help'")
        print("Usage: make_bash_completion.py /path/to/cmd")
        exit(1)
    name = os.path.basename(sys.argv[1])
    opts = get_all_options(sys.argv[1])
    output_case(sys.stdout, name, opts)
