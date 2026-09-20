# Third-party notices

The plugin uses Endstone's public C++ SDK, pinned to v0.11.12, under Apache-2.0. No proprietary BDS executable, server data, or reconstructed complete BDS headers are included in the release archive.

Dependencies included in the native binaries (OpenSSL, zlib and libc++ apply to Linux):

| Component | Version | License |
| --- | --- | --- |
| Endstone SDK | 0.11.12 | Apache-2.0 |
| expected-lite | 0.9.0 | Boost Software License 1.0 |
| funchook | 1.1.3 | GPL-2.0 with linking exception |
| diStorm (through funchook) | 3.5.2 | BSD-3-Clause |
| OpenSSL libcrypto | 3.6.4 | Apache-2.0 |
| nlohmann/json | 3.12.0 | MIT |
| zlib (through OpenSSL) | 1.3.2 | zlib |
| LLVM libc++ / libc++abi | 20 | Apache-2.0 with LLVM exceptions |

License texts from the installed packages are included in `licenses/`. The funchook Conan recipe and patch are derived from the pinned Endstone source and covered by its Apache-2.0 license. Runtime glibc, libm and libgcc_s remain dynamically linked system dependencies. Windows uses the system cryptographic provider and the dynamically linked MSVC runtime. The optional offline inspection scripts use separately installed Capstone and pyelftools.
