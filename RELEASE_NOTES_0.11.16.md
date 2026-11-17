Targets **Endstone 0.11.16**, **BDS 1.26.65.1 / runtime 26.65**, protocol **2225**, and **Python 3.14** on Linux and Windows x86-64. Linux binaries require **glibc 2.38+**.

Install either the matching platform bundle wheel or the two native plugins in `plugins/`. The wheel contains both WorldChunks and its optimizer. Replace old binaries/wheels while stopped, preserve configuration folders, restart, and check `/wc status` and `/wco status`.

Both platforms passed native compilation and 11 headless runtime checks: hook installation, eviction, manual pin protection, vanilla restoration, saved-block eviction/reload, matching bundle startup, policy/data restart persistence and clean shutdown. The Python compatibility tests and Linux optimizer/protection policy tests passed. Platform ZIPs include the exact artifact hashes and qualification reports.

Qualification used `enable-script=true` with no connected players. Automatic aggressive cleanup remains opt-in through compatibility-mode configuration.
