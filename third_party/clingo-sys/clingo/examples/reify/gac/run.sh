#!/bin/bash

guess=()
check=()
opts=()
base="$(dirname "$0")"

i=0
for x in "${@}"; do
    if [[ $x == "--" ]]; then
        (( i=i+1 ))
        continue
    fi
    case $i in
        0)
            guess+=($x)
            ;;
        1)
            check+=($x)
            ;;
        2)
            opts+=($x)
            ;;
        *)
            echo "invalid arguments"
            exit 1
            ;;
    esac
done

clingo --output=reify "${base}/domain.lp" "${guess[@]}"                   | \
    grep "^output(guess(.*).*)\.$"                                        | \
    clingo --output=reify --reify-sccs - "${base}/guess.lp" "${check[@]}" | \
    clingo -Wno-atom-undefined ${opts[@]} - "${base}/glue.lp" "${guess[@]}"
