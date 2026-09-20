import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "worldchunks_compat",
    Path(__file__).resolve().parents[1]
    / "integrations/ninjos_schematics/worldchunks_compat.py",
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.commands = []
        self.leases = set()
        self.error = None
        self.reply = True
        self.provider = SimpleNamespace(is_enabled=True)
        self.plugin = SimpleNamespace(
            server=SimpleNamespace(
                plugin_manager=SimpleNamespace(get_plugin=lambda _: self.provider),
                command_sender=object(),
                dispatch_command=self.dispatch,
            ),
            logger=SimpleNamespace(warning=lambda _: None),
            _tick_counter=0,
        )
        self.guard = module.WorldChunksGuard(self.plugin)
        self.job = SimpleNamespace(dimension_id="Overworld")

        def wrapper(sender, on_message, on_error):
            return SimpleNamespace(message=on_message, error=on_error)

        self.patch = patch.dict(
            "sys.modules",
            {"endstone.command": SimpleNamespace(CommandSenderWrapper=wrapper)},
        )
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def dispatch(self, sender, command):
        self.commands.append(command)
        fields = command.split()
        if self.error:
            sender.error(self.error)
        elif self.reply:
            if fields[1] == "protect":
                self.leases.add(fields[2])
            else:
                self.leases.discard(fields[2])
            sender.message(
                "WORLDCHUNKS_OPTIMIZER "
                + json.dumps(
                    {
                        "native": {
                            "protection_api": 1,
                            "protections": [{"id": lease} for lease in self.leases],
                        }
                    }
                )
            )
        return True  # Dispatch success alone does not prove command success.

    def test_optional_without_provider(self):
        self.provider = None
        self.guard.protect(self.job, -2, 3)
        self.assertFalse(self.commands)

    def test_acquire_renew_move_release(self):
        self.guard.protect(self.job, -2, 3)
        self.assertIn("Overworld -2 3 -2 3 1200", self.commands[-1])
        self.plugin._tick_counter = 99
        self.guard.protect(self.job, -2, 3)
        self.assertEqual(len(self.commands), 1)
        self.plugin._tick_counter = 100
        self.guard.protect(self.job, -2, 3)
        self.assertEqual(len(self.commands), 2)
        self.guard.protect(self.job, 5, 6)
        self.assertEqual(len(self.commands), 3)
        self.guard.release(self.job)
        self.assertFalse(self.leases)
        self.assertFalse(self.guard.renewals)

    def test_overlapping_jobs_use_separate_ids(self):
        second = SimpleNamespace(dimension_id="Overworld")
        self.guard.protect(self.job, 0, 0)
        self.guard.protect(second, 0, 0)
        self.guard.release(self.job)
        self.assertEqual(len(self.leases), 1)
        self.guard.release(second)
        self.assertFalse(self.leases)

    def test_error_stops_job_even_when_dispatch_returns_true(self):
        self.error = "Manual unload rule"
        with self.assertRaisesRegex(RuntimeError, "Manual unload rule"):
            self.guard.protect(self.job, 0, 0)
        self.assertFalse(self.guard.leases)

    def test_old_or_silent_provider_cannot_claim_protection(self):
        self.reply = False
        with self.assertRaisesRegex(RuntimeError, "did not confirm"):
            self.guard.protect(self.job, 0, 0)

    def test_lost_provider_stops_work_but_release_can_complete(self):
        self.guard.protect(self.job, 0, 0)
        self.provider.is_enabled = False
        with self.assertRaisesRegex(RuntimeError, "unavailable"):
            self.guard.protect(self.job, 0, 0)
        self.error = "Provider unavailable"
        self.guard.release(self.job)
        self.assertFalse(self.guard.leases)

    def test_all_dimensions(self):
        for dimension, expected in [
            ("minecraft:the_end", "TheEnd"),
            ("Nether", "Nether"),
            ("Overworld", "Overworld"),
        ]:
            self.job.dimension_id = dimension
            self.guard.protect(self.job, 0, 0)
            self.assertIn(" " + expected + " ", self.commands[-1])


if __name__ == "__main__":
    unittest.main()
