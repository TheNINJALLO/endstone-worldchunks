# NinjOS Schematic Cloud 1.6.0 adapter

See the [integration guide](../../docs/integrations.md) for behavior, requirements
and validation limits. Both WorldChunks native plugins must provide
`protection_api: 1`.

`prepare.py` creates a separate patched source copy using `worldchunks_compat.py`:

```text
python integrations/ninjos_schematics/prepare.py <original-source> <new-copy>
```

`schematic-cloud-1.6.0.patch` is the same lifecycle change for review or manual
application. When applying the patch manually, also copy `worldchunks_compat.py`
into `src/endstone_ninjos_schematics/`. Build the resulting source using the
schematic project's build instructions. Do not apply it blindly to a different
deployed version; its chunk lifecycle needs review first.

The original source is not modified by the preparation tool. The adapter does
not access or change database credentials, schematic files, or world data.
