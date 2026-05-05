#!/usr/bin/env bash
set -euo pipefail

mode="${1:-check}"

mapfile -t files < <(
  git ls-files \
    '*.c' '*.cc' '*.cpp' '*.cxx' \
    '*.h' '*.hh' '*.hpp' '*.hxx'
)

if [[ "${#files[@]}" -eq 0 ]]; then
  exit 0
fi

case "${mode}" in
  check)
    clang-format --dry-run --Werror "${files[@]}"
    ;;
  fix)
    clang-format -i "${files[@]}"
    ;;
  *)
    echo "Usage: $0 [check|fix]" >&2
    exit 2
    ;;
esac
