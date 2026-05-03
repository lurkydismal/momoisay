#!/bin/bash
export SCAN_BUILD=
./build.sh -buae 2>&1 |
    rg -i 'scan-view' |
    sed -n "s/^.*\(\/tmp\/scan-build[^']*\)'.*/scan-view \1/p"
