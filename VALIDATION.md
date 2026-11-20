# Validation — 12 novembre 2026

## Exécuté

- Compilation C11 du protocole et du transport avec `-Wall -Wextra -Werror -pedantic`.
- Tests des trames little endian, commandes de roues signées, datagrammes concaténés et tailles invalides.
- Tests JSON : réponse fragmentée, champs absents, valeurs hors limites, doublons, objets inconnus et refus explicite.
- Reconstitution vidéo : fragments inversés, doublons, retard, bornes et passage du compteur 65535 à 0.
- 50 000 entrées pseudo-aléatoires malformées injectées dans les trois décodeurs ; AddressSanitizer et UndefinedBehaviorSanitizer activés. LeakSanitizer désactivé car incompatible avec l’instrumentation de l’environnement.
- Tests d’intégration sur sockets réelles en boucle locale : négociation TCP avec réponse fragmentée, annonce du port UDP réel, retransmission avec même numéro de séquence, accusés de réception, événement batterie délivré une seule fois, réponse aux pings et vidéo.
- Vérification du blocage des déplacements tant que les commandes sont désactivées, expiration des consignes après 350 ms, intervalle entre sauts, STOP avec suppression des retransmissions de saut, verrouillage lors d’une batterie critique, perte de liaison après trois secondes et annulation rapide d’une connexion incomplète.
- Analyse syntaxique des fichiers Swift par tree-sitter, lecture du projet OpenStep et vérification des références.
- Lecture des fichiers plist/XML/JSON et vérification des chemins de ressources.

## À exécuter sur Mac et matériel

- Compilation et édition des liens avec Xcode / SDK iOS.
- Contrôle de la mise en page sur iPhone 12, en portrait et paysage.
- Installation signée, permission réseau local et connexion au Wi-Fi du robot.
- Confirmation sur le modèle réel : sens des roues, relâchement, STOP, sauts, batterie, MJPEG et comportement lors d’une coupure radio.

L’analyse syntaxique ne remplace pas la compilation Swift avec les frameworks Apple. Le simulateur réseau valide le transport logiciel, sans certifier le comportement de chaque micrologiciel Parrot.
