#!/bin/bash

# Simple front-end wrapper for the full analysis pipeline.
# Replaces the old analyzeData.sh and delegates to analyzeRun.sh which
# performs: convert -> calibration (prev CAL fallback) -> clustering -> analyzer (PDF).

set -e

export SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPTS_DIR/init.sh"

print_help() {
  cat <<EOF
*****************************************************************************
Usage: $0 [options]

This is a convenience wrapper that runs the full analysis pipeline.
It forwards all options to analyzeRun.sh.

Common options:
  -p | --home-path      Path to repo root (no trailing slash)
  -r | --run-name       Run file base name (with or without .dat)
  -f | --first-run      First run number (5 digits or integer)
  -l | --last-run       Last run number
  -j | --json-settings  Settings JSON (with inputDirectory/outputDirectory)
  -s | --n-sigma        Sigma threshold (overrides JSON)
  --no-compile          Skip compilation
  --clean-compile       Clean then compile
  -h | --help           Show this help

Examples:
  $0 -f 275 -j json/ev-settings.json
  $0 -r SCD_RUN00275_BEAM_20250811_202452 -j json/ev-settings.json -s 5
*****************************************************************************
EOF
}

# If only -h/--help is passed, show help and exit
for arg in "$@"; do
  if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
    print_help
    exit 0
  fi
done

# Delegate to the robust implementation
exec "$SCRIPTS_DIR/analyzeRun.sh" "$@"
