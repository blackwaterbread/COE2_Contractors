# AGENTS.md — COE2: Contractors

COE2: Contractors is an unofficial variant of Kexanone's COE2 (co-op dynamic operations) for Arma Reforger. It adds private property through the Marx framework: pay for completed operations, a shop instead of the free arsenal, and a personal stash. It is a bridge mod: it depends on COE2 and Marx and never edits their files.

## Platform
- Arma Reforger, Enfusion engine, Enforce Script (`.c`). Target: stable 1.8, experimental 1.9.
- Workbench (Arma Reforger Tools) is the compiler of record. Code is not "done" until it compiles in Workbench.
- Your Enfusion API knowledge may be outdated. Verify classes and methods against the current game scripts and the COE2 / Kex Scenario Core sources before using them. Never invent APIs.

## Dependencies
| Mod | GUID | Notes |
|---|---|---|
| COE2 (Kexanone, APL-SA) | `60926835F4A7B0CA` | Brings Kex Scenario Core `5ED61DC0AFE17E8E`, ACE Core Dev, ACE Captives Dev. Source: https://github.com/Kexanone/COE2_AR |
| Marx_Core | `6A885105A7EC72B9` | Services, models, storage, public API |
| Marx_UI | `6A8A0C05BCEEF932` | Dialog base, wallet HUD |
| Marx_Shop | `6A885683BA928BB5` | Shop logic, prefabs, shop UI |
| Marx_Stash | `6A8A0C37A19F762B` | Stash point, stash UI |

- Marx lives in the sibling repo `../Marx` (same author). Read its `AGENTS.md` and `docs/` before using it.
- Workshop downloads (COE2, KSC, ACE) are in `Documents/My Games/ArmaReforger/addons`, registered in the Workbench launcher.

## Repo layout
```
addons/COE2_Contractors/   gproj ID COE2_Contractors (GUID 6A8AE49001FAD827), everything inside gets packed
LICENSE, AGENTS.md         repo root, not packed
```

## Rules (non-negotiable)
- Never copy or edit COE2, Kex Scenario Core, ACE or Marx files. Extend them with `modded class`, event subscriptions, and inherited or overridden resources created in Workbench ("Inherit in" / "Override in").
- Use Marx only through its public API (`MRX_Marx`, its services and events). Marx is `v0`, so breaking changes are still possible.
- If Marx lacks something, change Marx generically in the Marx repo. Never put COE2-specific logic into Marx.
- Server-authoritative. Clients only send requests via RPC. RPCs carry IDs, never prices, amounts or permissions.
- Every balance change goes through `MRX_EconomyService` with an `MRX_TxContext` (source, reason, idempotency key). Make keys deterministic per event (e.g. task + player) so a reward is never paid twice. Money is `int`.
- Overriding any COE2 asset makes this mod a derivative of COE2. It stays APL-SA (`LICENSE`), credits Kexanone/COE2, and is never presented as official.
- Code that touches inventories, containers or replication must also be tested as a remote client (Workbench PeerTool), not only as the host. Inventory callbacks run on clients too.

## Known issues to check
- Marx Core and COE2 both override the vanilla `Configs/Systems/ChimeraSystemsConfig.conf` (Marx: `MRX_MarxSystem`; COE2: garbage rules + `COE_EnemySupportSystem`). One may replace the other. Verify in Play; if so, ship a merged override here.
- COE2 does not use persistence and its mission headers set no systems config. Marx then falls back to in-memory storage, and wallets and stashes are lost on restart. Keeping them needs mission headers here with a Marx persistence systems config.

## Conventions
- Script class prefix `CTR_`. Script path: `Scripts/Game/Contractors/...`.
- Follow BI Enforce Script conventions: `m_` member prefix + type letter (`m_iCount`, `m_sName`, `m_bActive`, `m_aItems`, `m_mLookup`), `s_` statics, `UPPER_CASE` constants, PascalCase methods.
- Mind `ref` ownership on `Managed` members; avoid strong ref cycles.
- Code, comments, identifiers: English.
- Commit titles use Conventional Commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`).

## Resources & GUIDs
- Never invent, copy, or hand-edit resource GUIDs or `.meta` files. Let Workbench generate them.
- Prefabs (`.et`), layouts (`.layout`), configs (`.conf`): prefer authoring in Workbench. Text edits must keep `{GUID}path` references intact.
- Commit `.meta` files. Do not commit `resourceDatabase.rdb`.

## Don'ts
- Don't add features outside the current task scope.
- Don't trust client data. Don't put secrets/tokens in addon files.
- Don't assume an API exists because it existed in Arma 3 / DayZ / older Reforger.
