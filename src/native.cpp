#include "worldchunks/native.h"
#include "worldchunks/abi.h"
#include "worldchunks/optimizer_policy.h"
#include <endstone/level/chunk.h>
#include <endstone/level/dimension.h>
#include <endstone/level/level.h>
#include <endstone/player.h>
#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#else
#include <dlfcn.h>
#include <link.h>
#include <openssl/evp.h>
#endif
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <funchook.h>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace worldchunks {
namespace {
using Json = nlohmann::json;
struct Pos {
    int x, z;
};
using Shared = std::shared_ptr<void>;
static_assert(sizeof(Shared) == 16 && sizeof(Pos) == 8 && sizeof(std::vector<Pos>) == 24);
#ifdef _WIN32
using Get = Shared* (*)(void*, Shared*, const Pos&);
using Load = Shared* (*)(void*, Shared*, const Pos&, int, bool);
using ViewUpdate = void (*)(void*, const void*);
#else
using Get = Shared (*)(void*, const Pos&);
using Load = Shared (*)(void*, const Pos&, int, bool);
#endif
Shared callGet(Get fn, void* self, const Pos& pos)
{
#ifdef _WIN32
    Shared result;
    fn(self, &result, pos);
    return result;
#else
    return fn(self, pos);
#endif
}
Shared callLoad(Load fn, void* self, const Pos& pos, int mode, bool read_only)
{
#ifdef _WIN32
    Shared result;
    fn(self, &result, pos, mode, read_only);
    return result;
#else
    return fn(self, pos, mode, read_only);
#endif
}
void setChunk(void* view, Shared chunk)
{
    auto method = (*reinterpret_cast<void***>(view))[abi::set_slot];
#ifdef _WIN32
    // MSVC passes non-trivial value parameters indirectly. The callee destroys it.
    alignas(Shared) unsigned char storage[sizeof(Shared)];
    auto* argument = new (storage) Shared(std::move(chunk));
    reinterpret_cast<void (*)(void*, Shared*)>(method)(view, argument);
#else
    reinterpret_cast<void (*)(void*, Shared)>(method)(view, std::move(chunk));
#endif
}
using Ctor = void (*)(void*, void*, int);
#ifdef _WIN32
using Copy = void* (*)(void*, const void*);
#else
using Copy = void (*)(void*, const void*);
#endif
using Dtor = void (*)(void*);
using Lock = void (*)(void*);
struct NativeLock {
    void* mutex;
    Lock unlock;
    NativeLock(void* value, Lock lock, Lock release) : mutex(value), unlock(release)
    {
        lock(mutex);
    }
    ~NativeLock()
    {
        unlock(mutex);
    }
    NativeLock(const NativeLock&) = delete;
};
using Key = std::tuple<void*, int, int>;
using TickOffsets = const std::vector<Pos>& (*)(void*);
template <class T> T& field(void* p, size_t offset)
{
    return *reinterpret_cast<T*>(static_cast<char*>(p) + offset);
}
template <class F> F slot(void* p, size_t i)
{
    return reinterpret_cast<F>((*reinterpret_cast<void***>(p))[i]);
}
std::string sha256(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read binary: " + path);
    }
#ifdef _WIN32
    HCRYPTPROV provider{};
    HCRYPTHASH hash{};
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Cannot open SHA256 provider");
    }
    auto cleanup = [&] {
        if (hash) {
            CryptDestroyHash(hash);
        }
        CryptReleaseContext(provider, 0);
    };
    std::array<unsigned char, 32> out{};
    try {
        if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
            throw std::runtime_error("SHA256 init failed");
        }
        std::array<char, 65536> buffer{};
        while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
            if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(buffer.data()), static_cast<DWORD>(input.gcount()),
                               0)) {
                throw std::runtime_error("SHA256 update failed");
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("Binary read failed");
        }
        DWORD count = static_cast<DWORD>(out.size());
        if (!CryptGetHashParam(hash, HP_HASHVAL, out.data(), &count, 0) || count != out.size()) {
            throw std::runtime_error("SHA256 failed");
        }
    }
    catch (...) {
        cleanup();
        throw;
    }
    cleanup();
#else
    auto ctx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("SHA256 init failed");
    }
    std::array<char, 65536> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
        if (EVP_DigestUpdate(ctx.get(), buffer.data(), input.gcount()) != 1) {
            throw std::runtime_error("SHA256 update failed");
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("Binary read failed");
    }
    std::array<unsigned char, 32> out{};
    unsigned count{};
    if (EVP_DigestFinal_ex(ctx.get(), out.data(), &count) != 1 || count != 32) {
        throw std::runtime_error("SHA256 failed");
    }
#endif
    std::ostringstream text;
    for (auto b : out) {
        text << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    }
    return text.str();
}
struct State {
    uintptr_t bds{}, runtime{};
    std::string runtime_path;
    funchook_t* hooks{};
    std::recursive_mutex views_mutex;
    std::set<void*> views;
    std::mutex policy_mutex;
    std::set<Key> denied;
    std::set<Key> optimizer_denied;
    bool optimizer_enabled{}, optimizer_suspended{}, optimizer_loading{};
    int keep_radius{2}, optimizer_radius{2}, batch_size{32};
    std::set<Key> player_centers;
    std::deque<Key> cleanup_queue, restore_queue;
    bool cleanup_requested{};
    uint64_t cleanup_runs{}, cleanup_marked{}, cleanup_skipped{};
    std::string cleanup_reason;
    std::map<Key, Shared> pins;
    std::map<Key, std::map<Key, Shared>> generation_neighbors;
    std::atomic<uint64_t> blocked{}, released{}, constructors{};
#ifdef _WIN32
    ViewUpdate view_update{};
#endif
    Ctor ctor{};
    Copy copy{};
    Dtor dtor{}, deleting_dtor{}, view_dtor{};
    Get main_get{};
    Load main_create{}, get_or_load{};
    TickOffsets tick_offsets{};
    int radius{};
    int effective_radius{}, grace_ticks{};
    bool spawn_pending{};
    std::set<void*> joined_players;
    std::vector<Pos>* native_offsets{};
    std::vector<Pos> vanilla_offsets;
    Lock lock{}, unlock{};
    bool installed{};
    bool blockedAt(void* source, const Pos& pos)
    {
        auto dim = field<void*>(source, 0x28);
        std::lock_guard guard(policy_mutex);
        const Key key{dim, pos.x, pos.z};
        return denied.contains(key) || (optimizer_enabled && !optimizer_suspended && optimizer_denied.contains(key));
    }
};
State* active{};
#ifndef _WIN32
void ctorHook(void* self, void* parent, int mode)
{
    active->ctor(self, parent, mode);
    std::lock_guard guard(active->views_mutex);
    active->views.insert(self);
    ++active->constructors;
}
#endif
#ifdef _WIN32
void* copyHook(void* self, const void* other)
{
#else
void copyHook(void* self, const void* other)
{
#endif
    active->copy(self, other);
    std::lock_guard guard(active->views_mutex);
    active->views.insert(self);
    ++active->constructors;
#ifdef _WIN32
    return self;
#endif
}
#ifndef _WIN32
void dtorHook(void* self)
{
    {
        std::lock_guard guard(active->views_mutex);
        active->views.erase(self);
    }
    active->dtor(self);
}
void deletingDtorHook(void* self)
{
    {
        std::lock_guard guard(active->views_mutex);
        active->views.erase(self);
    }
    active->deleting_dtor(self);
}
void viewDtorHook(void* self)
{
    // BDS inlines some ChunkViewSource destructors, including stack snapshots.
    // Every path still destroys the embedded ChunkView at +0x80.
    {
        std::lock_guard guard(active->views_mutex);
        active->views.erase(static_cast<char*>(self) - 0x80);
    }
    active->view_dtor(self);
}
#endif
#ifdef _WIN32
Shared* getHook(void* self, Shared* out, const Pos& pos)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return new (out) Shared{};
    }
    return active->main_get(self, out, pos);
}
Shared* createHook(void* self, Shared* out, const Pos& pos, int mode, bool read_only)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return new (out) Shared{};
    }
    return active->main_create(self, out, pos, mode, read_only);
}
Shared* loadHook(void* self, Shared* out, const Pos& pos, int mode, bool read_only)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return new (out) Shared{};
    }
    return active->get_or_load(self, out, pos, mode, read_only);
}
void viewUpdateHook(void* self, const void* bounds)
{
    active->view_update(self, bounds);
    // Windows inlines view construction. Register populated ChunkViews instead.
    auto* view = static_cast<char*>(self) - 0x78;
    uintptr_t vtable{};
    SIZE_T count{};
    if (ReadProcessMemory(GetCurrentProcess(), view, &vtable, sizeof(vtable), &count) && count == sizeof(vtable) &&
        vtable == active->bds + 0xa71a330) {
        std::lock_guard guard(active->views_mutex);
        if (active->views.insert(view).second) {
            ++active->constructors;
        }
    }
}
void gridDtorHook(void* self)
{
    {
        std::lock_guard guard(active->views_mutex);
        active->views.erase(static_cast<char*>(self) - 0x140);
    }
    active->view_dtor(self);
}
#else
Shared getHook(void* self, const Pos& pos)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return {};
    }
    return active->main_get(self, pos);
}
Shared createHook(void* self, const Pos& pos, int mode, bool read_only)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return {};
    }
    return active->main_create(self, pos, mode, read_only);
}
Shared loadHook(void* self, const Pos& pos, int mode, bool read_only)
{
    if (active->blockedAt(self, pos)) {
        ++active->blocked;
        return {};
    }
    return active->get_or_load(self, pos, mode, read_only);
}
int modules(dl_phdr_info* info, size_t, void* p)
{
    auto& state = *static_cast<State*>(p);
    std::string name = info->dlpi_name;
    if (name.empty()) {
        state.bds = info->dlpi_addr;
    }
    if (name.ends_with("libendstone_runtime.so")) {
        state.runtime = info->dlpi_addr;
        state.runtime_path = name;
    }
    return 0;
}
#endif

template <class F> void hook(State& s, F& original, uintptr_t address, F replacement)
{
    original = reinterpret_cast<F>(s.bds + address);
    if (funchook_prepare(s.hooks, reinterpret_cast<void**>(&original), reinterpret_cast<void*>(replacement)) != 0) {
        throw std::runtime_error(funchook_error_message(s.hooks));
    }
}
} // namespace

struct Native::Impl {
    endstone::Server& server;
    State state;
    explicit Impl(endstone::Server& s) : server(s) {}
    void requireMain()
    {
        if (!server.isPrimaryThread()) {
            throw std::runtime_error("Native operations require the server thread");
        }
        if (!state.installed) {
            throw std::runtime_error("Native adapter is unavailable");
        }
        if (!server.getLevel()) {
            throw std::runtime_error("World is unavailable");
        }
    }
    void* dimension(const std::string& name)
    {
        auto* level = server.getLevel();
        if (!level) {
            throw std::runtime_error("World is unavailable");
        }
        for (auto* dimension : level->getDimensions()) {
            if (dimension->getName() == name) {
                return reinterpret_cast<void* (*)(const void*)>(state.runtime + abi::dimension_handle)(dimension);
            }
        }
        throw std::runtime_error("Unknown dimension: " + name);
    }
    void* source(void* dim)
    {
        return field<void*>(dim, abi::dimension_source);
    }
    Shared existing(void* dim, Pos pos)
    {
        auto* src = source(dim);
        // Call the original MainChunkSource getter to observe real residency.
        if (field<uintptr_t>(src, 0) == state.bds + abi::main_vtable) {
            return callGet(state.main_get, src, pos);
        }
        return callGet(slot<Get>(src, abi::get_slot), src, pos);
    }
    void* player(endstone::Player* p)
    {
#ifdef _WIN32
        return p;
#else
        return reinterpret_cast<void* (*)(const void*)>(state.runtime + 0x152750)(p);
#endif
    }
    void captureOffsets()
    {
        if (state.native_offsets) {
            return;
        }
        auto* level = server.getLevel();
        if (!level) {
            throw std::runtime_error("World is unavailable");
        }
        auto* handle = reinterpret_cast<void* (*)(const void*)>(state.runtime + abi::level_handle)(level);
        auto& offsets =
            const_cast<std::vector<Pos>&>(reinterpret_cast<TickOffsets>(state.bds + abi::level_offsets)(handle));
        if (offsets.empty() || offsets.size() > 5000) {
            throw std::runtime_error("Ticking offset layout check failed");
        }
        for (auto pos : offsets) {
            if (std::abs(pos.x) > 32 || std::abs(pos.z) > 32) {
                throw std::runtime_error("Invalid native ticking offset");
            }
        }
        state.vanilla_offsets = offsets;
        state.native_offsets = &offsets;
    }
    void applyRadius()
    {
        if (!state.native_offsets) {
            return;
        }
        state.spawn_pending = false;
        for (auto* p : server.getOnlinePlayers()) {
            if (!state.joined_players.contains(player(p))) {
                state.spawn_pending = true;
            }
        }
        const int requested = state.optimizer_enabled ? state.optimizer_radius : state.radius;
        const int effective =
            (state.spawn_pending || state.grace_ticks > 0 || (state.optimizer_enabled && state.optimizer_loading))
                ? 0
                : requested;
        if (effective == state.effective_radius) {
            return;
        }
        std::vector<Pos> filtered;
        for (auto pos : state.vanilla_offsets) {
            if (effective == 0 || (std::abs(pos.x) <= effective && std::abs(pos.z) <= effective)) {
                filtered.push_back(pos);
            }
        }
        // The filtered range never exceeds the original capacity. Retain BDS storage.
        state.native_offsets->assign(filtered.begin(), filtered.end());
        state.effective_radius = effective;
    }
    void restoreViews(void* dim, Pos pos)
    {
        void* src = source(dim);
        auto chunk = callLoad(slot<Load>(src, abi::load_slot), src, pos, 1, false);
        if (!chunk) {
            return;
        }
        std::lock_guard guard(state.views_mutex);
        for (void* view : state.views) {
            if (field<void*>(view, 0x28) != dim) {
                continue;
            }
            if (pos.x < field<int>(view, abi::x1) || pos.z < field<int>(view, abi::z1) ||
                pos.x > field<int>(view, abi::x2) || pos.z > field<int>(view, abi::z2)) {
                continue;
            }
            setChunk(view, chunk);
        }
    }
    bool nearPlayer(const Key& key, int extra = 0)
    {
        const auto& [dim, x, z] = key;
        for (const auto& [d, cx, cz] : state.player_centers) {
            if (dim == d && insideRadius(x, z, cx, cz, state.keep_radius + extra)) {
                return true;
            }
        }
        return false;
    }
    bool pinned(const Key& key)
    {
        if (state.pins.contains(key)) {
            return true;
        }
        for (const auto& [center, neighbors] : state.generation_neighbors) {
            const auto& [d, cx, cz] = center;
            const auto& [dim, x, z] = key;
            if (d == dim && insideRadius(x, z, cx, cz, 6)) {
                return true;
            }
        }
        return false;
    }
    void clearOptimizerRules()
    {
        std::lock_guard guard(state.policy_mutex);
        for (auto key : state.optimizer_denied) {
            state.restore_queue.push_back(key);
        }
        state.optimizer_denied.clear();
        state.cleanup_queue.clear();
        state.cleanup_requested = false;
    }
    void restoreVisible(const Key& key)
    {
        const auto& [dim, x, z] = key;
        {
            std::lock_guard guard(state.policy_mutex);
            if (state.denied.contains(key) || state.optimizer_denied.contains(key)) {
                return;
            }
        }
        bool visible = false;
        {
            std::lock_guard guard(state.views_mutex);
            for (auto* view : state.views) {
                if (field<void*>(view, 0x28) == dim && x >= field<int>(view, abi::x1) &&
                    z >= field<int>(view, abi::z1) && x <= field<int>(view, abi::x2) &&
                    z <= field<int>(view, abi::z2)) {
                    visible = true;
                    break;
                }
            }
        }
        if (visible) {
            restoreViews(dim, {x, z});
        }
    }
    void updateOptimizer()
    {
        auto& s = state;
        if (s.optimizer_enabled) {
            std::set<Key> centers;
            for (auto* p : server.getOnlinePlayers()) {
                auto location = p->getLocation();
                centers.emplace(dimension(p->getDimension().getName()),
                                static_cast<int>(std::floor(location.getX() / 16.0)),
                                static_cast<int>(std::floor(location.getZ() / 16.0)));
            }
            const bool moved = centers != s.player_centers;
            s.player_centers = std::move(centers);
            if (moved) {
                s.grace_ticks = 100;
            }
            s.optimizer_loading = false;
            for (const auto& key : s.player_centers) {
                // Long generation can outlast the fixed travel grace. Keep the
                // vanilla generation neighborhood until the occupied chunk loads.
                bool manually_denied;
                {
                    std::lock_guard guard(s.policy_mutex);
                    manually_denied = s.denied.contains(key);
                }
                if (manually_denied) {
                    continue;
                }
                const auto& [dim, x, z] = key;
                auto chunk = existing(dim, {x, z});
                if (!chunk || field<uint8_t>(chunk.get(), abi::chunk_state) != 11) {
                    s.optimizer_loading = true;
                }
            }
            // Release the approaching player's neighborhood before the next BDS tick.
            // Generation needs a temporary six-chunk halo while the player travels.
            {
                std::lock_guard guard(s.policy_mutex);
                s.optimizer_suspended = s.spawn_pending || s.grace_ticks > 0 || s.optimizer_loading;
                for (auto it = s.optimizer_denied.begin(); moved && it != s.optimizer_denied.end();) {
                    if (nearPlayer(*it, moved ? 6 : 0) || pinned(*it)) {
                        s.restore_queue.push_back(*it);
                        it = s.optimizer_denied.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
            if (s.cleanup_requested && !s.optimizer_suspended && s.cleanup_queue.empty()) {
                s.cleanup_requested = false;
                ++s.cleanup_runs;
                // Once no view covers a coordinate, retaining an unload rule
                // there buys nothing. Prune history so exploration stays bounded.
                struct Bounds {
                    void* dim;
                    int x1, z1, x2, z2;
                };
                std::vector<Bounds> bounds;
                {
                    std::lock_guard guard(s.views_mutex);
                    for (auto* view : s.views) {
                        NativeLock lock(static_cast<char*>(view) + abi::view_mutex, s.lock, s.unlock);
                        bounds.push_back({field<void*>(view, 0x28), field<int>(view, abi::x1),
                                          field<int>(view, abi::z1), field<int>(view, abi::x2),
                                          field<int>(view, abi::z2)});
                    }
                }
                {
                    std::lock_guard guard(s.policy_mutex);
                    for (auto it = s.optimizer_denied.begin(); it != s.optimizer_denied.end();) {
                        const auto& [dim, x, z] = *it;
                        const bool covered = std::any_of(bounds.begin(), bounds.end(), [&](const Bounds& b) {
                            return dim == b.dim && x >= b.x1 && x <= b.x2 && z >= b.z1 && z <= b.z2;
                        });
                        if (!covered) {
                            it = s.optimizer_denied.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                }
                for (auto* d : server.getLevel()->getDimensions()) {
                    auto* dim = dimension(d->getName());
                    for (auto& chunk : d->getLoadedChunks()) {
                        Key key{dim, chunk->getX(), chunk->getZ()};
                        if (!nearPlayer(key) && !pinned(key)) {
                            s.cleanup_queue.push_back(key);
                        }
                    }
                }
            }
            if (!s.optimizer_suspended) {
                for (int n = 0; n < s.batch_size && !s.cleanup_queue.empty(); ++n) {
                    auto key = s.cleanup_queue.front();
                    s.cleanup_queue.pop_front();
                    if (nearPlayer(key) || pinned(key)) {
                        ++s.cleanup_skipped;
                        continue;
                    }
                    const auto& [dim, x, z] = key;
                    auto chunk = existing(dim, {x, z});
                    if (!chunk || field<uint8_t>(chunk.get(), abi::chunk_state) != 11) {
                        ++s.cleanup_skipped;
                        continue;
                    }
                    std::lock_guard guard(s.policy_mutex);
                    if (s.denied.contains(key)) {
                        continue;
                    }
                    // Bound temporary rules even on a server with a very long uptime.
                    if (s.optimizer_denied.size() >= 65536) {
                        ++s.cleanup_skipped;
                        continue;
                    }
                    if (s.optimizer_denied.insert(key).second) {
                        ++s.cleanup_marked;
                    }
                }
            }
        }
        for (int n = 0; n < s.batch_size && !s.restore_queue.empty(); ++n) {
            auto key = s.restore_queue.front();
            s.restore_queue.pop_front();
            restoreVisible(key);
        }
    }
    void advancePins()
    {
        std::set<void*> pending_sources;
        for (auto it = state.generation_neighbors.begin(); it != state.generation_neighbors.end();) {
            const auto& key = it->first;
            auto pin = state.pins.find(key);
            if (pin == state.pins.end() || field<uint8_t>(pin->second.get(), abi::chunk_state) == 11) {
                it = state.generation_neighbors.erase(it);
                continue;
            }
            const auto& [dim, x, z] = key;
            auto* src = source(dim);
            pending_sources.insert(src);
            // BDS generation stages need surrounding terrain, decorations and light.
            // Keep a bounded neighborhood only until the requested center is loaded.
            for (int dx = -6; dx <= 6; ++dx) {
                for (int dz = -6; dz <= 6; ++dz) {
                    if (!dx && !dz) {
                        continue;
                    }
                    Pos pos{x + dx, z + dz};
                    auto chunk = callLoad(slot<Load>(src, abi::load_slot), src, pos, 1, false);
                    if (chunk) {
                        it->second[{dim, pos.x, pos.z}] = std::move(chunk);
                    }
                    else {
                        it->second.erase({dim, pos.x, pos.z});
                    }
                }
            }
            auto chunk = callLoad(slot<Load>(src, abi::load_slot), src, {x, z}, 1, false);
            if (chunk) {
                pin->second = std::move(chunk);
            }
            ++it;
        }
        // The native ChunkView fetch callback also dispatches queued chunk tasks.
        // Merely requesting chunks leaves distant chunks waiting for lighting.
        for (void* src : pending_sources) {
            reinterpret_cast<void (*)(void*, bool)>(state.bds + abi::dispatch)(src, false);
        }
    }
    void sweep()
    {
        std::set<Key> policy;
        {
            std::lock_guard guard(state.policy_mutex);
            policy = state.denied;
            if (state.optimizer_enabled && !state.optimizer_suspended) {
                policy.insert(state.optimizer_denied.begin(), state.optimizer_denied.end());
            }
        }
        if (policy.empty()) {
            return;
        }
        std::vector<Shared> release;
        std::lock_guard guard(state.views_mutex);
        for (void* view : state.views) {
            auto* dim = field<void*>(view, 0x28);
            auto* mutex = static_cast<char*>(view) + abi::view_mutex;
            NativeLock native_lock(mutex, state.lock, state.unlock);
            // Both buffers retain shared ownership during a view move.
            for (size_t offset : {abi::buffer1, abi::buffer2}) {
                auto* begin = field<Shared*>(view, offset);
                auto* end = field<Shared*>(view, offset + 8);
                if ((!begin && end) || (begin && (end < begin || end - begin > 100000))) {
                    throw std::runtime_error("ChunkViewSource buffer layout check failed");
                }
                for (auto* entry = begin; entry != end; ++entry) {
                    if (!*entry) {
                        continue;
                    }
                    const auto pos = field<Pos>(entry->get(), abi::chunk_position);
                    if (!policy.contains({dim, pos.x, pos.z})) {
                        continue;
                    }
                    release.push_back(std::move(*entry));
                    ++state.released;
                }
            }
        }
        // Shared ownership is released normally; BDS performs its discard/save path.
    }
};
Native::Native(endstone::Server& server) : impl_(std::make_unique<Impl>(server)) {}
Native::~Native()
{
    stop();
}
void Native::install()
{
    auto& s = impl_->state;
    if (active) {
        throw std::runtime_error("Only one WorldChunks native adapter is allowed");
    }
#ifdef _WIN32
    s.bds = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto runtime = GetModuleHandleW(L"endstone_runtime.dll");
    if (!runtime) {
        throw std::runtime_error("Endstone runtime module is missing");
    }
    s.runtime = reinterpret_cast<uintptr_t>(runtime);
    std::array<char, 32768> path{};
    if (!GetModuleFileNameA(runtime, path.data(), static_cast<DWORD>(path.size()))) {
        throw std::runtime_error("Cannot locate Endstone runtime");
    }
    s.runtime_path = path.data();
    if (!GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()))) {
        throw std::runtime_error("Cannot locate BDS");
    }
    if (sha256(path.data()) != "76d547f82e02c18d0986c30b47132c9cc4171d0f2df1c00649e50ff35788b321") {
        throw std::runtime_error("Unsupported BDS binary; expected Windows 1.26.51.1");
    }
    if (sha256(s.runtime_path) != "81f0279301dc5b10e8c10b71c61c6eeb2ea04a8c0783399d6377a881cc86da9e") {
        throw std::runtime_error("Unsupported Endstone runtime; expected 0.11.11 CPython 3.14 Windows wheel");
    }
    s.lock = [](void* p) { AcquireSRWLockExclusive(static_cast<PSRWLOCK>(p)); };
    s.unlock = [](void* p) { ReleaseSRWLockExclusive(static_cast<PSRWLOCK>(p)); };
    s.hooks = funchook_create();
    if (!s.hooks) {
        throw std::runtime_error("Cannot allocate native hooks");
    }
    hook(s, s.view_update, 0x9e78b0, viewUpdateHook);
    hook(s, s.copy, 0x9e8d00, copyHook);
    hook(s, s.view_dtor, 0xa15f60, gridDtorHook);
    hook(s, s.main_get, 0x38dd630, getHook);
    hook(s, s.main_create, 0x38dd980, createHook);
    hook(s, s.get_or_load, 0x9e4900, loadHook);
#else
    dl_iterate_phdr(modules, &s);
    if (!s.runtime) {
        throw std::runtime_error("Endstone runtime module is missing");
    }
    if (sha256("/proc/self/exe") != "e93e739f373a84edfff7c9cd76fcb090c2176744b412e1143f1b38e91a49bed4") {
        throw std::runtime_error("Unsupported BDS binary; expected the supplied Linux 1.26.51.1 archive");
    }
    if (sha256(s.runtime_path) != "1358eae445a8700b7b14746170d0f691c025fea705c10f623055fa82b8d71c29") {
        throw std::runtime_error("Unsupported Endstone runtime; expected 0.11.11 CPython 3.14 Linux wheel");
    }
    s.lock = reinterpret_cast<Lock>(s.bds + 0x402d3d0);
    s.unlock = reinterpret_cast<Lock>(s.bds + 0x402d4a0);
    s.hooks = funchook_create();
    if (!s.hooks) {
        throw std::runtime_error("Cannot allocate native hooks");
    }
    hook(s, s.ctor, 0xca33dd0, ctorHook);
    hook(s, s.copy, 0xca35440, copyHook);
    hook(s, s.dtor, 0xca3a990, dtorHook);
    hook(s, s.deleting_dtor, 0xca3a9c0, deletingDtorHook);
    hook(s, s.view_dtor, 0x51a37b0, viewDtorHook);
    hook(s, s.main_get, 0xca0a330, getHook);
    hook(s, s.main_create, 0xca0a660, createHook);
    hook(s, s.get_or_load, 0xca310c0, loadHook);
    s.tick_offsets = reinterpret_cast<TickOffsets>(s.bds + 0xbb5a250);
#endif
    active = &s;
    if (funchook_install(s.hooks, 0) != 0) {
        active = nullptr;
        throw std::runtime_error(funchook_error_message(s.hooks));
    }
    s.installed = true;
}
void Native::stop()
{
    if (!impl_) {
        return;
    }
    auto& s = impl_->state;
    if (s.installed) {
        if (s.native_offsets && s.effective_radius) {
            *s.native_offsets = s.vanilla_offsets;
        }
        {
            std::lock_guard guard(s.policy_mutex);
            s.denied.clear();
            s.optimizer_denied.clear();
            s.optimizer_enabled = false;
        }
        s.pins.clear();
        s.generation_neighbors.clear();
        // Releasing the final owner queues disposal on the Dimension collector.
        // Drain it while Endstone still exists: its unload-event callback uses
        // the server singleton, which is already gone in the Level destructor.
        if (auto* level = impl_->server.getLevel()) {
            for (auto* dimension : level->getDimensions()) {
                auto* dim = impl_->dimension(dimension->getName());
                slot<void (*)(void*)>(dim, abi::flush_dimension)(dim); // flushLevelChunkGarbageCollector
                auto* src = impl_->source(dim);
                slot<void (*)(void*)>(src, abi::flush_batch)(src);   // flushThreadBatch
                slot<void (*)(void*)>(src, abi::flush_discard)(src); // flushPendingDiscardedChunkWrites
            }
        }
        funchook_uninstall(s.hooks, 0);
        s.installed = false;
        active = nullptr;
    }
    if (s.hooks) {
        funchook_destroy(s.hooks);
        s.hooks = nullptr;
    }
}
void Native::tick()
{
    impl_->requireMain();
    if (impl_->state.grace_ticks > 0) {
        --impl_->state.grace_ticks;
    }
    impl_->applyRadius();
    impl_->updateOptimizer();
    impl_->applyRadius();
    impl_->advancePins();
    impl_->sweep();
}
Json Native::status()
{
    impl_->requireMain();
    auto& s = impl_->state;
    Json dims = Json::array();
    for (auto* d : impl_->server.getLevel()->getDimensions()) {
        dims.push_back({{"name", d->getName()}, {"loaded", d->getLoadedChunks().size()}});
    }
    std::scoped_lock guard(s.views_mutex, s.policy_mutex);
    return {{"native", s.installed},
            {"tracked_views", s.views.size()},
            {"constructed_views", s.constructors.load()},
            {"denied", s.denied.size()},
            {"optimizer_denied", s.optimizer_denied.size()},
            {"optimizer_enabled", s.optimizer_enabled},
            {"pins", s.pins.size()},
            {"pending_generation", s.generation_neighbors.size()},
            {"blocked_requests", s.blocked.load()},
            {"released_view_references", s.released.load()},
            {"simulation_radius_cap", s.radius},
            {"effective_radius_cap", s.effective_radius},
            {"spawn_pending", s.spawn_pending},
            {"spawn_grace_ticks", s.grace_ticks},
            {"effective_tick_offsets", s.native_offsets ? s.native_offsets->size() : 0},
            {"dimensions", dims}};
}
Json Native::inspect(const std::string& name, int x, int z)
{
    impl_->requireMain();
    void* dim = impl_->dimension(name);
    auto chunk = impl_->existing(dim, {x, z});
    bool denied, optimized;
    {
        std::lock_guard guard(impl_->state.policy_mutex);
        denied = impl_->state.denied.contains({dim, x, z});
        optimized = impl_->state.optimizer_denied.contains({dim, x, z});
    }
    Json result = {{"dimension", name},
                   {"x", x},
                   {"z", z},
                   {"denied", denied},
                   {"resident", bool(chunk)},
                   {"optimizer_denied", optimized},
                   {"references", chunk ? chunk.use_count() - 1 : 0}};
    if (chunk) {
        result["state"] = field<uint8_t>(chunk.get(), abi::chunk_state);
        result["last_tick"] = field<uint64_t>(chunk.get(), abi::chunk_last_tick);
    }
    return result;
}
Json Native::deny(const std::string& name, int x, int z)
{
    impl_->requireMain();
    auto* dim = impl_->dimension(name);
    const Key key{dim, x, z};
    impl_->state.pins.erase(key);
    impl_->state.generation_neighbors.erase(key);
    for (auto& [center, neighbors] : impl_->state.generation_neighbors) {
        neighbors.erase(key);
    }
    {
        std::lock_guard guard(impl_->state.policy_mutex);
        impl_->state.denied.insert(key);
    }
    impl_->sweep();
    return inspect(name, x, z);
}
Json Native::allow(const std::string& name, int x, int z)
{
    impl_->requireMain();
    auto* dim = impl_->dimension(name);
    {
        std::lock_guard guard(impl_->state.policy_mutex);
        impl_->state.denied.erase({dim, x, z});
    }
    impl_->restoreViews(dim, {x, z});
    return inspect(name, x, z);
}
Json Native::pin(const std::string& name, int x, int z)
{
    impl_->requireMain();
    auto* dim = impl_->dimension(name);
    {
        std::lock_guard guard(impl_->state.policy_mutex);
        if (impl_->state.denied.contains({dim, x, z})) {
            throw std::runtime_error("Allow this chunk before loading it");
        }
    }
    {
        std::lock_guard guard(impl_->state.policy_mutex);
        for (auto it = impl_->state.optimizer_denied.begin(); it != impl_->state.optimizer_denied.end();) {
            const auto& [d, cx, cz] = *it;
            if (d == dim && insideRadius(x, z, cx, cz, 6)) {
                it = impl_->state.optimizer_denied.erase(it);
            }
            else {
                ++it;
            }
        }
    }
    auto* src = impl_->source(dim);
    auto chunk = callLoad(slot<Load>(src, abi::load_slot), src, {x, z}, 1, false);
    if (!chunk) {
        throw std::runtime_error("BDS refused the chunk request");
    }
    impl_->state.pins[{dim, x, z}] = std::move(chunk);
    if (field<uint8_t>(impl_->state.pins.at({dim, x, z}).get(), abi::chunk_state) != 11) {
        impl_->state.generation_neighbors.try_emplace(Key{dim, x, z});
    }
    return inspect(name, x, z);
}
Json Native::unpin(const std::string& name, int x, int z)
{
    impl_->requireMain();
    auto* dim = impl_->dimension(name);
    impl_->state.pins.erase({dim, x, z});
    impl_->state.generation_neighbors.erase({dim, x, z});
    return inspect(name, x, z);
}
Json Native::views()
{
    impl_->requireMain();
    Json result = Json::array();
    auto& s = impl_->state;
    std::lock_guard guard(s.views_mutex);
    for (auto* view : s.views) {
        auto* mutex = static_cast<char*>(view) + abi::view_mutex;
        NativeLock native_lock(mutex, s.lock, s.unlock);
        Json entry = {{"address", reinterpret_cast<uintptr_t>(view)},
                      {"dimension", reinterpret_cast<uintptr_t>(field<void*>(view, 0x28))}};
        entry["bounds"] = {field<int>(view, abi::x1), field<int>(view, abi::z1), field<int>(view, abi::x2),
                           field<int>(view, abi::z2)};
        entry["slots"] = field<int>(view, abi::view_slots);
        size_t count = 0;
        for (auto p = field<Shared*>(view, abi::buffer1); p != field<Shared*>(view, abi::buffer1 + 8); ++p) {
            if (*p) {
                ++count;
            }
        }
        entry["retained"] = count;
        result.push_back(std::move(entry));
    }
    return result;
}
Json Native::players()
{
    impl_->requireMain();
    Json result = Json::array();
    for (auto* p : impl_->server.getOnlinePlayers()) {
        auto location = p->getLocation();
#ifdef _WIN32
        Json entry = {
            {"name", p->getName()},
            {"dimension", p->getDimension().getName()},
            {"x", location.getX()},
            {"y", location.getY()},
            {"z", location.getZ()},
            {"vanilla_tick_offsets", impl_->state.vanilla_offsets.size()},
            {"effective_tick_offsets", impl_->state.native_offsets ? impl_->state.native_offsets->size() : 0}};
#else
        auto* handle = impl_->player(p);
        const auto& offsets = impl_->state.tick_offsets(handle);
        auto* view = field<void*>(handle, 0x520);
        Json entry = {{"name", p->getName()},
                      {"dimension", p->getDimension().getName()},
                      {"x", location.getX()},
                      {"y", location.getY()},
                      {"z", location.getZ()},
                      {"native_view_radius", field<unsigned>(handle, 0xadc)},
                      {"vanilla_tick_offsets",
                       impl_->state.native_offsets ? impl_->state.vanilla_offsets.size() : offsets.size()},
                      {"effective_tick_offsets", offsets.size()}};
        if (view) {
            entry["view_bounds"] = {field<int>(view, abi::x1), field<int>(view, abi::z1), field<int>(view, abi::x2),
                                    field<int>(view, abi::z2)};
        }
#endif
        result.push_back(std::move(entry));
    }
    return result;
}
Json Native::radius(int value)
{
    impl_->requireMain();
    if (value < 0 || value > 32) {
        throw std::runtime_error("Radius must be 0 (vanilla) or 1..32 chunks");
    }
    impl_->captureOffsets();
    auto& s = impl_->state;
    s.radius = value;
    impl_->applyRadius();
    return {{"simulation_radius_cap", value}, {"players", players()}};
}
Json Native::policy()
{
    impl_->requireMain();
    auto& s = impl_->state;
    Json result = {
        {"schema", 1}, {"simulation_radius_cap", s.radius}, {"denied", Json::array()}, {"pins", Json::array()}};
    std::lock_guard guard(s.policy_mutex);
    for (auto* dim : impl_->server.getLevel()->getDimensions()) {
        auto* handle = impl_->dimension(dim->getName());
        for (const auto& [d, x, z] : s.denied) {
            if (d == handle) {
                result["denied"].push_back({{"dimension", dim->getName()}, {"x", x}, {"z", z}});
            }
        }
        for (const auto& [key, chunk] : s.pins) {
            const auto& [d, x, z] = key;
            if (d == handle) {
                result["pins"].push_back({{"dimension", dim->getName()}, {"x", x}, {"z", z}});
            }
        }
    }
    return result;
}
Json Native::reset()
{
    impl_->requireMain();
    auto& s = impl_->state;
    std::set<Key> previous;
    {
        std::lock_guard guard(s.policy_mutex);
        previous.swap(s.denied);
    }
    radius(0);
    for (const auto& [dim, x, z] : previous) {
        impl_->restoreViews(dim, {x, z});
    }
    s.pins.clear();
    s.generation_neighbors.clear();
    return policy();
}
void Native::joined(endstone::Player& player)
{
    impl_->requireMain();
    impl_->state.joined_players.insert(impl_->player(&player));
    spawnGrace();
}
void Native::left(endstone::Player& player)
{
    impl_->requireMain();
    impl_->state.joined_players.erase(impl_->player(&player));
}
void Native::spawnGrace()
{
    impl_->requireMain();
    impl_->state.grace_ticks = 100;
    impl_->applyRadius();
}
Json Native::optimizer(const Json& request)
{
    impl_->requireMain();
    auto& s = impl_->state;
    const auto op = request.at("op").get<std::string>();
    if (op == "configure") {
        const bool enabled = request.at("enabled").get<bool>();
        const int keep = request.at("keep_radius").get<int>();
        const int radius = request.at("simulation_radius").get<int>();
        const int batch = request.at("batch_size").get<int>();
        if (keep < 1 || keep > 32 || radius < 1 || radius > keep || batch < 1 || batch > 512) {
            throw std::runtime_error("Optimizer requires radius 1..32, simulation 1..radius, batch 1..512");
        }
        impl_->captureOffsets();
        impl_->clearOptimizerRules();
        {
            std::lock_guard guard(s.policy_mutex);
            s.optimizer_enabled = enabled;
        }
        s.keep_radius = keep;
        s.optimizer_radius = radius;
        s.batch_size = batch;
        if (enabled) {
            s.grace_ticks = 100;
        }
        impl_->applyRadius();
    }
    else if (op == "disable") {
        {
            std::lock_guard guard(s.policy_mutex);
            s.optimizer_enabled = false;
        }
        impl_->clearOptimizerRules();
        impl_->applyRadius();
    }
    else if (op == "cleanup") {
        if (!s.optimizer_enabled) {
            throw std::runtime_error("Optimizer is disabled");
        }
        s.cleanup_requested = true;
        s.cleanup_reason = request.value("reason", "manual");
    }
    else if (op != "status") {
        throw std::runtime_error("Unknown optimizer API operation");
    }
    std::lock_guard guard(s.policy_mutex);
    return {
        {"api", 1},
        {"enabled", s.optimizer_enabled},
        {"keep_radius", s.keep_radius},
        {"simulation_radius", s.optimizer_radius},
        {"batch_size", s.batch_size},
        {"temporary_denies", s.optimizer_denied.size()},
        {"rule_limit", 65536},
        {"pending_chunks", s.cleanup_queue.size()},
        {"pending_restore", s.restore_queue.size()},
        {"cleanup_requested", s.cleanup_requested},
        {"cleanup_runs", s.cleanup_runs},
        {"chunks_marked", s.cleanup_marked},
        {"chunks_skipped", s.cleanup_skipped},
        {"last_reason", s.cleanup_reason},
        {"paused_for_players", s.optimizer_enabled && (s.spawn_pending || s.grace_ticks > 0 || s.optimizer_loading)},
        {"waiting_for_player_chunks", s.optimizer_enabled && s.optimizer_loading},
        {"grace_ticks", s.grace_ticks},
        {"player_centers", s.player_centers.size()},
        {"effective_simulation_radius", s.effective_radius}};
}
} // namespace worldchunks
