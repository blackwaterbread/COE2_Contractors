# COE2: Contractors

[![License](https://img.shields.io/badge/License-APL--SA-orange.svg?style=flat-square)](https://github.com/blackwaterbread/COE2_Contractors/blob/main/LICENSE)

Variant of Kexanone's [COE2](https://github.com/Kexanone/COE2_AR) for Arma Reforger that adds an economy system.

You play private military contractors: completed operations pay, every piece of gear is bought with your own money,
and a personal stash keeps it between sessions. Unofficial; not affiliated with or endorsed by the author of COE2.

## Features

- **Pay:** money for completed tasks, kills and treating teammates (bandages and CPR); deductions for deaths and
  friendly or civilian kills. Paid after the exfil, with a result screen showing the breakdown.
- **Exfil:** once the tasks are done (or the commander orders an early exfil), reach the exfil point within 10 minutes
  to get paid and return to the safehouse; vehicles come along with their crew. Miss it and you are missing in
  action: no pay, and everyone still outside the safehouse dies. The commander can cancel for part of the pay before
  the exfil, nothing during it. An operation timer shows the countdown.
- **Enemy pursuit:** enemy waves may chase you to the exfil point, more likely the more civilians you killed.
- **Shops:** the safehouse's two arsenal boxes sell weapons and equipment
  and buy them back at 50%. A quartermaster sells extra stash pages and loadout slots.
- **Stash:** a personal stash in the safehouse wardrobe, kept across deaths and server restarts, with saved loadouts
  that take what is missing from the stash or rebuy it (2 slots, up to 10 with slots from the quartermaster).
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

See [AGENTS.md](AGENTS.md).

## License

[Arma Public License Share Alike (APL-SA)](LICENSE)
