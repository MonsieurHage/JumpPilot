# Outils Windows pour Jump Pilot

- `check_prereqs.ps1` : vérifie les prérequis (Python, Sideloadly, pymobiledevice3).
- `install_jump_pilot.ps1` : télécharge la dernière IPA (depuis GitHub Releases) et lance Sideloadly.
- `install_signed_ipa.py` : installe une IPA déjà signée via USB avec pymobiledevice3.

Usage rapide :

```powershell
.\check_prereqs.ps1
.\install_jump_pilot.ps1 -Repo VOTRE_COMPTE/JumpPilot
```

Voir `INSTALLER_DEPUIS_WINDOWS.md` à la racine du projet pour le guide complet.
