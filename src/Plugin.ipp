#pragma once

#include "inventory_layout.hpp"
#include "inventory_history.hpp"
#include "inventory_archive.hpp"
#include "inventory_trade_rules.hpp"
#include "mouse_win32.hpp"

#include <array>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
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

#include "inventory_tracking.hpp"

Category Classify(const oCItem& item) {
    if (item.flags & ITM_FLAG_SHIELD) return Category::Armor;
    if (item.mainflag & (ITM_CAT_NF | ITM_CAT_FF)) return Category::Weapons;
    if (item.mainflag & ITM_CAT_ARMOR) return Category::Armor;
    if (item.mainflag & ITM_CAT_MUN) return Category::Ammo;
    if (item.mainflag & ITM_CAT_POTION) return Category::Potions;
    if (item.mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) return Category::Magic;
    if (item.mainflag & ITM_CAT_FOOD) return Category::Supplies;
    if (item.mainflag & ITM_CAT_DOCS) return Category::Documents;
    return Category::Other;
}

bool IsQuestItem(const oCItem& item) { return (item.flags & ITM_FLAG_MI) != 0; }

bool MissingRequirements(const oCItem& item) {
    constexpr auto equipment = ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_ARMOR | ITM_CAT_RUNE;
    constexpr auto accessories = ITM_FLAG_SHIELD | ITM_FLAG_RING | ITM_FLAG_AMULET | ITM_FLAG_BELT;
    if ((item.flags & ITM_FLAG_ACTIVE) || !(item.mainflag & equipment || item.flags & accessories)) return false;
    if (item.mag_circle > 0 && player->GetTalentSkill(oCNpcTalent::NPC_TAL_MAGE) < item.mag_circle) return true;
    for (int i = 0; i < ITM_COND_MAX; ++i) {
        const int attribute = item.cond_atr[i];
        if (attribute > 0 && attribute < NPC_ATR_MAX && player->GetAttribute(attribute) < item.cond_value[i]) return true;
    }
    return false;
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

void EditQuantityDigits(std::string& digits, bool& replaceText, int key) {
    constexpr std::array<int, 10> numpad{KEY_NUMPAD0, KEY_NUMPAD1, KEY_NUMPAD2, KEY_NUMPAD3,
        KEY_NUMPAD4, KEY_NUMPAD5, KEY_NUMPAD6, KEY_NUMPAD7, KEY_NUMPAD8, KEY_NUMPAD9};
    int digit = -1;
    if (key >= KEY_1 && key <= KEY_0) digit = (key - KEY_1 + 1) % 10;
    else if (const auto it = std::find(numpad.begin(), numpad.end(), key); it != numpad.end()) {
        digit = static_cast<int>(it - numpad.begin());
    }
    if (digit < 0 && key != KEY_BACK && key != KEY_DELETE) return;
    if (std::exchange(replaceText, false) || key == KEY_DELETE) digits.clear();
    else if (key == KEY_BACK && !digits.empty()) digits.pop_back();
    if (digit >= 0 && digits.size() < 10) digits.push_back(static_cast<char>('0' + digit));
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

#include "inventory_hotbar.hpp"

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
    {L"Последнее", "Recent"}, {L"Все", "All"}, {L"Оружие", "Weapons"}, {L"Броня", "Armor"}, {L"Снаряды", "Ammo"},
    {L"Магия", "Magic"}, {L"Зелья", "Potions"}, {L"Еда", "Food"}, {L"Записи", "Notes"},
    {L"Разное", "Other"}, {L"На продажу", "For sale"}
}};
constexpr std::array<const char*, CategoryCount> CategoryTextures{
    "G2NRI_TAB_RECENT.TGA", "G2NRI_TAB_ALL.TGA", "G2NRI_TAB_WEAPONS.TGA", "G2NRI_TAB_ARMOR.TGA",
    "G2NRI_TAB_AMMO.TGA", "G2NRI_TAB_MAGIC.TGA", "G2NRI_TAB_POTIONS.TGA", "G2NRI_TAB_SUPPLIES.TGA",
    "G2NRI_TAB_DOCUMENTS.TGA", "G2NRI_TAB_OTHER.TGA", "G2NRI_TAB_SALE.TGA"
};

struct InventoryPosition {
    Category category = Category::Recent;
    int firstRow = 0;
};
InventoryPosition lastInventoryPosition;

int CategoryTabHeight() { return std::clamp(screen->psizey / 32, 26, 40); }

enum class Command { None, Use, Close };

class InventoryPanel {
    friend class TradePanel;
    zCView panel{0, 0, 8192, 8192};
    zCView textView{0, 0, 8192, 8192};
    zCView tooltipText{0, 0, 8192, 8192};
    zCView quantityText{0, 0, 8192, 8192};
    zCView solid{0, 0, 8, 8};
    zCView preview{0, 0, 8, 8};
    zCView cursor{0, 0, 8, 8};
    zCView lockedMarker{0, 0, 8, 8};
    zCView unreadMarker{0, 0, 8, 8};
    zCView equippedMarker{0, 0, 8, 8};
    zCView questMarker{0, 0, 8, 8};
    std::array<zCView, CategoryCount> tabIcons;
    Box bounds, close, listBar;
    InventoryLayout::FittedTabs tabs;
    Grid grid;
    std::vector<Entry> entries;
    std::vector<oCItem*> nativeItems;
    std::array<int, CategoryCount> counts{};
    Category category = lastInventoryPosition.category;
    int inset = 0, lineHeight = 0, firstRow = lastInventoryPosition.firstRow;
    int selected = -1;
    oCItem* selectedItem = nullptr;
    int selectedInstance = -1;
    oCItem* tooltipItem = nullptr;
    int tooltipInstance = -1, tooltipTop = 0, tooltipMaxTop = 0;
    bool useSelection = false, cyrillic = true;

    struct QuantityEdit {
        oCItem* item;
        int instance, maximum, amount;
        POINT anchor;
        bool returning;
        bool typing = false, replaceText = false, dragging = false;
        std::string digits;
    };
    std::optional<QuantityEdit> quantity;

    struct QuantityBoxes { Box card, number, slider, confirm, cancel; };

    QuantityBoxes QuantityLayout() const {
        const int pad = 10, row = lineHeight + 6;
        const int width = std::clamp(bounds.width * 3 / 5, 240, 345);
        const Box viewport{2, 2, screen->psizex - 4, screen->psizey - 4};
        const Box card = InventoryLayout::PlaceTooltip(viewport, quantity->anchor.x, quantity->anchor.y,
                                                        width, 2 * pad + 5 * row, 14);
        const int body = width - 2 * pad, left = card.x + pad, top = card.y + pad;
        return {card, {left, top + 2 * row, body, row}, {left + 6, top + 3 * row, body - 12, row},
                {left, top + 4 * row, body / 2 - 3, row},
                {left + body / 2 + 3, top + 4 * row, body - body / 2 - 3, row}};
    }

    void MoveToSale(int instance, int amount, bool returning) {
        const int previous = history.ForSale(instance);
        history.SetForSale(instance, returning ? previous - amount
            : previous + (std::min)(amount, INT_MAX - previous));
    }

    void ConfirmQuantity() {
        const auto& edit = *quantity;
        if (edit.typing && edit.digits.empty()) return;
        const int amount = edit.typing ? InventoryLayout::QuantityFromText(edit.digits, edit.maximum) : edit.amount;
        MoveToSale(edit.instance, amount, edit.returning);
        quantity.reset();
    }

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

    void DrawText(zCView& view, int x, int y, std::string text, int width, unsigned long ink, bool right = false) {
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
        view.CreateText(view.anx(x - view.pposx), view.any(y - view.pposy), value, 0, color, FALSE, TRUE);
        view.CreateText(view.anx(x - view.pposx + 1), view.any(y - view.pposy + 1), value, 0, shadow, FALSE, TRUE);
    }

    void Text(int x, int y, std::string text, int width, unsigned long ink = Paper, bool right = false) {
        DrawText(textView, x, y, std::move(text), width, ink, right);
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

    void DrawMarker(zCView& sprite, int x, int y, int size) {
        if (!sprite.backTex) return;
        Place(sprite, {x, y, size, size});
        sprite.Blit();
    }

    static int MarkerSize(Box cell) { return std::clamp(cell.width / 3, 13, 24); }

    void DrawMissingVisualName(oCItem& item, int amount, Box cell) {
        if (item.GetVisual()) return;
        const int badgeSpace = IsQuestItem(item) && questMarker.backTex ? MarkerSize(cell) : 0;
        const int amountSpace = amount > 1 ? Width(std::to_string(amount)) : 0;
        Text(cell.x + 4 + badgeSpace, cell.Bottom() - lineHeight, item.name.ToChar(),
             cell.width - 10 - amountSpace - badgeSpace);
    }

    void DrawItemMarkers(oCItem& item, Box cell, zCView* upgrade = nullptr) {
        const int size = MarkerSize(cell);
        const bool equipped = (item.flags & ITM_FLAG_ACTIVE) != 0;
        if (equipped) DrawMarker(equippedMarker, cell.x + 2, cell.y + 2, size);
        if (upgrade) {
            const int x = cell.x + (equipped ? size + 3 : 2);
            const int arrowSize = equipped ? (std::min)(size, cell.width - 2 * size - 5) : size;
            if (arrowSize > 0) DrawMarker(*upgrade, x, cell.y + 2, arrowSize);
        }
        if (MissingRequirements(item)) DrawMarker(lockedMarker, cell.Right() - size - 2, cell.y + 2, size);
        else if (ReadableDocument(item) && !history.WasRead(item.GetInstance()))
            DrawMarker(unreadMarker, cell.Right() - size - 2, cell.y + 2, size);
        if (IsQuestItem(item)) DrawMarker(questMarker, cell.x + 2, cell.Bottom() - size - 2, size);
    }

    void DrawTabs(const InventoryLayout::FittedTabs& layout, Category current,
                  const std::array<int, CategoryCount>& available, std::optional<POINT> point) {
        for (int i = 0; i < CategoryCount; ++i) {
            const auto cell = layout.cells[i];
            if (cell.width <= 0) continue;
            const bool active = static_cast<Category>(i) == current;
            const bool hovered = point && cell.Hit(point->x, point->y);
            if (active || hovered) Fill({cell.x + 2, cell.y + 1, cell.width - 4, cell.height - 3}, 0xFF292D1E);
            if (active) Fill({cell.x + 3, cell.Bottom() - 2, cell.width - 6, 2}, Bronze);
            const int size = (std::min)(cell.width - 4, cell.height - 3);
            if (size <= 0) continue;
            auto& icon = tabIcons[i];
            icon.SetTransparency(active || hovered || available[i] ? 255 : 170);
            DrawMarker(icon, cell.x + (cell.width - size) / 2, cell.y + (cell.height - size) / 2 - 1, size);
        }
    }

    bool DrawTabTooltip(const InventoryLayout::FittedTabs& layout, POINT point) {
        const auto hovered = layout.Hit(point.x, point.y);
        if (!hovered) return false;
        const auto& label = CategoryLabels[static_cast<int>(*hovered)];
        const auto name = Label(label.ru, label.en);
        constexpr int pad = 7;
        const Box card = InventoryLayout::PlaceTooltip({1, 1, screen->psizex - 2, screen->psizey - 2},
            point.x, point.y, Width(name) + 2 * pad, lineHeight + 2 * pad, 13);
        Place(tooltipText, card);
        tooltipText.ClrPrintwin();
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        Fill(card, 0xFF202318);
        Frame(card, Rule);
        zrenderer->SetAlphaBlendSource(alpha);
        DrawText(tooltipText, card.x + pad, card.y + pad, name, card.width - 2 * pad, Bright);
        tooltipText.DrawItems();
        return true;
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

    void Wrap(std::vector<Line>& lines, std::string_view source, int width, unsigned long ink) {
        std::string line;
        for (std::size_t start = 0; start < source.size();) {
            const auto end = source.find_first_of(" \t\r\n", start);
            const auto word = source.substr(start, end == std::string_view::npos ? source.size() - start : end - start);
            if (!word.empty()) {
                const auto next = line.empty() ? std::string(word) : line + " " + std::string(word);
                if (!line.empty() && Width(next) > width) {
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

    std::vector<Line> ItemDetails(oCItem& item, int amount, int width, bool showValue) {
        std::vector<Line> lines;
        const auto wrap = [&](std::string text, unsigned long ink = Paper) {
            if (cyrillic) Normalize(text);
            Wrap(lines, text, width, ink);
        };
        const auto add = [&](const wchar_t* ru, const char* en, int value, unsigned long ink = Paper) {
            wrap(Label(ru, en) + "  " + std::to_string(value), ink);
        };
        const std::string name = item.name.ToChar();
        wrap(name, Bright);
        if (IsQuestItem(item)) wrap(Label(L"Квестовый предмет", "Quest item"), Bronze);
        if (item.flags & ITM_FLAG_ACTIVE) {
            wrap(Label(L"Экипировано", "Equipped"), Equipped);
        }
        if (ReadableDocument(item)) {
            if (history.WasRead(item.GetInstance())) wrap(Label(L"Прочитано", "Read"), Paper);
            else wrap(Label(L"Не прочитано", "Unread"), Bronze);
        }
        const bool weapon = Classify(item) == Category::Weapons;
        const int damage = weapon ? WeaponDamage(item) : 0;
        if (weapon) {
            const auto parts = std::count_if(std::begin(item.damage), std::end(item.damage), [](int value) { return value != 0; });
            std::string total = Label(L"Урон", "Damage") + "  " + std::to_string(damage);
            for (int i = 0; i < oEDamageIndex_MAX; ++i) {
                if (parts == 1 && damage != 0 && item.damage[i] == damage) {
                    total += " (" + Label(DamageLabels[i].ru, DamageLabels[i].en) + ")";
                }
            }
            wrap(std::move(total));
            for (int i = 0; i < oEDamageIndex_MAX; ++i) {
                if (item.damage[i] && (parts != 1 || item.damage[i] != damage)) {
                    add(DamageLabels[i].ru, DamageLabels[i].en, item.damage[i]);
                }
            }
            if (item.mainflag & ITM_CAT_NF && item.range > 0) add(L"Дальность удара", "Melee reach", item.range);
        }
        bool hasProtection = false;
        for (int i = 0; i < oEDamageIndex_MAX; ++i) {
            if (!item.protection[i]) continue;
            if (!std::exchange(hasProtection, true)) lines.push_back({Label(L"Защита", "Protection"), Bronze});
            add(DamageLabels[i].ru, DamageLabels[i].en, item.protection[i]);
        }
        for (int i = 0; i < ITM_COND_MAX; ++i) {
            const int attribute = item.cond_atr[i], required = item.cond_value[i];
            if (required <= 0 || attribute <= 0 || attribute >= NPC_ATR_MAX) continue;
            const auto& label = AttributeLabels[attribute];
            const int current = player->GetAttribute(attribute);
            wrap(Label(L"Требуется", "Requires") + "  " + Label(label.ru, label.en) + " " + std::to_string(required),
                 current < required ? Missing : Paper);
        }
        if (item.mag_circle > 0) {
            const int circle = player->GetTalentSkill(oCNpcTalent::NPC_TAL_MAGE);
            add(L"Круг магии", "Circle of magic", item.mag_circle, circle < item.mag_circle ? Missing : Paper);
        }
        for (int i = 0; i < ITM_COND_MAX; ++i) {
            const int attribute = item.change_atr[i], change = item.change_value[i];
            if (!change || attribute < 0 || attribute >= NPC_ATR_MAX) continue;
            const auto& label = AttributeLabels[attribute];
            wrap(Label(label.ru, label.en) + (change > 0 ? "  +" : "  ") + std::to_string(change));
        }
        const std::string description = item.GetDescription().ToChar();
        if (description != name) wrap(description);
        const auto hasRequirement = [&](int attribute, int value) {
            for (int i = 0; i < ITM_COND_MAX; ++i) {
                if (item.cond_atr[i] == attribute && item.cond_value[i] == value) return true;
            }
            return false;
        };
        for (int i = 0; i < ITM_TEXT_MAX; ++i) {
            const std::string text = item.GetText(i).ToChar();
            if (text.empty() || text == name || text == description) continue;
            const int value = item.GetCount(i);
            const auto end = text.find_last_not_of(" :\t");
            const std::string_view label(text.data(), end == std::string::npos ? 0 : end + 1);
            if (value > 0 && ((weapon && value == damage && label == Label(L"Урон", "Damage"))
                || (value == item.value && (label == Label(L"Цена", "Value") || label == "Price"))
                || (hasRequirement(NPC_ATR_STRENGTH, value) && label == Label(L"Необходимо силы", "Strength required"))
                || (hasRequirement(NPC_ATR_DEXTERITY, value) && label == Label(L"Необходимо ловкости", "Dexterity required")))) continue;
            wrap(value != 0 ? text + "  " + std::to_string(value) : text);
        }
        if (showValue && item.value > 0) {
            if (amount > 1) add(L"Цена за штуку", "Value per item", item.value);
            else add(L"Цена", "Value", item.value);
        }
        if (item.weight > 0) add(L"Вес", "Weight", item.weight);
        return lines;
    }

    void DrawTooltip(oCItem* item, int amount, std::optional<POINT> point, const std::string& tradePrice = {}) {
        const int instance = item ? item->GetInstance() : -1;
        if (tooltipItem != item || tooltipInstance != instance) {
            tooltipItem = item;
            tooltipInstance = instance;
            tooltipTop = tooltipMaxTop = 0;
        }
        if (!item || !point) return;
        const int gap = std::clamp(screen->psizey / 64, 10, 24);
        const int pad = std::clamp(screen->psizey / 110, 6, 12);
        const int cardWidth = (std::min)(std::clamp(screen->psizex * 2 / 5, 260, 480), screen->psizex / 2 - gap - 2);
        const int maxWidth = cardWidth - 2 * pad;
        const int maxRows = (screen->psizey * 3 / 4 - 2 * pad) / lineHeight;
        auto lines = ItemDetails(*item, amount, maxWidth, tradePrice.empty());
        if (!tradePrice.empty()) lines.insert(lines.begin() + (lines.empty() ? 0 : 1), {tradePrice, Bronze});
        if (lines.empty() || maxRows < 3) return;
        int width = 0;
        for (const auto& line : lines) width = (std::max)(width, Width(line.text));
        const int count = static_cast<int>(lines.size());
        const bool scrolls = count > maxRows;
        const int rows = scrolls ? maxRows - 1 : count;
        const auto scrollHint = scrolls ? Label(L"Shift + колесо", "Shift + wheel") : std::string{};
        if (scrolls) width = (std::max)(width, Width(scrollHint));
        width = (std::min)(width, maxWidth);
        tooltipMaxTop = count - rows;
        tooltipTop = std::clamp(tooltipTop, 0, tooltipMaxTop);

        const Box card = InventoryLayout::PlaceTooltip({1, 1, screen->psizex - 2, screen->psizey - 2},
            point->x, point->y, width + 2 * pad, (rows + scrolls) * lineHeight + 2 * pad, gap);
        Place(tooltipText, card);
        tooltipText.ClrPrintwin();
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        Fill(card, Rule);
        Fill({card.x + 1, card.y + 1, card.width - 2, card.height - 2}, 0xFF202318);
        zrenderer->SetAlphaBlendSource(alpha);
        for (int row = 0; row < rows; ++row) {
            const auto& line = lines[tooltipTop + row];
            DrawText(tooltipText, card.x + pad, card.y + pad + row * lineHeight, line.text, width, line.ink);
        }
        if (scrolls) {
            DrawText(tooltipText, card.x + pad, card.y + pad + rows * lineHeight, scrollHint, width, Bronze);
        }
        tooltipText.DrawItems();
    }

    void DrawQuantitySlider(Box slider, int amount, int maximum) {
        const int center = slider.y + slider.height / 2;
        const int position = InventoryLayout::QuantityPosition(slider, amount, maximum);
        Fill({slider.x, center - 2, slider.width, 4}, Rule);
        Fill({slider.x, center - 2, position - slider.x + 1, 4}, Bronze);
        Fill(InventoryLayout::QuantityThumb(slider, position), Bright);
    }

    void DrawQuantity(std::optional<POINT> point) {
        const auto& edit = *quantity;
        const auto [card, number, slider, confirm, cancel] = QuantityLayout();
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        Fill(card, 0xFF202318);
        Frame(card, Bronze);
        const auto hit = [&](Box box) { return point && box.Hit(point->x, point->y); };
        Fill(number, 0xFF141710);
        Frame(number, edit.typing || hit(number) ? Bronze : Rule);
        const int amount = edit.typing ? InventoryLayout::QuantityFromText(edit.digits, edit.maximum) : edit.amount;
        DrawQuantitySlider(slider, amount, edit.maximum);
        Fill(confirm, 0xFF35351F);
        Frame(confirm, hit(confirm) ? Bright : Bronze);
        Frame(cancel, hit(cancel) ? Bright : Rule);
        zrenderer->SetAlphaBlendSource(alpha);

        Place(quantityText, card);
        quantityText.ClrPrintwin();
        const auto write = [&](Box area, const std::string& label, unsigned long ink, bool center = false) {
            const int x = area.x + (center ? (std::max)(4, (area.width - Width(label)) / 2) : 4);
            DrawText(quantityText, x, area.y + (area.height - lineHeight) / 2,
                     label, area.Right() - x - 4, ink);
        };
        const int row = number.height;
        write({number.x, number.y - 2 * row, number.width, row},
              edit.returning ? Label(L"Вернуть в инвентарь", "Return to inventory") : Label(L"Отложить на продажу", "Set aside for sale"), Bronze);
        write({number.x, number.y - row, number.width, row}, edit.item->name.ToChar(), Paper);
        const auto current = edit.typing ? edit.digits + "|" : std::to_string(amount);
        write(number, current + " / " + std::to_string(edit.maximum), Bright, true);
        write(confirm, edit.returning ? Label(L"Вернуть", "Return") : Label(L"Отложить", "Set aside"), Bright, true);
        write(cancel, Label(L"Отмена", "Cancel"), Paper, true);
        quantityText.DrawItems();
    }

public:
    void DrawCursor(std::optional<POINT> point) {
        if (!point || !cursor.backTex) return;
        CURSORINFO info{};
        info.cbSize = sizeof(info);
        if (GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) && info.hCursor) return;
        const int size = std::clamp(screen->psizey / 42, 16, 48);
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        Place(cursor, {static_cast<int>(point->x), static_cast<int>(point->y), size, size});
        cursor.Blit();
        zrenderer->SetAlphaBlendSource(alpha);
    }

    InventoryPanel() {
        InitSprite(panel, "G2NRI_PANEL.TGA", 248);
        InitSprite(solid, "G2NRI_SOLID.TGA");
        InitSprite(cursor, "G2NRI_CURSOR.TGA");
        InitSprite(lockedMarker, "G2NRI_LOCKED.TGA");
        InitSprite(unreadMarker, "G2NRI_UNREAD.TGA");
        InitSprite(equippedMarker, "G2NRI_EQUIPPED.TGA");
        InitSprite(questMarker, "G2NRI_QUEST.TGA");
        for (int i = 0; i < CategoryCount; ++i) InitSprite(tabIcons[i], CategoryTextures[i]);
    }

    bool Ready() const { return panel.backTex && solid.backTex; }

    oCItem* Item(int index) const {
        return index >= 0 && index < static_cast<int>(entries.size()) ? nativeItems[entries[index].nativeIndex] : nullptr;
    }

    int Amount(int index) const { return Item(index) ? entries[index].quantity : 0; }

    oCItem* PressSource(POINT point) const {
        return quantity || category == Category::Sale ? nullptr : Item(ItemAt(point));
    }

    bool IsPickingQuantity() const { return quantity.has_value(); }

    bool SelectNative(oCNpcInventory& inv) const {
        if (category == Category::Sale || quantity || !Item(selected)) return false;
        inv.selectedItem = entries[selected].nativeIndex;
        inv.CheckSelectedItem();
        return inv.GetSelectedItem() == Item(selected);
    }

    void Layout() {
        panel.SetFont(screen->GetFont());
        textView.SetFont(screen->GetFont());
        tooltipText.SetFont(screen->GetFont());
        quantityText.SetFont(screen->GetFont());
        inset = std::clamp(screen->psizey / 80, 8, 20);
        lineHeight = (std::max)(12, panel.GetFont()->GetFontY()) + 4;
        const int margin = std::clamp(screen->psizex / 72, 10, 36);
        const int target = std::clamp(screen->psizey / 14, 42, 128);
        const int gap = std::clamp(screen->psizey / 360, 2, 5);
        const int scrollWidth = (std::max)(8, inset / 2);
        const int width = (std::max)(430, 7 * (target + gap) - gap + 2 * inset + scrollWidth);
        const int w = (std::min)(screen->psizex - 2 * margin, width);
        tabs = InventoryLayout::FitCategoryTabs(w - 2 * inset, CategoryTabHeight(), true);
        const int bodyTop = inset + lineHeight + 4 + tabs.height + 7;
        const int footerSpace = inset + lineHeight + 11;
        const int hotbarTop = InventoryLayout::PlaceHotbar(screen->psizex, screen->psizey).bounds.y - gap - 3;
        const int available = (std::min)(screen->psizey * 77 / 100, hotbarTop - inset);
        grid = InventoryLayout::MakeGrid({inset, bodyTop, w - 2 * inset - scrollWidth,
                                          available - bodyTop - footerSpace}, target, gap);
        const int h = grid.area.Bottom() + footerSpace;
        bounds = InventoryLayout::PlacePanel(screen->psizex, screen->psizey, w, h, margin);
        bounds.y = (std::max)(inset, (std::min)(bounds.y, hotbarTop - h));
        Place(panel, bounds);
        Place(textView, bounds);
        grid.area.x += bounds.x;
        grid.area.y += bounds.y;
        const int top = bounds.y + inset;
        close = {bounds.Right() - inset - Width("Esc") - 8, top, Width("Esc") + 8, lineHeight};
        tabs.MoveBy(bounds.x + inset, top + lineHeight + 4);
        listBar = {grid.area.Right(), grid.area.y, scrollWidth, grid.area.height};
    }

    void Refresh(oCNpcInventory& inv) {
        nativeItems.clear();
        entries.clear();
        counts.fill(0);
        std::array<bool, InventoryHistory::History::MaximumRecent> recentSeen{};
        std::map<int, int> remainingSale, ownedForSale;
        bool foundCyrillic = false;
        for (auto* node = inv.contents->GetNextInList(); node; node = node->GetNextInList()) {
            auto* item = node->GetData();
            const int index = static_cast<int>(nativeItems.size());
            nativeItems.push_back(item);
            if (!item || item->amount <= 0) continue;
            const auto categoryOfItem = Classify(*item);
            const int instance = item->GetInstance();
            const int reserved = history.ForSale(instance);
            int forSale = 0;
            if (reserved > 0 && !(item->flags & ITM_FLAG_ACTIVE)) {
                auto& owned = ownedForSale[instance];
                owned += (std::min)(item->amount, INT_MAX - owned);
                auto& remaining = remainingSale.try_emplace(instance, reserved).first->second;
                forSale = InventoryLayout::AllocateSale(item->amount, remaining);
            }
            if (forSale > 0) ++counts[static_cast<int>(Category::Sale)];
            const int available = item->amount - forSale;
            const int rank = history.Rank(instance);
            const bool recent = available > 0 && rank >= 0 && !std::exchange(recentSeen[rank], true);
            if (available > 0) {
                ++counts[static_cast<int>(Category::All)];
                ++counts[static_cast<int>(categoryOfItem)];
                if (recent) ++counts[static_cast<int>(Category::Recent)];
            }
            if (!foundCyrillic) {
                const std::string_view name(item->name.ToChar());
                foundCyrillic = std::any_of(name.begin(), name.end(), [](unsigned char c) { return c >= 0xC0; });
            }
            const bool matches = category == Category::All || category == categoryOfItem || (category == Category::Recent && recent);
            const int amount = category == Category::Sale ? forSale : matches ? available : 0;
            if (amount > 0) {
                entries.push_back({index, categoryOfItem, categoryOfItem == Category::Weapons ? WeaponDamage(*item) : 0, rank, amount});
            }
        }
        if (history.HasSale() && inv.GetNumItemsInPackString() == 0) history.ReconcileSale(ownedForSale);
        if (!nativeItems.empty()) cyrillic = foundCyrillic;
        InventoryLayout::Sort(entries, category);
        firstRow = std::clamp(firstRow, 0, grid.LastRow(static_cast<int>(entries.size())));
        int match = -1;
        if (selectedItem) {
            const auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) {
                return nativeItems[e.nativeIndex] == selectedItem && selectedItem->GetInstance() == selectedInstance;
            });
            if (it != entries.end()) match = static_cast<int>(it - entries.begin());
        }
        SetSelection(match >= 0 ? match : selected >= 0 ? selected : firstRow * grid.columns, false);
        if (useSelection) SelectNative(inv);
        if (quantity) {
            const auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) {
                auto* item = nativeItems[entry.nativeIndex];
                return item == quantity->item && item->GetInstance() == quantity->instance
                    && !(item->flags & ITM_FLAG_ACTIVE);
            });
            if (it == entries.end()) quantity.reset();
            else {
                quantity->maximum = it->quantity;
                quantity->amount = (std::min)(quantity->amount, quantity->maximum);
            }
        }
    }

    void ChangeCategory(int delta, oCNpcInventory& inv) {
        const int next = (static_cast<int>(category) + delta + CategoryCount) % CategoryCount;
        if (next == static_cast<int>(category)) return;
        category = static_cast<Category>(next);
        firstRow = tooltipTop = 0;
        selected = -1;
        selectedItem = nullptr;
        useSelection = true;
        Refresh(inv);
    }

    void RememberPosition() const { lastInventoryPosition = {category, firstRow}; }

    void MoveSelection(int dx, int dy, oCNpcInventory& inv) {
        useSelection = true;
        SetSelection((std::max)(0, selected) + dx + dy * grid.columns, true);
        SelectNative(inv);
    }

    void Scroll(POINT p, int wheel) {
        if (quantity || !wheel || !bounds.Hit(p.x, p.y)) return;
        if (tooltipMaxTop > 0 && (GetKeyState(VK_SHIFT) & 0x8000)) {
            auto* item = Item(ItemAt(p));
            if (item && item == tooltipItem && item->GetInstance() == tooltipInstance) {
                tooltipTop = std::clamp(tooltipTop - 3 * wheel, 0, tooltipMaxTop);
                return;
            }
        }
        firstRow = std::clamp(firstRow - wheel, 0, grid.LastRow(static_cast<int>(entries.size())));
    }

    bool QuantityKey(int key, oCNpcInventory& inv) {
        if (!quantity) return false;
        Refresh(inv);
        if (!quantity) return true;
        if (key == KEY_ESCAPE) quantity.reset();
        else if (key == KEY_RETURN || key == KEY_NUMPADENTER) ConfirmQuantity();
        else if (quantity->typing) EditQuantityDigits(quantity->digits, quantity->replaceText, key);
        return true;
    }

    void DragQuantity(std::optional<POINT> point, bool down) {
        if (!quantity || !quantity->dragging) return;
        if (point) quantity->amount = InventoryLayout::QuantityAt(QuantityLayout().slider, point->x, quantity->maximum);
        if (!down || !point) quantity->dragging = false;
    }

    Command Click(POINT p, bool shift, oCNpcInventory& inv) {
        if (quantity) {
            const auto [card, number, slider, confirm, cancel] = QuantityLayout();
            if (confirm.Hit(p.x, p.y)) ConfirmQuantity();
            else if (cancel.Hit(p.x, p.y)) quantity.reset();
            else if (number.Hit(p.x, p.y)) {
                if (!quantity->typing) quantity->digits = std::to_string(quantity->amount);
                quantity->typing = quantity->replaceText = true;
            } else if (InventoryLayout::QuantitySliderHitArea(slider).Hit(p.x, p.y)) {
                quantity->typing = false;
                quantity->dragging = true;
                DragQuantity(p, true);
            }
            return Command::None;
        }
        if (close.Hit(p.x, p.y)) return Command::Close;
        if (const auto tab = tabs.Hit(p.x, p.y)) {
            ChangeCategory(static_cast<int>(*tab) - static_cast<int>(category), inv);
            return Command::None;
        }
        if (listBar.Hit(p.x, p.y)) {
            const int last = grid.LastRow(static_cast<int>(entries.size()));
            if (last > 0) firstRow = std::clamp(MulDiv(p.y - listBar.y, last, (std::max)(1, listBar.height - 1)), 0, last);
            return Command::None;
        }
        const int index = ItemAt(p);
        auto* item = Item(index);
        if (!item) return Command::None;
        useSelection = true;
        SetSelection(index, false);
        const bool returning = category == Category::Sale;
        if (!shift) return returning ? Command::None : Command::Use;
        if (item->flags & ITM_FLAG_ACTIVE) return Command::None;
        const int instance = item->GetInstance(), amount = Amount(index);
        if (amount == 1) MoveToSale(instance, amount, returning);
        else if (amount > 1) {
            quantity.emplace(QuantityEdit{item, instance, amount, amount, p, returning});
            inv.SetHandleEventTop();
        }
        return Command::None;
    }

    void Draw(oCNpcInventory& inv, std::optional<POINT> point, bool dragging) {
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
        DrawTabs(tabs, category, counts, quantity || dragging ? std::nullopt : point);
        Fill({left, grid.area.y - 3, bounds.width - 2 * inset, 1}, Rule);
        const int first = firstRow * grid.columns, slots = grid.columns * grid.rows;
        for (int slot = 0; slot < slots; ++slot) {
            const int index = first + slot;
            const auto cell = grid.Cell(slot);
            auto* item = Item(index);
            const bool equipped = item && (item->flags & ITM_FLAG_ACTIVE);
            Fill(cell, equipped ? 0xFF292D1E : 0xFF1D2018);
            Frame(cell, index == hover ? Bright : index == selected && useSelection ? Bronze : Rule);
            if (!item) continue;
            const int amount = Amount(index);
            if (amount > 1) {
                Text(cell.x + 3, cell.Bottom() - lineHeight, std::to_string(amount), cell.width - 7, Bright, true);
            }
        }
        ScrollBar(listBar, grid.rows, (static_cast<int>(entries.size()) + grid.columns - 1) / grid.columns, firstRow);
        const int footer = bounds.Bottom() - inset - lineHeight;
        Fill({left, footer - 5, bounds.width - 2 * inset, 1}, Rule);
        const auto footerLabel = category == Category::Sale
            ? Label(L"Shift + ЛКМ  Вернуть", "Shift + click  Return to inventory")
            : Label(L"ЛКМ  Использовать    Shift + ЛКМ  Отложить", "Click  Use    Shift + click  Set aside");
        Text(left, footer, footerLabel, bounds.width - 2 * inset, Bronze);
        zrenderer->SetAlphaBlendSource(previousAlpha);

        oCItem::SetLightingSwell(FALSE);
        for (int slot = 0; slot < slots; ++slot) {
            auto* item = Item(first + slot);
            if (!item) continue;
            const auto cell = grid.Cell(slot);
            Place(preview, {cell.x + 3, cell.y + 4, cell.width - 6, cell.height - 9});
            // The native preview world can load a visual when the item is added to it.
            item->RenderItem(inv.rndWorld, &preview, 0.0f);
            DrawMissingVisualName(*item, Amount(first + slot), cell);
        }
        oCItem::SetLightingSwell(TRUE);
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        for (int slot = 0; slot < slots; ++slot) {
            if (auto* item = Item(first + slot)) DrawItemMarkers(*item, grid.Cell(slot));
        }
        zrenderer->SetAlphaBlendSource(previousAlpha);
        textView.DrawItems();
        if (quantity) DrawQuantity(point);
        else if (dragging || (point && DrawTabTooltip(tabs, *point))) DrawTooltip(nullptr, 0, std::nullopt);
        else DrawTooltip(Item(hover), Amount(hover), point);
    }
};

#include "inventory_trade.hpp"

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
    struct Press {
        oCItem* item;
        int instance;
        InventoryLayout::PointerMotion motion;
        bool dragging = false;
    };
    std::optional<Press> pressedItem;

    void ReleaseMouse(bool reacquire = true) {
        if (!std::exchange(active, false)) return;
        cursor.Leave();
        if (std::exchange(restoreMouse, false) && zinput && !zinput->GetDeviceEnabled(zINPUT_MOUSE)) {
            zinput->SetDeviceEnabled(zINPUT_MOUSE, TRUE);
            if (reacquire) ApplyMouseAcquisition();
        }
    }

    std::optional<POINT> ScreenPoint(std::optional<POINT> position) const {
        if (!position || !PtInRect(&client, *position)) return std::nullopt;
        return POINT{MulDiv(position->x - client.left, screen->psizex, client.right - client.left),
                     MulDiv(position->y - client.top, screen->psizey, client.bottom - client.top)};
    }

    std::optional<POINT> ClickPoint(std::optional<POINT> event, int button, bool& held) const {
        // Use one input path for the press. Sampling a press that already arrived via the hook can fire it twice.
        if (listening) return ScreenPoint(event);
        const bool down = (GetAsyncKeyState(button) & 0x8000) != 0;
        const bool edge = down && !held;
        held = down;
        return edge ? Position() : std::nullopt;
    }

    bool PrepareMouse() {
        const bool focused = InventoryMouse::HasFocus(window);
        if (events.TakeFocusLoss() || !focused) {
            ReleaseMouse();
            // The focus message already discarded old events. If focus is
            // back, any newly buffered clicks belong to the active window.
            if (!focused) events.Discard();
            panel.DragQuantity(std::nullopt, false);
            trade.DragQuantity(std::nullopt, false);
            pressedItem.reset();
        }
        if (!zinput || !focused || !InventoryMouse::ClientBounds(window, client)) {
            return false;
        }
        if (!active) {
            listening = events.Restart(window);
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
        return true;
    }

public:
    InventoryPanel panel;
    TradePanel trade{panel};
    bool closeLootRequested = false;

    ~InventorySession() { StopMouse(); }

    void StopMouse(bool reacquire = true) {
        events.Stop();
        ReleaseMouse(reacquire);
    }

    bool HasFocus() const { return InventoryMouse::HasFocus(window); }

    std::optional<POINT> Position() const {
        POINT point{};
        if (!active || !HasFocus() || !GetCursorPos(&point)) return std::nullopt;
        return ScreenPoint(point);
    }

    bool IsDragging() const { return pressedItem && pressedItem->dragging; }

    oCItem* DraggedItem(oCNpcInventory& inv) const {
        if (!IsDragging()) return nullptr;
        for (auto* node = inv.inventory.GetNextInList(); node; node = node->GetNextInList()) {
            auto* item = node->GetData();
            if (item == pressedItem->item && item->GetInstance() == pressedItem->instance && item->amount > 0) return item;
        }
        return nullptr;
    }

    bool PollPair() {
        if (!PrepareMouse()) return false;
        const auto input = events.Take();
        if (ClickPoint(input.rightClick, VK_RBUTTON, rightHeld)) return trade.Back();
        const auto click = ClickPoint(input.leftClick, VK_LBUTTON, leftHeld);
        const auto wheel = ScreenPoint(input.wheelPosition);
        if (click || (wheel && input.wheel)) {
            const bool wasConfirming = trade.HasConfirmation();
            trade.Refresh();
            trade.Layout();
            if (wasConfirming && !trade.HasConfirmation()) return false;
            if (click && trade.Click(*click)) return true;
            if (wheel && input.wheel) trade.Scroll(*wheel, input.wheel);
        }
        trade.DragQuantity(Position(), (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        trade.Pump();
        return false;
    }

    Command Poll(oCNpcInventory& inv) {
        if (!PrepareMouse()) return Command::None;
        const auto input = events.Take();
        if (const auto right = ClickPoint(input.rightClick, VK_RBUTTON, rightHeld)) {
            if (std::exchange(pressedItem, std::nullopt)) return Command::None;
            if (!panel.IsPickingQuantity()) {
                const int slot = InventoryLayout::PlaceHotbar(screen->psizex, screen->psizey).Hit(right->x, right->y);
                if (slot >= 0) {
                    history.SetHotbar(slot, -1);
                    return Command::None;
                }
            }
            if (!panel.QuantityKey(KEY_ESCAPE, inv)) return Command::Close;
            return Command::None;
        }
        const auto wheel = ScreenPoint(input.wheelPosition);
        const auto point = Position();
        const bool held = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool wasHeld = std::exchange(leftHeld, held);
        const auto click = listening ? ScreenPoint(input.leftClick) : held && !wasHeld ? point : std::nullopt;
        const bool released = listening ? input.leftRelease.has_value() : wasHeld && !held;
        const auto pointer = listening && released ? ScreenPoint(input.leftRelease) : point;
        panel.DragQuantity(point, held);
        if (click || (pressedItem && released) || (wheel && input.wheel)) {
            panel.Layout();
            panel.Refresh(inv);
        }
        const bool shift = input.leftClick ? input.leftShift : (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (click) {
            pressedItem.reset();
            if (!shift) {
                if (auto* item = panel.PressSource(*click)) {
                    pressedItem = Press{item, item->GetInstance(), {click->x, click->y}};
                }
            }
            if (!pressedItem) return panel.Click(*click, shift, inv);
        }
        if (pressedItem) {
            const int threshold = std::clamp(screen->psizey / 160, 4, 9);
            if (input.leftMotion) {
                const int thresholdX = (std::max)(1, MulDiv(threshold, client.right - client.left, screen->psizex));
                const int thresholdY = (std::max)(1, MulDiv(threshold, client.bottom - client.top, screen->psizey));
                pressedItem->dragging |= input.leftMotion->MovedBy(thresholdX, thresholdY);
            }
            if (pointer && (held || released)) {
                pressedItem->motion.Extend(pointer->x, pointer->y);
                pressedItem->dragging |= pressedItem->motion.MovedBy(threshold, threshold);
            }
            if (!released) return Command::None;
            auto* dragged = DraggedItem(inv);
            const auto started = *std::exchange(pressedItem, std::nullopt);
            if (!pointer) return Command::None;
            if (started.dragging) {
                const int slot = InventoryLayout::PlaceHotbar(screen->psizex, screen->psizey).Hit(pointer->x, pointer->y);
                if (slot >= 0 && dragged && StockFor(inv, started.instance).item) {
                    history.SetHotbar(slot, started.instance);
                }
            } else if (auto* item = panel.PressSource(*pointer); item == started.item && item->GetInstance() == started.instance) {
                return panel.Click(*pointer, false, inv);
            }
            return Command::None;
        }
        if (wheel && input.wheel) panel.Scroll(*wheel, input.wheel);
        return Command::None;
    }
};

// Release engine objects on the game thread, before the renderer and input
// are destroyed. DLL static destructors can run after both are gone.
InventorySession* session = nullptr;
HotbarPanel* hotbar = nullptr;
bool shuttingDown = false;
bool endingGameSession = false;
bool inMainMenu = false;
std::array<bool, HotbarSlots> functionHeld{};

bool InventoryAvailable() {
    return !shuttingDown && !endingGameSession && !inMainMenu && (!gameMan || !gameMan->exitGame);
}

void CloseSession(bool reacquire = true) {
    std::unique_ptr<InventorySession> closing(std::exchange(session, nullptr));
    if (!closing) return;
    if (!closing->trade.Pair().other) closing->panel.RememberPosition();
    closing->StopMouse(reacquire);
}

HotbarPanel& GetHotbar() {
    if (!hotbar) hotbar = new HotbarPanel();
    return *hotbar;
}

void ReleaseInventoryUi() {
    CloseSession(false);
    delete std::exchange(hotbar, nullptr);
    functionHeld.fill(false);
}

void BeginInventoryShutdown() {
    if (std::exchange(shuttingDown, true)) return;
    ReleaseInventoryUi();
}

int __fastcall InventoryKey(oCNpcInventory*, void*, int);
auto KeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::HandleEvent), &InventoryKey, Union::HookType::Hook_Detours);

bool IsLootInput(oCItemContainer* container) {
    if (!session) return false;
    const auto& pair = session->trade.Pair();
    return pair.IsLoot() && (container == pair.other || container == pair.hero) && pair.IsActive();
}

int HandleLootKey(int key) {
    if (!session->HasFocus() || session->closeLootRequested) return TRUE;
    if (zinput && key != KEY_TAB) {
        const auto logical = zinput->GetFirstBindedLogicalKey(key);
        if (logical == zLOGICKEY_END || logical == zLOGICKEY_INVENTORY) key = KEY_ESCAPE;
    }
    const bool wasConfirming = session->trade.HasConfirmation();
    if (key == KEY_RETURN || key == KEY_NUMPADENTER) session->trade.Refresh();
    if (wasConfirming && !session->trade.HasConfirmation()) return TRUE;
    if (session->trade.Key(key)) session->closeLootRequested = true;
    return TRUE;
}

int __fastcall ChestKey(oCItemContainer*, void*, int);
auto ChestKeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCItemContainer::HandleEvent), &ChestKey, Union::HookType::Hook_Detours);

int __fastcall ChestKey(oCItemContainer* container, void* edx, int key) {
    return IsLootInput(container) ? HandleLootKey(key) : ChestKeyHook(container, edx, key);
}

int __fastcall CorpseKey(oCNpcContainer*, void*, int);
auto CorpseKeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcContainer::HandleEvent), &CorpseKey, Union::HookType::Hook_Detours);

int __fastcall CorpseKey(oCNpcContainer* container, void* edx, int key) {
    return IsLootInput(container) ? HandleLootKey(key) : CorpseKeyHook(container, edx, key);
}

int __fastcall StealKey(oCStealContainer*, void*, int);
auto StealKeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCStealContainer::HandleEvent), &StealKey, Union::HookType::Hook_Detours);

int __fastcall StealKey(oCStealContainer* container, void* edx, int key) {
    return IsLootInput(container) ? HandleLootKey(key) : StealKeyHook(container, edx, key);
}

void TakeFromNpc(oCItemContainer& source, oCItem* expected) {
    if (!zinput) return;
    if (!source.IsActive()) source.Activate();
    if (source.GetSelectedItem() != expected) return;
    // Each native action takes one item and handles equipped loot or detection.
    source.SetTransferCount(0);
    if (source.GetMode() == INV_MODE_PLUNDER) {
        CorpseKeyHook(static_cast<oCNpcContainer*>(&source), nullptr, MOUSE_BUTTONLEFT);
    } else {
        StealKeyHook(static_cast<oCStealContainer*>(&source), nullptr, MOUSE_BUTTONLEFT);
    }
}

void CloseLoot(oCItemContainer* loot) {
    CloseSession();
    if (ActiveLoot() != loot) return;
    if (loot->GetMode() == INV_MODE_PLUNDER) {
        player->CloseDeadNpc();
        return;
    }
    // The end handler closes the engine views and finishes the interaction.
    if (!loot->IsActive()) loot->Activate();
    zCArray<unsigned short> keys;
    if (zinput && zinput->GetBinding(zLOGICKEY_END, keys) && keys.GetNum() > 0) loot->HandleEvent(keys[0]);
    else loot->HandleEvent(KEY_ESCAPE);
}

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
    if (IsLootInput(inv)) return HandleLootKey(key);
    if (!session || session->trade.Pair().other || !PersonalInventory(inv) || !zinput) return KeyHook(inv, edx, key);
    auto& panel = session->panel;
    if (panel.QuantityKey(key, *inv)) return TRUE;
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

int __fastcall TradeKey(oCViewDialogTrade*, void*, int);
auto TradeKeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCViewDialogTrade::HandleEvent), &TradeKey, Union::HookType::Hook_Detours);

int __fastcall TradeKey(oCViewDialogTrade* dialog, void* edx, int key) {
    if (!session || session->trade.Pair().shop != dialog || ActiveTrade() != dialog) return TradeKeyHook(dialog, edx, key);
    if (!session->HasFocus()) return TRUE;
    const bool wasConfirming = session->trade.HasConfirmation();
    if (key == KEY_RETURN || key == KEY_NUMPADENTER) session->trade.Refresh();
    if (wasConfirming && !session->trade.HasConfirmation()) return TRUE;
    if (session->trade.Key(key)) dialog->OnKeyEsc();
    return TRUE;
}

void __fastcall DrawInventory(oCItemContainer*, void*);
auto DrawHook = Union::CreateHook(
    SIGNATURE_OF(&oCItemContainer::Draw), &DrawInventory, Union::HookType::Hook_Detours);

void __fastcall DrawInventory(oCItemContainer* container, void* edx) {
    if (!InventoryAvailable()) {
        DrawHook(container, edx);
        return;
    }
    if (session && session->trade.Pair().other && session->trade.Pair().IsActive()) {
        const auto& pair = session->trade.Pair();
        if (container == pair.other) return;
        if (container == pair.hero) {
            session->trade.Draw(session->Position());
            return;
        }
    }
    if (session && !session->trade.Pair().other && PersonalInventory(container)) {
        auto& inv = player->inventory2;
        const auto point = session->Position();
        session->panel.Draw(inv, point, session->IsDragging());
        // The renderer can wrap and batch the native container pass. Draw the
        // editable bar here so its previews stay in the same pass as the grid.
        GetHotbar().Draw(inv, true, point, session->DraggedItem(inv));
        session->panel.DrawCursor(point);
    } else {
        DrawHook(container, edx);
    }
}

void __fastcall DrawHotbarHud(oCGame*, void*);
auto DrawHudHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::UpdatePlayerStatus), &DrawHotbarHud, Union::HookType::Hook_Detours);

void __fastcall DrawHotbarHud(oCGame* game, void* edx) {
    DrawHudHook(game, edx);
    if (InventoryAvailable() && game == ogame && zrenderer && HotbarInWorld()) {
        GetHotbar().Draw(player->inventory2, false, std::nullopt, nullptr);
    }
}

int __fastcall HotbarKey(oCGame*, void*, int);
auto HotbarKeyHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::HandleEvent), &HotbarKey, Union::HookType::Hook_Detours);

int __fastcall HotbarKey(oCGame* game, void* edx, int key) {
    if (!InventoryAvailable() || key < KEY_F1 || key > KEY_F10 || game != ogame || history.Hotbar()[key - KEY_F1] < 0
        || ((GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_SHIFT)) & 0x8000)
        || !InventoryMouse::HasFocus(hWndApp) || !HotbarInWorld()) return HotbarKeyHook(game, edx, key);
    const int slot = key - KEY_F1;
    const bool held = (GetAsyncKeyState(VK_F1 + slot) & 0x8000) != 0;
    if (!std::exchange(functionHeld[slot], held)) UseHotbar(slot);
    return TRUE;
}

void __fastcall DoneHotbar(oCGame*, void*);
auto DoneHotbarHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::Done), &DoneHotbar, Union::HookType::Hook_Detours);

void __fastcall DoneHotbar(oCGame* game, void* edx) {
    if (game != ogame) {
        DoneHotbarHook(game, edx);
        return;
    }
    const bool previous = std::exchange(endingGameSession, true);
    ReleaseInventoryUi();
    DoneHotbarHook(game, edx);
    endingGameSession = previous;
}

void __fastcall InventoryMainMenu(CGameManager*, void*, int);
auto MainMenuHook = Union::CreateHook(
    SIGNATURE_OF(&CGameManager::Menu), &InventoryMainMenu, Union::HookType::Hook_Detours);

void __fastcall InventoryMainMenu(CGameManager* manager, void* edx, int mode) {
    // The main menu sets exitGame directly. Clean up before its caller
    // unregisters menu views and starts dismantling the engine.
    CloseSession(InventoryAvailable());
    const bool previous = std::exchange(inMainMenu, true);
    MainMenuHook(manager, edx, mode);
    if (manager->exitGame) BeginInventoryShutdown();
    inMainMenu = previous;
}

void __fastcall StopInventory(CGameManager*, void*);
auto StopInventoryHook = Union::CreateHook(
    SIGNATURE_OF(&CGameManager::Done), &StopInventory, Union::HookType::Hook_Detours);

void __fastcall StopInventory(CGameManager* manager, void* edx) {
    BeginInventoryShutdown();
    StopInventoryHook(manager, edx);
}

void __fastcall CloseInventory(oCNpcInventory*, void*);
auto CloseHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::Close), &CloseInventory, Union::HookType::Hook_Detours);

void __fastcall CloseInventory(oCNpcInventory* inv, void* edx) {
    if (player && inv == &player->inventory2 && session && !session->trade.Pair().other) CloseSession(InventoryAvailable());
    CloseHook(inv, edx);
}

void __cdecl PollInventory();
auto PollHook = Union::CreateHook(
    reinterpret_cast<void*>(0x007A55C0), &PollInventory, Union::HookType::Hook_Detours);

void __cdecl PollInventory() {
    for (int i = 0; i < HotbarSlots; ++i) if (!(GetAsyncKeyState(VK_F1 + i) & 0x8000)) functionHeld[i] = false;
    PollHook();
    if (!InventoryAvailable()) return;
    auto* shop = ActiveTrade();
    auto* other = shop ? shop->DlgInventoryNpc->StealContainer : ActiveLoot();
    if (other) {
        const InventoryPair pair{other, &player->inventory2, shop};
        if (!session || !session->trade.Pair().Matches(pair)) {
            CloseSession();
            auto next = std::make_unique<InventorySession>();
            if (!next->trade.Open(pair)) return;
            auto& inv = *pair.hero;
            if (inv.GetNumItemsInPackString() > 0) inv.UnpackAllItems();
            if (!pair.IsActive()) return;
            next->trade.Refresh();
            next->trade.Layout();
            session = next.release();
        }
        if (session->closeLootRequested || session->PollPair()) {
            if (shop) {
                shop->OnKeyEsc();
                CloseSession();
            } else CloseLoot(other);
        } else if (!pair.IsActive()) {
            CloseSession();
        }
        return;
    }
    if (session && session->trade.Pair().other) CloseSession();
    if (!player || !PersonalInventory(&player->inventory2)) {
        CloseSession();
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
        session = next.release();
    }
    const auto result = session->Poll(inv);
    if (result == Command::Close) player->CloseInventory();
    if (result == Command::Use) UseSelected(inv);
}

} // namespace
} // namespace GOTHIC_NAMESPACE
