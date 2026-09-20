#include "worldchunks/protection.h"
#include <iostream>
#include <limits>

int main()
{
    int count = 0;
    auto check = [&](bool ok) {
        if (!ok) {
            throw std::runtime_error("Protection check " + std::to_string(count + 1) + " failed");
        }
        ++count;
    };
    auto rejects = [&](auto action) {
        bool rejected = false;
        try {
            action();
        }
        catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected);
    };
    worldchunks::ChunkProtections leases;
    leases.protect("copy:a", 1, "Overworld", -2, -3, -1, 0, 20, 100);
    check(leases.contains(1, -2, -3));
    check(leases.contains(1, -8, -9));
    check(!leases.contains(1, -9, -9));
    check(!leases.contains(2, -2, -3));
    leases.protect("copy:b", 1, "Overworld", -2, -3, -2, -3, 30, 100);
    leases.release("copy:a");
    check(leases.contains(1, -2, -3));
    leases.expire(129);
    check(leases.contains(1, -2, -3));
    leases.expire(130);
    check(!leases.contains(1, -2, -3));
    leases.protect("renew", 1, "Overworld", 0, 0, 0, 0, 20, 200);
    leases.protect("renew", 1, "Overworld", 0, 0, 0, 0, 20, 210);
    leases.expire(220);
    check(leases.contains(1, 0, 0));
    rejects([&] { leases.protect("renew", 1, "Overworld", 1, 0, 0, 0, 20, 220); });
    check(leases.contains(1, 0, 0)); // Invalid updates preserve the existing lease.
    leases.expire(230);
    check(leases.regions().empty());
    rejects([&] { leases.protect("bad id", 1, "Overworld", 0, 0, 0, 0, 20, 0); });
    rejects([&] { leases.protect("bad", 1, "Overworld", 0, 0, 64, 64, 20, 0); });
    rejects([&] { leases.protect("bad", 1, "Overworld", 0, 0, 0, 0, 0, 0); });
    rejects([&] { leases.protect("bad", 1, "Overworld", 0, 0, 0, 0, 72001, 0); });
    rejects([&] { leases.protect("bad", 1, "Overworld", std::numeric_limits<int>::min(), 0, 0, 0, 1, 0); });
    for (size_t i = 0; i < leases.limit; ++i) {
        leases.protect(std::to_string(i), 1, "Overworld", 0, 0, 0, 0, 1, 0);
    }
    rejects([&] { leases.protect("overflow", 1, "Overworld", 0, 0, 0, 0, 1, 0); });
    leases.protect("0", 1, "Overworld", 100, 100, 100, 100, 2, 0);
    check(leases.contains(1, 100, 100));
    leases.expire(1);
    check(leases.regions().size() == 1);
    leases.release("missing");
    leases.release("0");
    check(leases.regions().empty());
    std::cout << "PASS " << count << " protection policy checks\n";
}
