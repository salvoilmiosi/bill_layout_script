#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

IN_DIR=../layouts

rm $(find "$IN_DIR" -name "*.cache")