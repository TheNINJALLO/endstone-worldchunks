# Validation

## Endstone 0.11.12 update (0.3.1)

BDS remains **1.26.51.1**. On Linux and Windows, the updated native binaries each
passed 28 disposable-server checks, including startup/restart, chunk cleanup and
restoration, saved-block preservation, protection overlap/expiry, TPS-triggered
cleanup, and clean shutdown. Each platform bundle wheel also passed seven load,
cleanup, and shutdown checks. The two C++ policy suites passed 20 checks each;
all seven Python integration-helper tests passed.

Evidence: `evidence/endstone-0.11.12/{linux,windows}/`. These checks used no
connected players. The prior 0.11.11 player-session and compatibility-mode
records below remain historical evidence; they were not rerun in full for this
Endstone-only update. The user's structure add-on was not available to test.

## 0.3.1 compatibility mode

The compatibility-mode build passed 16 live checks on each of Linux and Windows
with BDS 1.26.51.1 / Endstone 0.11.11 on 2026-09-20. Tests cover migration of the
old configuration to the new default, reproduction of aggressive eviction,
clearing previous rules/queued work, restoration of the full native simulation
offsets even with a manual cap saved, and continued ticking of a remote explicit
area without any protection lease. Interval and real-TPS threshold conditions
were exercised with compatibility mode enabled, and neither requested eviction.
A remote structure was saved, loaded into another chunk, and checked by its block
contents. Config reload, invalid boolean input, and the explicit manual unload
override were also checked.

`tests/compatibility_live.py` and `evidence/compatibility-mode/` record this work.
The 12 protection-lease checks were also rerun on each platform with compatibility
mode explicitly disabled, confirming that the opt-in schematic integration still
works with aggressive cleanup (56 live checks total on this build).
These are vanilla structure-command and chunk-control checks. They do not validate
the user's unavailable structure add-on, naturally generated custom structures,
or repair of structures missed in existing terrain. Compatibility mode leaves
retention and simulation to Bedrock and gives up forced chunk reduction.

## 0.3.1 integration protection

The protection change was built against the same pinned SDK on Linux and Windows.
On 2026-09-20 it passed:

- 20 C++ protection checks (dimensions, negative coordinates, generation margin,
  overlapping jobs, renewal, expiry, invalid updates and bounded storage), plus
  the 20 existing optimizer policy checks.
- 12 live native protection checks on **each** platform, including reproduction
  of the unprotected ticking-area eviction, restoration with saved blocks intact,
  overlapping protections, unrelated cleanup, expiration, and manual-deny rejection.
- Four Python command-bridge checks inside the real Endstone runtime on each
  platform: acquire, renew, release, and confirm native release.
- Seven isolated Python adapter tests, including missing/old/unavailable providers
  and command errors despite a successful dispatch return.
- All 61 existing Schematic Cloud 1.6.0 tests on a separately patched source copy.

Evidence for the new live checks is under `evidence/integration-protection/`.
The TPS-trigger check uses the existing real TPS reading with a threshold of 20
for deterministic triggering. It does **not** measure TPS improvement or reproduce
the production paste workload. The complete Schematic Cloud copy/paste flow with
a player and storage service, and the user's custom behavior pack, have not been
tested together. This is an opt-in integration, not automatic protection of all
ticking areas. Historical release results below refer to their original binaries.

Tests use the supplied Linux BDS 1.26.51.1 archive, Endstone 0.11.11's CPython 3.14 Linux wheel, and a disposable creative flat world inside Docker on Windows. The actual installed Minecraft for Windows 26.51 client was used for player tests. Binary fingerprints and research revisions are in the compatibility manifest.

The original WorldChunks 0.1.0 run passed **36 automated checks**: 21 player checks, six explicit ticking-area checks, and nine generation/persistence checks. Additional checks confirmed fingerprint rejection and clean shutdown both with a fully loaded pin and with a newly requested state-0 pin plus an online player. These original results are retained under `evidence/v0.1.0/` with their original binary hash.

## Release 0.3.0

Both platform wheels were installed into Endstone's `plugins` folder and loaded
the native core and optimizer during startup. Settings stayed in the documented
folders and survived restarts.

| Check | Linux | Windows |
| --- | ---: | ---: |
| Native player regression | 21 | 21 |
| Optimizer player checks | 22 | 22 |
| No-player cleanup | 4 | 4 |
| Saved settings after restart | 4 | 4 |
| Readable and JSON command output | 4 | 4 |
| Real low-TPS cleanup and CPU quota restoration | 2 | Not run |
| Modified executable rejection and shutdown | Historical Linux check | 3 |

The C++ policy test also passed 20 checks, for **135 passing checks** across this release. Linux cleanup triggered at 1.42 TPS with an 18 TPS threshold during the CPU-quota test; the original unlimited quota was restored and verified. Each platform's final cleanup retained
25 loaded chunks with one connected Minecraft player at keep radius 2. The
release manifest in `evidence/release-0.3.0/manifest.json` records final binary
hashes and test counts. Linux results are in that directory; Windows results are
in `evidence/windows/`.

Windows player tests cover actual nearby and occupied chunk eviction, radius 1
and 2 ticking, distant generation and pins, saved blocks, movement, negative
coordinates, optimizer settings, and cleanup triggers. The compatibility test
appended a trailer to the disposable Windows executable, checked that the adapter
refused native access, stopped the server cleanly, and restored the original
executable and SHA256. The supplied archive was not changed.


## Optimizer validation

WorldChunks 0.2.0 and its companion passed **73 checks** together using the same actual Minecraft client and Docker lab: 20 policy checks, 22 optimizer player checks, four no-player checks, four restart checks, two real-TPS checks, and 21 original native regression checks. Those results and binary hashes are archived under `evidence/v0.2.0/`. The final cleanup left exactly 25 loaded chunks with one player at keep radius 2. The CPU-quota test triggered cleanup at 1.00 TPS with a threshold of 18; the kernel CPU quota was verified unlimited afterward.

- `tests/optimizer_policy.cpp` checks square-radius boundaries, negative coordinates, interval deadlines, sustained low TPS, recovery, inclusive thresholds, cooldown and disabled triggers using a deterministic clock.
- `tests/optimizer_empty.py` verifies eviction inside an explicit ticking area with no online players, preservation of a manual pin, and restoration when the optimizer is disabled.
- `tests/optimizer_live.py` verifies actual eviction below vanilla radius four, keep radii 1 and 2, radius expansion, retained chunk ticking, preservation of manual pins/denies, movement and negative coordinates, saved blocks, disable/restore behavior, interval and TPS-only cleanup, invalid settings and config reload.
- `tests/optimizer_restart.py` verifies persisted settings and enabled state after a clean restart, with no saved automatic deny history.
- `tests/optimizer_tps_stress.py` runs from the host and briefly applies a CPU quota to **only** the disposable `worldchunks-server` container. It checks an actual cleanup triggered below 18 TPS and restores/verifies an unlimited kernel CPU quota. It requires the exact lab mount and refuses a container with an existing NanoCPUs limit. Do not run it against a production container.
- The original `tests/live_regression.py` is rerun with `/wco off` to check the updated native provider independently of automatic cleanup.

The live TPS boundary test sets a threshold of 20 for deterministic triggering against real Endstone readings. The separate CPU-quota test demonstrates the trigger during an actual slowdown. Neither test establishes a performance improvement for every workload.

Run `optimizer_empty.py` and `optimizer_restart.py` before connecting the client. Run `optimizer_live.py` with one player near block 8, -60, 8. All server-side scripts use the existing Docker Python command shown below. Compile the clock/geometry test with C++20 and `-Iinclude`, then run the resulting executable. Run the host stress test with `python tests/optimizer_tps_stress.py` only in the prepared lab.

## Assertions

- `tests/live_regression.py` checks actual player connection, spawning with a saved radius cap, native last-tick changes at radii 1 and 2, nearby and occupied chunk eviction, blocked reload attempts, restored saved blocks, distant pins, pin release, and invalid command handling.
- `tests/ticking_area_regression.py` checks eviction and saved-block restoration inside an explicit ticking area with no connected players.
- `tests/persistence_regression.py` prepares a saved deny rule, radius and distant pin, then verifies them after a server restart. It also checks fresh generation and release of temporary generation neighbors.
- The unsupported-binary test appended a harmless trailer to the disposable extracted ELF. The adapter rejected the changed SHA256 before installing hooks, commands reported it unavailable, and the server exited cleanly. The trailer was removed and the original full SHA256 verified before subsequent tests. The supplied ZIP remained unchanged.

The JSON files in `evidence/` contain the automated results. The release manifest identifies the packaged plugin hashes and current checks; older evidence is kept in versioned archive folders. Personal player identifiers and raw server logs are excluded from the package.

## Reproducing the lab tests

Use only the disposable lab world. With the prepared Docker container and console driver running, set `wco off` and `wc radius 2`, connect the Minecraft client near block 8, -60, 8, and run:

```powershell
docker exec worldchunks-server /opt/endstone-python/bin/python /workspace/endstone-worldchunks/tests/live_regression.py
```

For the explicit ticking-area test, disconnect all players, add the area using `tickingarea add circle 8 0 8 2 wc_final true`, wait for loading, then run `tests/ticking_area_regression.py` using the same Python command. The test removes its area.

For persistence, run `tests/persistence_regression.py prepare`, stop and restart the server with the same plugin, then run `tests/persistence_regression.py verify`. The verify phase clears its deny/pin rules and leaves radius 2 configured.

## Boundaries

These checks establish server-side chunk residency and simulation behavior in the tested world. They do not establish long-duration stability, behavior with many simultaneous players, every entity type, dimension transfers, every terrain generator, or compatibility with other plugins. The exact server and runtime fingerprints are mandatory on both platforms.

Client rendering may retain a cached chunk after server eviction. Reduced simulation radius is temporarily suspended for spawning, teleporting and respawning. Explicit ticking areas remain independent of the player simulation cap. A denied generation neighbor can prevent a requested chunk from finishing; this is visible as a pending generation count.
