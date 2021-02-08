#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

for f in $(find ../layouts -name *.bls); do
    echo $f
    ../build/blsdump "$f" > /dev/null
done