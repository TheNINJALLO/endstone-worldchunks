# WorldChunks

> Endstone 0.11.15 release 0.3.4: This release includes verified Linux and Windows native artifacts and matching bundle wheels. The recorded validation defines the tested scope.

Chunk control and automatic cleanup for Endstone.

WorldChunks can unload chunks inside a player's normal ticking area, stop them from loading again, keep selected chunks loaded, and reduce player simulation distance below Bedrock's four-chunk minimum. The companion optimizer can handle cleanup around online players on a timer or when TPS drops.

**New in 0.3.1:** builds default to Bedrock's normal chunk retention and simulation. Forced cleanup and reduced simulation are opt-in because they can interrupt add-on structure generation and ticking-area jobs.

Version 0.3.4 targets **Endstone 0.11.15 and Bedrock Dedicated Server 1.26.60.6**. Native hooks are tied to a specific server build; see the [Linux](compatibility/linux-1.26.60.6.json) and [Windows](compatibility/windows-1.26.60.6.json) compatibility manifests. Network protocol is **2225**. Linux and Windows native plugins and bundle wheels passed live cleanup, protection, persistence, restart, and clean-shutdown qualification with the default `enable-script=true`.

## Install

Download the verified Linux and Windows artifacts from [WorldChunks 0.3.4](https://github.com/TheNINJALLO/endstone-worldchunks/releases/tag/v0.3.4). The published [WorldChunks 0.3.2](https://github.com/TheNINJALLO/endstone-worldchunks/releases/tag/v0.3.2) remains the release for Endstone 0.11.13. Published 0.3.0 files target Endstone 0.11.11.

Use Python 3.14 with `endstone==0.11.15`. The Linux binaries require glibc 2.34 or newer. Choose one installation format:

| Platform | Native files | Wheel containing both plugins |
| --- | --- | --- |
| Linux x86-64 | `endstone_worldchunks.so` and `endstone_worldchunks_optimizer.so` | `endstone_worldchunks_bundle-0.3.4-py3-none-linux_x86_64.whl` |
| Windows x86-64 | Source builds only; live qualification pending | No Windows artifacts published in 0.3.4 |

Copy either the two native files or the matching wheel into `plugins/`.
Remove older WorldChunks files before switching formats or updating the wheel.
Keep the `worldchunks` and `worldchunks_optimizer` configuration folders.

Restart the server and run `/wc status` and `/wco status`. The optimizer needs WorldChunks; WorldChunks also works on its own.

The native plugin must load during startup. Restart the server when updating either binary. `/wco reload` reloads the optimizer's settings only.

## Automatic cleanup

The optimizer starts enabled in **compatibility mode**. In this mode it samples TPS, but does not force chunks out of native views, block their reloads, or reduce simulation distance. Existing configs without `compatibility_mode` migrate to `true`. Manual `/wc unload` rules still apply.

These saved settings control forced cleanup when compatibility mode is disabled:

| Setting | Default |
| --- | --- |
| Compatibility mode | On |
| Loaded chunk radius | 2 chunks |
| Simulation radius | 2 chunks |
| Cleanup interval | 30 seconds |
| Low TPS trigger | At or below 18 TPS for 5 seconds |
| TPS cleanup cooldown | 30 seconds |
| Cleanup batch | 32 chunks per tick |

A radius of 2 covers a 5-by-5 square: up to 25 chunks per player. Overlapping areas share chunks. Cleanup considers every online player in each dimension and preserves chunks pinned with `/wc load`.

```text
/wco set compatibility_mode false
/wco set keep_radius 2
/wco set simulation_radius 1
/wco set cleanup_interval_seconds 30
/wco set tps_threshold 18
/wco run
```

Keep `/wco set compatibility_mode true` for worlds with unreviewed structure add-ons. It clears automatic rules and restores vanilla simulation, giving up the optimizer's chunk reduction. `/wco off` disables the optimizer and restores any manual `/wc radius` cap; use `/wc radius 0` as well to return to vanilla simulation. Settings are saved in `plugins/worldchunks_optimizer/config.json`.

## Manual control

```text
/wc inspect Overworld 2 0
/wc unload Overworld 2 0
/wc allow Overworld 2 0
/wc load Overworld 100 100
/wc release Overworld 100 100
/wc radius 1
```

Commands use **chunk coordinates**, calculated as `floor(block coordinate / 16)`. For example, block X `-1` is in chunk X `-1`.

Manual unload rules stay in effect until removed with `/wc allow` or `/wc reset`. They are saved in `plugins/worldchunks/policy.json`.

The [command reference](docs/commands.md) lists every command and permission. The [optimizer guide](docs/optimizer.md) covers settings, timing and interaction with manual rules.

## Behavior to keep in mind

- Loaded chunks, ticking chunks and terrain visible to the client are different things. The client can still display cached terrain after a server unload.
- Joining, teleporting, respawning and crossing chunk boundaries temporarily restore the normal simulation area so Bedrock can finish loading terrain.
- With compatibility mode disabled, automatic cleanup also affects explicit ticking areas outside the player radius unless their owner acquires a temporary protection. Manual pins are preserved. See [add-on and schematic compatibility](docs/integrations.md).
- Unloading a player's occupied chunk is allowed and can interrupt movement or interactions. Keep a world backup when testing new settings.

## Development

See [building and packaging](docs/building.md), [native implementation](docs/native-design.md), and [test results](docs/validation.md).

The 0.3.4 Linux native build passed disposable-server checks for cleanup, saved blocks, protection and TPS-triggered cleanup on Endstone 0.11.15. Twenty optimizer policy checks, twenty protection policy checks and seven Python compatibility tests passed. Windows native compilation and runtime identity mapping passed, but its CPython 3.14 runtime fixture timed out during startup. Evidence is under `evidence/endstone-0.11.15/`. The earlier 0.3.2 Linux/Windows and connected-client results remain historical evidence; no connected-client session was used for this update.

## License

[MIT](LICENSE). Bundled dependencies retain their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).
