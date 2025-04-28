#!/bin/zsh

export PYTHONPATH=../build/release/bin/python

last=""
inotifywait -m clingo -e modify |
    while read fpath action file; do
        case "$file" in 
            *.py)
                echo "file: $file"
                current="$(/usr/bin/md5sum "$fpath/$file")"
                if [[ "$last" != "$current" ]]; then
                    sleep 1
                    python -m pdoc --html --force clingo
                    last="$current"
                fi
                ;;
        esac
    done
