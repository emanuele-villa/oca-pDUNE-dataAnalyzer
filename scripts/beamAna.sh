#!/bin/bash

set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPTS_DIR/init.sh"

print_help(){
  cat <<EOF
Usage: $0 -j <json_settings> -b <parameters/beam_settings.dat> [--no-compile] [--clean-compile]
       [--inputs <pattern>] [--out-dir <dir>] [--n-sigma <N>]

Description:
  Aggregates ROOT runs into per-beam-condition files using beam_settings.dat, then
  runs dataAnalyzer on each aggregated file to produce reports.

Options:
  -j, --json-settings   Path to ev-settings.json
  -b, --beam-settings   Path to parameters/beam_settings.dat
      --inputs          Glob for input ROOT files (default: converted-data/*_converted.root)
      --out-dir         Output directory for aggregated files (default: converted-data/beam-sets)
      --n-sigma         Sigma threshold forwarded to dataAnalyzer (default from JSON)
      --no-compile      Do not recompile
      --clean-compile   Clean and recompile
  -h, --help            Show this help
EOF
}

settings=""; beamset=""; inputs_glob=""; out_dir=""; nsigma=""; noCompile=false; cleanCompile=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    -j|--json-settings) settings="$2"; shift 2;;
    -b|--beam-settings) beamset="$2"; shift 2;;
    --inputs) inputs_glob="$2"; shift 2;;
    --out-dir) out_dir="$2"; shift 2;;
    --n-sigma) nsigma="$2"; shift 2;;
    --no-compile) noCompile=true; shift;;
    --clean-compile) cleanCompile=true; shift;;
    -h|--help) print_help; exit 0;;
    *) shift;;
  esac
done

if [[ -z "$settings" || -z "$beamset" ]]; then
  echo "Missing required -j/--json-settings and -b/--beam-settings"; exit 1;
fi

HOME_DIR="$(cd "$SCRIPTS_DIR/.." && pwd)"
inputs_glob=${inputs_glob:-"$HOME_DIR/converted-data/*BEAM*_converted.root"}
out_dir=${out_dir:-"$HOME_DIR/converted-data/beam-sets"}

# Resolve JSON via finder to extract IO dirs and options
settings=$(. "$SCRIPTS_DIR/findSettings.sh" -j "$settings" | tail -n 1)
inputDirectory=$(awk -F'"' '/inputDirectory/{print $4}' "$settings")
outputDirectory=$(awk -F'"' '/outputDirectory/{print $4}' "$settings")
[[ -z "$nsigma" ]] && nsigma=$(awk -F'"' '/nSigma/{print $4}' "$settings")

# Compile
. "$SCRIPTS_DIR/compile.sh" -p "$HOME_DIR" --no-compile $noCompile --clean-compile $cleanCompile

mkdir -p "$out_dir"

# Aggregate
cd "$HOME_DIR/build"
# Collect inputs and filter to BEAM files only
inputs=( $(ls $inputs_glob 2>/dev/null || true) )
# Extra guard in case a broad glob is provided
tmp_inputs=()
for f in "${inputs[@]}"; do
  base=$(basename "$f")
  if [[ "$base" == *"_BEAM_"* ]]; then
    tmp_inputs+=("$f")
  fi
done
inputs=("${tmp_inputs[@]}")
if [[ ${#inputs[@]} -eq 0 ]]; then echo "No inputs matched $inputs_glob"; exit 0; fi
./beamAggregator "$beamset" "$out_dir" "${inputs[@]}"

# Run dataAnalyzer on aggregated outputs
agg_files=( $(ls "$out_dir"/*.root 2>/dev/null || true) )
for f in "${agg_files[@]}"; do
  # Use most recent calibration we have in converted-data
  cal=$(ls -1t "$outputDirectory"/*CAL*_converted.cal "$outputDirectory"/*.cal 2>/dev/null | head -n1 || true)
  [[ -z "$cal" ]] && echo "Warning: no CAL file found, skipping $f" && continue
  ./dataAnalyzer -j "$settings" -r "$f" -c "$cal" -o "$outputDirectory" -s "$nsigma"
done

echo "beamAna.sh completed"
