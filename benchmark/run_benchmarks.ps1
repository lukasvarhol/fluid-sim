function Get-ShortCommit{
  try {
    $commit = (& git rev-parse --short HEAD 2>$null)
    if ([string]::IsNullOrWhiteSpace($commit)) { "nogit" } else { $commit.Trim() }
  } catch { "nogit" }
}

$COMMIT = Get-ShortCommit

$particleCounts = @(100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000)
$backends = @("cpu-sequential", "cpu-parallel")

foreach($backend in $backends) {
  foreach($quantity in $particleCounts) {
    Write-Host "Running benchmark: $backend - $quantity particles"
    $elapsed = Measure-Command { & ".\build\Release\fluid-sim.exe" --benchmark -c $COMMIT -b $backend -p $quantity -f 2000}
    Write-Host "Completed in $($elapsed.TotalSeconds)s"
  }
}