#!/usr/bin/env bash
set -euo pipefail

# --- Config ---
BUILD_DIR="${BUILD_DIR:-build-bench}"
OUT_DIR="${OUT_DIR:-benchmarks/data}"
BENCH_EXE="${BENCH_EXE:-fluid-sim-bench}"
MIN_TIME="${MIN_TIME:-0.3s}"
REPS="${REPS:-5}"

mkdir -p "$OUT_DIR"

# --- Metadata ---
COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo nogit)"
SYS="$(uname -srm)"

# Output filenames
RAW_CSV="$OUT_DIR/bench_${COMMIT}.csv"

echo "[1/3] Configure (Release) -> $BUILD_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[2/3] Build target -> $BENCH_EXE"
cmake --build "$BUILD_DIR" -j --target "$BENCH_EXE"

echo "[3/3] Run benchmarks -> $RAW_CSV"
"./$BUILD_DIR/$BENCH_EXE" \
  --benchmark_repetitions="$REPS" \
  --benchmark_min_time="0.3s" \
  --benchmark_out_format=csv \
  --benchmark_out="$RAW_CSV"

