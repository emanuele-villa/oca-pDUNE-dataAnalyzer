#!/usr/bin/env bash
set -euo pipefail

# beamQuickPlot.sh - Generate quick subset report (pages 1,2,3,4,5,6,9) for aggregated beam files
# Usage: ./scripts/beamQuickPlot.sh -c <calibration.csv> -i <input_root> [-o out_dir] [--nsigma N]
#        Or: ./scripts/beamQuickPlot.sh -c <calibration.csv> -g "glob/pattern/*.root" [-o out_dir] [--nsigma N]
# Will place PDFs next to outputs in out_dir (default: ./quickReports)

usage(){
  grep '^#' "$0" | sed -e 's/^# //'
}

calib=""
input=""
globPat=""
outDir="quickReports"
nsigma=5
build=yes

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--cal) calib="$2"; shift 2;;
    -i|--input) input="$2"; shift 2;;
    -g|--glob) globPat="$2"; shift 2;;
    -o|--out) outDir="$2"; shift 2;;
    --nsigma) nsigma="$2"; shift 2;;
    --no-build) build=no; shift;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg $1"; usage; exit 1;;
  esac
done

if [[ -z "$calib" ]]; then echo "Calibration file required"; usage; exit 1; fi
if [[ ! -f "$calib" ]]; then echo "Calibration file $calib not found"; exit 1; fi
if [[ -z "$input" && -z "$globPat" ]]; then echo "Provide either --input or --glob"; usage; exit 1; fi

cd "$(dirname "$0")/.."

if [[ $build == yes ]]; then
  echo "[beamQuickPlot.sh] Building beamQuickPlot target..."
  cmake -S . -B build >/dev/null 2>&1 || cmake -S . -B build
  cmake --build build --target beamQuickPlot -j $(nproc)
fi

mkdir -p "$outDir"

run_one(){
  f="$1"
  if [[ ! -f "$f" ]]; then echo "Skip missing $f"; return; fi
  echo "[beamQuickPlot.sh] Processing $f";
  ./build/beamQuickPlot --root "$f" --cal "$calib" --out "$outDir" --nsigma "$nsigma" || echo "WARN: beamQuickPlot failed for $f";
}

if [[ -n "$input" ]]; then
  run_one "$input"
else
  shopt -s nullglob
  for f in $globPat; do
    run_one "$f"
  done
fi

echo "[beamQuickPlot.sh] Done. PDFs in $outDir";
