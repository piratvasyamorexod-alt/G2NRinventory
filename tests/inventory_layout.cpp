#include "../src/inventory_layout.hpp"
#include "../src/inventory_history.hpp"
#include "../src/inventory_trade_rules.hpp"

#include <cassert>
#include <climits>
#include <limits>
#include <sstream>

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
    assert(!history.WasRead(20));
    history.MarkRead(20);
    history.MarkRead(20);
    history.Acquire(20);
    assert(history.WasRead(20) && !history.WasRead(21));
    history.SetForSale(20, 15);
    assert(!history.HasHotbar());
    history.SetHotbar(0, 20);
    history.SetHotbar(9, 21);
    history.SetHotbar(10, 22);
    history.SetHotbar(-1, 22);
    assert(history.HasHotbar() && history.Hotbar()[0] == 20 && history.Hotbar()[9] == 21);
    int reserved = history.ForSale(20);
    assert(AllocateSale(4, reserved) == 4);
    assert(AllocateSale(16, reserved) == 11);
    assert(reserved == 0);
    history.SetForSale(21, 2);
    history.CapForSale(21, 1);
    assert(history.ForSale(21) == 1);
    std::stringstream save;
    history.Save(save, [](int instance) { return "IT_TEST_" + std::to_string(instance); });
    save << "R MISSING_SYMBOL\nD MISSING_SYMBOL\nU IT_TEST_21\nO IT_TEST_22\n"
         << "H MISSING_SYMBOL 0\nH IT_TEST_22 10\nH IT_TEST_22 -1\n";
    InventoryHistory::History loaded;
    loaded.Load(save, [](const std::string& name) {
        return name.compare(0, 8, "IT_TEST_") == 0 ? std::stoi(name.substr(8)) + 100 : -1;
    });
    assert((loaded.Recent() == std::vector<int>{120, 121}));
    assert(loaded.WasRead(120) && !loaded.WasRead(121) && !loaded.WasRead(122) && !loaded.WasRead(-1));
    loaded.MarkRead(121);
    assert(loaded.WasRead(121));
    assert(loaded.ForSale(120) == 15 && loaded.ForSale(121) == 1);
    assert(loaded.Hotbar()[0] == 120 && loaded.Hotbar()[9] == 121 && loaded.Hotbar()[8] == -1);
    loaded.SetHotbar(0, -1);
    assert(loaded.Hotbar()[0] == -1 && loaded.HasHotbar());
    loaded.ReconcileSale({{120, 7}});
    assert(loaded.ForSale(120) == 7 && loaded.ForSale(121) == 0);
    loaded.SetForSale(120, 7 - 7);
    assert(!loaded.HasSale());
    std::istringstream previousVersion("G2NRinventory 1\nR HERE\nD HERE\n");
    loaded.Load(previousVersion, [](const std::string&) { return 5; });
    assert(loaded.Rank(5) == 0 && loaded.WasRead(5) && !loaded.HasSale() && !loaded.HasHotbar());
    std::istringstream legacyRecent("G2NRinventory 2\nR LEGACY_DOCUMENT\nS LEGACY_DOCUMENT 1\n");
    loaded.Load(legacyRecent, [](const std::string&) { return 6; });
    assert(loaded.Rank(6) == 0 && !loaded.WasRead(5) && !loaded.WasRead(6) && loaded.ForSale(6) == 1);
    for (int index = 100; index < 200; ++index) history.Acquire(index);
    assert(history.Recent().size() == InventoryHistory::History::MaximumRecent);
    assert(history.Rank(20) == -1 && history.Rank(199) == 0 && history.WasRead(20));

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

    for (const auto viewport : {Box{0, 0, 480, 360}, Box{0, 0, 1920, 1080}, Box{0, 0, 3840, 2160}}) {
        const auto bar = PlaceHotbar(viewport.width, viewport.height);
        assert(bar.bounds.x > 0 && bar.bounds.Right() < viewport.width && bar.bounds.Bottom() < viewport.height);
        assert(bar.bounds.y > 3 * viewport.height / 4 && bar.bounds.x == (viewport.width - bar.bounds.width) / 2);
        for (int i = 0; i < HotbarSlots; ++i) {
            const auto cell = bar.Cell(i);
            assert(bar.Hit(cell.x, cell.y) == i && bar.Hit(cell.Right() - 1, cell.Bottom() - 1) == i);
            assert(bar.Hit(cell.Right(), cell.y) == -1);
        }
    }
    PointerMotion shortClick(100, 100);
    shortClick.Extend(102, 96);
    shortClick.Extend(98, 104);
    assert(!shortClick.MovedBy(6, 6));

    PointerMotion roundTrip(100, 100);
    roundTrip.Extend(94, 100);
    roundTrip.Extend(100, 100);
    assert(roundTrip.MovedBy(6, 6));
    assert(!roundTrip.MovedBy(7, 6));

    PointerMotion vertical(100, 100);
    vertical.Extend(101, 107);
    vertical.Extend(100, 100);
    assert(vertical.MovedBy(9, 6));
    assert(!vertical.MovedBy(9, 8));

    const int rowHeight = 32;
    for (const bool hero : {true, false}) {
        for (const int width : {487, 336, 195}) {
            auto layout = FitCategoryTabs(width, rowHeight, hero);
            assert(layout.height == (width == 195 || (hero && width == 336) ? 2 : 1) * rowHeight);
            layout.MoveBy(12, 70);
            int covered = 0;
            for (int i = 0; i < static_cast<int>(Category::Count); ++i) {
                const auto category = static_cast<Category>(i);
                const auto cell = layout.cells[i];
                if (!hero && (category == Category::Recent || category == Category::Sale)) {
                    assert(cell.width == 0);
                    continue;
                }
                assert(cell.width >= rowHeight && cell.Right() <= width + 12);
                covered += cell.width * cell.height;
                assert(layout.Hit(cell.x, cell.y) == category);
                assert(layout.Hit(cell.Right() - 1, cell.Bottom() - 1) == category);
            }
            assert(covered == width * layout.height);
            assert(!layout.Hit(12 + width, 70));
            assert(!layout.Hit(12, 70 + layout.height));
        }
    }

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
    const auto sliderHitArea = QuantitySliderHitArea(slider);
    const auto firstThumb = QuantityThumb(slider, QuantityPosition(slider, 1, 21));
    const auto lastThumb = QuantityThumb(slider, QuantityPosition(slider, 21, 21));
    assert(!slider.Hit(firstThumb.x, firstThumb.y));
    assert(!slider.Hit(lastThumb.Right() - 1, lastThumb.Bottom() - 1));
    assert(sliderHitArea.Hit(firstThumb.x, firstThumb.y));
    assert(sliderHitArea.Hit(lastThumb.Right() - 1, lastThumb.Bottom() - 1));
    assert(sliderHitArea.Hit(firstThumb.x - 1, firstThumb.y));
    assert(sliderHitArea.Hit(lastThumb.Right(), lastThumb.y));
    assert(QuantityAt(slider, firstThumb.x, 21) == 1);
    assert(QuantityAt(slider, lastThumb.Right() - 1, 21) == 21);
    assert(!sliderHitArea.Hit(slider.x, slider.y - 1));
    assert(!sliderHitArea.Hit(slider.x, slider.Bottom()));
    assert(QuantityFromText("15", 20) == 15);
    assert(QuantityFromText("100", 20) == 20);
    assert(QuantityFromText("0", 20) == 1);
    assert(QuantityFromText("9999999999", INT_MAX) == INT_MAX);

    assert(InventoryTrade::SalePrice(100, .3f) == 30);
    assert(InventoryTrade::SalePrice(10, .25f) == 2);
    assert(InventoryTrade::SalePrice(14, .25f) == 4);
    assert(InventoryTrade::SalePrice(1, .1f) == 1);
    assert(InventoryTrade::SalePrice(0, .3f) == 0);
    assert(InventoryTrade::SalePrice(100, std::numeric_limits<float>::infinity()) == 0);
    assert(InventoryTrade::SalePrice(SHRT_MAX, 1.f) == SHRT_MAX);
    assert(InventoryTrade::SalePrice(SHRT_MAX + 1, 1.f) == 0);
    assert(InventoryTrade::Affordable(10, 9, 35) == 3);
    assert(InventoryTrade::Affordable(10, 9, 8) == 0);
    assert(InventoryTrade::Affordable(3, 9, INT_MAX) == 3);
    assert(InventoryTrade::Affordable(10, 0, 0) == 10);
}
