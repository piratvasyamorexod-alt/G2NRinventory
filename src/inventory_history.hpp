#pragma once

#include "inventory_layout.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace InventoryHistory {

class History {
    std::vector<int> recent;
    std::set<int> readDocuments;
    std::map<int, int> sale;
    std::array<int, InventoryLayout::HotbarSlots> hotbar;

public:
    static constexpr int MaximumRecent = 64;
    History() { hotbar.fill(-1); }

    const auto& Hotbar() const { return hotbar; }

    bool HasHotbar() const {
        return std::any_of(hotbar.begin(), hotbar.end(), [](int instance) { return instance >= 0; });
    }

    void SetHotbar(int slot, int instance) {
        if (slot >= 0 && slot < static_cast<int>(hotbar.size())) hotbar[slot] = instance;
    }

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

    void MarkRead(int instance) { if (instance >= 0) readDocuments.insert(instance); }
    bool WasRead(int instance) const { return readDocuments.find(instance) != readDocuments.end(); }

    const auto& Recent() const { return recent; }
    bool HasSale() const { return !sale.empty(); }

    int ForSale(int instance) const {
        const auto entry = sale.find(instance);
        return entry == sale.end() ? 0 : entry->second;
    }

    void SetForSale(int instance, int quantity) {
        if (instance < 0) return;
        if (quantity > 0) sale[instance] = quantity;
        else sale.erase(instance);
    }

    void CapForSale(int instance, int owned) {
        if (ForSale(instance) > owned) SetForSale(instance, owned);
    }

    void ReconcileSale(const std::map<int, int>& owned) {
        for (auto it = sale.begin(); it != sale.end();) {
            const auto quantity = owned.find(it->first);
            if (quantity == owned.end() || quantity->second <= 0) {
                it = sale.erase(it);
            } else {
                it->second = (std::min)(it->second, quantity->second);
                ++it;
            }
        }
    }

    template<class Resolve>
    bool Load(std::istream& input, Resolve resolve) {
        recent.clear();
        readDocuments.clear();
        sale.clear();
        hotbar.fill(-1);
        std::string line;
        if (!std::getline(input, line) || (line != "G2NRinventory 1" && line != "G2NRinventory 2")) return false;
        while (std::getline(input, line)) {
            std::istringstream record(line);
            char kind;
            std::string name;
            if (!(record >> kind >> name)) continue;
            if (kind == 'R') Acquire(resolve(name));
            if (kind == 'D') MarkRead(resolve(name));
            int quantity;
            if (kind == 'S' && record >> quantity && quantity > 0) SetForSale(resolve(name), quantity);
            int slot;
            if (kind == 'H' && record >> slot) {
                const int instance = resolve(name);
                if (instance >= 0) SetHotbar(slot, instance);
            }
        }
        return !input.bad();
    }

    template<class Resolve>
    void Save(std::ostream& output, Resolve resolve) const {
        output << "G2NRinventory 2\n";
        const auto write = [&](char kind, int instance) {
            const auto name = resolve(instance);
            if (!name.empty()) output << kind << ' ' << name << '\n';
        };
        for (auto it = recent.rbegin(); it != recent.rend(); ++it) write('R', *it);
        for (int instance : readDocuments) write('D', instance);
        for (const auto& [instance, quantity] : sale) {
            const auto name = resolve(instance);
            if (!name.empty()) output << "S " << name << ' ' << quantity << '\n';
        }
        for (int i = 0; i < static_cast<int>(hotbar.size()); ++i) {
            if (hotbar[i] < 0) continue;
            const auto name = resolve(hotbar[i]);
            if (!name.empty()) output << "H " << name << ' ' << i << '\n';
        }
    }
};

} // namespace InventoryHistory
