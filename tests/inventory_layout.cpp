#include "../src/inventory_layout.hpp"
#include "../src/inventory_history.hpp"

#include <cassert>
#include <climits>
#include <sstream>

using namespace InventoryLayout;
using InventoryHistory::DocumentState;

int main() {
    std::vector<Entry> entries{
        {0, Category::Weapons, 30}, {1, Category::Armor, 999}, {2, Category::Weapons, 70},
        {3, Category::Weapons, 20}, {4, Category::Weapons, 70}, {5, Category::Supplies, 0},
        {6, Category::Ammo, 200}, {7, Category::Weapons, 0},
    };
    Sort(entries);
    const std::vector<int> expected{2, 4, 0, 3, 7, 1, 6, 5};
    for (std::size_t i = 0; i < entries.size(); ++i) assert(entries[i].nativeIndex == expected[i]);

    InventoryHistory::History history;
    history.Acquire(20);
    history.Acquire(21);
    history.Acquire(20);
    assert((history.Recent() == std::vector<int>{20, 21}));
    assert(history.Rank(20) == 0 && history.Rank(21) == 1 && history.Rank(22) == -1);
    std::vector<Entry> recent{{0, Category::Weapons, 90, history.Rank(21)},
                              {1, Category::Supplies, 0, history.Rank(20)}};
    Sort(recent, Category::Recent);
    assert(recent[0].nativeIndex == 1);
    assert(history.ReadingState(20) == DocumentState::Unknown);
    history.ObserveDocument(20);
    history.ObserveDocument(20, true);
    assert(history.ReadingState(20) == DocumentState::Unknown);
    history.MarkRead(20);
    history.Acquire(20);
    history.ObserveDocument(20, true);
    history.ObserveDocument(21, true);
    history.ObserveDocument(21);
    history.ObserveDocument(22);
    assert(history.ReadingState(20) == DocumentState::Read);
    assert(history.ReadingState(21) == DocumentState::Unread);
    history.SetForSale(20, 15);
    int reserved = history.ForSale(20);
    assert(AllocateSale(4, reserved) == 4);
    assert(AllocateSale(16, reserved) == 11);
    assert(reserved == 0);
    history.SetForSale(21, 2);
    history.CapForSale(21, 1);
    assert(history.ForSale(21) == 1);
    std::stringstream save;
    history.Save(save, [](int instance) { return "IT_TEST_" + std::to_string(instance); });
    save << "R MISSING_SYMBOL\n";
    InventoryHistory::History loaded;
    loaded.Load(save, [](const std::string& name) {
        return name.compare(0, 8, "IT_TEST_") == 0 ? std::stoi(name.substr(8)) + 100 : -1;
    });
    assert((loaded.Recent() == std::vector<int>{120, 121}));
    assert(loaded.ReadingState(120) == DocumentState::Read);
    assert(loaded.ReadingState(121) == DocumentState::Unread);
    loaded.ObserveDocument(122, true);
    assert(loaded.ReadingState(122) == DocumentState::Unknown);
    loaded.MarkRead(121);
    assert(loaded.ReadingState(121) == DocumentState::Read);
    assert(loaded.ForSale(120) == 15 && loaded.ForSale(121) == 1);
    loaded.ReconcileSale({{120, 7}});
    assert(loaded.ForSale(120) == 7 && loaded.ForSale(121) == 0);
    loaded.SetForSale(120, 7 - 7);
    assert(!loaded.HasSale());
    std::istringstream previousVersion("G2NRinventory 1\nR HERE\nD HERE\n");
    loaded.Load(previousVersion, [](const std::string&) { return 5; });
    assert(loaded.Rank(5) == 0 && loaded.ReadingState(5) == DocumentState::Read && !loaded.HasSale());
    std::istringstream legacyRecent("G2NRinventory 2\nR LEGACY_DOCUMENT\nS LEGACY_DOCUMENT 1\n");
    loaded.Load(legacyRecent, [](const std::string&) { return 6; });
    assert(loaded.Rank(6) == 0 && loaded.ReadingState(6) == DocumentState::Unknown && loaded.ForSale(6) == 1);
    for (int index = 100; index < 200; ++index) history.Acquire(index);
    assert(history.Recent().size() == InventoryHistory::History::MaximumRecent);
    assert(history.Rank(20) == -1 && history.Rank(199) == 0 && history.ReadingState(20) == DocumentState::Read);

    for (const auto available : {Box{10, 15, 250, 170}, Box{10, 15, 480, 270}, Box{10, 15, 900, 650}}) {
        const auto grid = MakeGrid(available, 95, 4);
        assert(grid.columns >= 4 && grid.columns <= 7);
        assert(grid.area.Right() <= available.Right() && grid.area.Bottom() <= available.Bottom());
        assert(grid.LastRow(0) == 0);
        const int count = grid.columns * (grid.rows + 2) + 1;
        const int firstRow = grid.LastRow(count);
        const auto first = grid.Cell(0);
        assert(grid.ItemAt(first.x, first.y, firstRow, count) == firstRow * grid.columns);
        assert(grid.ItemAt(first.Right(), first.y, firstRow, count) == -1);
        assert(grid.ItemAt(first.x, first.Bottom(), firstRow, count) == -1);
        const auto lastVisible = grid.Cell(grid.columns * (grid.rows - 1));
        assert(grid.ItemAt(lastVisible.x, lastVisible.y, firstRow, count) == count - 1);
        assert(grid.ItemAt(lastVisible.Right() + grid.gap, lastVisible.y, firstRow, count) == -1);
    }

    const Box viewport{1, 1, 1918, 1078};
    const auto panel = PlacePanel(1920, 1080, 591, 780, 26);
    assert(panel.x > 1920 / 2 && panel.Right() == 1920 - 26 && panel.y == 150);
    const auto compact = MakeGrid({0, 0, 557, 680}, 77, 3);
    assert(compact.columns == 7 && compact.rows == 8 && compact.size == 77);
    const auto nearPointer = PlaceTooltip(viewport, 650, 400, 330, 160, 16);
    assert(nearPointer.x == 666 && nearPointer.y == 416);
    const auto nearEdges = PlaceTooltip(viewport, 1915, 1075, 330, 160, 16);
    assert(nearEdges.Right() == 1899 && nearEdges.Bottom() == 1059);
    const auto tall = PlaceTooltip(viewport, 1800, 500, 480, 800, 16);
    assert(tall.Right() == 1784 && tall.y == viewport.y && tall.Bottom() <= viewport.Bottom());

    const Box slider{100, 110, 201, 20};
    assert(QuantityAt(slider, 50, 21) == 1);
    assert(QuantityAt(slider, 200, 21) == 11);
    assert(QuantityAt(slider, 400, 21) == 21);
    assert(QuantityPosition(slider, 11, 21) == 200);
    assert(QuantityPosition(slider, 1, 1) == 100);
    assert(QuantityFromText("15", 20) == 15);
    assert(QuantityFromText("100", 20) == 20);
    assert(QuantityFromText("0", 20) == 1);
    assert(QuantityFromText("9999999999", INT_MAX) == INT_MAX);
}
