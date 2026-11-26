#pragma once

#include <algorithm>
#include <istream>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace InventoryHistory {

class History {
    std::vector<int> recent;
    std::set<int> read;

public:
    static constexpr int MaximumRecent = 64;

    void Acquire(int instance) {
        if (instance < 0) return;
        const auto existing = std::find(recent.begin(), recent.end(), instance);
        if (existing != recent.end()) recent.erase(existing);
        recent.insert(recent.begin(), instance);
        if (recent.size() > MaximumRecent) recent.pop_back();
    }

    int Rank(int instance) const {
        const auto entry = std::find(recent.begin(), recent.end(), instance);
        return entry == recent.end() ? -1 : static_cast<int>(entry - recent.begin());
    }

    void MarkRead(int instance) { if (instance >= 0) read.insert(instance); }
    bool WasRead(int instance) const { return read.find(instance) != read.end(); }

    const auto& Recent() const { return recent; }

    template<class Resolve>
    void Load(std::istream& input, Resolve resolve) {
        recent.clear();
        read.clear();
        std::string header;
        if (!std::getline(input, header) || header != "G2NRinventory 1") return;
        char kind;
        std::string name;
        while (input >> kind >> name) {
            if (kind == 'R') Acquire(resolve(name));
            if (kind == 'D') MarkRead(resolve(name));
        }
    }

    template<class Resolve>
    void Save(std::ostream& output, Resolve resolve) const {
        output << "G2NRinventory 1\n";
        const auto write = [&](char kind, int instance) {
            const auto name = resolve(instance);
            if (!name.empty()) output << kind << ' ' << name << '\n';
        };
        for (auto it = recent.rbegin(); it != recent.rend(); ++it) write('R', *it);
        for (int instance : read) write('D', instance);
    }
};

} // namespace InventoryHistory
