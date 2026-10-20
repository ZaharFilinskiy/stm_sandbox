#!/usr/bin/env sh
# Resolve the launcher relative to this script, including paths containing spaces.
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
exec python3 "$script_dir/run.py" "$@"
