#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

rm $(find "../layouts" -name "*.cache")