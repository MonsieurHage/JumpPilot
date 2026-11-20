# Jump Pilot — iPhone 12

Projet d’application iOS native pour piloter les robots **Parrot Jumping Sumo, Jumping Night et Jumping Race**. « FreeFlight Jumping » désigne l’application Parrot utilisée avec ces modèles.

**Installation : Mac, Xcode et iPhone sous iOS 16 ou ultérieur.** Le dossier contient le projet à compiler et signer avec votre compte Apple. Il n’inclut pas d’application iOS déjà signée. Sans accès à un Mac, cette livraison ne peut pas être installée directement sur l’iPhone.

Ouvrez **Commencer.html** pour consulter le guide illustré par étapes dans votre navigateur.

## Installer sur l’iPhone

1. Sur un Mac, installez une version de Xcode compatible avec la version d’iOS de votre iPhone.
2. Décompressez le dossier et ouvrez **JumpPilot.xcodeproj**.
3. Dans Xcode > Settings > Apple Accounts, ajoutez votre compte Apple.
4. Sélectionnez le projet JumpPilot, puis TARGETS > JumpPilot > Signing & Capabilities. Cochez **Automatically manage signing** et choisissez votre **Team**. Si Xcode demande un identifiant unique, remplacez `fr.raphael.JumpPilot` par un identifiant personnel, par exemple `fr.votrenom.JumpPilot`.
5. Branchez l’iPhone au Mac, déverrouillez-le et acceptez « Faire confiance à cet ordinateur ». Activez **Réglages > Confidentialité et sécurité > Mode développeur** si iOS le demande, puis suivez les étapes de redémarrage.
6. Choisissez votre iPhone comme destination de compilation dans la barre de Xcode et cliquez sur **▶ Run**.
7. Si nécessaire, autorisez le développeur dans Réglages > Général > VPN et gestion de l’appareil.

Un compte Apple gratuit permet l’installation personnelle. Avec une Personal Team, le profil expire au bout de **sept jours** ; il faut alors relancer l’installation depuis Xcode. La publication sur l’App Store ou TestFlight nécessite une autre procédure et n’est pas incluse.

## Se connecter au robot

1. Allumez le Parrot Jumping et fermez FreeFlight sur les autres appareils.
2. Sur l’iPhone, rejoignez le Wi-Fi du robot dans **Réglages > Wi-Fi** : son nom commence généralement par JumpingSumo, JumpingNight ou JumpingRace. Restez sur ce réseau même s’il n’a pas d’accès Internet.
3. Ouvrez Jump Pilot > **Connexion > Se connecter**.
4. Acceptez l’autorisation **Réseau local**. Si la première tentative échoue pendant l’affichage de cette autorisation, touchez de nouveau Se connecter.
5. Quand l’état indique Connecté, touchez **Activer**.

Adresse par défaut : **192.168.2.1**. Elle est modifiable dans l’écran Connexion. La session utilise le port TCP 44444 pour négocier les ports UDP, sans serveur intermédiaire. Aucun accès Internet n’est nécessaire au pilotage une fois l’application installée.

## Piloter

- Maintenez le joystick : haut = avancer ; bas = reculer ; gauche/droite = tourner. Sa position règle l’intensité de la consigne.
- Relâchez le joystick pour envoyer une consigne nulle aux roues.
- Le curseur limite la vitesse, avec **35 % au démarrage**.
- **Saut haut** et **Saut long** demandent le mouvement correspondant. Un délai de trois secondes sépare les demandes.
- **STOP** met les consignes des roues à zéro, désactive le pilotage et demande au robot d’annuler son saut ou son animation. Touchez Activer pour repartir. Un saut déjà lancé ne peut pas être annulé en l’air.
- Le retour caméra provient du drone. Une image sans mise à jour depuis deux secondes est assombrie.
- Le niveau de batterie est affiché dès réception. Une alerte de batterie critique désactive les commandes.
- Une interruption désactive les commandes. Le passage en arrière-plan ou le verrouillage déconnecte le robot.

## Vérifications réalisées et limites

Le moteur réseau C a été compilé et exécuté sous Linux avec avertissements stricts. Ses tests couvrent les échanges TCP/UDP avec un robot simulé, les retransmissions, les commandes signées, la batterie, les fragments vidéo, le relâchement des consignes et la déconnexion. Le décodeur de protocole a également reçu 50 000 entrées malformées sous AddressSanitizer et UndefinedBehaviorSanitizer.

La syntaxe Swift, la structure du projet Xcode, les fichiers de configuration et les ressources ont été contrôlés. **La compilation avec le SDK iOS, l’affichage sur un iPhone et la commande d’un robot physique n’ont pas pu être exécutés dans cet environnement.** La compatibilité matérielle reste à confirmer avec votre modèle et son micrologiciel.

Premier essai : utilisez un sol dégagé, une vitesse basse, et vérifiez successivement connexion, marche avant/arrière, rotation, relâchement, STOP, batterie et caméra. Testez les sauts en dernier. Gardez le robot en vue : une rupture radio empêche l’application de garantir la réception d’un ordre d’arrêt.

Le programme vise uniquement la famille Jumping Wi-Fi. Les drones aériens Mambo, Rolling Spider, Bebop et Anafi utilisent d’autres commandes ou transports.

## Dépannage

- **Drone inaccessible** : vérifiez le réseau Wi-Fi, son adresse et Réglages > Confidentialité et sécurité > Réseau local > Jump Pilot. Fermez les autres applications de pilotage, puis réessayez.
- **Connexion refusée** : fermez FreeFlight et les autres contrôleurs. Redémarrez le robot si nécessaire.
- **Caméra absente, commandes disponibles** : reconnectez-vous et vérifiez le micrologiciel du modèle. Le flux MJPEG doit utiliser le protocole ARStream 1 documenté par Parrot.
- **Liaison perdue** : reconnectez-vous manuellement puis réactivez le pilotage. Aucune reprise automatique du déplacement n’est effectuée.
- **Application expirée après quelques jours** : rebranchez l’iPhone au Mac et relancez Run dans Xcode avec votre Personal Team.

## Pour poursuivre le développement

- `JumpPilot/App` : interface SwiftUI, gestion du joystick et cycle de vie iOS.
- `JumpPilot/Core` : protocole ARSDK, sockets TCP/UDP, commandes et réception vidéo.
- `Tests` : tests de protocole et simulation du robot.
- `Scripts/verify_macos.sh` : compilation du simulateur iOS à exécuter sur Mac.
- `VALIDATION.md` : détail des contrôles et limites.
- `SOURCES.md` : références officielles consultées.

Les tests du cœur se lancent avec `make test` depuis ce dossier ; ils nécessitent un compilateur C, Python 3 et les bibliothèques de sanitisation. L’application iOS n’a aucune dépendance externe à télécharger.

## Installation depuis un PC Windows (sans Mac)

Si vous ne disposez pas de Mac, consultez **INSTALLER_DEPUIS_WINDOWS.md**.
Le principe : compilation de l’IPA dans le cloud (GitHub Actions, runner macOS
gratuit), puis signature et installation sur l’iPhone depuis Windows avec
Sideloadly. Un compte Apple gratuit suffit ; le profil expire au bout de
sept jours et se renouvelle en relançant l’installation.

Application indépendante, sans affiliation à Parrot. Les noms de produits appartiennent à leurs titulaires.
