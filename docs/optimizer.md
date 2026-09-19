# WorldChunks Optimizer

Install WorldChunks and WorldChunks Optimizer from the same release, or install
the platform wheel containing both. See the [installation instructions](../README.md#install).
Both plugins require the matching BDS 1.26.51.1 and Endstone 0.11.11 CPython 3.14 binaries.

Operators configure it with `/wco`. Permission: `worldchunks.optimizer.admin`, granted to operators by default.

```text
/wco status
/wco set keep_radius 2
/wco set simulation_radius 1
/wco set cleanup_interval_seconds 30
/wco set tps_threshold 18
/wco set low_tps_seconds 5
/wco set tps_cooldown_seconds 30
/wco set batch_size 32
/wco run
/wco off
/wco on
/wco reload
```

`keep_radius` is a **square of chunk coordinates** around each online player, including the center. Radius 1 protects up to 9 chunks; radius 2 protects up to 25; radius 3 protects up to 49. Overlapping player areas share chunks. Players only protect chunks in their current dimension. Set the simulation radius no larger than the keep radius; lower simulation first when reducing keep radius. There is no per-player override in this version.

The keep radius is a retention limit, not a promise to generate that number of chunks. BDS still loads the player's area. The optimizer evicts fully loaded chunks outside the union of player areas, including chunks retained by explicit ticking areas. With no players online, all unpinned loaded chunks are eligible. Manual `/wc load` pins and their generation neighborhoods are protected. Manual `/wc unload` rules still take precedence inside the protected radius.

Cleanup runs on the interval **or** when Endstone's current TPS is at or below the threshold for the sustained duration. TPS is sampled once per second on the server thread. A cooldown bounds repeated TPS cleanup during persistent lag. Both timers use monotonic wall time; execution still requires the server to tick. This can reduce chunk work but cannot guarantee a TPS recovery or recover a completely frozen server.

Default saved configuration at `plugins/worldchunks_optimizer/config.json`:

```json
{
  "schema": 1,
  "enabled": true,
  "keep_radius": 2,
  "simulation_radius": 2,
  "cleanup_interval_seconds": 30,
  "tps_threshold": 18.0,
  "low_tps_seconds": 5,
  "tps_cooldown_seconds": 30,
  "batch_size": 32
}
```

| Setting | Allowed values |
| --- | --- |
| `keep_radius` | 1–32 chunks |
| `simulation_radius` | 1–keep_radius; also capped by the original BDS ticking offsets |
| `cleanup_interval_seconds` | 0 disables the timer; otherwise 1–86400 |
| `tps_threshold` | 0 disables TPS cleanup; otherwise greater than 0 through 20 |
| `low_tps_seconds` | 1–300 |
| `tps_cooldown_seconds` | 1–3600 |
| `batch_size` | 1–512 candidate chunks processed each server tick |

`/wco set` validates and saves changes immediately. `/wco reload` validates the complete file before applying it. Invalid changes leave the active configuration intact. Radius/configuration changes clear the previous temporary rules and restart the scheduling timers. The optimizer is enabled by default; `/wco off` persists the disabled state.

Automatic unload restrictions stay in memory and are separate from `worldchunks/policy.json`. `/wco off` clears them, restores visible chunks in batches, and restores the underlying `/wc radius` setting. It preserves manual pins and denies. `/wc allow` removes a manual deny only; use a manual pin to protect a remote chunk from subsequent automatic cleanup. `/wc status` and `/wc inspect` expose optimizer restrictions separately.

Travel, joining, teleporting, respawning, and configuration changes allow a temporary grace period of 100 server ticks. Chunk-boundary movement extends it and restores an approaching player's generation neighborhood. During grace the simulation cap returns to vanilla and automatic eviction pauses. If an occupied chunk is still loading, this pause continues until it finishes. Continuous travel can therefore keep a larger area loaded. This is deliberate: BDS generation and spawning require neighboring chunks. Cleanup resumes after players settle and a trigger has requested it. Manual denies can block neighboring generation and must be released by an operator; the optimizer never overrides them. Existing native owners can delay disposal, and clients can retain cached terrain.

The candidate scan uses Endstone's loaded chunk list; eviction is then spread across ticks. The batch setting limits candidates, not scan time or all native disposal work. Each cleanup discards old rules for coordinates that no native view covers anymore. Temporary deny rules are also capped at 65,536 coordinates; if that ceiling is reached, further candidates are skipped until rules are released. `status` reports pending work, skipped candidates, grace, trigger counts, and the effective simulation radius. No unload restriction is saved across restarts; settings are.

The companion communicates through a versioned C ABI with JSON strings owned by the provider. It does not install a second set of BDS hooks. A missing/old provider or unsupported native binary stops the optimizer with a clear log error.
