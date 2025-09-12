#!/usr/bin/env bash
# Usage:
#   source ./scripts/load_env.sh [--print] [path/to/.env]

_load_env_file() {
  local file="${1:-.env}"
  local print_only="${2:-0}"

  if [[ ! -f "$file" ]]; then
    echo "load_env.sh: file not found: $file" >&2
    return 1
  fi

  # Save current shell options and temporarily relax nounset while sourcing
  local _saved_opts
  _saved_opts="$(set +o)"   # capture current -e/-u/-o state
  set +u                    # disable nounset for safe sourcing

  if [[ "$print_only" == "1" ]]; then
    (
      set -a
      # shellcheck disable=SC1090
      . "$file"
      set +a
      while IFS= read -r line; do
        [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue
        if [[ "$line" =~ ^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*= ]]; then
          key="${BASH_REMATCH[1]}"
          val="${!key-}"
          printf 'export %s=%q\n' "$key" "$val"
        fi
      done < "$file"
    )
  else
    set -a
    # shellcheck disable=SC1090
    . "$file"
    set +a
  fi

  eval "$_saved_opts"       # restore original shell options
}

# Detect sourced vs executed
if (return 0 2>/dev/null); then
  if [[ "${1-}" == "--print" ]]; then shift; _load_env_file "${1:-.env}" "1"; else _load_env_file "${1:-.env}" "0"; fi
else
  if [[ "${1-}" == "--print" ]]; then shift; _load_env_file "${1:-.env}" "1"; else
    echo "NOTE: source this script to modify your current shell:" >&2
    echo "  source ./scripts/load_env.sh [--print] [path/to/.env]" >&2
    echo; echo "# Dry-run:"; _load_env_file "${1:-.env}" "1"
  fi
fi
