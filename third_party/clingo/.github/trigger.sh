#!/bin/bash

repo=clingo

function list() {
    curl \
      -X GET \
      -H "Accept: application/vnd.github.v3+json" \
      "https://api.github.com/repos/potassco/${repo}/actions/workflows" \
      -d "{\"ref\":\"ref\"}"
}

function dispatch() {
    token=$(grep -A1 workflow_dispatch ~/.tokens | tail -n 1)
    curl \
      -u "rkaminsk:$token" \
      -X POST \
      -H "Accept: application/vnd.github.v3+json" \
      "https://api.github.com/repos/potassco/${repo}/actions/workflows/$1/dispatches" \
      -d "{\"ref\":\"$3\",\"inputs\":{\"wip\":\"$2\"${4:+,$4}}}"
}

function usage() {
    cat <<EOF
trigger.sh -h
    show this help
trigger.sh list
    list available workflows
trigger.sh {release|dev} BRANCH
    deploy release or development packages
EOF
}

function check_action() {
    if [[ "$1" == "$2" ]]; then
        if (($3 + $4 != $5 )); then
            if (( $4 == 0 )); then
                echo "action '${2}' expects no arguments" >&2
            elif (( $4 == 1 )); then
                echo "action '${2}' expects 1 argument" >&2
            else
                echo "action '${2}' expects ${4} arguments" >&2
            fi
            exit 1
        fi
        return 0
    fi
    return 1
}

function fail() {
    echo "unexpected action '$1'" >&2
    exit 1
}

while getopts ":h" flag; do
    case "$flag" in
        h)
            usage
            exit 0
            ;;
        :)
            echo "ERROR: option '-${OPTARG}' expects an argument" >&2
            exit 1
            ;;
        ?)
            echo "ERROR: invalid option '-${OPTARG}'" >&2
            exit 1
            ;;
    esac
done

if (( $OPTIND > $# )); then
    echo "ERROR: no action given" >&2
    usage
    exit 1
fi

action="${@:$OPTIND:1}"

check_action list "$action" $OPTIND 0 $# ||
check_action release "$action" $OPTIND 1 $# ||
check_action dev "$action" $OPTIND 1 $# ||
fail "$action"

wip=true

case "$action" in
    list)
        list
        ;;
    release)
        wip=false
        ;&
    dev)
        branch="${@:$OPTIND+1:1}"
        # .github/workflows/ppa-dev.yml
        dispatch 4881510 "$wip" "$branch"
        # .github/workflows/conda-dev.yml
        dispatch 4923491 "$wip" "$branch"
        # .github/workflows/cibuildwheel.yml
        dispatch 34889579 "$wip" "$branch"
        ;;
esac
