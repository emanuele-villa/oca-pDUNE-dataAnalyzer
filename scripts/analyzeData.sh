#!/bin/bash
# Deprecated shim: use analyze.sh instead.
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/analyze.sh" "$@"
