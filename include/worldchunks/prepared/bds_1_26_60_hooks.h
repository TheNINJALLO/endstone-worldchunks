// Generated offline BDS 1.26.60.6 preparation; never a qualified runtime profile.
#pragma once
#include <cstdint>
#include <optional>
namespace worldchunks::prepared::bds_1_26_60 {
inline constexpr bool RuntimeQualified = false;
inline constexpr bool ReleaseReady = false;
inline constexpr const char* PackageVersion = "1.26.60.6";
namespace Linux {
struct BdsOffsets {
    static constexpr std::optional<std::uintptr_t> chunk_view_source_copy = std::uintptr_t{0xcfe4950ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_source_ctor = std::uintptr_t{0xcfe3560ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_source_deleting_dtor = std::uintptr_t{0xcfe96e0ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_source_dtor = std::uintptr_t{0xcfe96b0ULL};
    static constexpr std::optional<std::uintptr_t> dimension_flush_chunk_garbage_collector = std::uintptr_t{0xcf20f30ULL};
    static constexpr std::optional<std::uintptr_t> dispatch_pending_chunk_tasks = std::uintptr_t{0xcfdc9f0ULL};
    static constexpr std::optional<std::uintptr_t> embedded_chunk_view_dtor = std::uintptr_t{0x53231c0ULL};
    static constexpr std::optional<std::uintptr_t> get_or_load = std::uintptr_t{0xcfe0bc0ULL};
    static constexpr std::optional<std::uintptr_t> level_get_ticking_offsets = std::uintptr_t{0xce3a0e0ULL};
    static constexpr std::optional<std::uintptr_t> main_chunk_source_vtable_address_point = std::uintptr_t{0xfbcee98ULL};
    static constexpr std::optional<std::uintptr_t> main_create_new = std::uintptr_t{0xcfbbf10ULL};
    static constexpr std::optional<std::uintptr_t> main_get_existing = std::uintptr_t{0xcfbbd10ULL};
    static constexpr std::optional<std::uintptr_t> native_shared_mutex_lock = std::uintptr_t{0x41fe1d0ULL};
    static constexpr std::optional<std::uintptr_t> native_shared_mutex_unlock = std::uintptr_t{0x41fe2a0ULL};
    static constexpr std::optional<std::uintptr_t> player_get_ticking_offsets = std::uintptr_t{0xbcf8110ULL};
};
} // namespace Linux
namespace Windows {
struct BdsOffsets {
    static constexpr std::optional<std::uintptr_t> chunk_grid_destructor = std::uintptr_t{0xbff570ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_source_copy = std::uintptr_t{0xbf1820ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_source_vtable = std::uintptr_t{0xae05e20ULL};
    static constexpr std::optional<std::uintptr_t> chunk_view_update = std::uintptr_t{0x2e363f0ULL};
    static constexpr std::optional<std::uintptr_t> dispatch_pending_chunk_tasks = std::uintptr_t{0xbe7710ULL};
    static constexpr std::optional<std::uintptr_t> get_or_load = std::uintptr_t{0xbed420ULL};
    static constexpr std::optional<std::uintptr_t> level_get_ticking_offsets = std::uintptr_t{0x7cb7f0ULL};
    static constexpr std::optional<std::uintptr_t> main_chunk_source_vtable = std::uintptr_t{0xaeed9a0ULL};
    static constexpr std::optional<std::uintptr_t> main_create_new = std::uintptr_t{0x3149ce0ULL};
    static constexpr std::optional<std::uintptr_t> main_get_existing = std::uintptr_t{0x3149990ULL};
};
} // namespace Windows
} // namespace worldchunks::prepared::bds_1_26_60
