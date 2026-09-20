# Schematic and add-on compatibility

WorldChunks 0.3.0 automatic cleanup can evict chunks retained by explicit ticking
areas. Interval cleanup or a TPS-triggered pass during a save/paste can therefore
remove the very chunk the job is waiting for or editing. The locally reviewed
NinjOS Schematic Cloud 1.6.0 source creates one temporary ticking area per active
chunk on Endstone 0.11 runtimes without direct chunk tickets. Its retry and
readback checks detect some interruptions, but do not prevent the optimizer from
evicting those chunks again.

Version 0.3.1 builds now default to **compatibility mode**: Bedrock controls retention
and simulation, and WorldChunks does not force automatic chunk evictions. This
avoids imposing the player's small keep radius on unmodified structure add-ons,
ticking areas and generation work. It gives up automatic chunk reduction; it is
not automatic identification of each add-on's work. Existing configs missing
`compatibility_mode` migrate to `true`.

If aggressive cleanup is explicitly enabled, the protection API lets an integration exempt its current work region and
six surrounding chunks on every side. Other regions remain eligible for cleanup.
Region protection is **opt-in**: installing the updated WorldChunks alone does not detect all
existing ticking areas or identify which add-on owns them.

## Immediate workaround for an existing installation

Run `/wco off`, then `/wc radius 0`, and wait for chunks to load. This removes
both automatic cleanup and the manual simulation cap. Leave them off while
diagnosing a structure add-on. On an updated build use
`/wco set compatibility_mode true` and `/wco on` to keep the companion available
with native retention and simulation. This clears automatic unload rules
and restores affected visible chunks in batches. Explicit `/wc unload` rules
remain in effect; inspect a blocked chunk and use `/wc allow` if appropriate.
Do not clear all ticking areas: another add-on may be using them.

## NinjOS Schematic Cloud 1.6.0

The adapter and a reproducible source-copy patcher are in
[`integrations/ninjos_schematics`](../integrations/ninjos_schematics).
Prepare a separate source tree:

```text
python integrations/ninjos_schematics/prepare.py <schematic-cloud-source> <new-output-directory>
```

The patcher checks the reviewed chunk-lifecycle insertion points before writing,
refuses an existing destination, and leaves the supplied source unchanged. The
output includes `worldchunks-integration.patch` for review and the adapter module.
Build/install that copy using Schematic Cloud's existing build instructions.
Install both newly built WorldChunks native plugins and restart the server.
The native status must advertise `protection_api: 1`; the adapter rejects older
WorldChunks installations instead of silently running without protection.

The integration acquires protection **before** requesting the existing chunk
ticket, renews at most once every 100 server ticks when admitting a work batch,
and releases after the ticket is dropped. The shared lifecycle covers save,
paste, undo, redo, cancellation, failure, and plugin shutdown. Independent job
IDs prevent one operation from releasing another's protection. If WorldChunks
is not installed, Schematic Cloud keeps its original behavior. If WorldChunks
disappears during an active protected job, further work fails visibly.

Leases expire after 1,200 server ticks without renewal (60 seconds at 20 TPS,
longer during lag). This also recovers protection left by a failed client.
Protection is not a chunk-loading ticket or an assurance that a chunk is ready;
the schematic plugin still acquires, waits for, verifies, and releases its own
ticket. Manual unload rules are never removed by the integration.

## Other plugins and behavior packs

Use console commands, or the C ABI in `include/worldchunks/api.h` from the primary
server thread. Example for chunk X 100, Z -2 in the Overworld:

```text
wco protect addon:job42 Overworld 100 -2 100 -2 1200 --json
```

Confirm the response contains that ID under `native.protections` before requesting
the ticking area and beginning work. Dispatch returning true alone is insufficient:
it can mean the command was handled but reported an error. Renew the same ID
before expiry; after completing/cancelling work and releasing the area's ticket:

```text
wco unprotect addon:job42 --json
```

Native request equivalents:

```json
{"op":"protect","id":"addon:job42","dimension":"Overworld","x1":100,"z1":-2,"x2":100,"z2":-2,"ticks":1200}
{"op":"unprotect","id":"addon:job42"}
```

Check `ok` and the returned protection list. Existing automatic restrictions
within the region and margin are removed synchronously; restoration of holes in
existing views happens in the usual batches. Protection is rechecked when an
already queued cleanup candidate is processed. Configuration reloads and toggles
retain leases; restarts clear them. A protection does not change player simulation
distance or override a later deliberate manual unload command.

Behavior-pack command access depends on the pack's script API and permissions.
Review the actual pack before choosing its integration point. Useful inputs are
the behavior pack (especially `manifest.json`, scripts and functions), the deployed
schematic plugin version, and console errors around a failed operation. Server
panel credentials are not needed.

These controls do not retroactively recreate structures omitted from already
generated terrain. The actual add-on must be reviewed and tested in a fresh
disposable area/world before making recovery changes to the production world.

## TPS expectations

Protection addresses unloading during work. It does not reduce the block updates,
entity work, chunk generation, or storage work caused by a large paste. Preserve
the schematic plugin's per-tick work limits and profile the actual failing job
before changing them. Keeping necessary chunks resident can increase memory and
tick work while a job is active. This change makes no general TPS improvement claim.
