#pragma once
#include <cmath>
#include <optional>
#include <string>

namespace worldchunks {
inline bool insideRadius(int x, int z, int cx, int cz, int radius)
{
    return std::abs(static_cast<long long>(x) - cx) <= radius && std::abs(static_cast<long long>(z) - cz) <= radius;
}

// Times are monotonic seconds, not server ticks, so lag cannot stretch intervals.
class CleanupTrigger {
    double next_interval_{}, next_tps_{};
    std::optional<double> low_since_;

  public:
    void reset(double now, double interval)
    {
        next_interval_ = now + interval;
        next_tps_ = now;
        low_since_.reset();
    }
    std::string poll(double now, double tps, double interval, double threshold, double sustained, double cooldown)
    {
        if (threshold > 0 && std::isfinite(tps) && tps > 0 && tps <= threshold) {
            if (!low_since_) {
                low_since_ = now;
            }
        }
        else {
            low_since_.reset();
        }
        if (low_since_ && now - *low_since_ >= sustained && now >= next_tps_) {
            next_tps_ = now + cooldown;
            if (interval > 0) {
                next_interval_ = now + interval;
            }
            return "low_tps";
        }
        if (interval > 0 && now >= next_interval_) {
            next_interval_ = now + interval;
            return "interval";
        }
        return {};
    }
};
} // namespace worldchunks
