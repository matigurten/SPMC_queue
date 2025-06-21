Write-Host "Testing Multi-Consumer Snapshot System" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green

# Clean up any existing shared memory
Write-Host "Cleaning up existing shared memory..." -ForegroundColor Yellow
if (Test-Path "/dev/shm/myqueue*") { Remove-Item "/dev/shm/myqueue*" -Force }
if (Test-Path "/dev/shm/snapshots*") { Remove-Item "/dev/shm/snapshots*" -Force }

# Build the programs
Write-Host "Building programs..." -ForegroundColor Yellow
./build.sh

# Start the writer in background
Write-Host "Starting writer..." -ForegroundColor Yellow
Start-Process -FilePath "./shm_writer" -ArgumentList "myqueue" -WindowStyle Hidden
$writerProcess = Get-Process | Where-Object { $_.ProcessName -eq "shm_writer" }

# Wait a moment for writer to start
Start-Sleep -Seconds 1

# Start the reader in background
Write-Host "Starting reader..." -ForegroundColor Yellow
Start-Process -FilePath "./shm_reader" -ArgumentList "myqueue", "/snapshots" -WindowStyle Hidden
$readerProcess = Get-Process | Where-Object { $_.ProcessName -eq "shm_reader" }

# Wait a moment for reader to start
Start-Sleep -Seconds 2

# Start multiple consumers
Write-Host "Starting consumers..." -ForegroundColor Yellow
Start-Process -FilePath "./snapshot_consumer" -ArgumentList "/snapshots", "consumer1" -WindowStyle Hidden
$consumer1Process = Get-Process | Where-Object { $_.ProcessName -eq "snapshot_consumer" }

Start-Process -FilePath "./snapshot_consumer" -ArgumentList "/snapshots", "consumer2" -WindowStyle Hidden
$consumer2Process = Get-Process | Where-Object { $_.ProcessName -eq "snapshot_consumer" }

Start-Process -FilePath "./snapshot_monitor" -ArgumentList "/snapshots" -WindowStyle Hidden
$monitorProcess = Get-Process | Where-Object { $_.ProcessName -eq "snapshot_monitor" }

Write-Host "All processes started:" -ForegroundColor Green
Write-Host "  Writer PID: $($writerProcess.Id)" -ForegroundColor Cyan
Write-Host "  Reader PID: $($readerProcess.Id)" -ForegroundColor Cyan
Write-Host "  Consumer1 PID: $($consumer1Process.Id)" -ForegroundColor Cyan
Write-Host "  Consumer2 PID: $($consumer2Process.Id)" -ForegroundColor Cyan
Write-Host "  Monitor PID: $($monitorProcess.Id)" -ForegroundColor Cyan

Write-Host ""
Write-Host "System is running. Press Enter to stop all processes..." -ForegroundColor Yellow
Read-Host

# Stop all processes
Write-Host "Stopping all processes..." -ForegroundColor Yellow
if ($writerProcess) { Stop-Process -Id $writerProcess.Id -Force }
if ($readerProcess) { Stop-Process -Id $readerProcess.Id -Force }
if ($consumer1Process) { Stop-Process -Id $consumer1Process.Id -Force }
if ($consumer2Process) { Stop-Process -Id $consumer2Process.Id -Force }
if ($monitorProcess) { Stop-Process -Id $monitorProcess.Id -Force }

# Wait for processes to stop
Start-Sleep -Seconds 2

# Clean up
Write-Host "Cleaning up shared memory..." -ForegroundColor Yellow
if (Test-Path "/dev/shm/myqueue*") { Remove-Item "/dev/shm/myqueue*" -Force }
if (Test-Path "/dev/shm/snapshots*") { Remove-Item "/dev/shm/snapshots*" -Force }

Write-Host "Test completed." -ForegroundColor Green 