#include "../src/inventory_layout.hpp"

#include <cassert>

using namespace InventoryLayout;

int main() {
    std::vector<Entry> entries{
        {0, Category::Weapons, 30}, {1, Category::Armor, 999}, {2, Category::Weapons, 70},
        {3, Category::Weapons, 20}, {4, Category::Weapons, 70}, {5, Category::Supplies, 0},
        {6, Category::Ammo, 200}, {7, Category::Weapons, 0},
    };
    Sort(entries);
    const std::vector<int> expected{2, 4, 0, 3, 7, 1, 6, 5};
    for (std::size_t i = 0; i < entries.size(); ++i) assert(entries[i].nativeIndex == expected[i]);

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
    const auto nearPointer = PlaceTooltip(viewport, 650, 400, 330, 160, 16);
    assert(nearPointer.x == 666 && nearPointer.y == 416);
    const auto nearEdges = PlaceTooltip(viewport, 1915, 1075, 330, 160, 16);
    assert(nearEdges.Right() == 1899 && nearEdges.Bottom() == 1059);
    const auto tall = PlaceTooltip(viewport, 1800, 500, 480, 800, 16);
    assert(tall.Right() == 1784 && tall.y == viewport.y && tall.Bottom() <= viewport.Bottom());
}
