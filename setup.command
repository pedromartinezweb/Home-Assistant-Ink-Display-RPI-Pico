#!/bin/sh
cd "$(dirname "$0")" || exit 1
./setup.sh
printf '\nPress Enter to close this window.'
read -r _
