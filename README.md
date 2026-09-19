# WorldChunks

Chunk control and automatic cleanup for Endstone.

WorldChunks can unload chunks inside a player's normal ticking area, stop them from loading again, keep selected chunks loaded, and reduce player simulation distance below Bedrock's four-chunk minimum. The companion optimizer handles cleanup around online players on a timer or when TPS drops.

Built for **Endstone 0.11.11 and Bedrock Dedicated Server 1.26.51.1**. Native hooks are tied to a specific server build; see the [Linux](compatibility/linux-1.26.51.1.json) and [Windows](compatibility/windows-1.26.51.1.json) compatibility manifests.

## Install

Download the plugin files from [Releases](https://github.com/TheNINJALLO/endstone-worldchunks/releases).

Use Python 3.14 with `endstone==0.11.11`. The Linux binaries require glibc 2.34 or newer. Choose one installation format:

| Platform | Native files | Wheel containing both plugins |
| --- | --- | --- |
| Linux x86-64 | `endstone_worldchunks.so` and `endstone_worldchunks_optimizer.so` | `endstone_worldchunks_bundle-0.3.0-py3-none-linux_x86_64.whl` |
| Windows x86-64 | `endstone_worldchunks.dll` and `endstone_worldchunks_optimizer.dll` | `endstone_worldchunks_bundle-0.3.0-py3-none-win_amd64.whl` |

Copy either the two native files or the matching wheel into `plugins/`.
Remove older WorldChunks files before switching formats or updating the wheel.
Keep the `worldchunks` and `worldchunks_optimizer` configuration folders.

Restart the server and run `/wc status` and `/wco status`. The optimizer needs WorldChunks; WorldChunks also works on its own.

The native plugin must load during startup. Restart the server when updating either binary. `/wco reload` reloads the optimizer's settings only.

## Automatic cleanup

The optimizer starts enabled with these defaults:

| Setting | Default |
| --- | --- |
| Loaded chunk radius | 2 chunks |
| Simulation radius | 2 chunks |
| Cleanup interval | 30 seconds |
| Low TPS trigger | At or below 18 TPS for 5 seconds |
| TPS cleanup cooldown | 30 seconds |
| Cleanup batch | 32 chunks per tick |

A radius of 2 covers a 5-by-5 square: up to 25 chunks per player. Overlapping areas share chunks. Cleanup considers every online player in each dimension and preserves chunks pinned with `/wc load`.

```text
/wco set keep_radius 2
/wco set simulation_radius 1
/wco set cleanup_interval_seconds 30
/wco set tps_threshold 18
/wco run
```

Use `/wco off` to disable automatic cleanup. Settings are saved in `plugins/worldchunks_optimizer/config.json`.

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
- Automatic cleanup also affects explicit ticking areas outside the player radius. Manual pins are preserved.
- Unloading a player's occupied chunk is allowed and can interrupt movement or interactions. Keep a world backup when testing new settings.

## Development

See [building and packaging](docs/building.md), [native implementation](docs/native-design.md), and [test results](docs/validation.md).

Both native builds and their wheels have been tested with a connected Minecraft client, including radius 1 and 2 cleanup, movement, saved blocks, manual pins, and restart persistence. Linux also has a real TPS slowdown check. Tests and their limits are recorded in the validation document.

## License

[MIT](LICENSE). Bundled dependencies retain their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).
