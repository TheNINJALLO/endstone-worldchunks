#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace worldchunks {
// Leases use server ticks: a TPS dip must not consume a job's loading deadline.
class ChunkProtections {
  public:
    struct Region {
        uintptr_t dimension;
        std::string dimension_name;
        int x1, z1, x2, z2;
        uint64_t expires;
        bool contains(uintptr_t dim, int x, int z) const
        {
            // Match the generation neighborhood used by native pins.
            return dimension == dim && x >= x1 - 6 && x <= x2 + 6 && z >= z1 - 6 && z <= z2 + 6;
        }
    };
    static constexpr size_t limit = 256;
    static void validate(const std::string& id, int x1, int z1, int x2, int z2, int ticks)
    {
        if (id.empty() || id.size() > 64 ||
            id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:-.") !=
                std::string::npos) {
            throw std::runtime_error("Protection ID must contain 1..64 letters, digits, _, :, - or .");
        }
        if (x1 < -1874999 || z1 < -1874999 || x2 > 1874999 || z2 > 1874999 || x1 > x2 || z1 > z2 ||
            (static_cast<int64_t>(x2) - x1 + 1) * (static_cast<int64_t>(z2) - z1 + 1) > 4096) {
            throw std::runtime_error(
                "Protection requires ordered chunk bounds within the world border, at most 4096 chunks");
        }
        if (ticks < 1 || ticks > 72000) {
            throw std::runtime_error("Protection duration must be 1..72000 server ticks");
        }
    }
    void protect(const std::string& id, uintptr_t dim, const std::string& name, int x1, int z1, int x2, int z2,
                 int ticks, uint64_t now)
    {
        validate(id, x1, z1, x2, z2, ticks);
        if (!regions_.contains(id) && regions_.size() >= limit) {
            throw std::runtime_error("Protection limit reached (256 active jobs)");
        }
        regions_.insert_or_assign(id, Region{dim, name, x1, z1, x2, z2, now + static_cast<uint64_t>(ticks)});
    }
    void release(const std::string& id)
    {
        regions_.erase(id);
    }
    void expire(uint64_t now)
    {
        std::erase_if(regions_, [now](const auto& entry) { return entry.second.expires <= now; });
    }
    bool contains(uintptr_t dim, int x, int z) const
    {
        for (const auto& [id, region] : regions_) {
            if (region.contains(dim, x, z)) {
                return true;
            }
        }
        return false;
    }
    const std::map<std::string, Region>& regions() const
    {
        return regions_;
    }

  private:
    std::map<std::string, Region> regions_;
};
} // namespace worldchunks
