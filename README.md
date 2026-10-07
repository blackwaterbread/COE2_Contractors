# COE2: Contractors

[![License](https://img.shields.io/badge/License-APL--SA-orange.svg?style=flat-square)](https://github.com/blackwaterbread/COE2_Contractors/blob/main/LICENSE)

Variant of Kexanone's [COE2](https://github.com/Kexanone/COE2_AR) for Arma Reforger that adds an economy system.

You play private military contractors: completed operations pay, every piece of gear is bought with your own money,
and a personal stash keeps it between sessions. Unofficial; not affiliated with or endorsed by the author of COE2.

## Features

- **Pay:** money for completed tasks, kills and treating teammates (bandages and CPR); deductions for deaths and
  friendly or civilian kills. Paid when the operation ends, with a result screen showing the breakdown.
- **Return to base:** loot time after an operation, then everyone still in the AO returns to the safehouse. Players can
  return earlier from the pause menu (Operation); drivers bring their vehicle and passengers along.
- **Shops:** the safehouse's two arsenal boxes sell weapons and equipment
  and buy them back at 50%. A quartermaster sells extra stash pages.
- **Stash:** a personal stash in the safehouse wardrobe, kept across deaths and server restarts, with saved loadouts
  that rebuy what is missing.
- **Respawn kit:** every role respawns with the same minimal issued kit, worth nothing in the shops.
- **Safe zone:** no firing or throwing inside the safehouse.
- **Factions:** RHS ION contractors against RHS AFRF by default; the commander can change them.

## Workshop

https://reforger.armaplatform.com/workshop/6A8AE49001FAD827

## Mods JSON

```json
{
    "modId": "6A8AE49001FAD827",
    "name": "COE2: Contractors"
}
```

Dependencies are downloaded automatically: COE2, Kex Scenario Core, Marx Core, Marx UI, Marx Shop, Marx Stash,
RHS - Status Quo, ACE Medical Core (Dev), ACE Medical Circulation (Dev).

## Scenario IDs

- COE2: Contractors - Arland: `"{078A52D9F1F7F204}Missions/CTR_COE2_Arland.conf"`
- COE2: Contractors - Everon: `"{B7E9512B6C118CBF}Missions/CTR_COE2_Eden.conf"`
- COE2: Contractors - Kolguyev: `"{14C9BE5B86B03F2C}Missions/CTR_COE2_Cain.conf"`

Use these scenarios rather than the COE2 ones: their headers set the systems config that saves wallets and stashes.

## Development

- Workbench (Arma Reforger Tools) with the Workshop mods above installed, and [Marx](https://github.com/blackwaterbread/Marx)
  cloned next to this repository (or pass its location with `tools/launch-workbench.ps1 -MarxDir`).
- `tools/launch-workbench.ps1` starts Workbench with this project, the Marx addons and the Workshop addons, without the
  launcher. `-Tests` runs the Workbench-only test harness on every Play; `-AutoCloseTests` also leaves Play after the
  run. Play a COE2 world (`worlds/COE/<Map>/COE2_<Map>.ent`) with the World Systems Config
  `Configs/Contractors/Systems/CTR_Systems.conf`.
- Generated configs (shop catalogs, mission headers, persistence) come from the WorldEditor plugins in
  Plugins > Contractors.
- See [AGENTS.md](AGENTS.md) for the rules and conventions of this repository.

## License

[Arma Public License Share Alike (APL-SA)](LICENSE)
