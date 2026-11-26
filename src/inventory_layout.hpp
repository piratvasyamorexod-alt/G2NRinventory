#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace InventoryLayout {

enum class Category { Recent, All, Weapons, Armor, Ammo, Magic, Supplies, Documents, Other, Sale, Count };

struct Box {
    int x = 0, y = 0, width = 0, height = 0;
    int Right() const { return x + width; }
    int Bottom() const { return y + height; }
    bool Hit(int px, int py) const { return x <= px && px < Right() && y <= py && py < Bottom(); }
};

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

inline void Sort(std::vector<Entry>& entries, Category category = Category::All) {
    if (category == Category::Recent) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.recentRank < b.recentRank;
        });
        return;
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.category == Category::Weapons && a.damage > b.damage;
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
