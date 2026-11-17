# WorldChunks 0.3.5

Chunk control and automatic cleanup for **Endstone 0.11.16**, **BDS 1.26.65.1 / runtime 26.65**, protocol **2225**, and **CPython 3.14** on Linux and Windows x86-64. Native hashes and hook addresses are pinned to this exact pair.

Both platforms passed native builds and disposable-server qualification: chunk eviction, manual pin protection, vanilla restoration, saved-block recovery, matching bundle startup, restart persistence and clean shutdown. The checks used `enable-script=true` and no connected players. See the [successful workflow](https://github.com/TheNINJALLO/endstone-worldchunks/actions/runs/43986186975) and [qualification profile](compatibility/bds-1.26.65.json).

## Install

Download the matching platform artifact from the [stable v0.3.5 release](https://github.com/TheNINJALLO/endstone-worldchunks/releases/tag/v0.3.5).

The qualified Linux binaries require glibc 2.38 or newer.

Use Python 3.14 with `endstone==0.11.16`. Choose either the two native files or the matching wheel:

| Platform | Native files | Wheel containing both plugins |
| --- | --- | --- |
| Linux x86-64 | `endstone_worldchunks.so`, `endstone_worldchunks_optimizer.so` | `endstone_worldchunks_bundle-0.3.5-py3-none-linux_x86_64.whl` |
| Windows x86-64 | `endstone_worldchunks.dll`, `endstone_worldchunks_optimizer.dll` | `endstone_worldchunks_bundle-0.3.5-py3-none-win_amd64.whl` |

Copy your chosen format into `plugins/`, replacing older WorldChunks binaries or wheels while preserving the configuration folders. Restart and run `/wc status` and `/wco status`. The optimizer requires WorldChunks. `/wco reload` reloads configuration only.

Build from source with `scripts/build.sh` or `scripts/build.ps1`, then `scripts/build_wheel.py`. The Windows profile uses LLVM 20 and the v144 runtime toolset installed on the build runner. The [previous release documentation](docs/history/endstone-0.11.15.md) and older profiles are preserved.

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

Linux and Windows native builds, matching bundle wheels, and all 11 headless runtime checks passed on Endstone 0.11.16 / BDS 1.26.65.1 with `enable-script=true`. Each platform passed chunk eviction, manual pin protection, vanilla restoration, saved-block eviction/reload, wheel startup, policy and saved-block restart persistence, and clean shutdown. Connected-player behavior was not retested. Evidence is under `evidence/endstone-0.11.16/`; older evidence remains historical.

## License

[MIT](LICENSE). Bundled dependencies retain their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).
