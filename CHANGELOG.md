# Changelog

## 0.3.4 - 2026-10-28

Update the exact native target to Minecraft 26.60 / BDS 1.26.60.6 and Endstone 0.11.15 (CPython 3.14). Refresh private addresses, instruction guards, and changed object layouts. Qualification evidence records the executed runtime checks.

## 0.3.2 - Endstone 0.11.13 / Minecraft 1.26.52

- Update the pinned SDK and runtime gates to Endstone 0.11.13 and BDS 1.26.52.3 / runtime 26.52.
- Refresh native hook addresses, binary identities, and instruction fingerprints.
- Keep network protocol 2193 and preserve earlier compatibility evidence.

## 0.3.1

- Rebuild for Endstone 0.11.12 on Linux and Windows with unchanged BDS 1.26.51.1.
- Refresh runtime hashes and accessor addresses; preserve exact binary verification.
- Compatibility mode enabled by default (including old-config migration), leaving
  chunk retention and simulation to Bedrock for unmodified structure add-ons.
  Aggressive eviction and simulation caps now require explicit opt-in.

- Temporary region protection for schematic and add-on chunk tickets, including
  generation neighbors, renewal, overlapping jobs, and expiry in server ticks.
- `wco protect` / `wco unprotect` commands and matching native JSON API operations.
- Optional NinjOS Schematic Cloud 1.6.0 compatibility patch for save, paste, undo,
  redo, cancellation, and failure cleanup.

## 0.3.0

- Windows x86-64 support for the exact Endstone 0.11.11 / BDS 1.26.51.1 build.
- Linux and Windows wheels containing both native plugins.
- Readable command replies, with `--json` for automation.
- Shared configuration paths for native and wheel installations.
- Installation, command, build, and optimizer documentation.

## 0.2.0

- Companion optimizer with player keep radius, simulation radius, timed cleanup,
  sustained low-TPS triggers, cooldown, and bounded cleanup batches.
- Movement and spawn grace, protected pins, and persistent operator settings.

## 0.1.0

- Linux native chunk loading, unloading, pinning, and simulation-radius control.
