function Get-ShortCommit{
  try {
    $commit = (& git rev-parse --short HEAD 2>$null)
    if ([string]::IsNullOrWhiteSpace($commit)) { "nogit" } else { $commit.Trim() }
  } catch { "nogit" }
}

$COMMIT = Get-ShortCommit

$particleCounts = @(200, 500, 1000, 2000, 5000, 10000, 20000, 50000)
$colliderCounts = @(1, 2, 5, 10, 15, 25, 35, 45)
$backends = @("cpu-sequential", "cpu-parallel")
$runs = 3

foreach($backend in $backends) {
  foreach ($quantity in $colliderCounts) {
    for ($run = 1; $run -le $runs; $run++){
      Write-Host "Running benchmark: $backend - $quantity colliders"
      $elapsed = Measure-Command { & ".\build\Release\fluid-sim.exe" --benchmark -c $COMMIT -b $backend -p 30000 -f 2000 -sdf $quantity -r $run}
      Write-Host "Completed in $($elapsed.TotalSeconds)s"
    }
  }
  foreach($N in $particleCounts) {
    for ($run = 1; $run -le $runs; $run++){ 
      Write-Host "Running benchmark: $backend - $N particles"
      $elapsed = Measure-Command { & ".\build\Release\fluid-sim.exe" --benchmark -c $COMMIT -b $backend -p $N -f 2000 -sdf 10 -r $run}
      Write-Host "Completed in $($elapsed.TotalSeconds)s"
    }
  }
}