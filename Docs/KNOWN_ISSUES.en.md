# Known issues, technical boundaries, and unimplemented combinations (v0.93.14)

[Back to README](../README.en.md) · [中文](KNOWN_ISSUES.zh-CN.md)

## Confirmed bugs

| Issue | Behavior and current workaround |
| --- | --- |
| `!dor clear` debug command fails | It may fail to remove an active fusion, and application failures have been recorded after a failed clear. Do not use this command; use the Space Rig's `Maintenance` page for uninstall cleanup. |
| Initial handshake protocol text is visible | Initial `DOR-HS3` compatibility messages may appear in the host's chat. Subsequent normal equipment queries use a hidden channel. A client joining an unmodded host remains silent. |
| LOK-1 lock display mismatch | Smart Trigger OS + Executioner has an actual maximum of 1 lock, while the UI may still show 3 segments. The underlying mechanism works; the display issue is deferred. |
| Projectiles missing for unmodded spectators | With a host using Cluster Charges + Salvo Module, a client without this mod loaded cannot see some charged Salvo projectiles. This is a visual issue for unmodded clients. |

## Current architectural and feature boundaries

- **Matching builds:** Host and client must use the same build for client fusion effects and the draft UI. With only the host modded, unmodded clients receive resupply during squad reward rounds.
- **Promotion requirement:** An unpromoted class cannot receive a fusion; its automatic reward becomes resupply. A promoted class not owning a particular original Overclock is not itself a reason for rejection.
- **Automatic drafts versus manual loadouts:** Automatic drafts build fusion candidates from weapons with a base Overclock equipped, with resupply when candidates are insufficient. The host can manually assign a combination even without a base Overclock equipped.
- **AI suspension and protection:** Late-join catch-up in ordinary missions protects only the recipient. Squad-draft AI suspension covers objects managed by this mod; native environmental and other mods' logic may continue.
- **Reconnects and channel lifetime:** Deep Dives retain native joining restrictions: same-stage reconnects and restoration across stages are supported, not unrestricted late joining. Communication channels are rebuilt between the Space Rig and missions, and between Deep Dive stages.
- **High-load combination filter:** The default-off list of 37 combinations is AI-assisted and based on mechanisms such as multiple projectiles and persistent effects. It is precautionary filtering, not a per-combination performance benchmark.

## Eight unimplemented combinations

These 8 are not included in the 453 available combinations. Ideas that preserve both original Overclocks' characteristics while controlling projectile and persistent-effect counts are welcome:

| Combination | Why it is unimplemented | Potential custom design (undecided) |
| --- | --- | --- |
| **Cryo Bolt + Fire Bolt** | Conflicting effects. | Repeated, delayed temperature shocks, although this differs substantially from the original effects. |
| **Disperser Compound + Goo Bomber Special** | Goo Bomber Special lacks impacts, while Disperser Compound is an impact effect. | Increase the density along the bombing trail, although that differs from the original dispersal effect. |
| **Goo Bomber Special + Sludge Blast** | No clear combined interaction has been designed. | No idea yet. |
| **Roll Control + Spinning Death** | Overlapping rotation effects make a direct combination less meaningful. | Hold the fire button after firing to steer a slow projectile; release it to leave the projectile stationary. |
| **Smart Trigger OS + Neuro-Lasso** | One requires locking, while the other removes it. | No idea yet. |
| **Minelayer System + Plasma Burster Missiles** | Leaving mines on repeated terrain penetrations could create severe performance and balance problems. | Limit the number of mines per projectile; the limit is undecided. |
| **Cluster Charges + Minelayer System** | Mines from every child projectile could create severe performance and balance problems. | Small mines from children and normal mines from the parent, or retain a direct combination; undecided. |
| **Cluster Charges + Plasma Burster Missiles** | A direct combination lacks a clear purpose. | Allow the parent missile to split more than once, although this differs substantially from Plasma Burster Missiles. |

## Older-save residue (fixed in 0.93.14)

Older versions could allow fusion combinations into the native Overclock reward pool: portraitless choices appeared during blank Matrix Core infusion, or fusion entries appeared in the Forge. **0.93.14** now fills qualifications on the Space Rig and completes saving and readback verification before publishing blueprints. Mission applications no longer temporarily grant qualifications or add unregistered blueprints to the native catalog.

If an older save still shows these symptoms:

1. Record the actual loaded mod version, entry name, and affected screen. Keep a copy of the current save first.
2. Update to the current release and use `ModHub → Maintenance → Clean and Disable` on the Space Rig to remove the mod's records. Do not repeatedly forge the affected entries as an experiment.
3. Send the before/after observations, logs, and a relevant save copy to the author.
