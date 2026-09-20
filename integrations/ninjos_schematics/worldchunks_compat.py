"""Optional WorldChunks protection for NinjOS Schematic Cloud 1.6.0.

Call on the server thread before requesting a chunk and before every work batch.
WorldChunks protection supplements the job's existing chunk-loading ticket.
"""

import json
import uuid


class WorldChunksGuard:
    def __init__(self, plugin):
        self.plugin = plugin
        self.prefix = "schem:" + uuid.uuid4().hex[:16] + ":"
        self.leases = {}
        self.renewals = {}

    def _provider(self):
        manager = getattr(getattr(self.plugin, "server", None), "plugin_manager", None)
        return (
            manager.get_plugin("worldchunks_optimizer") if manager is not None else None
        )

    def _call(self, command):
        from endstone.command import CommandSenderWrapper

        messages, errors = [], []
        sender = CommandSenderWrapper(
            self.plugin.server.command_sender, messages.append, errors.append
        )
        self.plugin.server.dispatch_command(sender, command + " --json")
        if errors:
            raise RuntimeError("WorldChunks: " + "; ".join(map(str, errors)))
        for message in messages:
            marker = "WORLDCHUNKS_OPTIMIZER "
            if marker in message:
                result = json.loads(message.split(marker, 1)[1])
                native = result["native"]
                if native.get("protection_api") == 1:
                    return native
        raise RuntimeError(
            "WorldChunks did not confirm protection; install the core and optimizer "
            "build with protection_api 1 before using schematic jobs"
        )

    def protect(self, job, chunk_x, chunk_z):
        provider = self._provider()
        if provider is None or not provider.is_enabled:
            if id(job) in self.leases:
                raise RuntimeError(
                    "WorldChunks became unavailable during a protected job"
                )
            return
        tick = self.plugin._tick_counter
        previous = self.renewals.get(id(job))
        coordinates = (str(job.dimension_id), int(chunk_x), int(chunk_z))
        if (
            previous is not None
            and previous[0] == coordinates
            and 0 <= tick - previous[1] < 100
        ):
            return
        # Renew at most once per 100 ticks per chunk. Expiry stretches with lag.
        dimensions = {
            "overworld": "Overworld",
            "0": "Overworld",
            "nether": "Nether",
            "1": "Nether",
            "theend": "TheEnd",
            "the_end": "TheEnd",
            "end": "TheEnd",
            "2": "TheEnd",
        }
        name = str(job.dimension_id).lower().removeprefix("minecraft:")
        if name not in dimensions:
            raise RuntimeError(f"Unsupported WorldChunks dimension: {job.dimension_id}")
        lease_id = self.leases.get(id(job), self.prefix + format(id(job), "x"))
        result = self._call(
            f"wco protect {lease_id} {dimensions[name]} "
            f"{int(chunk_x)} {int(chunk_z)} {int(chunk_x)} {int(chunk_z)} 1200"
        )
        if not any(entry["id"] == lease_id for entry in result["protections"]):
            raise RuntimeError("WorldChunks did not retain the schematic protection")
        self.leases[id(job)] = lease_id
        self.renewals[id(job)] = (coordinates, tick)

    def release(self, job):
        lease_id = self.leases.pop(id(job), None)
        self.renewals.pop(id(job), None)
        if lease_id is None:
            return
        try:
            self._call("wco unprotect " + lease_id)
        except Exception as exc:
            # Cleanup must finish even if a provider is disabled or unloaded.
            self.plugin.logger.warning(
                f"WorldChunks protection will expire after 1200 server ticks: {exc}"
            )
