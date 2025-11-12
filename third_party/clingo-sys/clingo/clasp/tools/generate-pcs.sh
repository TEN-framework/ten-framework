#!/bin/bash

# OPTIONS
asp=0
opt=0
val=1
inp=""
config=""
while [[ $# > 0 ]]; do
  case $1 in
    "--with-asp")
      asp=1
      ;;
    "--with-opt")
      opt=1
      ;;
    --arg-values*)
      T=`echo "$1"| sed 's/^--[a-z-]*=*//'`
      if [ -z "$T" ]; then
        if [ -z "$2" ]; then
          echo "error: required parameter missing after '$1'"
          exit 1
        fi
        T=$2
        shift
      fi
      case $T in
        "r") val=1;;
        "d") val=0;;
        *)
          "*** Error: unknown value $T for option '--arg-values'"
          echo "type '$0 --help' for an overview of supported options"
          exit 1
          ;;
      esac
      ;;
    "--help")
      echo
      echo "$0 [options] <file>"
      echo
      echo "  --help             : show this help"
      echo "  --with-asp         : enable ASP-specific options in output"
      echo "  --with-opt         : enable optimization-specific options in output"
      echo "  --arg-values={r,d} : use discrete values (d) or ranges (r) for arguments"
      echo
      exit 0
      ;;
    *)
      if [ -z "$inp" ]; then
        inp=$1
      else
        echo "*** Error: unknown option $1"
        echo "type '$0 --help' for an overview of supported options"
        exit 1
      fi
  esac
  shift
done
if [ -z "$inp" ]; then
  echo "*** Error: input <file> required"
  echo "type '$0 --help' for usage information"
  exit 1
fi
echo "Generating pcs file..."
while read line; do
  echo ${line/"//"/"#"}
done < <(gcc -E -P -C -nostdinc -xc -DWITH_ASP=$asp -DWITH_OPTIMIZATION=$opt -DWITH_RANGES=$val $inp)
