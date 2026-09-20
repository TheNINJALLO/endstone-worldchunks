# Native adapter

This adapter is specific to the supplied Linux BDS 1.26.51.1 executable and the Endstone 0.11.12 CPython 3.14 Linux runtime. Both complete files must match the SHA256 values in `compatibility/linux-1.26.51.1.json` before any hook is prepared. ASLR is handled by resolving module load addresses. Another Python wheel can have different private function addresses even with the same Endstone version.

## Ownership and unload

`MainChunkSource` indexes live chunks through weak ownership. Player views, ticking-area views, generation views, temporary snapshots, and plugin pins retain strong ownership. Returning no chunk from a getter alone does not evict those objects.

WorldChunks tracks `ChunkViewSource` construction, copying, and destruction from startup. It also intercepts the destructor of the embedded `ChunkView`, because BDS inlines some enclosing destructors. Omitting that path leaves expired stack snapshots in the registry.

An unload rule blocks `MainChunkSource::getExistingChunk`, `createNewChunk`, and the generic `ChunkSource::getOrLoadChunk`. The server-thread sweep locks each view using BDS's own shared-mutex routines, removes matching strong references from both view buffers, and releases the references normally after unlocking. BDS owns the final discard and storage path. The plugin never deletes LevelDB records or forcibly frees a chunk object.

`inspect` calls the original main-source getter, bypassing the deny hook. Consequently `resident:false` means the native source no longer has a live object, not simply that a plugin lookup was filtered. Extra owners can delay disposal; check residency again on later ticks. State 11 is fully loaded; a successful load request can initially report state 0 while generation or disk loading is pending.

`allow` removes a rule and fills matching live view slots through native virtual `setLevelChunk`. This is necessary because stationary views otherwise retain holes after an unload. `load` owns a separate shared pointer; `release` drops only that plugin pin and does not override vanilla owners.

Shutdown releases plugin ownership, then flushes each Dimension's chunk garbage collector (virtual slot 6), the chunk source's thread batch (slot 29), and pending discarded writes (slot 28) before removing hooks. This ordering was established on Endstone 0.11.11: deferred chunk disposal during the later Level destructor can emit an unload event after Endstone's server singleton has been destroyed. A GDB trace reproduced that null-server access before the explicit flush was added.

New distant chunks require more than a single `getOrLoadChunk` call. While a pinned center is unfinished, the adapter requests and retains a thirteen-by-thirteen neighborhood, retries the native loading pipeline each tick, and dispatches pending chunk tasks using the same native routine as the ChunkView fetch callback (`0xca2c930`, boolean argument false). The center advances through terrain, decoration, replacement-data checks and lighting. Once state 11 is reached, the temporary neighborhood is released. Denied neighbors remain denied and can prevent generation from completing; `status.pending_generation` exposes unfinished pins. Releasing a pin also releases its temporary neighborhood.

## Simulation radius

In the 0.3.1 companion's default compatibility mode, the native provider
restores the captured vanilla offsets and ignores the configured optimizer and
manual radius caps while the companion is enabled. It clears previous automatic
rules and skips automatic denial, candidate eviction and view detachment. Even
direct native cleanup requests are suppressed. Manual denies remain explicit
operator overrides. Disabling the companion restores the underlying manual cap.
The reduced-radius behavior below applies when compatibility mode is disabled
or a manual cap is used without the companion.

The native Level owns the ticking-offset vector used by player simulation. Some BDS callers inline the Player getter, so intercepting that getter does not reliably change simulation. The adapter captures the original vector and filters its actual contents to offsets with both absolute coordinates at most the requested radius. It preserves the native allocation and never grows the vector beyond its original size. A radius of zero restores the original contents; it does not mean zero ticking chunks.

The cap is global to player simulation. With the tested vanilla tick-distance of four, radius two leaves 25 offsets and radius one leaves nine. Client view bounds remain unchanged. Login, teleport and respawn temporarily restore the original offsets so the native spawn pipeline can complete. All vector changes occur on the server thread; plugin shutdown restores the original contents.

## Layouts checked against the exact binaries

| Object | Offset / slot | Meaning |
| --- | --- | --- |
| Dimension | `0x1a8` | Main chunk source |
| ChunkSource | `0x28` | Dimension pointer |
| ChunkSource vtable | slot 4 / 9 | Get existing / get or load |
| ChunkViewSource | `0x80` | Embedded ChunkView |
| ChunkViewSource | `0x110` | Native shared mutex |
| ChunkViewSource | `0x1a0,0x1a8,0x1ac,0x1b4` | Inclusive X/Z bounds |
| ChunkViewSource | `0x1d0,0x1e8` | Two vectors of shared chunk references |
| ChunkViewSource vtable | slot 37 | Set level chunk |
| LevelChunk | `0x50` | Chunk X/Z |
| LevelChunk | `0xb8` | Load state |
| LevelChunk | `0xf0` | Last tick |
| Player | `0x520` | Main chunk view |
| Player | `0xadc` | Native view radius, distinct from simulation radius |
| Level | `0x610` | Native ticking-offset container pointer |

Clang 20 and libc++ are essential: shared pointers and vectors cross the ABI. The plugin uses shared `libgcc_s` for exception unwinding; its libc++ and other dependencies are hidden from symbol interposition. Public Endstone dimension/player/level wrappers are converted using verified private getter functions in the exact runtime.

## Repository research

- [endstone](https://github.com/EndstoneMC/endstone/tree/v0.11.12): plugin API, startup lifecycle, scheduler, public loaded-chunk enumeration, native chunk-source declarations, and wrapper implementations.
- [bedrock-protocol](https://github.com/EndstoneMC/bedrock-protocol): versioned codecs and chunk packet schemas. Protocol 2193 matches this server generation.
- [protocol-docs](https://github.com/EndstoneMC/protocol-docs): documents the supplied 1.26.51.1 release. RequestChunkRadius (69), ChunkRadiusUpdated (70), and NetworkChunkPublisherUpdate (121) concern network visibility and publication; they do not release native chunk owners.
- [protocol-dumper](https://github.com/EndstoneMC/protocol-dumper): explains the cereal/EnTT reflection pipeline behind generated packet schemas. It is a schema discovery tool, not an ownership controller; it is not preloaded into the test server.
- [dwarf2cpp](https://github.com/EndstoneMC/dwarf2cpp): requires DWARF debug information. The supplied retail Linux ELF has no DWARF sections, so full automatic header recovery is unavailable. RTTI, vtables, unwind function boundaries, Endstone runtime symbols, disassembly, and live tests were used instead.
- [bedrock-server-data](https://github.com/EndstoneMC/bedrock-server-data): release metadata and platform archive hashes. Both supplied ZIP files were identified and hashed, and each platform has its own verified ABI adapter.
- [remote-dev](https://github.com/EndstoneMC/remote-dev): Clang 20 / libc++ toolchain reference.

Exact repository revisions are recorded in the compatibility manifest. The research tools in `scripts/` inspect local binaries; they do not turn guessed addresses into a supported adapter. Packet injection is deliberately absent because server residency is controlled through native ownership.

## Windows adapter

The Windows build checks its own complete executable and runtime hashes in
`compatibility/windows-1.26.51.1.json`. It uses the MSVC C++ ABI, including the
hidden return buffer for shared pointers and the callee-owned value argument
passed to `setLevelChunk`.

Windows inlines many `ChunkViewSource` constructors. The adapter tracks populated
views after the common embedded view update, checks the enclosing vtable before
registration, and tracks the non-inlined copy constructor. The grid destructor
removes views before their retained references are destroyed. The sweep takes
the native SRW lock exclusively.

Windows offsets are kept beside the Linux offsets in `include/worldchunks/abi.h`.
The main source is at Dimension + `0x1d8`; view bounds start at `0x140`, its lock
is at `0x138`, and its two ownership buffers are at `0x170` and `0x188`. Chunk
position, state, and last tick are at `0x78`, `0xf0`, and `0x128`. The Level ticking
vector is reached through the getter at `0x7db1b0` (Level + `0x650`).

The Endstone runtime PDB and read-only probes provided the initial layouts;
live tests check actual residency, ownership release, and persistence. The
Windows player diagnostic uses public player information and the shared Level
ticking vector; it does not expose the Linux-only native render-view fields.
