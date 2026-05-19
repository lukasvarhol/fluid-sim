function Get-ShortCommit{
  try {
    $commit = (& git rev-parse --short HEAD 2>$null)
    if ([string]::IsNullOrWhiteSpace($commit)) { "nogit" } else { $commit.Trim() }
  } catch { "nogit" }
}

$COMMIT = Get-ShortCommit

$colliderCounts = @(1, 2, 5, 10, 15, 25, 35, 45)
$backends = @("cpu-sequential", "cpu-parallel")

foreach($backend in $backends) {
  foreach($quantity in $colliderCounts) {
    Write-Host "Running benchmark: $backend - $quantity particles"
    $elapsed = Measure-Command { & ".\build\Release\fluid-sim.exe" --benchmark -c $COMMIT -b $backend -p 30000 -f 2000 -sdf $quantity}
    Write-Host "Completed in $($elapsed.TotalSeconds)s"
  }
}