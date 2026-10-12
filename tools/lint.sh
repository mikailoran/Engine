#!/usr/bin/env bash
# Lints the project with clang-tidy, plus clazy's checks wherever a .clang-tidy
# enables them. Shared by the VS Code tasks and the git hooks.
#
# Usage: tools/lint.sh [-p BUILD_DIR] full | diff [BASE]
#   full          every .cpp under src/ and tests/
#   diff [BASE]   only lines under src/ and tests/ changed since BASE, committed
#                 or not (default: where HEAD branched from main). Untracked
#                 files are skipped.
#   -p BUILD_DIR  holds compile_commands.json (default: build/Debug)
#
# Prints all of clang-tidy's output, each diagnostic once. Exits non-zero when
# a WarningsAsErrors check fires, a file fails to compile or setup is broken.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build/Debug"
plugin=ClazyClangTidy.so

while getopts "p:" opt; do
  case "$opt" in
    p) build_dir="$OPTARG" ;;
    *) exit 2 ;;
  esac
done
shift $((OPTIND - 1))

mode="${1:-}"
if [[ "$mode" != full && "$mode" != diff ]]; then
  echo "usage: tools/lint.sh [-p BUILD_DIR] full | diff [BASE]" >&2
  exit 2
fi

# run-clang-tidy reads .clang-tidy from the working directory
build_dir="$(realpath -m "$build_dir")"
cd "$root"

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
  echo "lint: no compile_commands.json in $build_dir; configure first" >&2
  exit 1
fi

# clang-tidy only warns when -load fails, then runs without clazy
if ! clang-tidy -load "$plugin" --list-checks -checks='-*,clazy-*' 2>/dev/null |
  grep -q clazy-; then
  echo "lint: clang-tidy can't load $plugin; is clazy installed for this LLVM?" >&2
  exit 1
fi

if [[ "$mode" == diff ]]; then
  base="${2:-$(git merge-base HEAD main)}"
  tidy_diff="$(command -v clang-tidy-diff.py ||
    echo /usr/share/clang/clang-tidy-diff.py)"
  if [[ ! -f "$tidy_diff" ]]; then
    echo "lint: clang-tidy-diff.py not found" >&2
    exit 1
  fi
fi

case "$mode" in
  full)
    run-clang-tidy -p "$build_dir" -quiet -load "$plugin" \
      "^$root/(src|tests)/.*\.cpp$"
    ;;
  diff)
    # Absolute paths, so diagnostics match the full run's
    git diff -U0 --no-color --dst-prefix="$root/" "$base" -- src tests |
      python3 "$tidy_diff" -p 0 -path "$build_dir" -quiet -load "$plugin" \
        -regex "$root/(src|tests)/.*\.(cpp|h)"
    ;;
esac 2>&1 |
  # Collapse "/x/../", drop blank lines and diagnostics repeated by other TUs
  awk '
    /^\/.+:[0-9]+:[0-9]+: (warning|error|note): / {
      while (sub(/\/[^\/]+\/\.\.\//, "/")) {}
    }
    /^\/.+:[0-9]+:[0-9]+: (warning|error): / {
      skip = ($0 in seen)
      seen[$0] = 1
    }
    /^\[ *[0-9]+\/[0-9]+\]/ { skip = 0 }
    /^$/ { next }
    !skip { print; fflush() }
  '
