"""Create a patched copy of Schematic Cloud 1.6.0; leave the supplied source intact."""

import argparse
import difflib
from pathlib import Path
import shutil


def prepare(source: Path, destination: Path):
    source, destination = source.resolve(), destination.resolve()
    if destination == source or source in destination.parents:
        raise ValueError("Destination must be separate from the supplied source")
    plugin_path = Path("src/endstone_ninjos_schematics/plugin.py")
    original = (source / plugin_path).read_text(encoding="utf-8")
    modified = original
    replacements = [
        (
            "    def _ensure_job_chunk(self, job: Any, dimension: Any, chunk_x: int, chunk_z: int) -> bool:",
            "    def _protect_worldchunks(self, job: Any, chunk_x: int, chunk_z: int) -> None:\n"
            "        from .worldchunks_compat import WorldChunksGuard\n\n"
            "        if not hasattr(self, '_worldchunks_guard'):\n"
            "            self._worldchunks_guard = WorldChunksGuard(self)\n"
            "        self._worldchunks_guard.protect(job, chunk_x, chunk_z)\n\n"
            "    def _ensure_job_chunk(self, job: Any, dimension: Any, chunk_x: int, chunk_z: int) -> bool:",
        ),
        (
            "                self._release_job_chunk(job, dimension)\n            if self._auto_load_chunks:\n",
            "                self._release_job_chunk(job, dimension)\n"
            "            self._protect_worldchunks(job, *requested)\n"
            "            if self._auto_load_chunks:\n",
        ),
        (
            "        state = chunk_loaded_state(dimension, *requested)\n",
            "        self._protect_worldchunks(job, *requested)\n"
            "        state = chunk_loaded_state(dimension, *requested)\n",
        ),
        (
            "        job.ticket_chunk = None\n        job.ticket_owned = False\n",
            "        guard = getattr(self, '_worldchunks_guard', None)\n"
            "        if guard is not None:\n"
            "            guard.release(job)\n"
            "        job.ticket_chunk = None\n        job.ticket_owned = False\n",
        ),
    ]
    for old, new in replacements:
        if modified.count(old) != 1:
            raise ValueError(
                "Source differs from the reviewed 1.6.0 chunk lifecycle; no files changed"
            )
        modified = modified.replace(old, new)
    if destination.exists():
        raise FileExistsError("Destination already exists; choose a new directory")
    shutil.copytree(
        source,
        destination,
        ignore=shutil.ignore_patterns(
            ".git", "__pycache__", "*.egg-info", "dist", "build", ".pytest_cache"
        ),
    )
    (destination / plugin_path).write_text(modified, encoding="utf-8")
    shutil.copyfile(
        Path(__file__).with_name("worldchunks_compat.py"),
        destination / plugin_path.parent / "worldchunks_compat.py",
    )
    patch = "".join(
        difflib.unified_diff(
            original.splitlines(keepends=True),
            modified.splitlines(keepends=True),
            fromfile="a/" + plugin_path.as_posix(),
            tofile="b/" + plugin_path.as_posix(),
        )
    )
    (destination / "worldchunks-integration.patch").write_text(patch, encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
