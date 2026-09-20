# Changelog

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
