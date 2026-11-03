#!/bin/bash
export THING_MEASURE_CACHE=0 THING_INDEX_RANGE=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing Measure.sh" "$@"
