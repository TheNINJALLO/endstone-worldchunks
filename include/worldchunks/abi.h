#pragma once
#include <cstddef>
#include <cstdint>

namespace worldchunks::abi {
#ifdef _WIN32
inline constexpr size_t dimension_source = 0x1d8, view_mutex = 0x138;
inline constexpr size_t x1 = 0x140, z1 = 0x148, x2 = 0x14c, z2 = 0x154;
inline constexpr size_t view_slots = 0x168, buffer1 = 0x170, buffer2 = 0x188;
inline constexpr size_t chunk_position = 0x78, chunk_state = 0xf0, chunk_last_tick = 0x128;
inline constexpr size_t get_slot = 3, load_slot = 8, set_slot = 36, flush_dimension = 5, flush_batch = 28,
                        flush_discard = 27;
inline constexpr uintptr_t dimension_handle = 0x1216b0, level_handle = 0x6e950;
inline constexpr uintptr_t level_offsets = 0x7db1b0, main_vtable = 0xa80d090, dispatch = 0x9ddb10;
#else
inline constexpr size_t dimension_source = 0x1a8, view_mutex = 0x110;
inline constexpr size_t x1 = 0x1a0, z1 = 0x1a8, x2 = 0x1ac, z2 = 0x1b4;
inline constexpr size_t view_slots = 0x1c8, buffer1 = 0x1d0, buffer2 = 0x1e8;
inline constexpr size_t chunk_position = 0x50, chunk_state = 0xb8, chunk_last_tick = 0xf0;
inline constexpr size_t get_slot = 4, load_slot = 9, set_slot = 37, flush_dimension = 6, flush_batch = 29,
                        flush_discard = 28;
inline constexpr uintptr_t dimension_handle = 0x1d8480, level_handle = 0x1d97d0;
inline constexpr uintptr_t level_offsets = 0xc8a3a90, main_vtable = 0xf2ef6c0, dispatch = 0xca2c930;
#endif
} // namespace worldchunks::abi
