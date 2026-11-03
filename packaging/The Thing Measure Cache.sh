#!/bin/bash
export THING_MEASURE_CACHE=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing Measure.sh" "$@"
