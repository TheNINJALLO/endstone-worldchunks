"""Build an isolated Endstone probe wheel, then install it only in a disposable lab."""

from pathlib import Path
from types import SimpleNamespace
import json


def probe(plugin):
    from wc_protection_probe.worldchunks_compat import WorldChunksGuard

    plugin._tick_counter = 0
    guard = WorldChunksGuard(plugin)
    job = SimpleNamespace(dimension_id="Overworld")
    try:
        guard.protect(job, 40, -20)
        assert id(job) in guard.leases, "provider did not acquire lease"
        first = guard.leases[id(job)]
        plugin._tick_counter = 100
        guard.protect(job, 40, -20)
        assert guard.leases[id(job)] == first, "renewal changed ID"
        guard.release(job)
        assert not guard.leases, "release did not clear lease"
        state = guard._call("wco status")
        assert not any(item["id"] == first for item in state["protections"])
        result = {
            "passed": 4,
            "test": "Python bridge acquisition, renewal, release, native confirmation",
        }
        plugin.logger.info("WC_BRIDGE_PASS " + json.dumps(result))
    except Exception as exc:
        plugin.logger.error("WC_BRIDGE_FAIL " + repr(exc))
        raise
    finally:
        guard.release(job)


def build(destination):
    import zipfile

    root = Path(__file__).resolve().parents[1]
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(
            "wc_protection_probe/__init__.py",
            """from endstone.plugin import Plugin
from .probe import probe
class ProtectionProbe(Plugin):
    api_version = '0.11'
    depend = ['worldchunks_optimizer']
    def on_enable(self):
        self.server.scheduler.run_task(self, lambda: probe(self), delay=40)
""",
        )
        archive.write(__file__, "wc_protection_probe/probe.py")
        archive.write(
            root / "integrations/ninjos_schematics/worldchunks_compat.py",
            "wc_protection_probe/worldchunks_compat.py",
        )
        info = "endstone_wc_protection_probe-0.0.1.dist-info/"
        archive.writestr(
            info + "METADATA",
            "Metadata-Version: 2.1\nName: endstone-wc-protection-probe\nVersion: 0.0.1\n",
        )
        archive.writestr(
            info + "WHEEL",
            "Wheel-Version: 1.0\nRoot-Is-Purelib: true\nTag: py3-none-any\n",
        )
        archive.writestr(
            info + "entry_points.txt",
            "[endstone]\nwc_protection_probe = wc_protection_probe:ProtectionProbe\n",
        )
        archive.writestr(info + "RECORD", "")


if __name__ == "__main__":
    import sys

    build(Path(sys.argv[1]))
