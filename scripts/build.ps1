$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# Run from an x64 Visual Studio developer shell with LLVM 20 on PATH.
conan export "$root/recipes/funchook/all" --version 1.1.3
if ($LASTEXITCODE) { exit $LASTEXITCODE }
conan install "$root" --output-folder "$root/build-windows/conan" --build=missing --profile:host "$root/profiles/windows-clang20" --profile:build "$root/profiles/windows-clang20" --lockfile "$root/conan.lock"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake -S "$root" -B "$root/build-windows" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$root/build-windows/conan/conan_toolchain.cmake"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build "$root/build-windows" --parallel 4
exit $LASTEXITCODE
