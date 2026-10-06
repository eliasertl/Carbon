#!/usr/bin/env sh
# Formats all Carbon sources with clang-format. Run before every commit.
#   Scripts/Format.sh           formats in place
#   Scripts/Format.sh --check   fails if any file is not formatted (used by CI)
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [ "${1:-}" = "--check" ]; then
    find Framework Extensions Reflection Examples Tests Benchmarks -type f \( -name '*.h' -o -name '*.cpp' \) -print0 |
        xargs -0 -r clang-format --dry-run --Werror
else
    find Framework Extensions Reflection Examples Tests Benchmarks -type f \( -name '*.h' -o -name '*.cpp' \) -print0 |
        xargs -0 -r clang-format -i
fi
