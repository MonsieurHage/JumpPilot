# Références techniques

Consultation : 12 novembre 2026. Les identifiants sont issus des sources Parrot ; le moteur réseau de ce projet est une implémentation originale du protocole documenté.

- Parrot, **ARSDK Protocols**, négociation TCP, trames ARNetwork, configuration de la famille Jumping, ARCommands et ARStream 1 : https://developer.parrot.com/docs/SDK3/ARSDK_Protocols.pdf
- Parrot Developers, **JumpingSumoSample**, adresse 192.168.2.1 et port de découverte 44444 : https://github.com/Parrot-Developers/Samples/blob/master/Unix/JumpingSumoSample/JumpingSumoSample.c
- Parrot Developers, **jpsumo.xml**, version antérieure au retrait de la famille Jumping du dépôt principal : https://github.com/Parrot-Developers/arsdk-xml/blob/acbd609ba4af0534b39a486c93e06c5c05b3b976/xml/jpsumo.xml
- Parrot Developers, **common.xml**, états généraux et batterie : https://github.com/Parrot-Developers/arsdk-xml/blob/master/xml/common.xml
- Apple, **TN3179 — Understanding local network privacy** : https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy
- Apple, **Running your app on simulated or physical devices** : https://developer.apple.com/documentation/xcode/running-your-app-on-simulated-or-physical-devices
- Apple, **Developer account overview**, conditions de la Personal Team et durée des profils : https://developer.apple.com/help/account/basics/about-your-developer-account/

## Sous-ensemble implémenté

- Commande de roues : projet 3, classe 0, commande 0 ; flag u8, vitesse i8, rotation i8 ; canal 10 non acquitté, 20 Hz.
- Saut : projet 3, classe 2, commande 3 ; enum 32 bits little endian, long = 0 et haut = 1 ; canal 11 acquitté.
- Annulation : projet 3, classe 2, commande 1, et arrêt d’animation simple, commande 4, enum 0.
- Vidéo : projet 3, classe **18**, commande 0 ; activation u8. Le flux MJPEG arrive sur le canal 125.
- Batterie : projet 0, classe 5, événement 1 ; pourcentage u8.
- Alertes : projet 3, classe 1, événement 1 ; enum 32 bits. Batterie critique = 1, faible = 2.
- État du moteur de saut : projet 3, classe 3, événement 2.
- Initialisation : Common.Settings.AllSettings puis Common.Common.AllStates ; activation vidéo.

L’application utilise une adresse directe, sans recherche multicast/Bonjour. La clé NSLocalNetworkUsageDescription est définie. Aucun ancien SDK binaire Parrot, compte distant, Bluetooth, accès au microphone ou à la caméra de l’iPhone n’est requis.
