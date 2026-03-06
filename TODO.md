# TODO — Diviseur Cowells RGB61

## En attente

- [ ] Tests diagnostics non-bloquants : l'interface se fige pendant l'exécution d'une étape si les lectures UART ont des timeouts. Envisager une exécution test-par-test via `/api/diag/run` individuel ou un mécanisme polling côté client.
- [ ] Je souhaitais une interface claire, dans des couleurs pastel
- [ ] Il faudrait indiquer les pourcentage d'utilisation de l'espace de l'arduino (flash et RAM), avec la température du processeur
- [ ] Pour la saisie des divisions, un clavier numérique en popup serait plus pratique
- [ ] Dans le titre, à côté du Cowells RGB61, on peut rappeler le rapport de division (40:1)
- [ ] On pourrait également indiquer un numéro de version... Chaque commit pourrait incrémenter un indice de révision. Les changement de versions seront explicites

## Terminé

- [x] Remplacer les credentials WiFi codés en dur par WiFiManager
- [x] Implémentation complète du diviseur (moteur NEMA 14, driver TMC2209, interface web)
- [x] Il faudrait un design clair pour l'interface qui est difficile à lire dans le mode proposé Clair (fond blanc/gris) → **thème sombre complet (fond #0d1421, cartes #162236)**
- [x] Division courante / total → **affiché en grand dans la carte principale**
- [x] Le réglage ± du nombre de divisions (2 à 360) directement depuis l'écran → **boutons − et + avec liste préréglée**
- [x] Les boutons ◀ RECUL et ▶ AVANCE → **présents, style télécommande**
- [x] Une barre de statut qui pulse pendant les mouvements → **animation pulse sur le point vert**
- [x] Style de bouton comme une télécommande → **grands boutons arrondis, contraste élevé**
- [x] Choix du mode StealthChop ou SpreadCycle → **toggle dans les réglages driver**
- [x] Est-il possible de s'assurer que toute l'interface soit visible sur l'écran de l'iPhone 13 Pro → **max-width 430px, viewport-fit=cover, safe-area-inset, boutons ≥44px**
- [x] Peux-tu écrire un programme de test pour le montage → **page /diag avec 10 tests en 4 étapes**
- [x] Une page pour le contrôleur et une page pour les tests, dans deux onglets → **onglets CONTRÔLEUR / DIAGNOSTIC en haut de l'écran**
- [x] Lorsque j'exécute les tests, l'interface devient inactive → **le bouton "Tester" affiche "⏳ Test…" pendant l'exécution ; l'étape est lancée puis rafraîchie**
- [x] UART TMC2209 bidirectionnel (lecture registres, température, détection driver)
- [x] Bouton STOP d'urgence dans l'interface principale
- [x] Toggle activation/désactivation moteur dans l'interface principale
