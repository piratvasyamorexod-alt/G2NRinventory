#pragma once

#include <algorithm>
#include <vector>

namespace InventoryLayout {

enum class Category { All, Weapons, Armor, Ammo, Magic, Supplies, Documents, Other, Count };

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
};

inline void Sort(std::vector<Entry>& entries) {
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
    const int columns = std::clamp((available.width + gap) / (target + gap), 4, 6);
    const int size = (available.width - gap * (columns - 1)) / columns;
    const int rows = (std::max)(1, (available.height + gap) / (size + gap));
    return {{available.x, available.y, columns * (size + gap) - gap, rows * (size + gap) - gap},
            columns, rows, gap, size};
}

}  // namespace InventoryLayout
