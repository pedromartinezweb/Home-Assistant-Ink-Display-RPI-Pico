#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir="$project_dir/build-test"

mkdir -p "$output_dir"
cc -std=c11 -Wall -Wextra -Werror \
    -I"$project_dir/tests/include" \
    -I"$project_dir/src" \
    "$project_dir/src/frame.c" \
    "$project_dir/src/dashboard.c" \
    "$project_dir/tests/dashboard_test.c" \
    -o "$output_dir/dashboard_test"
"$output_dir/dashboard_test"
