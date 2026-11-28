#pragma once

oCViewDialogTrade* ActiveTrade() {
    if (!player || !ogame || !screen || !screen->GetFont() || screen->psizex < 480 || screen->psizey < 360
        || zCMenu::GetActive() || !player->inventory2.IsOpen()) return nullptr;
    auto& info = oCInformationManager::GetInformationManager();
    auto* dialog = info.DlgTrade;
    if (info.Mode != oCInformationManager::INFO_MGR_MODE_TRADE || info.Player != player || !dialog
        || !dialog->IsActive() || dialog->HasFinished() || dialog->NpcRight != player || !dialog->NpcLeft
        || !dialog->DlgInventoryPlayer || dialog->DlgInventoryPlayer->Inventory != &player->inventory2
        || !dialog->DlgInventoryNpc) return nullptr;
    auto* stock = dialog->DlgInventoryNpc->StealContainer;
    if (!stock || !stock->IsOpen() || !stock->contents || !stock->rndWorld
        || !player->inventory2.contents || !player->inventory2.rndWorld) return nullptr;
    return dialog;
}

class TradePanel {
    struct Slot { oCItem* item; int price; Entry entry; };
    struct Lot { oCItem* item; int instance, amount, price; };
    struct Side {
        Grid grid;
        InventoryLayout::FittedTabs tabs;
        Box scrollbar;
        std::vector<Slot> items;
        std::array<int, CategoryCount> counts{};
        Category category = Category::All;
        int firstRow = 0;
    };
    struct Choice { int side, index; };
    struct Deal {
        bool buy, bulk, reserved;
        std::vector<Lot> lots;
        int amount = 1;
        std::size_t current = 0;
        std::int64_t moved = 0, gold = 0;
        bool started = false;
        bool dragging = false, typing = false, replaceText = false;
        std::string digits;
    };

    InventoryPanel& ui;
    std::array<Side, 2> sides;
    Box close, bulkButton, footer, card, confirm, cancel, number, slider;
    int currency = -1;
    std::int64_t wallet = 0;
    std::vector<Lot> marked;
    std::optional<Deal> deal;
    std::optional<Choice> keyboard;
    std::string status;

    int SellPrice(oCViewDialogTrade& dialog, oCItem& item) const {
        if (item.GetInstance() == currency || !CanMoveSaleItem(item, false)) return 0;
        return InventoryTrade::SalePrice(item.GetValue(), dialog.DlgInventoryNpc->ValueMultiplier);
    }

    std::int64_t Count(oCItemContainer& container, int instance) const {
        std::int64_t amount = 0;
        if (!container.contents) return 0;
        for (auto* node = container.contents->GetNextInList(); node; node = node->GetNextInList()) {
            auto* item = node->GetData();
            if (item && item->amount > 0 && item->GetInstance() == instance) amount += item->amount;
        }
        return amount;
    }

    const Slot* Item(Choice choice) const {
        const auto& items = sides[choice.side].items;
        return choice.index >= 0 && choice.index < static_cast<int>(items.size()) ? &items[choice.index] : nullptr;
    }

    std::optional<Choice> Hit(POINT point) const {
        for (int i = 0; i < 2; ++i) {
            const auto& side = sides[i];
            const int index = side.grid.ItemAt(point.x, point.y, side.firstRow, static_cast<int>(side.items.size()));
            if (index >= 0) return Choice{i, index};
        }
        return std::nullopt;
    }

    void Begin(Choice choice) {
        const auto* slot = Item(choice);
        if (!slot) return;
        keyboard = choice;
        const bool buy = choice.side == 0;
        if (!buy && slot->price <= 0) {
            status = ui.Label(L"Предмет не продаётся", "Item cannot be sold");
            return;
        }
        const int limit = InventoryTrade::Affordable(slot->entry.quantity, slot->price, buy ? wallet : INT_MAX - wallet);
        if (limit <= 0) {
            status = buy ? ui.Label(L"Не хватает золота", "Not enough gold") : ui.Label(L"Достигнут предел золота", "Gold limit reached");
            return;
        }
        const bool reserved = !buy && sides[choice.side].category == Category::Sale;
        deal.emplace(Deal{buy, false, reserved, {{slot->item, slot->item->GetInstance(), limit, slot->price}}});
        status.clear();
    }

    void Confirm() {
        if (!deal || deal->started) return;
        if (!deal->bulk) {
            if (deal->typing && deal->digits.empty()) return;
            deal->lots.front().amount = Quantity();
        }
        deal->started = true;
    }

    int Quantity() const {
        return deal->typing ? InventoryLayout::QuantityFromText(deal->digits, deal->lots.front().amount) : deal->amount;
    }

    int TransferOne(oCViewDialogTrade& dialog, const Lot& lot) {
        if (ActiveTrade() != &dialog) return 0;
        auto& inv = *dialog.DlgInventoryPlayer->Inventory;
        auto& source = *Container(dialog, deal->buy ? 0 : 1);
        int index = 0;
        oCItem* selected = nullptr;
        const int reserved = history.ForSale(lot.instance);
        int unassigned = reserved;
        for (auto* node = source.contents->GetNextInList(); node; node = node->GetNextInList(), ++index) {
            auto* item = node->GetData();
            if (!item || item->amount <= 0 || item->GetInstance() != lot.instance) continue;
            const int held = deal->buy || (item->flags & ITM_FLAG_ACTIVE) ? 0 : InventoryLayout::AllocateSale(item->amount, unassigned);
            if (item != lot.item) continue;
            if (deal->reserved ? held <= 0 : item->amount <= held) return 0;
            selected = item;
            break;
        }
        if (!selected || lot.instance == currency) return 0;
        const int price = deal->buy ? (std::max)(0, selected->GetValue()) : SellPrice(dialog, *selected);
        if (price != lot.price || (!deal->buy && price <= 0)) return 0;
        const auto gold = Count(inv, currency);
        if (deal->buy ? gold < price : gold + price > INT_MAX) return 0;

        const auto section = deal->buy ? oCViewDialogTrade::TRADE_SECTION_LEFT_INVENTORY
                                       : oCViewDialogTrade::TRADE_SECTION_RIGHT_INVENTORY;
        if (dialog.SectionTrade != section) dialog.OnSection(section);
        source.selectedItem = index;
        source.CheckSelectedItem();
        if (source.GetSelectedItem() != selected) return 0;
        const auto before = Count(inv, lot.instance);
        if (deal->buy) dialog.OnTransferRight(1);
        else dialog.OnTransferLeft(1);
        const auto difference = Count(inv, lot.instance) - before;
        const auto moved = deal->buy ? difference : -difference;
        if (moved <= 0) return 0;
        if (deal->reserved) {
            history.SetForSale(lot.instance, reserved - static_cast<int>((std::min)(std::int64_t(reserved), moved)));
        }
        deal->moved += moved;
        deal->gold += Count(inv, currency) - gold;
        return static_cast<int>((std::min)(std::int64_t(lot.amount), moved));
    }

    void Finish(bool interrupted = false) {
        if (!deal->moved) {
            status = ui.Label(L"Обмен не состоялся", "No items transferred");
        } else {
            status = deal->buy ? ui.Label(L"Куплено", "Bought") : ui.Label(L"Продано", "Sold");
            status += "  " + std::to_string(deal->moved) + "    " + (deal->gold >= 0 ? "+" : "") + std::to_string(deal->gold);
            if (interrupted) status += ui.Label(L"  Частично", "  Partial");
        }
        deal.reset();
    }

    void Control(Box area, const std::string& label, std::optional<POINT> point, bool enabled = true) {
        const bool hover = enabled && point && area.Hit(point->x, point->y);
        ui.Fill(area, hover ? 0xFF35351F : 0xFF202318);
        ui.Frame(area, hover ? Bright : enabled ? Bronze : Rule);
        const int x = area.x + (std::max)(5, (area.width - ui.Width(label)) / 2);
        ui.Text(x, area.y + (area.height - ui.lineHeight) / 2, label, area.Right() - x - 4, enabled ? Paper : Bronze);
    }

    void DrawDeal(std::optional<POINT> point) {
        ui.Fill(card, 0xFF181C15);
        ui.Frame(card, Bronze);
        const int x = card.x + 14, w = card.width - 28, h = ui.lineHeight;
        const auto title = deal->bulk ? ui.Label(L"Продать отложенное", "Sell set-aside items")
            : deal->buy ? ui.Label(L"Купить", "Buy") : ui.Label(L"Продать", "Sell");
        ui.Text(x, card.y + 10, title, w, Bronze);
        if (deal->started) {
            ui.Text(x, card.y + 10 + 2 * h, ui.Label(L"Обменено", "Transferred") + "  " + std::to_string(deal->moved), w);
            Control(cancel, ui.Label(L"Остановить", "Stop"), point);
            return;
        }
        std::int64_t total = 0, amount = 0;
        if (deal->bulk) {
            for (const auto& lot : deal->lots) {
                amount += lot.amount;
                total += std::int64_t(lot.amount) * lot.price;
            }
            ui.Text(x, card.y + 10 + h, ui.Label(L"Предметов", "Items") + "  " + std::to_string(amount), w);
        } else {
            const auto& lot = deal->lots.front();
            const int selected = Quantity();
            total = std::int64_t(selected) * lot.price;
            ui.Text(x, card.y + 10 + h, lot.item->name.ToChar(), w);
            const auto count = (deal->typing ? deal->digits + "|" : std::to_string(selected)) + " / " + std::to_string(lot.amount);
            if (lot.amount > 1) Control(number, count, point);
            else ui.Text(x, number.y, count, w);
            if (lot.amount > 1) ui.DrawQuantitySlider(slider, selected, lot.amount);
        }
        ui.Text(x, card.y + 13 + 4 * h, ui.Label(L"Золото", "Gold") + "  " + (deal->buy ? "-" : "+") + std::to_string(total), w, Bright);
        Control(confirm, ui.Label(L"Подтвердить", "Confirm"), point);
        Control(cancel, ui.Label(L"Отмена", "Cancel"), point);
    }

public:
    explicit TradePanel(InventoryPanel& view) : ui(view) {}

    static oCItemContainer* Container(oCViewDialogTrade& dialog, int side) {
        if (side == 0) return dialog.DlgInventoryNpc->StealContainer;
        return dialog.DlgInventoryPlayer->Inventory;
    }

    bool Open() {
        currency = oCItemContainer::GetCurrencyInstance();
        return ui.Ready() && currency >= 0;
    }

    void Layout() {
        ui.panel.SetFont(screen->GetFont());
        ui.textView.SetFont(screen->GetFont());
        ui.tooltipText.SetFont(screen->GetFont());
        const int h = ui.lineHeight = (std::max)(12, ui.panel.GetFont()->GetFontY()) + 4;
        const int gap = std::clamp(screen->psizey / 360, 2, 5);
        const int pad = std::clamp(screen->psizey / 80, 8, 20);
        const int target = std::clamp(screen->psizey / 15, 40, 108);
        const int preferredPane = 13 * (target + gap) / 2;
        const int width = (std::min)(screen->psizex * 94 / 100, 2 * preferredPane + 5 * pad);
        const int pane = (width - 5 * pad) / 2;
        sides[0].tabs = InventoryLayout::FitCategoryTabs(pane, CategoryTabHeight(), false);
        sides[1].tabs = InventoryLayout::FitCategoryTabs(pane, CategoryTabHeight(), true);
        const int tabsHeight = (std::max)(sides[0].tabs.height, sides[1].tabs.height);
        const int chrome = 4 * h + 4 * pad + tabsHeight + gap;
        auto grid = InventoryLayout::MakeGrid({0, 0, pane, screen->psizey * 78 / 100 - chrome}, target, gap);
        const int height = grid.area.height + chrome;
        ui.bounds = {(screen->psizex - width) / 2, (screen->psizey - height) / 2, width, height};
        Place(ui.panel, ui.bounds);
        Place(ui.textView, ui.bounds);
        const int x = ui.bounds.x + pad, tabsTop = ui.bounds.y + 2 * h + 2 * pad;
        for (int i = 0; i < 2; ++i) {
            grid.area.x = x + i * (pane + 3 * pad);
            grid.area.y = tabsTop + tabsHeight + gap;
            sides[i].grid = grid;
            sides[i].tabs.MoveBy(grid.area.x, tabsTop);
            sides[i].firstRow = std::clamp(sides[i].firstRow, 0, grid.LastRow(static_cast<int>(sides[i].items.size())));
            sides[i].scrollbar = {grid.area.Right() + 1, grid.area.y, pad, grid.area.height};
        }
        close = {ui.bounds.Right() - pad - ui.Width("Esc") - 9, ui.bounds.y + pad, ui.Width("Esc") + 9, h};
        footer = {x, ui.bounds.Bottom() - 2 * h - pad, width - 2 * pad, 2 * h};
        const int button = (std::min)(footer.width / 2, ui.Width(ui.Label(L"Продать отложенное", "Sell set-aside items")) + 21);
        bulkButton = {footer.Right() - button, footer.y + 4, button, h + 8};
        const int cardWidth = (std::min)(width - 2 * pad, (std::max)(300, 13 * h));
        const int cardHeight = 6 * h + 30;
        card = {(screen->psizex - cardWidth) / 2, (screen->psizey - cardHeight) / 2, cardWidth, cardHeight};
        number = {card.x + 14, card.y + 10 + 2 * h, card.width - 28, h};
        slider = {card.x + 20, card.y + 10 + 3 * h, card.width - 40, h};
        const int w = (card.width - 35) / 2;
        confirm = {card.x + 14, card.Bottom() - h - 13, w, h + 5};
        cancel = {confirm.Right() + 7, confirm.y, w, confirm.height};
    }

    void Refresh(oCViewDialogTrade& dialog) {
        marked.clear();
        wallet = 0;
        std::map<int, int> reserved;
        std::array<bool, InventoryHistory::History::MaximumRecent> recentSeen{};
        bool cyrillic = false;
        for (int i = 0; i < 2; ++i) {
            auto& side = sides[i];
            const bool hero = i == 1;
            side.items.clear();
            side.counts.fill(0);
            for (auto* node = Container(dialog, i)->contents->GetNextInList(); node; node = node->GetNextInList()) {
                auto* item = node->GetData();
                if (!item || item->amount <= 0) continue;
                if (!cyrillic) {
                    const std::string_view name(item->name.ToChar());
                    cyrillic = std::any_of(name.begin(), name.end(), [](unsigned char c) { return c >= 0xC0; });
                }
                const int instance = item->GetInstance();
                if (instance == currency) {
                    if (hero) wallet += item->amount;
                    continue;
                }
                const int price = hero ? SellPrice(dialog, *item) : (std::max)(0, item->GetValue());
                int held = 0;
                if (hero && !(item->flags & ITM_FLAG_ACTIVE)) {
                    const int queued = history.ForSale(instance);
                    if (queued > 0) {
                        auto& remaining = reserved.try_emplace(instance, queued).first->second;
                        held = InventoryLayout::AllocateSale(item->amount, remaining);
                        if (held > 0 && price > 0) marked.push_back({item, instance, held, price});
                    }
                }
                const int available = item->amount - held;
                const auto kind = Classify(*item);
                const int rank = hero ? history.Rank(instance) : -1;
                const bool recent = available > 0 && rank >= 0 && !std::exchange(recentSeen[rank], true);
                if (held > 0) ++side.counts[static_cast<int>(Category::Sale)];
                if (available > 0) {
                    ++side.counts[static_cast<int>(Category::All)];
                    ++side.counts[static_cast<int>(kind)];
                    if (recent) ++side.counts[static_cast<int>(Category::Recent)];
                }
                const bool matches = side.category == Category::All || side.category == kind
                    || (side.category == Category::Recent && recent);
                const int amount = side.category == Category::Sale ? held : matches ? available : 0;
                if (amount > 0) {
                    const int damage = kind == Category::Weapons ? WeaponDamage(*item) : 0;
                    side.items.push_back({item, price, {0, kind, damage, rank, amount}});
                }
            }
            std::stable_sort(side.items.begin(), side.items.end(), [&](const Slot& a, const Slot& b) {
                return InventoryLayout::ComesBefore(a.entry, b.entry, side.category);
            });
            side.firstRow = std::clamp(side.firstRow, 0, side.grid.LastRow(static_cast<int>(side.items.size())));
        }
        ui.cyrillic = cyrillic;
        if (deal && !deal->started && !deal->bulk) {
            auto& lot = deal->lots.front();
            const auto& slots = sides[deal->buy ? 0 : 1].items;
            const auto match = std::find_if(slots.begin(), slots.end(), [&](const Slot& slot) {
                return slot.item == lot.item && slot.item->GetInstance() == lot.instance;
            });
            const auto gold = deal->buy ? wallet : INT_MAX - wallet;
            const int limit = match == slots.end() || match->price != lot.price
                ? 0 : InventoryTrade::Affordable(match->entry.quantity, lot.price, gold);
            if (limit < Quantity()) {
                deal.reset();
                status = ui.Label(L"Условия обмена изменились", "Trade terms changed");
            } else lot.amount = limit;
        }
    }

    void Pump(oCViewDialogTrade& dialog) {
        if (!deal || !deal->started) return;
        constexpr int transfersPerFrame = 12;
        for (int i = 0; i < transfersPerFrame && deal->current < deal->lots.size(); ++i) {
            auto& lot = deal->lots[deal->current];
            if (const int moved = TransferOne(dialog, lot)) lot.amount -= moved;
            else if (!deal->bulk) { Finish(true); return; }
            else { ++deal->current; continue; }
            if (lot.amount == 0) ++deal->current;
        }
        if (deal->current == deal->lots.size()) {
            const bool remaining = std::any_of(deal->lots.begin(), deal->lots.end(), [](const Lot& lot) { return lot.amount > 0; });
            Finish(remaining);
        }
    }

    bool HasConfirmation() const { return deal.has_value(); }

    bool Click(POINT point, oCViewDialogTrade& dialog) {
        if (deal) {
            if (cancel.Hit(point.x, point.y)) return Back();
            if (!deal->started && confirm.Hit(point.x, point.y)) Confirm();
            else if (!deal->started && !deal->bulk && deal->lots.front().amount > 1) {
                if (number.Hit(point.x, point.y)) {
                    if (!deal->typing) deal->digits = std::to_string(deal->amount);
                    deal->typing = deal->replaceText = true;
                } else if (InventoryLayout::QuantitySliderHitArea(slider).Hit(point.x, point.y)) {
                    deal->typing = false;
                    deal->dragging = true;
                    DragQuantity(point, true);
                }
            }
            return false;
        }
        if (close.Hit(point.x, point.y)) return true;
        for (int i = 0; i < 2; ++i) {
            auto& side = sides[i];
            if (const auto category = side.tabs.Hit(point.x, point.y)) {
                if (side.category != *category) {
                    side.category = *category;
                    side.firstRow = 0;
                    keyboard = Choice{i, 0};
                    Refresh(dialog);
                }
                return false;
            }
        }
        if (bulkButton.Hit(point.x, point.y) && !marked.empty()) {
            deal.emplace(Deal{false, true, true, marked});
            status.clear();
        } else if (auto choice = Hit(point)) Begin(*choice);
        else for (auto& side : sides) {
            if (!side.scrollbar.Hit(point.x, point.y)) continue;
            const int last = side.grid.LastRow(static_cast<int>(side.items.size()));
            side.firstRow = MulDiv(point.y - side.scrollbar.y, last, (std::max)(1, side.scrollbar.height - 1));
        }
        return false;
    }

    bool Back() {
        if (!deal) return true;
        if (deal->started) Finish(true);
        else deal.reset();
        return false;
    }

    void DragQuantity(std::optional<POINT> point, bool down) {
        if (!deal || deal->started || !deal->dragging) return;
        if (point) deal->amount = InventoryLayout::QuantityAt(slider, point->x, deal->lots.front().amount);
        if (!down || !point) deal->dragging = false;
    }

    void Scroll(POINT point, int wheel) {
        if (deal) return;
        if (ui.tooltipMaxTop > 0 && (GetKeyState(VK_SHIFT) & 0x8000)) {
            if (const auto choice = Hit(point)) {
                auto* item = Item(*choice)->item;
                if (item == ui.tooltipItem && item->GetInstance() == ui.tooltipInstance) {
                    ui.tooltipTop = std::clamp(ui.tooltipTop - 3 * wheel, 0, ui.tooltipMaxTop);
                    return;
                }
            }
        }
        for (auto& side : sides) {
            if (!side.grid.area.Hit(point.x, point.y) && !side.scrollbar.Hit(point.x, point.y)) continue;
            side.firstRow = std::clamp(side.firstRow - wheel, 0, side.grid.LastRow(static_cast<int>(side.items.size())));
        }
    }

    bool Key(int key) {
        if (key == KEY_ESCAPE) return Back();
        if (deal) {
            if (key == KEY_RETURN || key == KEY_NUMPADENTER) Confirm();
            else if (!deal->started && !deal->bulk) {
                if (deal->typing) EditQuantityDigits(deal->digits, deal->replaceText, key);
                else if (key == KEY_LEFT && deal->amount > 1) --deal->amount;
                else if (key == KEY_RIGHT && deal->amount < deal->lots.front().amount) ++deal->amount;
            }
            return false;
        }
        if (key == KEY_RETURN || key == KEY_NUMPADENTER) {
            if (keyboard) Begin(*keyboard);
            return false;
        }
        if (key != KEY_TAB && key != KEY_LEFT && key != KEY_RIGHT && key != KEY_UP && key != KEY_DOWN) return false;
        auto choice = keyboard.value_or(Choice{0, 0});
        if (key == KEY_TAB) choice = {1 - choice.side, 0};
        auto& side = sides[choice.side];
        if (side.items.empty()) { keyboard = choice; return false; }
        const int step = key == KEY_LEFT ? -1 : key == KEY_RIGHT ? 1 : key == KEY_UP ? -side.grid.columns : key == KEY_DOWN ? side.grid.columns : 0;
        choice.index = std::clamp(choice.index + step, 0, static_cast<int>(side.items.size()) - 1);
        const int row = choice.index / side.grid.columns;
        side.firstRow = std::clamp(side.firstRow, (std::max)(0, row - side.grid.rows + 1), row);
        keyboard = choice;
        return false;
    }

    void Draw(oCViewDialogTrade& dialog, std::optional<POINT> point) {
        Refresh(dialog);
        Layout();
        ui.textView.ClrPrintwin();
        const auto hover = point && !deal ? Hit(*point) : std::nullopt;
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        ui.panel.Blit();
        const int left = footer.x, top = close.y, h = ui.lineHeight;
        ui.Text(left, top, ui.Label(L"ТОРГОВЛЯ", "TRADE"), footer.width / 2, Bronze);
        ui.Text(left + footer.width / 2, top, ui.Label(L"Золото", "Gold") + "  " + std::to_string(wallet), close.x - left - footer.width / 2 - 12, Bright, true);
        ui.Text(close.x, top, "Esc", close.width, Bronze, true);
        ui.Fill({left, top + h, footer.width, 1}, Rule);
        for (int i = 0; i < 2; ++i) {
            const auto& side = sides[i];
            const auto title = i == 0 ? std::string(dialog.NpcLeft->name[0].ToChar()) : ui.Label(L"Вещи героя", "Your items");
            ui.Text(side.grid.area.x, top + h + 5, title, side.grid.area.width, Bronze);
            ui.Fill({side.grid.area.x, side.grid.area.y - 2, side.grid.area.width, 1}, Rule);
            ui.DrawTabs(side.tabs, side.category, side.counts, deal ? std::nullopt : point);
            const int slots = side.grid.columns * side.grid.rows, first = side.firstRow * side.grid.columns;
            for (int slot = 0; slot < slots; ++slot) {
                const auto cell = side.grid.Cell(slot);
                const int index = first + slot;
                const bool hot = hover && hover->side == i && hover->index == index;
                const bool selected = keyboard && keyboard->side == i && keyboard->index == index && Item(*keyboard);
                const auto* entry = Item({i, index});
                const bool equipped = entry && (entry->item->flags & ITM_FLAG_ACTIVE);
                ui.Fill(cell, equipped ? 0xFF292D1E : 0xFF1D2018);
                ui.Frame(cell, hot ? Bright : selected ? Bronze : Rule);
                if (entry && entry->entry.quantity > 1) ui.Text(cell.x + 3, cell.Bottom() - h,
                    std::to_string(entry->entry.quantity), cell.width - 7, Bright, true);
            }
            ui.ScrollBar(side.scrollbar, side.grid.rows,
                         (static_cast<int>(side.items.size()) + side.grid.columns - 1) / side.grid.columns, side.firstRow);
        }
        ui.Fill({left, footer.y, footer.width, 1}, Rule);
        ui.Text(left, footer.y + 8, status.empty() ? ui.Label(L"ЛКМ  Купить или продать", "Click  Buy or sell") : status,
                bulkButton.x - left - 10, Bronze);
        Control(bulkButton, ui.Label(L"Продать отложенное", "Sell set-aside items"), deal ? std::nullopt : point, !marked.empty());
        zrenderer->SetAlphaBlendSource(alpha);

        oCItem::SetLightingSwell(FALSE);
        for (int i = 0; i < 2; ++i) {
            const auto& side = sides[i];
            const int slots = side.grid.columns * side.grid.rows, first = side.firstRow * side.grid.columns;
            for (int slot = 0; slot < slots; ++slot) {
                const auto* entry = Item({i, first + slot});
                if (!entry) continue;
                const auto cell = side.grid.Cell(slot);
                Place(ui.preview, {cell.x + 3, cell.y + 4, cell.width - 6, cell.height - 9});
                entry->item->RenderItem(Container(dialog, i)->rndWorld, &ui.preview, 0.0f);
                ui.DrawMissingVisualName(*entry->item, entry->entry.quantity, cell);
            }
        }
        oCItem::SetLightingSwell(TRUE);
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        for (int i = 0; i < 2; ++i) {
            const auto& side = sides[i];
            const int slots = side.grid.columns * side.grid.rows, first = side.firstRow * side.grid.columns;
            for (int slot = 0; slot < slots; ++slot) {
                if (const auto* entry = Item({i, first + slot})) ui.DrawItemMarkers(*entry->item, side.grid.Cell(slot));
            }
        }
        zrenderer->SetAlphaBlendSource(alpha);
        if (deal) {
            zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
            ui.textView.DrawItems();
            ui.textView.ClrPrintwin();
            DrawDeal(point);
            zrenderer->SetAlphaBlendSource(alpha);
        }
        ui.textView.DrawItems();
        if (hover) {
            const auto* entry = Item(*hover);
            const auto price = hover->side == 1 && entry->price <= 0 ? ui.Label(L"Предмет не продаётся", "Item cannot be sold")
                : (hover->side == 0 ? ui.Label(L"Покупка за штуку", "Buy per item") : ui.Label(L"Продажа за штуку", "Sell per item"))
                    + "  " + std::to_string(entry->price);
            ui.DrawTooltip(entry->item, entry->entry.quantity, point, price);
        } else {
            ui.DrawTooltip(nullptr, 0, std::nullopt);
            if (point && !deal && !ui.DrawTabTooltip(sides[0].tabs, *point)) ui.DrawTabTooltip(sides[1].tabs, *point);
        }
        ui.DrawCursor(point);
    }
};
