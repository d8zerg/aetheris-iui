#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build/debug}"

if [[ ! -f "${build_dir}/compile_commands.json" ]]; then
  echo "Missing ${build_dir}/compile_commands.json. Configure CMake first." >&2
  exit 2
fi

mapfile -t sources < <(
  git ls-files \
    'domain/*.cpp' \
    'application/*.cpp' \
    'infrastructure/*.cpp' \
    'interface/*.cpp'
)

if [[ "${#sources[@]}" -gt 0 ]]; then
  clang-tidy --quiet -p "${build_dir}" --extra-arg=-w "${sources[@]}" \
    2> >(grep -vE '^[0-9]+ warnings generated\.$' >&2)
fi

cppcheck \
  --project="${build_dir}/compile_commands.json" \
  --file-filter='*/domain/src/*' \
  --file-filter='*/application/src/*' \
  --file-filter='*/infrastructure/src/*' \
  --file-filter='*/interface/src/*' \
  --enable=warning,style,performance,portability \
  --inline-suppr \
  --suppressions-list=.cppcheck-suppressions \
  --error-exitcode=1
