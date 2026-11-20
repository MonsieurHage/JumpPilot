#!/usr/bin/env python3
"""Installation d'une IPA DÉJÀ SIGNÉE sur iPhone via USB (pymobiledevice3).

Usage :
    pip install pymobiledevice3
    python install_signed_ipa.py chemin/vers/JumpPilot.ipa

L'iPhone doit être branché en USB, déverrouillé et avoir fait confiance à ce PC.
Cette voie ne signe PAS l'application : l'IPA doit déjà être signée
(construite avec build-ios-signed.yml ou signée manuellement).
"""

import subprocess
import sys
import os


def run(cmd):
    print(">", " ".join(cmd))
    return subprocess.run(cmd, check=True)


def main():
    if len(sys.argv) < 2:
        print("Usage : python install_signed_ipa.py <fichier.ipa>")
        sys.exit(1)
    ipa = sys.argv[1]
    if not os.path.isfile(ipa):
        print(f"Fichier introuvable : {ipa}")
        sys.exit(1)

    try:
        import pymobiledevice3  # noqa: F401
    except ImportError:
        print("pymobiledevice3 n'est pas installé. Lancez : pip install pymobiledevice3")
        sys.exit(1)

    print("Vérification de l'appareil connecté...")
    run([sys.executable, "-m", "pymobiledevice3", "usbmux", "list"])
    print("Installation de l'IPA...")
    run([sys.executable, "-m", "pymobiledevice3", "apps", "install", ipa])
    print("Terminé. Jump Pilot devrait apparaître sur l'iPhone.")


if __name__ == "__main__":
    main()
