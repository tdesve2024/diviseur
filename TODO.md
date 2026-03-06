# TODO — Diviseur Cowells RGB61

## En attente

- [ ] remplacer pas/div par ang/div dans le premier panneau
- [ ] Insérer une barre de progression qui donne le nombre de step à faire pour l'exécution (en fonction du paramétrage). Si possible mettre en évidence vitesse et accélération/décélération
- [ ] Bloquer les boutons avance et recule pendant l'exécution
- [ ] Tests diagnostics non-bloquants : l'interface se fige pendant l'exécution d'une étape si les lectures UART ont des timeouts. Envisager une exécution test-par-test ou un mécanisme polling côté client.

## Terminé

- [x] Remplacer les credentials WiFi codés en dur par WiFiManager
- [x] Implémentation complète du diviseur (moteur NEMA 14, driver TMC2209, interface web)
- [x] Il faudrait un design clair pour l'interface qui est difficile à lire dans le mode proposé Clair (fond blanc/gris) → **thème pastel (fond #eef2fb, cartes blanches, accents bleu #3d7ae8)**
- [x] Division courante / total → **affiché en grand dans la carte principale**
- [x] Le réglage ± du nombre de divisions (2 à 360) directement depuis l'écran → **boutons − et + ; clavier numérique popup avec préréglages**
- [x] Les boutons ◀ RECUL et ▶ AVANCE → **présents, style télécommande pastel**
- [x] Une barre de statut qui pulse pendant les mouvements → **animation pulse sur le point vert**
- [x] Style de bouton comme une télécommande → **grands boutons arrondis, contraste élevé**
- [x] Choix du mode StealthChop ou SpreadCycle → **toggle dans les réglages driver**
- [x] Est-il possible de s'assurer que toute l'interface soit visible sur l'écran de l'iPhone 13 Pro → **max-width 430px, viewport-fit=cover, safe-area-inset, boutons ≥44px**
- [x] Peux-tu écrire un programme de test pour le montage → **page /diag avec 10 tests en 4 étapes**
- [x] Une page pour le contrôleur et une page pour les tests, dans deux onglets → **onglets CONTRÔLEUR / DIAGNOSTIC en haut de l'écran**
- [x] Lorsque j'exécute les tests, l'interface devient inactive → **bouton "Tester" affiche "⏳ Test…" pendant l'exécution**
- [x] UART TMC2209 bidirectionnel (lecture registres, température, détection driver)
- [x] Bouton STOP d'urgence dans l'interface principale
- [x] Toggle activation/désactivation moteur dans l'interface principale
- [x] Je souhaitais une interface claire, dans des couleurs pastel → **thème pastel bleu/blanc v1.0**
- [x] Il faudrait indiquer les pourcentages d'utilisation de l'espace de l'arduino (flash et RAM) → **carte Système : RAM libre et Flash utilisé avec %**
- [x] Pour la saisie des divisions, un clavier numérique en popup serait plus pratique → **clavier 3×4 avec préréglages en chips**
- [x] Dans le titre, à côté du Cowells RGB61, on peut rappeler le rapport de division (40:1) → **sous-titre : Cowells RGB61 · 40:1 · NEMA 14 · vX.Y**
- [x] On pourrait indiquer un numéro de version, chaque commit incrémente un indice → **v1.0 défini par `#define FW_VERSION`, retourné par l'API**
