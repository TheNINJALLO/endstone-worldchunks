#pragma once
// Linux/Windows C ABI. Resolve this symbol from the loaded WorldChunks plugin module.
// Call only on the primary server thread. JSON input/output use UTF-8.
// The provider owns the returned string; copy it before the next API call.
// Exceptions and C++ allocations never cross the plugin boundary.
using WorldChunksRequestV1 = const char* (*)(const char*) noexcept;
inline constexpr char WORLDCHUNKS_API_SYMBOL[] = "worldchunks_request_v1";
