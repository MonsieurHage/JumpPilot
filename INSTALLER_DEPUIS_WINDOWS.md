# Installer Jump Pilot sur l'iPhone depuis un PC Windows

Ce guide complète `LIRE_MOI.md`. Il permet d'installer l'application **sans Mac**,
en compilant l'IPA dans le cloud (GitHub Actions) puis en la signant/installant
depuis Windows avec Sideloadly.

## Vue d'ensemble

1. Vous mettez le projet sur GitHub (gratuit).
2. GitHub Actions compile l'IPA sur un Mac virtuel (gratuit pour un dépôt public).
3. Vous téléchargez l'IPA sur votre PC Windows.
4. Sideloadly signe l'IPA avec votre identifiant Apple et l'installe sur l'iPhone branché en USB.
5. Sur l'iPhone, vous faites confiance au développeur.

Avec un compte Apple gratuit, le profil expire au bout de **7 jours** : relancez
l'étape 4 pour renouveler.

## Prérequis

- Un compte GitHub gratuit.
- Un compte Apple (gratuit suffit pour un usage personnel).
- Un PC Windows avec Internet.
- Un iPhone sous iOS 16 ou ultérieur.
- Un câble USB pour brancher l'iPhone au PC.
- Git for Windows : https://git-scm.com/download/win
  (ou GitHub Desktop : https://desktop.github.com/)

## Étape 0 — (Optionnel) Vérifier les prérequis

Ouvrez PowerShell dans le dossier du projet et lancez :

```powershell
.\windows\check_prereqs.ps1
```

## Étape 1 — Mettre le projet sur GitHub

1. Créez un nouveau dépôt sur https://github.com/new (par exemple `JumpPilot`).
   - Visibilité : **Public** (pour bénéficier des minutes macOS gratuites) ou Private.
2. Dans le dossier du projet, ouvrez Git Bash ou PowerShell et lancez :

```bash
git init
git add .
git commit -m "Jump Pilot initial"
git branch -M main
git remote add origin https://github.com/VOTRE_COMPTE/JumpPilot.git
git push -u origin main
```

Si Git n'est pas installé, utilisez GitHub Desktop : "Add existing repository" puis "Publish".

## Étape 2 — Lancer la compilation cloud

1. Sur GitHub, ouvrez votre dépôt > onglet **Actions**.
2. Sélectionnez le workflow **Build Jump Pilot (IPA non signée)**.
3. Cliquez sur **Run workflow** > **Run workflow**.
4. Attendez la fin (quelques minutes). Une coche verte apparaît.
5. Dans le run, section **Artifacts**, téléchargez `JumpPilot-unsigned`.

> Astuce : pour publier automatiquement une Release téléchargeable, créez un tag :
> ```bash
> git tag v1.0
> git push origin v1.0
> ```
> Le workflow créera alors une Release avec l'IPA en asset.

## Étape 3 — Télécharger l'IPA sur le PC

Option A — via le script (recommandé) :

```powershell
.\windows\install_jump_pilot.ps1 -Repo VOTRE_COMPTE/JumpPilot
```

Le script télécharge la dernière Release et lance Sideloadly.

Option B — manuel : dézippez l'artefact téléchargé, vous obtenez `JumpPilot-unsigned.ipa`.

## Étape 4 — Installer Sideloadly

1. Téléchargez Sideloadly : https://sideloadly.io/
2. Installez-le (suivant, suivant, terminer).
3. Installez aussi **iTunes** (version bureau, pas Microsoft Store) et **iCloud**
   pour Windows si Sideloadly le demande — ils fournissent les pilotes USB Apple.

Vérifiez avec :

```powershell
.\windows\check_prereqs.ps1
```

## Étape 5 — Signer et installer sur l'iPhone

1. Branchez l'iPhone au PC en USB et déverrouillez-le.
2. Sur l'iPhone, touchez **Faire confiance à cet ordinateur** et saisissez le code.
3. Lancez Sideloadly (ou le script de l'étape 3 l'a déjà lancé).
4. Dans Sideloadly :
   - Glissez `JumpPilot-unsigned.ipa` dans la fenêtre (ou elle est déjà chargée).
   - Sélectionnez votre iPhone dans la liste.
   - Saisissez votre identifiant Apple et votre mot de passe.
   - Cliquez sur **Start**.
5. Sideloadly signe et installe l'application. Une icône "Jump Pilot" apparaît.

## Étape 6 — Faire confiance au développeur sur l'iPhone

Réglages > Général > VPN et gestion de l'appareil > touchez votre identifiant Apple
> **Faire confiance**. L'application peut alors se lancer.

## Renouvellement (compte gratuit)

Le profil expire au bout de 7 jours. L'icône reste mais l'app ne se lance plus.
Relancez simplement :

```powershell
.\windows\install_jump_pilot.ps1 -Repo VOTRE_COMPTE/JumpPilot
```

Sideloadly réinstalle et prolonge pour 7 jours supplémentaires. Vos réglages
dans l'app sont conservés.

## Alternative 1 — Codemagic (sans GitHub Actions)

Si vous préférez ne pas gérer GitHub Actions :

1. Créez un compte sur https://codemagic.io (gratuit).
2. Connectez votre dépôt GitHub.
3. Choisissez "iOS" > workflow par défaut.
4. Codemagic compile et vous propose de télécharger l'IPA.
5. Installez avec Sideloadly (étapes 4 à 6).

## Alternative 2 — IPA déjà signée (compte Apple payant)

Si vous avez un compte Apple Developer payant (99 USD/an) :

1. Créez un certificat et un profil de provisioning (type Development).
2. Exportez le certificat en `.p12` et encodez-le en base64.
3. Dans GitHub > Settings > Secrets and variables > Actions, créez :
   - `APPLE_CERT_BASE64`, `APPLE_CERT_PASSWORD`
   - `APPLE_PROVISIONING_PROFILE_BASE64`, `APPLE_TEAM_ID`
4. Lancez le workflow **Build Jump Pilot (IPA signée, compte Apple payant)**.
5. Téléchargez l'artefact `JumpPilot-signed`.
6. Installez l'IPA signée avec le script Python (optionnel) :

```powershell
pip install pymobiledevice3
python .\windows\install_signed_ipa.py chemin\vers\JumpPilot.ipa
```

ou avec Sideloadly / Apple Configurator.

## Alternative 3 — Appareil jailbreaké

Sur un iPhone jailbreaké, vous pouvez installer l'IPA non signée directement
avec Filza ou via SSH, sans Sideloadly ni compte Apple.

## Dépannage

- **"Untrusted Developer"** : faites confiance dans Réglages > Général > VPN et gestion.
- **Sideloadly ne détecte pas l'iPhone** : installez iTunes bureau + iCloud pour Windows,
  changez de câble/port USB, déverrouillez l'iPhone.
- **Erreur de signature** : vérifiez l'identifiant Apple et le mot de passe ; si la
  double authentification est activée, générez un mot de passe d'application sur
  https://appleid.apple.com.
- **L'app expire après 7 jours** : relancez l'installation (compte gratuit).
- **Build GitHub Actions échoue** : ouvrez le run, lisez les logs ; le plus souvent
  un problème de version Xcode. Essayez de changer `macos-14` en `macos-13` dans
  `.github/workflows/build-ios.yml`.
- **Drone inaccessible** : voir `LIRE_MOI.md` (section Dépannage).

## Sécurité

- L'IPA non signée ne contient aucun secret.
- Ne commitez jamais de certificat `.p12` ou de mot de passe dans le dépôt ;
  utilisez les secrets GitHub.
- Sideloadly envoie vos identifiants Apple uniquement aux serveurs Apple, pas à
  un tiers, mais utilisez un mot de passe d'application si possible.
