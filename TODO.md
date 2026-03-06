# TODO — Diviseur Cowells RGB61

## En attente

- [ ] Il faudrai un design clair pour l’interface qui est difficile à lire dans le mode proposé Clair (fond blanc/gris)
- [ ] Division courante / total
- [ ] Le réglage ± du nombre de divisions (2 à 360) directement depuis l'écran
- [ ] Les boutons ◀ RECUL et ▶ AVANCE
- [ ] Une barre de statut qui pulse pendant les mouvements
- [ ] Style de bouton comme une télécommande
- [ ] Choix du mode StealthChop ou SpreadCycle
- [ ] Est-il possible de s’assurer que toute l’interface soit visible sur l’écran de l’iphone 13 pro
- [ ] Peux-tu écrire un programme de test pour le montage qui aiderai à tester les composants et les branchements ?  Il faudrait qu’il affiche les tests à réaliser et les résultats. Un test raté ne devrait pas stopper l’exécution…
- [ ] une page pour le controlleur et une page pour les tests, dans deux onglets accessible en haut de l'écran ? L'idée est que je vais câbler progressivement les composants entre eux et faire les tests au fur et à mesure. J'utilise des breadboard ce qui facilite le montage. Par ailleurs, lorsque j'exécute les tests, l'interface deviens totalement inactive et ne se raffaichi qu'à la fin de l'ensemble des tests. Tu pourrais suggérer un ordre pour le cablage et organiser les tests en conséquence.

## Terminé
- [x] Remplacer les credentials WiFi codés en dur par WiFiManager
- [x] Implémentation complète du diviseur (moteur NEMA 14, driver TMC2209, interface web)
