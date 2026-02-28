#!/usr/bin/env pwsh
#Requires -Version 5.1
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# --- Config (env overrides with defaults) ---
$BUILD_DIR = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { "build-bench" }
$OUT_DIR   = if ($env:OUT_DIR)   { $env:OUT_DIR }   else { "benchmarks/data" }
$BENCH_EXE = if ($env:BENCH_EXE) { $env:BENCH_EXE } else { "fluid-sim-bench" }
$MIN_TIME  = if ($env:MIN_TIME)  { $env:MIN_TIME }  else { "0.3s" }
$REPS      = if ($env:REPS)      { $env:REPS }      else { "5" }

New-Item -ItemType Directory -Force -Path $OUT_DIR | Out-Null

function Get-ShortCommit {
  try {
    $c = (& git rev-parse --short HEAD 2>$null)
    if ([string]::IsNullOrWhiteSpace($c)) { "nogit" } else { $c.Trim() }
  } catch { "nogit" }
}
$COMMIT = Get-ShortCommit
$RAW_CSV = Join-Path $OUT_DIR ("bench_{0}.csv" -f $COMMIT)

# --- Choose generator/toolchain ---
$cmakeGen = $env:CMAKE_GENERATOR
if (-not $cmakeGen) {
  $hasNinja = $null -ne (Get-Command ninja -ErrorAction SilentlyContinue)
  if ($hasNinja) {
    $cmakeGen = "Ninja"
  } else {
    # Try to find Visual Studio via vswhere
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
      # Use a modern VS generator; CMake will pick the newest installed VS
      $cmakeGen = "Visual Studio 17 2022"
    }
  }
}

if (-not $cmakeGen) {
  throw @"
No suitable CMake generator found.

Install ONE of the following, then rerun:
  - Visual Studio Build Tools (C++ workload), and run this script from "Developer PowerShell for VS"
  - Ninja (and a compiler toolchain), then rerun

Or set CMAKE_GENERATOR explicitly, e.g.:
  `$env:CMAKE_GENERATOR = "Visual Studio 17 2022"
  `$env:CMAKE_GENERATOR = "Ninja"
"@
}

Write-Host "Using CMake generator: $cmakeGen"

# If using VS generator, default to x64 unless user overrides
$cmakeArchArgs = @()
if ($cmakeGen -like "Visual Studio*") {
  if (-not $env:CMAKE_GENERATOR_PLATFORM) {
    $cmakeArchArgs += @("-A", "x64")
  }
}

Write-Host "[1/3] Configure (Release) -> $BUILD_DIR"
& cmake -S . -B $BUILD_DIR -G $cmakeGen @cmakeArchArgs `
  -DBUILD_BENCHMARKS=ON `
  -DCMAKE_BUILD_TYPE=Release

Write-Host "[2/3] Build target -> $BENCH_EXE"
# --parallel works across generators; VS uses --config for multi-config
$buildArgs = @("--build", $BUILD_DIR, "--target", $BENCH_EXE, "--parallel")
if ($cmakeGen -like "Visual Studio*") { $buildArgs += @("--config", "Release") }
& cmake @buildArgs

Write-Host "[3/3] Run benchmarks -> $RAW_CSV"

# Find the exe for single-config and multi-config layouts
$exeCandidates = @(
  (Join-Path $BUILD_DIR "$BENCH_EXE.exe"),
  (Join-Path $BUILD_DIR (Join-Path "Release" "$BENCH_EXE.exe")),
  (Join-Path $BUILD_DIR (Join-Path "bin" "$BENCH_EXE.exe")),
  (Join-Path $BUILD_DIR (Join-Path "bin\Release" "$BENCH_EXE.exe"))
)
$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
  throw "Benchmark executable not found. Looked in: $($exeCandidates -join ', ')"
}

& $exePath `
  "--benchmark_repetitions=$REPS" `
  "--benchmark_min_time=$MIN_TIME" `
  "--benchmark_out_format=csv" `
  "--benchmark_out=$RAW_CSV"

Write-Host "Done. CSV: $RAW_CSV"