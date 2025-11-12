#!/bin/bash

cd "$(dirname "$0")/.."

export cpucount=$[ $(cat /proc/cpuinfo | grep processor | wc -l) - 1 ]

(
    cd _gen
    make -j${cpucount} -C../../../build/release pyclingo
    python3 gen.py
)

bundle install --path "$(pwd)/.gem"
bundle exec jekyll serve -l &
trap "kill $!" EXIT
cd _gen


inotifywait -rme ATTRIB --format "%f" . ../../../build/release/bin/python ../../../libpyclingo | while read file; do
    case "$file" in
        *.py)
            ;&
        *.mako)
            ;&
        *.so)
            sleep 1
            python3 gen.py
            ;;
        *.h)
            ;&
        *.cc)
            make -j${cpucount} -C../../../build/release pyclingo
            ;;
    esac
done
