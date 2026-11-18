# Second Overclock, Roguelike DRG | 第二超频，肉鸽深岩

[中文](README.md) · [mod.io page](https://mod.io/g/drg/m/second-overclock-roguelike-drg) · [Known issues and unimplemented combinations](Docs/KNOWN_ISSUES.en.md)

This project is a gameplay-mechanic expansion mod for **Deep Rock Galactic**. It lets you add and fuse a **second Overclock** onto a weapon while retaining the benefits, penalties, and special mechanics of the original Overclocks. It currently supports all 4 classes and their 24 primary and secondary weapons, with **453 available fusion combinations**, including adaptations for underlying mechanic conflicts, dynamic equipment loading, and multiplayer synchronization.

> **For players:** This repository hosts source code, data tables, and build tools only. Downloading the source ZIP does not install the mod. **Visit the [mod.io page](https://mod.io/g/drg/m/second-overclock-roguelike-drg) to subscribe and read the full gameplay description.** Use it with [ModHub](https://mod.io/g/drg/m/mod-hub); both native in-game subscriptions and MINT / mintcat loaders are supported.

## Engineering documentation

This repository does not include original game assets, Unreal Engine, third-party templates, or test saves. Developers must prepare external inputs locally:

1. [Contributor workflow (CONTRIBUTOR-WORKFLOW.md)](Engineering/CONTRIBUTOR-WORKFLOW.md): directory responsibilities, tool installation, stable identities, and fork build requirements.
2. [Public input preparation and build toolchain (PUBLIC-TOOLCHAIN.md)](Engineering/PUBLIC-TOOLCHAIN.md): prepare inputs from local game originals, generate assets, Cook, verify, and package.
3. [Tool gaps and supported boundaries (TOOL-GAPS.md)](Engineering/TOOL-GAPS.md): supported edits and areas without a unified editing interface.

## Build environment

- **Requirements:** Windows, PowerShell 7, Node.js 22, .NET 8 SDK, Unreal Engine 4.27.2, and a compatible FSD template at the pinned version.
- **Build entry point:** For a new environment, follow the public preparation route in `PUBLIC-TOOLCHAIN.md`. `npm run build:mod` is for the legacy input directory layout, not a fresh-setup build command.
- **Version identities:** The published player version is **0.93.14**. The source tag `alpha170-contributor-reviewed16-nonrelease` identifies build-tool validation only.

## Development plans (TODO)

- Random starting original Overclocks, and single-Overclock drafts for weapons with none equipped.
- Consistent public file and log names, moving settings and backups into a dedicated mod directory while preserving existing data.
- Investigate connection reuse across maps and reduce visible initial handshake messages while maintaining multiplayer reliability.
- Automatically enable detailed diagnostics after a failure, until game exit, replacing the current manual switch.
- Add an “All” scope to Debug batch testing.
- Evaluate moving the Shard Diffractor's Automated Beam Controller + Overdrive Booster from its separate equipment-preparation order to the shared route, retaining necessary beam adaptations.

## Feedback and log collection

See [Known issues and unimplemented combinations](Docs/KNOWN_ISSUES.en.md) for confirmed bugs, architectural boundaries, and the 8 conflicting combinations not yet implemented.

- **GitHub Issues:** [Bug reports, feature suggestions, mechanic discussions, and build problems](https://github.com/meeseek42/Second-Overclock/issues)
- **mod.io comments:** [Gameplay feedback](https://mod.io/g/drg/m/second-overclock-roguelike-drg)
- **Email:** `jioujiouningwang@gmail.com` for logs, crash files, or saves.

For faults or crashes, collect files from the following locations. For multiplayer issues, logs from both the host and the affected client in the same session are recommended:

- **Mod diagnostics:** `<Game Directory>\FSD\Mods\DualOverclockReward`. Use “Copy Log Folder Path” under `ModHub → Debug`, then ZIP the folder.
- **Native game log and crash directory:** `<Game Directory>\FSD\Saved\Logs\FSD.log` and `<Game Directory>\FSD\Saved\Crashes`.

## License

Original source code and authored data in this repository are released under the [Mozilla Public License 2.0](LICENSE). The game, engine, and third-party tools remain subject to their own licenses. See [License scope and third-party notices (LICENSE-SCOPE.md)](LICENSE-SCOPE.md).
