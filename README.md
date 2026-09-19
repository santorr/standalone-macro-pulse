# MacroPulse

[![CI](https://github.com/santorr/standalone-macro-pulse/actions/workflows/ci.yml/badge.svg)](https://github.com/santorr/standalone-macro-pulse/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/santorr/standalone-macro-pulse)](https://github.com/santorr/standalone-macro-pulse/releases/latest)

Application desktop **Windows 10/11 x64**, en **C++20 / Win32**, pour automatiser des clics et composer manuellement des macros. Interface en français, thème sombre violet, exécutable autonome sans installation ni bibliothèque tierce.

L'interface comprend des boutons de souris visuels, des préréglages à 5 / 10 / 50 clics par seconde et une icône originale intégrée. Les informations d'implémentation restent dans cette documentation. Les champs de position apparaissent lorsque « Position fixe » est sélectionnée. Dans les macros, les flèches permettent de monter ou descendre l'action sélectionnée.

## Lancer

Télécharger `MacroPulse-X.Y.Z-windows-x64.exe` depuis les [releases GitHub](https://github.com/santorr/standalone-macro-pulse/releases/latest), ou extraire l'archive ZIP qui contient aussi la documentation. Après une compilation locale, ouvrir `build/Release/MacroPulse.exe`. Fermer une ancienne version avant la mise à jour. Une seule instance de la bibliothèque peut être ouverte dans une session Windows : relancer l'application la ramène au premier plan. L'exécutable peut être copié seul dans un autre dossier Windows. Il ne demande pas les droits administrateur.

Les exécutables ne sont pas signés avec un certificat Authenticode. Le fichier `SHA256SUMS.txt` de chaque release permet de vérifier l'intégrité du téléchargement avec `Get-FileHash -Algorithm SHA256 <fichier>`.

La fenêtre porte toujours le nom **MacroPulse**, quelle que soit la macro ouverte ou modifiée.

La version 1.3.2 supprime les cadres de focus en pointillés des boutons, des onglets et des raccourcis. À la souris, aucun contour de focus supplémentaire n'est dessiné. La navigation avec Tab ou les flèches conserve un repère discret grâce à la bordure violette du contrôle ; un clic rétablit l'apparence souris. La capture d'une touche garde son état violet pendant l'écoute.

La version 1.3.1 corrige le rendu au changement d'onglet : les contrôles reçoivent directement leur visibilité finale, les déplacements sont regroupés avant le redessin et les fonds sont sombres dès l'effacement. Le double tampon Windows englobe la fenêtre et ses contrôles enfants ([WS_EX_COMPOSITED](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)).

### Préférences et raccourcis

La page **Préférences** permet de modifier les quatre raccourcis : auto-clicker, exécution de macro, arrêt global et capture de position. Cliquer sur le raccourci, puis appuyer sur la touche ou la combinaison souhaitée (`A`, `F10`, `Ctrl+F6`, `Alt+Q`…), comme dans un jeu. Relâcher les touches et cliquer sur **Appliquer les raccourcis**. **Échap**, un clic ailleurs ou un changement de fenêtre annule la capture en conservant la valeur précédente. Le bouton **Valeurs par défaut** prépare F6/F7/F8/F9 ; cliquer sur Appliquer pour confirmer ce changement.

Les lettres, chiffres, touches de fonction, flèches, touches de navigation et du pavé numérique peuvent être utilisées seules ou avec Ctrl/Alt/Shift. La ponctuation suit la disposition du clavier. F12, Alt+F4 et les combinaisons Windows sont réservées ; Ctrl/Alt/Shift seuls ne servent pas de raccourcis globaux. Les doublons sont refusés. En cas de conflit avec une autre application, l'application des nouvelles valeurs conserve les anciens raccourcis. Si le raccourci d'arrêt est indisponible, l'exécution est désactivée jusqu'à sa reconfiguration. Pendant la capture, tous les raccourcis sont suspendus jusqu'au relâchement des touches, puis réactivés avec vérification des conflits. Hors capture, les raccourcis de démarrage n'exécutent rien lorsque Préférences est au premier plan ; l'arrêt reste disponible.

L'intervalle, le nombre de clics, le bouton, la position, le délai de départ, la page ouverte et les raccourcis appliqués sont mémorisés automatiquement. Les réglages sont enregistrés dans `%LOCALAPPDATA%\MacroPulse\settings.dat`, après une courte pause dans les modifications et à la fermeture. Un champ numérique invalide conserve sa dernière valeur valide enregistrée. Un fichier de réglages illisible ramène aux valeurs par défaut avec un message.

La bibliothèque et la macro sélectionnée sont restaurées au lancement, sans exécution automatique. Lors de la première ouverture de la version 1.4, la dernière macro utilisée avec l'ancienne version est copiée dans la bibliothèque si elle existe encore ; son fichier original reste intact.

Pendant la saisie dans un champ (notamment le nom d'une macro), les raccourcis globaux sont suspendus pour laisser saisir librement les lettres, même si elles sont affectées à l'auto-clicker. Ils sont rétablis en quittant le champ ou l'application, après relâchement des touches.

Les raccourcis indiqués dans les sections suivantes sont les valeurs par défaut ; l'interface affiche toujours les combinaisons réellement configurées.

### Auto-clicker

1. Choisir le bouton gauche, droit ou milieu.
2. Définir l'intervalle en millisecondes (1 à 60 000) et le nombre de clics (0 = continu).
3. Suivre le curseur ou choisir une position fixe. **F9** capture les coordonnées écran, y compris sur les écrans secondaires avec coordonnées négatives.
4. Placer le curseur sur la cible et utiliser **F6**. Le délai de départ commun vaut 1 500 ms par défaut.
5. **F8** arrête toute exécution, même lorsque MacroPulse est en arrière-plan. F6/F7 arrêtent aussi une exécution déjà en cours.

### Bibliothèque et éditeur de macros

**Mes macros** affiche toutes les macros dans une bibliothèque intégrée. Sélectionner une macro dans la liste, modifier son nom dans le panneau de droite, puis cliquer sur **Modifier la macro** (ou double-cliquer sur la liste) pour composer ses actions. **Nouvelle macro** crée une séquence vide ; **Dupliquer** en fait une copie indépendante ; **Supprimer** demande une confirmation. Le bouton **Mes macros** dans l'éditeur revient à la bibliothèque. **Exécuter** ou **F7** lance la macro sélectionnée ; une seule séquence s'exécute à la fois.

Il n'y a plus de boutons Ouvrir/Enregistrer, de sélecteur de fichier, de glisser-déposer ou de chemin à gérer. Les noms, répétitions, actions ajoutées ou appliquées, réorganisations et suppressions sont sauvegardés automatiquement après 600 ms, au changement de macro et à la fermeture. Les séquences vides sont conservées aussi. Les paramètres d'une étape en cours de saisie sont intégrés à la séquence avec **Ajouter** ou **Appliquer**.

Choisir une action, remplir les paramètres et cliquer sur **Ajouter**. Sélectionner une ligne pour la modifier, puis **Appliquer**. Les boutons permettent de dupliquer, déplacer ou supprimer les étapes.

Pour une touche, cliquer sur le bouton affichant la combinaison et appuyer directement sur le clavier. Les touches Tab, Entrée et Échap sont capturées aussi. Dans cet éditeur, cliquer ailleurs annule la capture (Échap est une touche de macro valide). Pour les actions Appuyer / Relâcher, presser puis relâcher une touche seule ; les modificateurs seuls comme Ctrl ou Shift sont également disponibles.

Actions :

- Clic souris à la position actuelle ou à une position fixe.
- Déplacement absolu du curseur.
- Touche ou combinaison, par exemple `A`, `Enter`, `Ctrl+C`, `Ctrl+Shift+S`.
- Appuyer / relâcher une touche seule, pour la maintenir pendant plusieurs étapes.
- Pause.
- Molette verticale, en crans positifs ou négatifs.

Le délai est une **attente minimale avant l'action**. Pour une pause, il est sa durée totale. Les boucles peuvent être finies ou continues (`0`). Les touches maintenues sont relâchées en fin de boucle et lors de l'arrêt. Une boucle continue exige au moins 1 ms d'attente cumulée.

**F7** lance la macro sélectionnée par défaut. Les touches principales des quatre raccourcis configurés sont réservées, même avec d'autres modificateurs, pour éviter qu'une macro déclenche ses propres commandes globales. Modifier les raccourcis dans Préférences libère les anciennes touches. Une macro récupérée de l'ancienne version qui utilise une touche réservée peut être corrigée ; son exécution est bloquée tant que le conflit subsiste.

Le stockage interne se trouve dans `%LOCALAPPDATA%\MacroPulse\library.dat`, à côté des réglages. Un remplacement atomique et une copie `.bak` conservent la génération précédente valide. Si le stockage principal est illisible, la copie est récupérée automatiquement. Si les deux sont illisibles, ils sont préservés et la bibliothèque reste indisponible au lieu d'être écrasée par une bibliothèque vide. Un échec d'écriture garde les modifications en mémoire, affiche un message et empêche une fermeture normale tant qu'elles ne sont pas sauvegardées. Limites : 1 000 macros, 10 000 étapes par macro, 200 000 étapes au total ; les noms Unicode font de 1 à 80 caractères.

## Compilation et tests

Prérequis : Windows, Visual Studio 2022 avec **Développement Desktop en C++**, Windows SDK, composant CMake et **PowerShell 7**.

```powershell
pwsh -File .\scripts\build.ps1
.\build\Release\MacroPulse.exe
```

Le script construit en Release x64 et exécute les tests. `-Configuration Debug` sélectionne Debug. Le runtime MSVC est lié statiquement.

Depuis un environnement disposant de CMake :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Les tests couvrent les fichiers invalides, les sauvegardes, les raccourcis, l'ordre des entrées, les répétitions, l'interruption des longues attentes, les touches relâchées et les injections partielles/refusées. Un test de l'interface exerce ses vrais contrôles (ajout, édition, déplacement, duplication, suppression, navigation) sans enregistrer de raccourcis globaux ni injecter d'entrées. Les tests du moteur utilisent une sortie simulée ; ils ne valident pas la réception des événements par une application externe.

Les tests de préférences vérifient aussi les chemins Unicode, le rejet des fichiers corrompus et le remplacement des raccourcis sans perte des anciens en cas d'échec. Deux processus de test successifs vérifient l'enregistrement puis la restauration de la session dans un dossier du build, sans toucher aux réglages de l'utilisateur. Un test Windows réserve brièvement une combinaison Ctrl+Alt+Shift+F13–F24 libre entre deux fenêtres invisibles pour vérifier le conflit puis la libération ; il n'injecte aucune touche.

Les tests de bibliothèque couvrent les noms Unicode, les séquences vides, les copies indépendantes, la suppression de la dernière macro, la sélection restaurée, les limites, les écritures refusées et les données invalides. Un test de l'application vérifie la migration de l'ancienne macro, la récupération de la copie de secours et la protection des données lorsque les deux copies sont illisibles. Le test de session retrouve deux macros distinctes après redémarrage. Tous ces fichiers de test sont isolés des données utilisateur.

Les tests de capture exercent les messages clavier : combinaison Ctrl/Alt, répétition d'une touche maintenue, annulation par Échap ou perte de focus, touche réservée, suspension et réactivation des raccourcis après relâchement, ainsi que Tab/Échap/modificateur seul dans une macro. La restauration de session vérifie un raccourci à lettre seule.

Le test d'interface vérifie aussi douze changements d'onglet successifs, les champs conditionnels et la conservation des valeurs. Il préremplit un tampon en blanc, appelle les vrais gestionnaires d'effacement de la fenêtre et de chaque contrôle visible, puis contrôle leurs couleurs aux coins et au centre. Les aperçus statiques valident le rendu final ; ces vérifications ne constituent pas un enregistrement vidéo du bureau.

Les aperçus de validation sont dans `artifacts/`. Pour les régénérer, utiliser `MacroPulse.exe --render-preview <dossier>`. Ce mode exécute le test de l'interface et rend son propre dessin ainsi que ses contrôles natifs en PNG ; il n'enregistre pas les autres fenêtres du bureau et n'injecte aucune entrée. L'icône et son prompt de génération sont documentés dans `assets/README.md`.

```powershell
.\build\Release\pulse_tests.exe --benchmark
```

Ce benchmark mesure le moteur de temporisation avec une sortie simulée. Il ne mesure pas la capacité d'une application cible à recevoir des clics. Un exemple de mesure sur la machine de développement : 983 actions/s demandées à 1 ms, médiane 1,006 ms, p95 1,248 ms. Les résultats varient avec la charge système.

## CI/CD et publication

Le dépôt conserve les sources, les tests, l'icône, les scripts et la documentation. Les dossiers `build/`, `build-*/`, `out/`, `dist/` et `artifacts/`, les préférences locales et les secrets sont exclus par `.gitignore`. Les exécutables sont distribués via les releases, pas dans l'historique Git.

- **CI** : chaque push sur `main` et chaque pull request compile et exécute les six tests en **Debug et Release x64** sur Windows Server 2022 / Visual Studio 2022. Les packages Release restent téléchargeables comme artefacts pendant 14 jours. Un lancement manuel est aussi possible depuis Actions.
- **Release** : un tag `vX.Y.Z` lance le même pipeline complet. Le tag doit correspondre exactement au fichier `VERSION`. Après réussite des deux configurations, le workflow vérifie les empreintes, charge les fichiers dans un brouillon, puis publie la release avec les notes générées par GitHub.
- Les actions officielles sont figées par SHA de commit, avec des propositions de mise à jour hebdomadaires par Dependabot. Seul le job de publication possède le droit `contents: write`. Aucun token personnel ni secret supplémentaire n'est nécessaire.

### Préparer une nouvelle version

`VERSION` est la source unique de la version : CMake génère les ressources Windows et le manifeste à partir de ce fichier. Utiliser trois nombres sans zéros initiaux : **MAJEUR** pour une incompatibilité, **MINEUR** pour une fonctionnalité compatible, **CORRECTIF** pour une correction. Chaque nombre est limité à 65535 par le format de version Windows. Ce pipeline publie uniquement des versions stables.

1. Modifier `VERSION` et ajouter les changements dans `CHANGELOG.md`.
2. Compiler et tester, puis intégrer le commit sur `main` et vérifier que la CI est verte.
3. Créer et pousser le tag correspondant. Exemple pour une prochaine correction :

```powershell
git switch main
git pull --ff-only
# VERSION et CHANGELOG.md doivent déjà être commités avec la version 1.4.1.
git tag -a v1.4.1 -m "MacroPulse 1.4.1"
git push origin v1.4.1
```

Le workflow produit `MacroPulse-1.4.1-windows-x64.exe`, `MacroPulse-1.4.1-windows-x64.zip` et `SHA256SUMS.txt`. Les archives de sources sont fournies automatiquement par GitHub. Consulter [Actions](https://github.com/santorr/standalone-macro-pulse/actions) pour les résultats.

Pour valider les packages localement :

```powershell
pwsh -File .\scripts\build.ps1 -Configuration Debug
pwsh -File .\scripts\build.ps1 -Configuration Release
pwsh -File .\scripts\package.ps1
```

Le script de packaging vérifie que l'exécutable Release porte la version attendue. Le workflow de release impose en plus les tests des deux configurations. Ne pas déplacer un tag publié : corriger avec une nouvelle version. En cas d'échec transitoire, relancer le workflow depuis GitHub Actions ; il peut reprendre un brouillon, mais refuse d'écraser une release déjà publiée.

## Architecture et performances

- `src/engine.*` : un thread de travail dédié, prioritaire au-dessus de la normale ; `SendInput` pour l'injection ; `QueryPerformanceCounter` et un waitable timer haute résolution pour l'ordonnancement, avec repli sur un timer standard si nécessaire.
- `src/model.*` : validation, noms de touches et fichiers versionnés, indépendants de l'interface.
- `src/preferences.*` : réglages locaux versionnés, validation et remplacement transactionnel des raccourcis. `src/preferences_ui.inl` relie la restauration et la sauvegarde différée aux contrôles.
- `src/library.*` : bibliothèque interne versionnée, validation, sauvegarde atomique avec copie de secours. `src/library_ui.inl` gère la sélection, la migration, l'édition des noms et la sauvegarde automatique.
- `src/main.cpp` : interface Win32, raccourcis globaux et éditeur. Contrôles Windows natifs, adaptation DPI et rafraîchissement des statistiques à 10 Hz pendant l'exécution. Aucun rafraîchissement périodique du dessin au repos.
- Aucun polling actif d'attente, hook global clavier/souris, réseau, télémétrie, pilote ou modification globale de résolution des timers.
- Un événement Windows réveille immédiatement les attentes lors de l'arrêt. L'auto-clicker saute les créneaux manqués au lieu de générer une rafale de rattrapage.
- Les sauvegardes passent par un fichier temporaire puis un remplacement ; un échec de validation/lecture conserve la macro courante.

Windows n'est pas un système temps réel : demander 1 ms ne garantit ni une échéance exacte ni 1 000 clics reçus par seconde. Le comportement dépend de la charge et de la façon dont la cible traite les événements. Une touche « appuyer » maintient son état ; elle ne produit pas automatiquement la répétition matérielle du clavier. Les raccourcis sont interprétés avec la disposition clavier de la fenêtre active au démarrage ; les modificateurs physiquement maintenus peuvent affecter les entrées.

`SendInput` respecte les niveaux de privilèges Windows : une application normale ne peut pas injecter dans une cible exécutée comme administrateur. MacroPulse signale les injections refusées sans tenter de contourner cette restriction. L'absence d'erreur ne garantit pas qu'une application accepte les entrées synthétiques.

Références Microsoft : [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput), [waitable timers](https://learn.microsoft.com/en-us/windows/win32/sync/waitable-timer-objects).

## Ancien format `.mpulse` v1 (migration uniquement)

Format texte strict, limité à 4 Mo / 10 000 étapes :

```text
MACROPULSE 1
repetitions nombre_etapes
action delai_ms x y position_fixe bouton touche_vk modificateurs crans
```

Actions 0..6 : clic, déplacement, touche, appuyer, relâcher, pause, molette. Boutons 0..2 : gauche, droit, milieu. Modificateurs : Ctrl=1, Alt=2, Shift=4, Win=8. Tous les champs sont numériques ; le fichier n'exécute pas de code ni de commandes système.
