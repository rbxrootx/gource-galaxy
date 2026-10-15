#!/bin/bash
# Collect gource.exe, its DLLs and the data folder into ./dist (run build.sh first).
set -e
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/dist"
rm -rf "$D" && mkdir -p "$D"
cp /tmp/g/gource.exe "$D/"
cp -r /tmp/g/data "$D/data"
ldd /tmp/g/gource.exe | grep -i ucrt64 | awk '{print $3}' | while read f; do cp "$f" "$D/"; done
cp "$D/../README.md" "$D/README-GALAXY.md" 2>/dev/null || true
printf '@echo off\r\n"%%~dp0gource.exe" %%*\r\n' > "$D/gource-galaxy.bat"
ls "$D"
