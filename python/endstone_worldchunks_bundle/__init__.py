"""Load the WorldChunks native plugins shipped in this wheel."""

from pathlib import Path
import sys

from endstone.plugin import Plugin


class WorldChunksBundle(Plugin):
    api_version = "0.11"
    load = "STARTUP"
    description = "WorldChunks native plugin bundle"
    website = "https://github.com/TheNINJALLO/endstone-worldchunks"

    def on_load(self):
        extension = ".dll" if sys.platform == "win32" else ".so"
        manager = self.server.plugin_manager
        for name in ("worldchunks", "worldchunks_optimizer"):
            if manager.get_plugin(name) is not None:
                raise RuntimeError(
                    "Install either the WorldChunks wheel or the native files. "
                    f"A separate {name} plugin is already installed."
                )
        for name in ("worldchunks", "worldchunks_optimizer"):
            path = Path(__file__).parent / "native" / f"endstone_{name}{extension}"
            if manager.load_plugin(str(path.resolve())) is None:
                raise RuntimeError(f"Could not load {name}; check the startup log")
