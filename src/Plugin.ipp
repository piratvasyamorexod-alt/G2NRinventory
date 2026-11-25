#pragma once

#include "inventory_layout.hpp"
#include "mouse_win32.hpp"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace GOTHIC_NAMESPACE {
namespace {

using InventoryLayout::Box;
using InventoryLayout::Category;
using InventoryLayout::Entry;
using InventoryLayout::Grid;

constexpr unsigned long Bronze = 0xFFDDB67D;
constexpr unsigned long Paper = 0xFFF0DFBB;
constexpr unsigned long Bright = 0xFFFFEAC4;
constexpr unsigned long Rule = 0xFF655740;
constexpr unsigned long Equipped = 0xFFB7C98E;
constexpr unsigned long Missing = 0xFFFFB797;
constexpr int CategoryCount = static_cast<int>(Category::Count);

Category Classify(const oCItem& item) {
    if (item.flags & ITM_FLAG_SHIELD) return Category::Armor;
    if (item.mainflag & (ITM_CAT_NF | ITM_CAT_FF)) return Category::Weapons;
    if (item.mainflag & ITM_CAT_ARMOR) return Category::Armor;
    if (item.mainflag & ITM_CAT_MUN) return Category::Ammo;
    if (item.mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) return Category::Magic;
    if (item.mainflag & (ITM_CAT_FOOD | ITM_CAT_POTION)) return Category::Supplies;
    if (item.mainflag & ITM_CAT_DOCS) return Category::Documents;
    return Category::Other;
}

int WeaponDamage(oCItem& item) {
    return item.damageTotal > 0 ? item.damageTotal : (std::max)(0, item.GetFullDamage());
}

bool PersonalInventory(const oCItemContainer* container) {
    if (!player || container != &player->inventory2 || !screen || !screen->GetFont()
        || screen->psizex < 480 || screen->psizey < 360 || zCMenu::GetActive()) return false;
    auto& inv = player->inventory2;
    if (!inv.IsOpen() || !inv.IsActive() || inv.IsPassive() || inv.GetMode() != INV_MODE_DEFAULT
        || !inv.contents || !inv.rndWorld) return false;
    const auto& containers = oCItemContainer::contList;
    for (auto* node = containers.GetNextInList(); node; node = node->GetNextInList()) {
        auto* other = node->GetData();
        if (other && other != &inv && other->IsOpen()) return false;
    }
    return true;
}

void Place(zCView& view, Box box) {
    view.pposx = box.x;
    view.pposy = box.y;
    view.psizex = box.width;
    view.psizey = box.height;
}

zCOLOR Ink(unsigned long value) {
    zCOLOR color;
    color.dword = value;
    return color;
}

void InitSprite(zCView& view, const char* name, int alpha = 255) {
    view.InsertBack(zSTRING(name));
    view.SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
    view.SetTransparency(alpha);
}

struct DamageLabel { const wchar_t* ru; const char* en; };
constexpr std::array<DamageLabel, oEDamageIndex_MAX> DamageLabels{{
    {L"Барьер", "Barrier"}, {L"Дробящий", "Blunt"}, {L"Рубящий", "Edge"}, {L"Огонь", "Fire"},
    {L"Отбрасывание", "Fly"}, {L"Магия", "Magic"}, {L"Колющий", "Point"}, {L"Падение", "Fall"}
}};
constexpr std::array<DamageLabel, NPC_ATR_MAX> AttributeLabels{{
    {L"Здоровье", "Health"}, {L"Макс. здоровье", "Max health"}, {L"Мана", "Mana"}, {L"Макс. мана", "Max mana"},
    {L"Сила", "Strength"}, {L"Ловкость", "Dexterity"}, {L"Регенерация здоровья", "Health regeneration"},
    {L"Регенерация маны", "Mana regeneration"}
}};
constexpr std::array<DamageLabel, CategoryCount> CategoryLabels{{
    {L"Все", "All"}, {L"Оружие", "Weapons"}, {L"Броня", "Armor"}, {L"Снаряды", "Ammo"},
    {L"Магия", "Magic"}, {L"Припасы", "Supplies"}, {L"Записи", "Notes"}, {L"Разное", "Other"}
}};

enum class Command { None, Use, Close };

class InventoryPanel {
    zCView panel{0, 0, 8192, 8192};
    zCView textView{0, 0, 8192, 8192};
    zCView solid{0, 0, 8, 8};
    zCView preview{0, 0, 8, 8};
    zCView cursor{0, 0, 8, 8};
    Box bounds, details, close, tabs, listBar, detailBar;
    Grid grid;
    std::vector<Entry> entries;
    std::vector<oCItem*> nativeItems;
    std::array<int, CategoryCount> counts{};
    Category category = Category::All;
    int inset = 0, lineHeight = 0, firstRow = 0, detailTop = 0, detailCount = 0, detailRows = 0;
    int selected = -1;
    oCItem* selectedItem = nullptr;
    int selectedInstance = -1;
    oCItem* lastClicked = nullptr;
    int lastInstance = -1;
    ULONGLONG lastClickAt = 0;
    oCItem* previousDetail = nullptr;
    int previousDetailInstance = -1;
    bool useSelection = false, cyrillic = true;

    std::string Label(const wchar_t* ru, const char* en) const {
        if (!cyrillic) return en;
        const int bytes = WideCharToMultiByte(1251, 0, ru, -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1) return en;
        std::string result(bytes, '\0');
        WideCharToMultiByte(1251, 0, ru, -1, result.data(), bytes, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    static void Normalize(std::string& text) {
        std::replace(text.begin(), text.end(), '\xA8', '\xC5');
        std::replace(text.begin(), text.end(), '\xB8', '\xE5');
    }

    int Width(const std::string& text) { return panel.GetFont()->GetFontX(zSTRING(text.c_str())); }

    void Text(int x, int y, std::string text, int width, unsigned long ink = Paper, bool right = false) {
        if (text.empty() || width <= 0) return;
        if (cyrillic) Normalize(text);
        if (Width(text) > width) {
            const int dots = Width("...");
            if (dots > width) return;
            while (!text.empty() && Width(text) + dots > width) text.pop_back();
            text += "...";
        }
        if (right) x += width - Width(text);
        const zSTRING value(text.c_str());
        auto color = Ink(ink);
        auto shadow = Ink(0xFF0A0907);
        textView.CreateText(textView.anx(x - bounds.x), textView.any(y - bounds.y), value, 0, color, FALSE, TRUE);
        textView.CreateText(textView.anx(x - bounds.x + 1), textView.any(y - bounds.y + 1), value, 0, shadow, FALSE, TRUE);
    }

    void Fill(Box box, unsigned long color) {
        if (box.width <= 0 || box.height <= 0) return;
        Place(solid, box);
        solid.SetColor(Ink(color));
        solid.Blit();
    }

    void Frame(Box box, unsigned long color) {
        Fill({box.x, box.y, box.width, 1}, color);
        Fill({box.x, box.Bottom() - 1, box.width, 1}, color);
        Fill({box.x, box.y + 1, 1, box.height - 2}, color);
        Fill({box.Right() - 1, box.y + 1, 1, box.height - 2}, color);
    }

    Box Tab(int index) const {
        const int left = tabs.x + index * tabs.width / CategoryCount;
        return {left, tabs.y, tabs.x + (index + 1) * tabs.width / CategoryCount - left, tabs.height};
    }

    void ScrollBar(Box bar, int visible, int count, int top) {
        if (count <= visible || bar.height <= 0 || visible <= 0) return;
        const int thumb = std::clamp(bar.height * visible / count, (std::min)(12, bar.height), bar.height);
        const int y = bar.y + (bar.height - thumb) * top / (count - visible);
        Fill({bar.x + bar.width / 2, bar.y, 1, bar.height}, Rule);
        Fill({bar.x + bar.width / 2 - 1, y, 3, thumb}, Bronze);
    }

    int ItemAt(POINT p) const { return grid.ItemAt(p.x, p.y, firstRow, static_cast<int>(entries.size())); }

    void SetSelection(int index, bool reveal) {
        const int count = static_cast<int>(entries.size());
        if (count <= 0) {
            selected = -1;
            selectedItem = nullptr;
            selectedInstance = -1;
            return;
        }
        selected = std::clamp(index, 0, count - 1);
        selectedItem = Item(selected);
        selectedInstance = selectedItem->GetInstance();
        if (reveal) {
            const int row = selected / grid.columns;
            firstRow = std::clamp(firstRow, (std::max)(0, row - grid.rows + 1), row);
        }
    }

    struct Line { std::string text; unsigned long ink; };

    void Wrap(std::vector<Line>& lines, std::string_view source, unsigned long ink) {
        std::string line;
        for (std::size_t start = 0; start < source.size();) {
            const auto end = source.find_first_of(" \t\r\n", start);
            const auto word = source.substr(start, end == std::string_view::npos ? source.size() - start : end - start);
            if (!word.empty()) {
                const auto next = line.empty() ? std::string(word) : line + " " + std::string(word);
                if (!line.empty() && Width(next) > details.width) {
                    lines.push_back({std::move(line), ink});
                    line.assign(word);
                } else {
                    line = next;
                }
            }
            if (end == std::string_view::npos) break;
            if (source[end] == '\n' && !line.empty()) {
                lines.push_back({std::move(line), ink});
                line.clear();
            }
            start = end + 1;
        }
        if (!line.empty()) lines.push_back({std::move(line), ink});
    }

    std::vector<Line> ItemDetails(oCItem& item) {
        std::vector<Line> lines;
        const auto add = [&](const wchar_t* ru, const char* en, int value, unsigned long ink = Paper) {
            lines.push_back({Label(ru, en) + "  " + std::to_string(value), ink});
        };
        const std::string name = item.name.ToChar();
        Wrap(lines, name, Bright);
        if (item.flags & ITM_FLAG_ACTIVE) {
            lines.push_back({Label(L"Экипировано", "Equipped"), Equipped});
        }
        const int classId = static_cast<int>(Classify(item));
        const auto& categoryLabel = CategoryLabels[classId];
        lines.push_back({Label(categoryLabel.ru, categoryLabel.en), Bronze});
        lines.push_back({});
        if (classId == static_cast<int>(Category::Weapons)) {
            add(L"Урон", "Damage", WeaponDamage(item));
            if (item.mainflag & ITM_CAT_NF && item.range > 0) add(L"Дальность удара", "Melee reach", item.range);
            for (int i = 0; i < oEDamageIndex_MAX; ++i) {
                if (item.damage[i]) add(DamageLabels[i].ru, DamageLabels[i].en, item.damage[i]);
            }
        }
        bool hasProtection = false;
        for (int i = 0; i < oEDamageIndex_MAX; ++i) {
            if (!item.protection[i]) continue;
            if (!std::exchange(hasProtection, true)) lines.push_back({Label(L"Защита", "Protection"), Bronze});
            add(DamageLabels[i].ru, DamageLabels[i].en, item.protection[i]);
        }
        bool hasRequirements = false;
        for (int i = 0; i < ITM_COND_MAX; ++i) {
            const int attribute = item.cond_atr[i], required = item.cond_value[i];
            if (required <= 0 || attribute < 0 || attribute >= NPC_ATR_MAX) continue;
            if (!std::exchange(hasRequirements, true)) {
                lines.push_back({Label(L"Требования", "Requirements"), Bronze});
            }
            const auto& label = AttributeLabels[attribute];
            const int current = player->GetAttribute(attribute);
            add(label.ru, label.en, required, current < required ? Missing : Paper);
        }
        if (item.mag_circle > 0) add(L"Круг магии", "Circle of magic", item.mag_circle);
        for (int i = 0; i < ITM_COND_MAX; ++i) {
            const int attribute = item.change_atr[i], change = item.change_value[i];
            if (!change || attribute < 0 || attribute >= NPC_ATR_MAX) continue;
            const auto& label = AttributeLabels[attribute];
            lines.push_back({Label(label.ru, label.en) + (change > 0 ? "  +" : "  ") + std::to_string(change), Paper});
        }
        const std::string description = item.GetDescription().ToChar();
        if (!description.empty() && description != name) {
            lines.push_back({});
            Wrap(lines, description, Paper);
        }
        for (int i = 0; i < ITM_TEXT_MAX; ++i) {
            const std::string text = item.GetText(i).ToChar();
            if (text.empty() || text == name || text == description) continue;
            const int value = item.GetCount(i);
            Wrap(lines, value != 0 ? text + "  " + std::to_string(value) : text, Paper);
        }
        lines.push_back({});
        if (item.amount > 1) add(L"В стопке", "Stack", item.amount);
        if (item.value > 0) add(L"Цена за штуку", "Value per item", item.value);
        if (item.weight > 0) add(L"Вес", "Weight", item.weight);
        return lines;
    }

    void DrawDetails(oCItem* item) {
        const int instance = item ? item->GetInstance() : -1;
        if (previousDetail != item || previousDetailInstance != instance) {
            previousDetail = item;
            previousDetailInstance = instance;
            detailTop = 0;
        }
        if (!item) {
            detailCount = 0;
            Text(details.x, details.y, Label(L"Наведите на предмет", "Point at an item"), details.width, Bronze);
            return;
        }
        const auto lines = ItemDetails(*item);
        detailCount = static_cast<int>(lines.size());
        detailTop = std::clamp(detailTop, 0, (std::max)(0, detailCount - detailRows));
        for (int i = detailTop; i < detailCount && i < detailTop + detailRows; ++i) {
            Text(details.x, details.y + (i - detailTop) * lineHeight, lines[i].text, details.width, lines[i].ink);
        }
        ScrollBar(detailBar, detailRows, detailCount, detailTop);
    }

public:
    InventoryPanel() {
        InitSprite(panel, "G2NRI_PANEL.TGA", 248);
        InitSprite(solid, "G2NRI_SOLID.TGA");
        InitSprite(cursor, "G2NRI_CURSOR.TGA");
    }

    bool Ready() const { return panel.backTex && solid.backTex; }

    oCItem* Item(int index) const {
        return index >= 0 && index < static_cast<int>(entries.size()) ? nativeItems[entries[index].nativeIndex] : nullptr;
    }

    bool SelectNative(oCNpcInventory& inv) const {
        if (!Item(selected)) return false;
        inv.selectedItem = entries[selected].nativeIndex;
        inv.CheckSelectedItem();
        return inv.GetSelectedItem() == Item(selected);
    }

    void Layout() {
        panel.SetFont(screen->GetFont());
        const int w = (std::min)(screen->psizex * 92 / 100, screen->psizey * 145 / 100);
        const int h = screen->psizey * 77 / 100;
        bounds = {(screen->psizex - w) / 2, (screen->psizey - h) / 2, w, h};
        Place(panel, bounds);
        Place(textView, bounds);
        textView.SetFont(screen->GetFont());
        inset = std::clamp(screen->psizey / 48, 10, 26);
        lineHeight = (std::max)(12, panel.GetFont()->GetFontY()) + 4;
        const int top = bounds.y + inset;
        close = {bounds.Right() - inset - Width("Esc") - 8, top, Width("Esc") + 8, lineHeight};
        tabs = {bounds.x + inset, top + lineHeight + 4, w - 2 * inset, lineHeight + 5};
        const int bodyY = tabs.Bottom() + 9, bodyBottom = bounds.Bottom() - inset - lineHeight - 5;
        const int divider = bounds.x + w * 62 / 100;
        const int scrollWidth = (std::max)(8, inset / 2);
        grid = InventoryLayout::MakeGrid({bounds.x + inset, bodyY, divider - bounds.x - 2 * inset - scrollWidth,
                                          bodyBottom - bodyY}, std::clamp(screen->psizey / 10, 60, 128), 4);
        listBar = {divider - inset - scrollWidth, bodyY, scrollWidth, grid.area.height};
        details = {divider + inset, bodyY, bounds.Right() - inset - divider - inset - scrollWidth, bodyBottom - bodyY};
        detailBar = {details.Right() + 3, bodyY, scrollWidth - 3, details.height};
        detailRows = (std::max)(1, details.height / lineHeight);
    }

    void Refresh(oCNpcInventory& inv) {
        nativeItems.clear();
        entries.clear();
        counts.fill(0);
        bool foundCyrillic = false;
        for (auto* node = inv.contents->GetNextInList(); node; node = node->GetNextInList()) {
            auto* item = node->GetData();
            const int index = static_cast<int>(nativeItems.size());
            nativeItems.push_back(item);
            if (!item || item->amount <= 0) continue;
            const auto categoryOfItem = Classify(*item);
            ++counts[static_cast<int>(Category::All)];
            ++counts[static_cast<int>(categoryOfItem)];
            if (!foundCyrillic) {
                const std::string_view name(item->name.ToChar());
                foundCyrillic = std::any_of(name.begin(), name.end(), [](unsigned char c) { return c >= 0xC0; });
            }
            if (category == Category::All || category == categoryOfItem) {
                entries.push_back({index, categoryOfItem, categoryOfItem == Category::Weapons ? WeaponDamage(*item) : 0});
            }
        }
        if (!nativeItems.empty()) cyrillic = foundCyrillic;
        InventoryLayout::Sort(entries);
        firstRow = std::clamp(firstRow, 0, grid.LastRow(static_cast<int>(entries.size())));
        int match = -1;
        if (selectedItem) {
            const auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) {
                return nativeItems[e.nativeIndex] == selectedItem && selectedItem->GetInstance() == selectedInstance;
            });
            if (it != entries.end()) match = static_cast<int>(it - entries.begin());
        }
        SetSelection(match >= 0 ? match : selected, false);
        if (useSelection) SelectNative(inv);
    }

    void ChangeCategory(int delta, oCNpcInventory& inv) {
        const int next = (static_cast<int>(category) + delta + CategoryCount) % CategoryCount;
        if (next == static_cast<int>(category)) return;
        category = static_cast<Category>(next);
        firstRow = detailTop = 0;
        selected = -1;
        selectedItem = lastClicked = nullptr;
        useSelection = true;
        Refresh(inv);
    }

    void MoveSelection(int dx, int dy, oCNpcInventory& inv) {
        lastClicked = nullptr;
        useSelection = true;
        SetSelection((std::max)(0, selected) + dx + dy * grid.columns, true);
        SelectNative(inv);
    }

    void Scroll(POINT p, int wheel) {
        if (!wheel) return;
        lastClicked = nullptr;
        if (details.Hit(p.x, p.y) || detailBar.Hit(p.x, p.y)) {
            useSelection = true;
            detailTop = std::clamp(detailTop - 3 * wheel, 0, (std::max)(0, detailCount - detailRows));
        } else if (bounds.Hit(p.x, p.y)) {
            firstRow = std::clamp(firstRow - wheel, 0, grid.LastRow(static_cast<int>(entries.size())));
        }
    }

    Command Click(POINT p, oCNpcInventory& inv) {
        if (close.Hit(p.x, p.y)) return Command::Close;
        for (int i = 0; i < CategoryCount; ++i) {
            if (!Tab(i).Hit(p.x, p.y)) continue;
            ChangeCategory(i - static_cast<int>(category), inv);
            return Command::None;
        }
        if (listBar.Hit(p.x, p.y)) {
            const int last = grid.LastRow(static_cast<int>(entries.size()));
            if (last > 0) firstRow = std::clamp(MulDiv(p.y - listBar.y, last, (std::max)(1, listBar.height - 1)), 0, last);
            lastClicked = nullptr;
            return Command::None;
        }
        if (detailBar.Hit(p.x, p.y)) {
            useSelection = true;
            lastClicked = nullptr;
            const int last = (std::max)(0, detailCount - detailRows);
            detailTop = std::clamp(MulDiv(p.y - detailBar.y, last, (std::max)(1, detailBar.height - 1)), 0, last);
            return Command::None;
        }
        const int index = ItemAt(p);
        auto* clicked = Item(index);
        if (!clicked) {
            lastClicked = nullptr;
            return Command::None;
        }
        useSelection = true;
        SetSelection(index, false);
        SelectNative(inv);
        const auto time = GetTickCount64();
        const int instance = clicked->GetInstance();
        const bool twice = clicked == lastClicked && instance == lastInstance && time - lastClickAt <= GetDoubleClickTime();
        lastClicked = twice ? nullptr : clicked;
        lastInstance = instance;
        lastClickAt = time;
        return twice ? Command::Use : Command::None;
    }

    void Draw(oCNpcInventory& inv, std::optional<POINT> point) {
        Layout();
        Refresh(inv);
        textView.ClrPrintwin();
        const int hover = point ? ItemAt(*point) : -1;
        const auto previousAlpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        panel.Blit();
        const int left = bounds.x + inset;
        Text(left, close.y, Label(L"ИНВЕНТАРЬ", "INVENTORY"), close.x - left, Bronze);
        Text(close.x, close.y, "Esc", close.width, point && close.Hit(point->x, point->y) ? Bright : Bronze, true);
        Fill({tabs.x, tabs.Bottom() - 1, tabs.width, 1}, Rule);
        for (int i = 0; i < CategoryCount; ++i) {
            const auto cell = Tab(i);
            const auto& label = CategoryLabels[i];
            const auto name = Label(label.ru, label.en);
            const bool active = i == static_cast<int>(category);
            const int x = cell.x + (std::max)(2, (cell.width - Width(name)) / 2);
            Text(x, cell.y, name, cell.Right() - x - 1,
                 active || (point && cell.Hit(point->x, point->y)) ? Bright : counts[i] ? Bronze : Paper);
            if (active) Fill({cell.x + 5, cell.Bottom() - 2, cell.width - 10, 2}, Bronze);
        }
        Fill({details.x - inset, details.y, 1, details.height}, Rule);
        if (entries.empty()) {
            Text(grid.area.x, grid.area.y, Label(L"Здесь пока пусто", "No items in this category"), grid.area.width, Bronze);
        }
        const int first = firstRow * grid.columns, slots = grid.columns * grid.rows;
        for (int slot = 0; slot < slots; ++slot) {
            const int index = first + slot;
            const auto cell = grid.Cell(slot);
            auto* item = Item(index);
            const bool equipped = item && (item->flags & ITM_FLAG_ACTIVE);
            Fill(cell, equipped ? 0xFF292D1E : 0xFF1D2018);
            Frame(cell, index == hover ? Bright : index == selected && useSelection ? Bronze : Rule);
            if (!item) continue;
            if (equipped) Fill({cell.x + 3, cell.y + 3, 5, 5}, Equipped);
            if (item->amount > 1) {
                Text(cell.x + 3, cell.Bottom() - lineHeight, std::to_string(item->amount), cell.width - 7, Bright, true);
            }
        }
        ScrollBar(listBar, grid.rows, (static_cast<int>(entries.size()) + grid.columns - 1) / grid.columns, firstRow);
        DrawDetails(hover >= 0 ? Item(hover) : useSelection ? Item(selected) : nullptr);
        const int footer = bounds.Bottom() - inset - lineHeight;
        Fill({left, footer - 5, tabs.width, 1}, Rule);
        Text(left, footer, Label(L"ЛКМ  Выбрать     Двойной клик / Enter  Использовать     Tab  Категория",
                                 "Click  Select     Double-click / Enter  Use     Tab  Category"), tabs.width, Bronze);
        zrenderer->SetAlphaBlendSource(previousAlpha);

        oCItem::SetLightingSwell(FALSE);
        for (int slot = 0; slot < slots; ++slot) {
            auto* item = Item(first + slot);
            if (!item || !item->GetVisual()) continue;
            const auto cell = grid.Cell(slot);
            Place(preview, {cell.x + 3, cell.y + 4, cell.width - 6, cell.height - 9});
            item->RenderItem(inv.rndWorld, &preview, 0.0f);
        }
        oCItem::SetLightingSwell(TRUE);
        textView.DrawItems();
        if (point && cursor.backTex) {
            CURSORINFO info{};
            info.cbSize = sizeof(info);
            if (!GetCursorInfo(&info) || !(info.flags & CURSOR_SHOWING) || !info.hCursor) {
                const int size = std::clamp(screen->psizey / 42, 16, 48);
                zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
                Place(cursor, {static_cast<int>(point->x), static_cast<int>(point->y), size, size});
                cursor.Blit();
                zrenderer->SetAlphaBlendSource(previousAlpha);
            }
        }
    }
};

void ApplyMouseAcquisition() {
    reinterpret_cast<int(__cdecl*)()>(0x004D3D30)();
}

class InventorySession {
    InventoryMouse::MouseEvents events;
    InventoryMouse::InventoryPointer cursor;
    HWND window = InventoryMouse::WindowRoot(hWndApp);
    RECT client{};
    bool active = false, listening = false, restoreMouse = false, center = true;
    bool leftHeld = false, rightHeld = false;

    void ReleaseMouse() {
        if (!std::exchange(active, false)) return;
        cursor.Leave();
        if (std::exchange(restoreMouse, false) && zinput && !zinput->GetDeviceEnabled(zINPUT_MOUSE)) {
            zinput->SetDeviceEnabled(zINPUT_MOUSE, TRUE);
            ApplyMouseAcquisition();
        }
    }

    std::optional<POINT> ScreenPoint(std::optional<POINT> position) const {
        if (!position || !PtInRect(&client, *position)) return std::nullopt;
        return POINT{MulDiv(position->x - client.left, screen->psizex, client.right - client.left),
                     MulDiv(position->y - client.top, screen->psizey, client.bottom - client.top)};
    }

    std::optional<POINT> ClickPoint(std::optional<POINT> event, int button, bool& held) const {
        const bool down = (GetAsyncKeyState(button) & 0x8000) != 0;
        const bool edge = down && !held;
        held = down;
        return event ? ScreenPoint(event) : edge ? Position() : std::nullopt;
    }

public:
    InventoryPanel panel;

    ~InventorySession() { ReleaseMouse(); }

    std::optional<POINT> Position() const {
        POINT point{};
        if (!active || !InventoryMouse::HasFocus(window) || !GetCursorPos(&point)) return std::nullopt;
        return ScreenPoint(point);
    }

    Command Poll(oCNpcInventory& inv) {
        if (events.TakeFocusLoss() || !InventoryMouse::HasFocus(window)) {
            ReleaseMouse();
            events.Discard();
        }
        if (!zinput || !InventoryMouse::HasFocus(window) || !InventoryMouse::ClientBounds(window, client)) {
            return Command::None;
        }
        if (!active) {
            if (!listening) listening = events.Start(window);
            restoreMouse = zinput->GetDeviceEnabled(zINPUT_MOUSE) != 0;
            if (restoreMouse) {
                zinput->SetDeviceEnabled(zINPUT_MOUSE, FALSE);
                ApplyMouseAcquisition();
            }
            cursor.Enter(window, client, std::exchange(center, false));
            leftHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            rightHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            active = true;
        }
        const auto input = events.Take();
        if (ClickPoint(input.rightClick, VK_RBUTTON, rightHeld)) return Command::Close;
        const auto click = ClickPoint(input.leftClick, VK_LBUTTON, leftHeld);
        const auto wheel = ScreenPoint(input.wheelPosition);
        if (!click && !(wheel && input.wheel)) return Command::None;
        panel.Layout();
        panel.Refresh(inv);
        if (click) return panel.Click(*click, inv);
        panel.Scroll(*wheel, input.wheel);
        return Command::None;
    }
};

std::unique_ptr<InventorySession> session;

int __fastcall InventoryKey(oCNpcInventory*, void*, int);
auto KeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::HandleEvent), &InventoryKey, Union::HookType::Hook_Detours);

void UseSelected(oCNpcInventory& inv) {
    if (!session || !zinput || !PersonalInventory(&inv) || !session->panel.SelectNative(inv)) return;
    auto* input = zinput;
    // The native inventory recognizes its mouse action when the input edge is set.
    constexpr int button = MOUSE_BUTTONLEFT;
    const int pressed = input->KeyPressed(button);
    if (!pressed) input->SetKey(button, TRUE);
    KeyHook(&inv, nullptr, button);
    if (!pressed && zinput == input) {
        input->SetKey(button, FALSE);
        input->KeyToggled(button);
    }
}

int __fastcall InventoryKey(oCNpcInventory* inv, void* edx, int key) {
    if (!session || !PersonalInventory(inv) || !zinput) return KeyHook(inv, edx, key);
    auto& panel = session->panel;
    const auto logical = zinput->GetFirstBindedLogicalKey(key);
    if (key == KEY_ESCAPE) {
        player->CloseInventory();
        return TRUE;
    }
    if (logical == zLOGICKEY_INVENTORY) return KeyHook(inv, edx, key);
    if (key >= MIN_MOUSE_BTNS && key < MAX_MOUSE_BTNS) return TRUE;
    if ((logical == zLOGICKEY_ACTION || logical == zLOGICKEY_SMOVE) && !panel.SelectNative(*inv)) return TRUE;
    switch (key) {
    case KEY_TAB: panel.ChangeCategory(1, *inv); return TRUE;
    case KEY_RETURN:
    case KEY_NUMPADENTER: UseSelected(*inv); return TRUE;
    case KEY_LEFT: panel.MoveSelection(-1, 0, *inv); return TRUE;
    case KEY_RIGHT: panel.MoveSelection(1, 0, *inv); return TRUE;
    case KEY_UP: panel.MoveSelection(0, -1, *inv); return TRUE;
    case KEY_DOWN: panel.MoveSelection(0, 1, *inv); return TRUE;
    default: break;
    }
    switch (logical) {
    case zLOGICKEY_LEFT:
    case zLOGICKEY_STRAFELEFT: panel.MoveSelection(-1, 0, *inv); return TRUE;
    case zLOGICKEY_RIGHT:
    case zLOGICKEY_STRAFERIGHT: panel.MoveSelection(1, 0, *inv); return TRUE;
    case zLOGICKEY_UP: panel.MoveSelection(0, -1, *inv); return TRUE;
    case zLOGICKEY_DOWN: panel.MoveSelection(0, 1, *inv); return TRUE;
    default: return KeyHook(inv, edx, key);
    }
}

void __fastcall DrawInventory(oCItemContainer*, void*);
auto DrawHook = Union::CreateHook(
    SIGNATURE_OF(&oCItemContainer::Draw), &DrawInventory, Union::HookType::Hook_Detours);

void __fastcall DrawInventory(oCItemContainer* container, void* edx) {
    if (session && PersonalInventory(container)) {
        session->panel.Draw(player->inventory2, session->Position());
    } else {
        DrawHook(container, edx);
    }
}

void __fastcall CloseInventory(oCNpcInventory*, void*);
auto CloseHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::Close), &CloseInventory, Union::HookType::Hook_Detours);

void __fastcall CloseInventory(oCNpcInventory* inv, void* edx) {
    if (player && inv == &player->inventory2) session.reset();
    CloseHook(inv, edx);
}

void __cdecl PollInventory();
auto PollHook = Union::CreateHook(
    reinterpret_cast<void*>(0x007A55C0), &PollInventory, Union::HookType::Hook_Detours);

void __cdecl PollInventory() {
    PollHook();
    if (!player || !PersonalInventory(&player->inventory2)) {
        session.reset();
        return;
    }
    auto& inv = player->inventory2;
    if (!session) {
        auto next = std::make_unique<InventorySession>();
        if (!next->panel.Ready()) return;
        // The engine owns these stacks. Materialize them once so every category can be listed.
        if (inv.GetNumItemsInPackString() > 0) inv.UnpackAllItems();
        next->panel.Layout();
        next->panel.Refresh(inv);
        session = std::move(next);
    }
    const auto result = session->Poll(inv);
    if (result == Command::Close) player->CloseInventory();
    if (result == Command::Use) UseSelected(inv);
}

} // namespace
} // namespace GOTHIC_NAMESPACE
