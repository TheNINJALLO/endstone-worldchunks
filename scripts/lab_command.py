from pathlib import Path
import sys

name = "windows-commands.txt" if sys.platform == "win32" else "commands.txt"
with (Path(__file__).resolve().parents[1] / "lab" / name).open("a") as handle:
    for command in sys.argv[1:]:
        handle.write(command + "\n")
