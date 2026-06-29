# Instrukce pro iterativní review plan.sqlite3

## Inicializace (při každém novém kontextu)

1. Přečti `CLAUDE.md` a `NEXT.md`.
2. Otevři `plan.sqlite3` — tabulka `task` se sloupci: `id, namespace, name, type, internal, outofscope, status`.

## Workflow — jedna iterace

### Krok 1 — Vyber další položku

- Vezmi první záznam kde `status = ''` nebo `status = 'todo'` (přeskoč `ignore` a `ported`).
- **Priorita:** namespace začínající `System` má přednost před ostatními.

### Krok 2 — Popiš položku

Vypiš:
```
namespace:  <hodnota>
name:       <hodnota>
type:       <hodnota>
status:     <aktuální hodnota>
```
Poté stručně popiš, co daný typ dělá (podívej se do `/rv/tmp/runtime/src/libraries/`), a navrhni svůj názor — zda má smysl portovat do sharp-runtime (např. reflexe, threading, diagnostics apod. mohou být out of scope).

### Krok 3 — Otázka uživateli

> **Mám portovat?**

- **Ano** → zkontroluj, zda příslušný soubor v sharp-runtime již existuje:
  - **Existuje** → **nelze rovnou označit jako `ported`** — soubor musí být zkontrolován dle celého checklistu v `CLAUDE.md` (API surface, doc-comments, SPDX, build, testy) jako by ještě neexistoval. Teprve po úspěšné kontrole nastav `status = 'ported'`.
  - **Neexistuje** → portuj dle checklistu v `CLAUDE.md`, po dokončení nastav `status = 'ported'`.
- **Ne** → nastav `status = 'ignore'`, pak se zeptej:

> **Out of scope?**

  - **Ano** → nastav `outofscope = 1`.
  - **Ne** → nastav `outofscope = 0`.

### Krok 4 — Ulož do DB a přejdi na další iteraci

```sql
UPDATE task SET status = '...', outofscope = ... WHERE id = ...;
```

## Povolené hodnoty `status`

| Hodnota | Význam |
|---------|--------|
| `''`    | Dosud nerozhodnuto |
| `todo`  | Bude portováno |
| `ported`| Hotovo, splňuje checklist |
| `ignore`| Přeskočit (mimo rozsah nebo irelevantní) |

> `in_progress` **neexistuje** — portování probíhá přímo, bez mezistavu.
