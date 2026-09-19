#!/usr/bin/env bash
set -euo pipefail
export PATH=/opt/endstone-python/bin:/usr/lib/llvm-20/bin:$PATH
export CC=clang-20 CXX=clang++-20
root=$(cd "$(dirname "$0")/.." && pwd)
if [[ -z ${CONAN_HOME:-} && -d /opt/endstone-conan ]]; then export CONAN_HOME=/opt/endstone-conan; fi
conan export "$root/recipes/funchook/all" --version 1.1.3
conan install "$root" --output-folder "$root/build/conan" --build=missing \
  --profile:host "$root/profiles/linux-clang20" --profile:build "$root/profiles/linux-clang20" \
  --lockfile "$root/conan.lock"
extra=()
if [[ -f "$root/research/endstone/CMakeLists.txt" ]]; then
  extra+=("-DFETCHCONTENT_SOURCE_DIR_ENDSTONE=$root/research/endstone")
fi
cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$root/build/conan/conan_toolchain.cmake" \
  "${extra[@]}"
cmake --build "$root/build" --parallel 4
