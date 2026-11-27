#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <vector>

namespace InventoryLayout {

inline constexpr int HotbarSlots = 10;
enum class Category { Recent, All, Weapons, Armor, Ammo, Magic, Supplies, Documents, Other, Sale, Count };
using CategoryWidths = std::array<int, static_cast<int>(Category::Count)>;

struct Box {
    int x = 0, y = 0, width = 0, height = 0;
    int Right() const { return x + width; }
    int Bottom() const { return y + height; }
    bool Hit(int px, int py) const { return x <= px && px < Right() && y <= py && py < Bottom(); }
};

struct HotbarLayout {
    Box bounds;
    int cellSize, gap, inset;

    Box Cell(int index) const {
        return {bounds.x + inset + index * (cellSize + gap), bounds.y + inset, cellSize, cellSize};
    }

    int Hit(int x, int y) const {
        for (int i = 0; i < HotbarSlots; ++i) if (Cell(i).Hit(x, y)) return i;
        return -1;
    }
};

inline HotbarLayout PlaceHotbar(int width, int height) {
    const int gap = std::clamp(height / 450, 2, 4);
    const int inset = 2 * gap;
    const int maxSize = (width - 2 * inset - (HotbarSlots + 1) * gap) / HotbarSlots;
    const int size = (std::min)(maxSize, std::clamp(height / 18, 35, 72));
    const int barWidth = HotbarSlots * size + (HotbarSlots - 1) * gap + 2 * inset;
    const int barHeight = size + 2 * inset;
    const int bottomMargin = std::clamp(height / 60, 8, 28);
    return {{(width - barWidth) / 2, height - bottomMargin - barHeight, barWidth, barHeight}, size, gap, inset};
}

inline bool DraggedEnough(int x, int y, int initialX, int initialY, int threshold) {
    return std::abs(x - initialX) >= threshold || std::abs(y - initialY) >= threshold;
}

inline Box CategoryTab(Box tabs, Category category) {
    const int index = static_cast<int>(category);
    constexpr int columns = 5;
    const bool lower = index >= columns;
    const int column = index % columns;
    const int height = tabs.height / 2;
    constexpr std::array<int, 6> top{0, 26, 44, 62, 80, 100}, bottom{0, 18, 37, 55, 74, 100};
    const auto& stops = lower ? bottom : top;
    const int left = tabs.x + stops[column] * tabs.width / 100;
    return {left, tabs.y + (lower ? height : 0), tabs.x + stops[column + 1] * tabs.width / 100 - left, height};
}

inline std::optional<Category> CategoryAt(Box tabs, int x, int y) {
    if (!tabs.Hit(x, y)) return std::nullopt;
    for (int i = 0; i < static_cast<int>(Category::Count); ++i) {
        const auto category = static_cast<Category>(i);
        if (CategoryTab(tabs, category).Hit(x, y)) return category;
    }
    return std::nullopt;
}

struct FittedTabs {
    std::array<Box, static_cast<int>(Category::Count)> cells{};
    int height = 0;

    std::optional<Category> Hit(int x, int y) const {
        for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
            if (cells[i].Hit(x, y)) return static_cast<Category>(i);
        }
        return std::nullopt;
    }
};

inline int TwoRowTabWidth(const CategoryWidths& widths, bool hero) {
    const int first = static_cast<int>(hero ? Category::Recent : Category::All);
    const int split = static_cast<int>(Category::Magic);
    const int end = static_cast<int>(hero ? Category::Count : Category::Sale);
    int upper = 0, lower = 0;
    for (int i = first; i < split; ++i) upper += widths[i];
    for (int i = split; i < end; ++i) lower += widths[i];
    return (std::max)(upper, lower);
}

inline FittedTabs FitCategoryTabs(int x, int width, int rowHeight, const CategoryWidths& widths, bool hero) {
    const int first = static_cast<int>(hero ? Category::Recent : Category::All);
    const int end = static_cast<int>(hero ? Category::Count : Category::Sale);
    const int split = width >= TwoRowTabWidth(widths, hero) ? static_cast<int>(Category::Magic) : end;
    FittedTabs tabs;
    for (int start = first; start < end;) {
        const int rowEnd = start < split ? split : end;
        int next = start + 1, used = (std::min)(widths[start], width);
        while (next < rowEnd && widths[next] <= width - used) used += widths[next++];
        int left = x, spare = width - used;
        for (int i = start; i < next; ++i) {
            const int padding = spare / (next - i);
            const int cellWidth = (std::min)(widths[i], width) + padding;
            tabs.cells[i] = {left, tabs.height, cellWidth, rowHeight};
            left += cellWidth;
            spare -= padding;
        }
        tabs.height += rowHeight;
        start = next;
    }
    return tabs;
}

struct Entry {
    int nativeIndex;
    Category category;
    int damage;
    int recentRank = -1;
    int quantity = 0;
};

inline int AllocateSale(int amount, int& reserved) {
    const int sale = (std::min)(amount, reserved);
    reserved -= sale;
    return sale;
}

inline int QuantityFromText(std::string_view digits, int maximum) {
    std::int64_t quantity = 0;
    for (const char digit : digits) {
        if (digit < '0' || digit > '9') break;
        quantity = quantity * 10 + digit - '0';
        if (quantity >= maximum) return maximum;
    }
    return (std::max)(1, static_cast<int>(quantity));
}

inline int QuantityAt(Box slider, int x, int maximum) {
    const int span = (std::max)(1, slider.width - 1);
    const auto offset = static_cast<std::int64_t>(std::clamp(x - slider.x, 0, span));
    return 1 + static_cast<int>((offset * (maximum - 1) + span / 2) / span);
}

inline int QuantityPosition(Box slider, int quantity, int maximum) {
    if (maximum <= 1) return slider.x;
    const auto span = static_cast<std::int64_t>((std::max)(0, slider.width - 1));
    return slider.x + static_cast<int>(((quantity - 1) * span + (maximum - 1) / 2) / (maximum - 1));
}

inline bool ComesBefore(const Entry& a, const Entry& b, Category view) {
    if (view == Category::Recent) return a.recentRank < b.recentRank;
    if (a.category != b.category) return a.category < b.category;
    return a.category == Category::Weapons && a.damage > b.damage;
}

inline void Sort(std::vector<Entry>& entries, Category category = Category::All) {
    std::stable_sort(entries.begin(), entries.end(), [category](const Entry& a, const Entry& b) {
        return ComesBefore(a, b, category);
    });
}

struct Grid {
    Box area;
    int columns = 1, rows = 1, gap = 0, size = 1;

    int LastRow(int count) const {
        return (std::max)(0, (count + columns - 1) / columns - rows);
    }

    Box Cell(int slot) const {
        return {area.x + slot % columns * (size + gap), area.y + slot / columns * (size + gap), size, size};
    }

    int ItemAt(int x, int y, int firstRow, int count) const {
        if (!area.Hit(x, y)) return -1;
        const int dx = x - area.x, dy = y - area.y;
        const int col = dx / (size + gap), row = dy / (size + gap);
        if (col >= columns || row >= rows || dx % (size + gap) >= size || dy % (size + gap) >= size) return -1;
        const int index = (row + firstRow) * columns + col;
        return index < count ? index : -1;
    }
};

inline Grid MakeGrid(Box available, int target, int gap) {
    const int columns = std::clamp((available.width + gap) / (target + gap), 4, 7);
    const int size = (available.width - gap * (columns - 1)) / columns;
    const int rows = (std::max)(1, (available.height + gap) / (size + gap));
    return {{available.x, available.y, columns * (size + gap) - gap, rows * (size + gap) - gap},
            columns, rows, gap, size};
}

inline Box PlacePanel(int screenWidth, int screenHeight, int width, int height, int margin) {
    return {screenWidth - width - margin, (screenHeight - height) / 2, width, height};
}

inline Box PlaceTooltip(Box viewport, int x, int y, int width, int height, int gap) {
    const int maxX = viewport.Right() - width, maxY = viewport.Bottom() - height;
    return {std::clamp(x + gap <= maxX ? x + gap : x - gap - width, viewport.x, maxX),
            std::clamp(y + gap <= maxY ? y + gap : y - gap - height, viewport.y, maxY), width, height};
}

}  // namespace InventoryLayout
