# Sonde de continuation — 2026-11-22

Objet : séparer fin de tour, limite technique `steps`, quota, permission et superviseur, puis prouver un passage automatique entre deux lots **sans message utilisateur**. Une seule session principale modifie le dépôt (`ses_e013333e0ffeGFnmaQSDr1eAPP`).

## 1. Limite `steps` effective

Documentation OpenCode V2, page « Agents », section **Steps** (fetch réel de `https://opencode.ai/docs/agents/`) :

> `steps` sets a positive maximum number of model steps. On the final step, OpenCode removes tools and asks the model to summarize in text. New user input resets the allowance.

Agent de la session principale, relevé par l'API réelle :

```
$ opencode api get /api/session/$env:OPENCODE_SESSION_ID
{"data":{"id":"ses_e013333e0ffeGFnmaQSDr1eAPP", ... "agent":"build",
"model":{"id":"big-pickle","providerID":"opencode","variant":"default"}, ...}}
```

`opencode.json` ne définit ni `default_agent`, ni `steps` pour `build`. Les seules valeurs `steps` du dépôt sont dans des **sous-agents** : `architecte` 12, `qa` 12, `gameplay` 14, `graphismes` 14, `niveaux` 14, `narration-ui` 14 (fichiers `.opencode/agents/*.md`). Vérification API complémentaire : `opencode api get /api/agent` répond `{"location":{"directory":"C:\\Users\\Liori"},"data":[]}` (aucun agent enregistré globalement ; les agents du projet sont des fichiers Markdown), et `opencode api get /api/config` répond uniquement `[{"type":"directory","path":"C:\\Users\\Liori\\.config\\opencode"}]`.

Conclusion : la limite `steps` **n'est pas configurée pour l'agent principal** (`build`). La limite observée « Limite maximale d'étapes de cet agent est atteinte. Aucun autre appel d'outil n'est possible. » venait du sous-agent `graphismes` (14 steps), pas de la session principale. Ce rapport ne déclare pas pour autant une continuation infinie : il documente la limite effective observée.

## 2. Superviseur

```
$ opencode service status
http://127.0.0.1:28894
$ opencode api get /api/info
{"version":"2.3.2","pid":9780,"hostname":"DESKTOP-0F9GTK5", ... "capabilities":{"persistentPty":true}}
```

Le service d'arrière-plan OpenCode existe et fonctionne (pid 9780). Ce n'est **pas** un superviseur de reprise : la documentation dit qu'à la dernière étape l'outillage est retiré et le modèle produit un résumé ; aucun mécanisme de relance automatique d'un nouveau lot n'est décrit ni configuré (`opencode.json` contient seulement `plugins`, `permissions`, `lsp`, `mcp`). Aucun script ou hook « supervisor » n'existe dans le dépôt (recherche `superviseur|supervisor` : 0 résultat ; `glob **/*hook*` dans `.opencode/` : aucun fichier).

État réel de la session principale relevé par API : `{"id":"ses_e013333e0ffeGFnmaQSDr1eAPP",...,"agent":"build","model":{"id":"big-pickle","providerID":"opencode","variant":"default"},"cost":0,...,"outcome":"succeeded",...}`. `outcome: succeeded` correspond à une **fin de tour normale**, pas à une erreur de quota ni à une limite d'étapes.

Conclusion : **aucun superviseur de continuation vérifié**. La continuation repose sur la session principale active et sur le protocole de reprise `notes.md` § « ÉTAT ACTUEL — REPRISE » + skill `ps1-night-handoff`.

## 3. Distinctions

| Cause | Preuve | Effet observé |
|---|---|---|
| Fin de tour | Réponse textuelle normale, session toujours active (`outcome` API) | Arrêt de l'émission, pas de perte de la session |
| Limite technique `steps` | `steps:` dans `.opencode/agents/*.md` ; message exact du sous-agent `graphismes` | Outils retirés, résumé texte forcé |
| Quota modèle | `docs/visuel/tranche1_avant.md` : « autorisation explicite après épuisement des quotas des modèles configurés » | Changement de modèle, pas une limite d'étapes |
| Permission | `opencode.json` : `edit`/`shell`/`subagent` = `allow`, `question` = `deny` | Aucun blocage d'écriture ; les questions utilisateur sont interdites par configuration |

## 4. Test de passage automatique entre deux lots

Lot A : sous-agent `explore` (`ses_dfc2de0fcffewk71cT7TTrMLcf`) lancé depuis la session principale pour produire un rapport READ-ONLY sur les interactions Paco/Prunewell/Alexia. Lot B : implémentation du lot v0.4.0 dans la session principale.

**Résultat observé (preuve).** Aucun message utilisateur n'a été envoyé entre les deux lots. Pendant que le Lot A tournait, la session principale a continué des travaux indépendants (configuration OpenCode, baseline `PS1_BUILD: PASS`, lecture de la chaîne de build). La notification est arrivée spontanément :

```
<subagent sessionID="ses_dfc2de0fcffewk71cT7TTrMLcf" state="completed" description="Lot A — repérer interactions Paco">
```

Le rapport contenait les points d'insertion réels (`src/ps1/boot.cpp`, `dialogue_ps1.cpp`, `pf.h`), les constantes PC (`14/10`, rayon `20`) et les identifiants (`prunewell`, `prunewell_apres`, `chef_paco`, `alexia_epave`). La session principale a repris l'implémentation dans le même tour, sans intervention utilisateur. **Ce passage automatique sous-agent → agent principal est donc vérifié.** Une seule session principale modifie le dépôt (`ses_e013333e0ffeGFnmaQSDr1eAPP`) ; le sous-agent était READ-ONLY.

**Limite exacte non franchie.** Ce test ne prouve PAS une relance automatique de la session principale après sa propre fin de tour ou après épuisement de ses `steps` : la documentation décrit à la dernière étape un retrait des outils et un résumé texte, et aucun hook/superviseur n'est configuré. En cas d'arrêt de la session principale, la reprise passe par `notes.md` § « ÉTAT ACTUEL — REPRISE » et le skill `ps1-night-handoff`. Aucune continuation autonome entre sessions n'est déclarée sans nouvelle preuve.
