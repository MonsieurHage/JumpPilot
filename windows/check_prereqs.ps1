# Vérifie les prérequis Windows pour installer Jump Pilot.
$ok = $true

function Test-Cmd($name) {
    $c = Get-Command $name -ErrorAction SilentlyContinue
    if ($c) { Write-Host "  [OK] $name : $($c.Source)" -ForegroundColor Green; return $true }
    else { Write-Host "  [!!] $name introuvable" -ForegroundColor Red; return $false }
}

Write-Host "Prérequis Jump Pilot (Windows)" -ForegroundColor Magenta

Write-Host "`nOutils système :"
Test-Cmd python | Out-Null
Test-Cmd node | Out-Null

Write-Host "`nSideloadly :"
$sideloadly = @(
    "C:\Program Files\Sideloadly\Sideloadly.exe",
    "C:\Program Files (x86)\Sideloadly\Sideloadly.exe",
    (Join-Path $env:LOCALAPPDATA "Sideloadly\Sideloadly.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($sideloadly) { Write-Host "  [OK] $sideloadly" -ForegroundColor Green }
else { Write-Host "  [!!] Sideloadly non installé -> https://sideloadly.io/" -ForegroundColor Red; $ok = $false }

Write-Host "`nPython (pymobiledevice3, optionnel) :"
try {
    python -c "import pymobiledevice3" 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host "  [OK] pymobiledevice3 disponible" -ForegroundColor Green }
    else { Write-Host "  [--] pymobiledevice3 non installé (optionnel)" -ForegroundColor Yellow }
} catch { Write-Host "  [--] pymobiledevice3 non installé (optionnel)" -ForegroundColor Yellow }

Write-Host ""
if ($ok) { Write-Host "Prérequis principaux OK." -ForegroundColor Green }
else { Write-Host "Installez les éléments manquants puis relancez." -ForegroundColor Red }
