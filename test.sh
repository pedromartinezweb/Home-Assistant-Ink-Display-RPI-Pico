#!/bin/sh
set -eu

project_dir=$(unset CDPATH; cd -- "$(dirname -- "$0")" && pwd)
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

cc -std=c11 -Wall -Wextra -Werror \
    -I"$project_dir/tests/include" \
    -I"$project_dir/src" \
    "$project_dir/src/frame.c" \
    "$project_dir/src/dashboard.c" \
    "$project_dir/src/ink_protocol.c" \
    "$project_dir/tests/ink_protocol_test.c" \
    -o "$output_dir/ink_protocol_test"
"$output_dir/ink_protocol_test"

python3 "$project_dir/tests/protocol_py_test.py"
python3 -m compileall -q "$project_dir/custom_components/ha_ink_display"
for json in \
    "$project_dir/hacs.json" \
    "$project_dir/custom_components/ha_ink_display/manifest.json" \
    "$project_dir/custom_components/ha_ink_display/strings.json" \
    "$project_dir/custom_components/ha_ink_display/translations/en.json"
do
    python3 -m json.tool "$json" >/dev/null
done
