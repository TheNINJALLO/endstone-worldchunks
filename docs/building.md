# Building WorldChunks

Build each platform against the pinned Endstone 0.11.12 SDK. The native adapter
also checks the BDS and Endstone runtime binaries at startup; a successful build
does not make another server version compatible.

## Linux x86-64

Install Clang 20 with libc++/libc++abi 20, CMake 3.29 or newer, Ninja, Python 3.14,
Conan 2, Git, Perl, and the usual C/C++ build tools. Endstone plugins use libc++.
Then run:

```sh
bash scripts/build.sh
```

The script exports the bundled funchook recipe, installs locked dependencies,
fetches the pinned SDK, and writes both `.so` files to `build/`. If a checkout
exists at `research/endstone`, it is used as the SDK source instead.

## Windows x86-64

Install Visual Studio 2022 Build Tools with the C++ workload and Windows SDK,
LLVM 20 (including clang-cl and lld-link), Python 3.14, CMake, Ninja, and Conan 2.
From an x64 developer PowerShell with those tools on PATH:

```powershell
./scripts/build.ps1
```

The DLLs are written to `build-windows/`. Use the release runtime (`/MD`); the
adapter shares C++ ownership objects with BDS and must match its ABI.

## Wheels and release archives

Build the native binaries first, then run:

```sh
python scripts/build_wheel.py linux_x86_64
python scripts/build_wheel.py win_amd64
python scripts/package.py
python scripts/check_release.py
```

Each wheel contains both native plugins and a small Endstone loader. The wheel
loads its private binaries during startup; settings stay under
`plugins/worldchunks` and `plugins/worldchunks_optimizer`. Install one format
per server. Remove the previous wheel before copying an updated one.

Release artifacts go in `dist/`. BDS archives, worlds, runtime binaries, raw logs,
local tooling, and downloaded research repositories are excluded.

## Tests

See [validation](validation.md) for the live checks and their limits. The pure
policy test can be compiled with any C++20 compiler:

```sh
clang++ -std=c++20 -Iinclude tests/optimizer_policy.cpp -o optimizer_policy
./optimizer_policy
```

`scripts/lab_server.py` runs a disposable flat server. On Linux it uses
`/opt/worldchunks-server`; on Windows it uses `lab/windows-server`. Place the
matching supplied server ZIP in the project root and launch the script using a
Python 3.14 environment with `endstone==0.11.12`. Add `--wheel` to test a wheel
installation. Linux test ports are 19142/19144; Windows ports are 19152/19154.
These are separate from the normal Bedrock ports.

The live scripts write commands to the lab console file. For Windows, set
`WORLDCHUNKS_LAB_PLATFORM=windows` before running them. Never point these tests at
a production world: they change blocks, ticking areas, and chunk policy.

The offline inspection tools require `capstone` and `pyelftools` for ELF, or
`capstone` and `pefile` for PE. For example:

```sh
python scripts/inspect_pe.py path/to/bedrock_server.exe 0x38dd630
python scripts/inspect_native.py path/to/bedrock_server
```

These tools inspect local binaries; they do not install hooks or establish
compatibility with untested server builds.
