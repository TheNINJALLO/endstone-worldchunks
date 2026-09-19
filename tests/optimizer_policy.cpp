#include "worldchunks/optimizer_policy.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    int count = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            throw std::runtime_error(message);
        }
        ++count;
    };
    using worldchunks::insideRadius;
    check(insideRadius(-3, -1, -1, 1, 2), "negative boundary included");
    check(!insideRadius(-4, -1, -1, 1, 2), "one beyond boundary excluded");
    check(insideRadius(2, 2, 0, 0, 2), "square includes corners");
    check(!insideRadius(0, 0, 10, 10, 2), "distant player does not protect");
    worldchunks::CleanupTrigger trigger;
    trigger.reset(0, 30);
    auto poll = [&](double time, double tps, double interval = 30, double threshold = 18) {
        return trigger.poll(time, tps, interval, threshold, 5, 30);
    };
    check(poll(29, 20).empty(), "interval not early");
    check(poll(30, 20) == "interval", "interval at deadline");
    check(poll(31, 20).empty(), "interval does not repeat immediately");
    check(poll(32, 18).empty(), "low TPS requires sustained duration");
    check(poll(36, 18).empty(), "sustained timer not early");
    check(poll(37, 18) == "low_tps", "inclusive TPS threshold fires");
    check(poll(42, 10).empty(), "cooldown suppresses repeated low TPS");
    check(poll(67, 10) == "low_tps", "sustained low TPS can retry after cooldown");
    check(poll(68, 20).empty(), "recovery resets sustained timer");
    check(poll(98, 17, 0).empty(), "new low TPS starts new timer");
    check(poll(100, 20, 0).empty(), "brief dip recovered");
    trigger.reset(0, 0);
    check(poll(500, 20, 0, 0).empty(), "both automatic triggers disabled");
    check(poll(501, 0, 0).empty(), "uninitialized zero TPS ignored");
    check(poll(502, std::numeric_limits<double>::quiet_NaN(), 0).empty(), "NaN TPS ignored");
    check(poll(503, 17, 0).empty(), "TPS trigger works without interval");
    check(poll(508, 17, 0) == "low_tps", "TPS-only cleanup fires");
    std::cout << "PASS " << count << " optimizer policy checks\n";
}
