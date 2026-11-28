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
enum class Category { Recent, All, Weapons, Armor, Ammo, Magic, Potions, Supplies, Documents, Other, Sale, Count };

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

class PointerMotion {
    int originX, originY;
    int distanceX = 0, distanceY = 0;

public:
    PointerMotion(int x, int y) : originX(x), originY(y) {}

    void Extend(int x, int y) {
        distanceX = (std::max)(distanceX, std::abs(x - originX));
        distanceY = (std::max)(distanceY, std::abs(y - originY));
    }

    bool MovedBy(int thresholdX, int thresholdY) const {
        return distanceX >= thresholdX || distanceY >= thresholdY;
    }
};

struct FittedTabs {
    std::array<Box, static_cast<int>(Category::Count)> cells{};
    int height = 0;

    std::optional<Category> Hit(int x, int y) const {
        for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
            if (cells[i].Hit(x, y)) return static_cast<Category>(i);
        }
        return std::nullopt;
    }

    void MoveBy(int dx, int dy) {
        for (auto& cell : cells) {
            cell.x += dx;
            cell.y += dy;
        }
    }
};

inline FittedTabs FitCategoryTabs(int width, int rowHeight, bool hero) {
    const int first = static_cast<int>(hero ? Category::Recent : Category::All);
    const int end = static_cast<int>(hero ? Category::Count : Category::Sale);
    const int count = end - first;
    const int columns = std::clamp(width / rowHeight, 1, count);
    const int rows = (count + columns - 1) / columns;
    const int perRow = (count + rows - 1) / rows;
    FittedTabs tabs;
    for (int start = first; start < end; start += perRow) {
        const int columnsHere = (std::min)(perRow, end - start);
        for (int i = 0; i < columnsHere; ++i) {
            const int left = width * i / columnsHere;
            tabs.cells[start + i] = {left, tabs.height, width * (i + 1) / columnsHere - left, rowHeight};
        }
        tabs.height += rowHeight;
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

inline constexpr int QuantityThumbHalfWidth = 3;

inline Box QuantityThumb(Box slider, int position) {
    return {position - QuantityThumbHalfWidth, slider.y + slider.height / 2 - 7,
            2 * QuantityThumbHalfWidth + 1, 14};
}

inline Box QuantitySliderHitArea(Box slider) {
    // The thumb is centered on the track endpoints and protrudes on both sides.
    constexpr int padding = QuantityThumbHalfWidth + 4;
    return {slider.x - padding, slider.y, slider.width + 2 * padding, slider.height};
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
