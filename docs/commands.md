# Commands

Run these in chat with `/`, or in the server console without it. Both permissions below are granted to operators by default.

## WorldChunks

Permission: `worldchunks.admin`.

| Command | Description |
| --- | --- |
| `/wc status` | Show loaded chunks, simulation radius and active controls. |
| `/wc players` | Show player positions and ticking information. |
| `/wc inspect <dimension> <x> <z>` | Check whether a chunk is loaded, blocked or still generating. |
| `/wc unload <dimension> <x> <z>` | Unload a chunk and block future loads. |
| `/wc allow <dimension> <x> <z>` | Remove a manual unload rule and restore the chunk to matching views. |
| `/wc load <dimension> <x> <z>` | Load and pin a chunk, generating it if needed. |
| `/wc release <dimension> <x> <z>` | Remove the plugin's pin. Other owners may keep it loaded. |
| `/wc radius <0..32>` | Set the player simulation cap. `0` restores the server's original offsets. |
| `/wc policy` | Show saved unload rules, pins and the manual radius setting. |
| `/wc reset` | Clear manual rules and pins, and reset the manual simulation cap. |
| `/wc views` | Show native chunk-view diagnostics. |
| `/wc help` | Show command syntax. |

Dimensions are `Overworld`, `Nether` and `TheEnd`; names are case-sensitive. X and Z are chunk coordinates, not block coordinates. Divide the block coordinate by 16 and round down.

```text
/wc unload Overworld 2 0
/wc allow Overworld 2 0
/wc load Nether -10 12
/wc release Nether -10 12
```

`unload` blocks a chunk until you explicitly allow it again. World data stays on disk. Loading and unloading can take several ticks, so use `inspect` to confirm the result.

When the optimizer is enabled, its simulation setting takes precedence over `/wc radius`. `/wc reset` changes manual policy only; use `/wco off` to release automatic restrictions as well.

## Optimizer

Permission: `worldchunks.optimizer.admin`.

| Command | Description |
| --- | --- |
| `/wco status` | Show settings, TPS and cleanup progress. |
| `/wco config` | Show the saved configuration. |
| `/wco on` | Enable automatic management and save the enabled state. |
| `/wco off` | Disable automatic management, release its restrictions and restore the manual radius. |
| `/wco run` | Request forced cleanup now; requires compatibility mode off. Loading grace still applies. |
| `/wco protect <id> <dimension> <x1> <z1> <x2> <z2> <ticks>` | Protect a chunk rectangle and its six-chunk generation margin from automatic cleanup. Repeat the ID to renew/update. |
| `/wco unprotect <id>` | Release this job's protection; overlapping jobs keep theirs. |
| `/wco set <setting> <value>` | Change and save a setting. |
| `/wco reload` | Read `config.json` again. Invalid settings leave the active configuration intact. |
| `/wco help` | Show command syntax and setting names. |

| Setting | Range | Default |
| --- | --- | --- |
| `compatibility_mode` | `true` uses Bedrock retention/simulation; `false` enables forced cleanup and the simulation cap | `true` |
| `keep_radius` | 1 to 32 chunks | 2 |
| `simulation_radius` | 1 to `keep_radius` | 2 |
| `cleanup_interval_seconds` | 0 to 86400; 0 disables the timer | 30 |
| `tps_threshold` | 0 to 20; 0 disables TPS cleanup | 18 |
| `low_tps_seconds` | 1 to 300 | 5 |
| `tps_cooldown_seconds` | 1 to 3600 | 30 |
| `batch_size` | 1 to 512 | 32 |

For structure add-ons, leave `/wco set compatibility_mode true`. It restores
vanilla simulation and disables forced cleanup. Manual unload rules still apply.
With the optimizer disabled, `/wc radius 0` also clears the manual simulation cap.
The radius and trigger examples below take effect only after explicitly setting
`/wco set compatibility_mode false`.

To use a one-chunk radius, lower simulation first:

```text
/wco set simulation_radius 1
/wco set keep_radius 1
```

To keep a larger area loaded while ticking only nearby chunks:

```text
/wco set keep_radius 3
/wco set simulation_radius 1
```

For cleanup triggered only by low TPS:

```text
/wco set cleanup_interval_seconds 0
/wco set tps_threshold 18
/wco set low_tps_seconds 5
```

The loaded radius is a square around each player: radius 1 covers 9 coordinates, radius 2 covers 25, and radius 3 covers 49. It is a retention boundary; it does not force Bedrock to generate every coordinate in the square.

Append `--json` to any data command for machine-readable output. For example,
`wc status --json` and `wco status --json`. JSON replies retain the
`WORLDCHUNKS` and `WORLDCHUNKS_OPTIMIZER` prefixes.

Protection is temporary and does not load chunks itself. Acquire it before creating
the job's ticking area, renew it during work, and release it after dropping the
job's chunk ticket. Durations are 1–72,000 **server ticks**, so lag extends them.
IDs contain 1–64 letters, digits, `_`, `:`, `-` or `.`; use a unique ID per job.
Bounds are ordered, inclusive chunk coordinates and limited to 4,096 chunks per
lease, excluding its margin; at most 256 leases can be active. Manual unload
rules inside the region or its margin reject acquisition. Protection survives
optimizer configuration changes but does not persist across server restarts.
