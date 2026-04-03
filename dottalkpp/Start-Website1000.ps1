<#
    Start-Website1000.ps1
    Canonical launcher for local Next.js development on port 1000
#>

$projectPath = "D:\dev\x64base-IIS"
$port        = 1000
$url         = "http://localhost:$port"

Write-Host "Navigating to project directory..." -ForegroundColor Cyan
Set-Location $projectPath

# Ensure node_modules exists
if (-not (Test-Path "$projectPath\node_modules")) {
    Write-Host "Installing dependencies (npm install)..." -ForegroundColor Yellow
    npm install
}

Write-Host "Starting Next.js development server on port $port..." -ForegroundColor Green
Start-Process "powershell" -ArgumentList "-NoExit", "-Command", "cd `"$projectPath`"; npm run dev -- --port $port"

Start-Sleep -Seconds 2

Write-Host "Opening $url ..." -ForegroundColor Cyan
Start-Process $url