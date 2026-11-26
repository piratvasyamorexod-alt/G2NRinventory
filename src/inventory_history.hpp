#pragma once

#include <algorithm>
#include <istream>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace InventoryHistory {

enum class DocumentState { Unknown, Unread, Read };

class History {
    std::vector<int> recent;
    std::map<int, DocumentState> documents;
    std::map<int, int> sale;

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

    void ObserveDocument(int instance, bool newlyAcquired = false) {
        if (instance >= 0) documents.try_emplace(instance, newlyAcquired ? DocumentState::Unread : DocumentState::Unknown);
    }

    void MarkRead(int instance) { if (instance >= 0) documents[instance] = DocumentState::Read; }

    DocumentState ReadingState(int instance) const {
        const auto entry = documents.find(instance);
        return entry == documents.end() ? DocumentState::Unknown : entry->second;
    }

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
    void Load(std::istream& input, Resolve resolve) {
        recent.clear();
        documents.clear();
        sale.clear();
        std::string line;
        if (!std::getline(input, line) || (line != "G2NRinventory 1" && line != "G2NRinventory 2")) return;
        while (std::getline(input, line)) {
            std::istringstream record(line);
            char kind;
            std::string name;
            if (!(record >> kind >> name)) continue;
            if (kind == 'R') Acquire(resolve(name));
            if (kind == 'D') MarkRead(resolve(name));
            if (kind == 'U' || kind == 'O') ObserveDocument(resolve(name), kind == 'U');
            int quantity;
            if (kind == 'S' && record >> quantity && quantity > 0) SetForSale(resolve(name), quantity);
        }
    }

    template<class Resolve>
    void Save(std::ostream& output, Resolve resolve) const {
        output << "G2NRinventory 2\n";
        const auto write = [&](char kind, int instance) {
            const auto name = resolve(instance);
            if (!name.empty()) output << kind << ' ' << name << '\n';
        };
        for (auto it = recent.rbegin(); it != recent.rend(); ++it) write('R', *it);
        for (const auto& [instance, state] : documents) {
            write(state == DocumentState::Read ? 'D' : state == DocumentState::Unread ? 'U' : 'O', instance);
        }
        for (const auto& [instance, quantity] : sale) {
            const auto name = resolve(instance);
            if (!name.empty()) output << "S " << name << ' ' << quantity << '\n';
        }
    }
};

} // namespace InventoryHistory
