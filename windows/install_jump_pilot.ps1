<#
.SYNOPSIS
  Prépare et lance l'installation de Jump Pilot sur un iPhone depuis Windows.

.DESCRIPTION
  1. Télécharge la dernière IPA non signée depuis les Releases GitHub (ou utilise un fichier local).
  2. Vérifie la présence de Sideloadly et le lance avec l'IPA.
  L'utilisateur saisit ensuite son identifiant Apple dans Sideloadly, qui signe
  et installe l'application sur l'iPhone branché en USB.

.PARAMETER IpaPath
  Chemin vers une IPA locale. Si vide, téléchargement depuis GitHub.

.PARAMETER Repo
  Dépôt GitHub au format "utilisateur/depot".

.PARAMETER ReleaseTag
  Tag de release à télécharger ("latest" par défaut).

.EXAMPLE
  .\install_jump_pilot.ps1
  .\install_jump_pilot.ps1 -IpaPath C:\Downloads\JumpPilot-unsigned.ipa
#>

param(
    [string]$IpaPath = "",
    [string]$Repo = "VOTRE_COMPTE/JumpPilot",
    [string]$ReleaseTag = "latest"
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)  { Write-Host "    OK : $msg" -ForegroundColor Green }
function Write-Warn($msg){ Write-Host "    !  $msg" -ForegroundColor Yellow }
function Write-Err($msg) { Write-Host "    X  $msg" -ForegroundColor Red }

Write-Host ""
Write-Host "Jump Pilot - installation depuis Windows" -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta

# --- 1. Obtenir l'IPA ------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($IpaPath)) {
    Write-Step "Téléchargement de la dernière IPA depuis GitHub"
    if ($Repo -eq "VOTRE_COMPTE/JumpPilot") {
        Write-Err "Configurez le paramètre -Repo avec votre dépôt GitHub, par exemple :"
        Write-Host "    .\install_jump_pilot.ps1 -Repo raphael/JumpPilot" -ForegroundColor White
        exit 1
    }
    $workDir = Join-Path $env:TEMP "JumpPilot"
    New-Item -ItemType Directory -Force -Path $workDir | Out-Null
    $IpaPath = Join-Path $workDir "JumpPilot-unsigned.ipa"
    if ($ReleaseTag -eq "latest") {
        $url = "https://github.com/$Repo/releases/latest/download/JumpPilot-unsigned.ipa"
    } else {
        $url = "https://github.com/$Repo/releases/download/$ReleaseTag/JumpPilot-unsigned.ipa"
    }
    Write-Host "    URL : $url"
    try {
        Invoke-WebRequest -Uri $url -OutFile $IpaPath -UseBasicParsing
    } catch {
        Write-Err "Téléchargement impossible : $($_.Exception.Message)"
        Write-Warn "Vérifiez que la Release existe et que le fichier s'appelle JumpPilot-unsigned.ipa."
        exit 1
    }
    Write-Ok "IPA téléchargée : $IpaPath"
} else {
    if (-not (Test-Path $IpaPath)) {
        Write-Err "Fichier introuvable : $IpaPath"
        exit 1
    }
    Write-Ok "IPA locale : $IpaPath"
}

# --- 2. Vérifier Sideloadly -------------------------------------------------
Write-Step "Recherche de Sideloadly"
$sideloadlyCandidates = @(
    "C:\Program Files\Sideloadly\Sideloadly.exe",
    "C:\Program Files (x86)\Sideloadly\Sideloadly.exe",
    (Join-Path $env:LOCALAPPDATA "Sideloadly\Sideloadly.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Sideloadly\Sideloadly.exe")
)
$sideloadly = $sideloadlyCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $sideloadly) {
    Write-Warn "Sideloadly n'est pas installé."
    Write-Host ""
    Write-Host "  1. Téléchargez Sideloadly : https://sideloadly.io/" -ForegroundColor White
    Write-Host "  2. Installez-le (suivant, suivant, terminer)." -ForegroundColor White
    Write-Host "  3. Relancez ce script." -ForegroundColor White
    Write-Host ""
    try { Start-Process "https://sideloadly.io/" } catch {}
    exit 1
}
Write-Ok "Sideloadly trouvé : $sideloadly"

# --- 3. Lancer Sideloadly avec l'IPA --------------------------------------
Write-Step "Lancement de Sideloadly"
try {
    Start-Process -FilePath $sideloadly -ArgumentList "`"$IpaPath`""
    Write-Ok "Sideloadly ouvert avec l'IPA."
} catch {
    Write-Err "Impossible de lancer Sideloadly : $($_.Exception.Message)"
    exit 1
}

Write-Host ""
Write-Host "Dans Sideloadly :" -ForegroundColor Yellow
Write-Host "  - Branchez l'iPhone en USB et déverrouillez-le." -ForegroundColor White
Write-Host "  - Acceptez 'Faire confiance à cet ordinateur' sur l'iPhone." -ForegroundColor White
Write-Host "  - Saisissez votre identifiant Apple et votre mot de passe." -ForegroundColor White
Write-Host "  - Cliquez sur 'Start' pour signer et installer Jump Pilot." -ForegroundColor White
Write-Host ""
Write-Host "Après installation, sur l'iPhone :" -ForegroundColor Yellow
Write-Host "  Réglages > Général > VPN et gestion de l'appareil > faire confiance au développeur." -ForegroundColor White
Write-Host ""
Write-Host "Renouvellement : avec un compte gratuit, le profil expire au bout de 7 jours." -ForegroundColor Yellow
Write-Host "Relancez ce script pour réinstaller." -ForegroundColor Yellow
