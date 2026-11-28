#pragma once

using InventoryLayout::HotbarSlots;

enum class HotbarAction { None, Equip, Use, Torch };

HotbarAction ActionFor(oCItem& item) {
    if (item.flags & ITM_FLAG_TORCH) return HotbarAction::Torch;
    constexpr int equipment = ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_ARMOR | ITM_CAT_RUNE;
    constexpr int accessories = ITM_FLAG_SHIELD | ITM_FLAG_RING | ITM_FLAG_AMULET | ITM_FLAG_BELT;
    if ((item.mainflag & equipment) || (item.flags & accessories)) return HotbarAction::Equip;
    if (item.GetStateEffectFunc(0) > 0 || !item.scemeName.IsEmpty()) return HotbarAction::Use;
    return HotbarAction::None;
}

struct HotbarStock { oCItem* item = nullptr; int amount = 0; };

HotbarStock StockFor(oCNpcInventory& inv, int instance) {
    HotbarStock result;
    if (instance < 0) return result;
    int reserved = history.ForSale(instance);
    for (auto* node = inv.inventory.GetNextInList(); node; node = node->GetNextInList()) {
        auto* item = node->GetData();
        if (!item || item->amount <= 0 || item->GetInstance() != instance) continue;
        const int held = item->flags & ITM_FLAG_ACTIVE ? 0 : InventoryLayout::AllocateSale(item->amount, reserved);
        const int available = item->amount - held;
        if (available <= 0) continue;
        if (!result.item || (item->flags & ITM_FLAG_ACTIVE)) result.item = item;
        result.amount += (std::min)(available, INT_MAX - result.amount);
    }
    return result;
}

void MaterializeHotbarItem(oCNpcInventory& inv, int instance) {
    if (instance < 0 || inv.packString.IsEmpty()) return;
    const bool previous = std::exchange(trackingPaused, true);
    inv.IsIn(instance, 1);
    trackingPaused = previous;
}

bool HotbarInWorld() {
    if (!ogame || !player || !player->GetHomeWorld() || !screen || !screen->GetFont()
        || screen->psizex < 480 || screen->psizey < 360 || ogame->inLoadSaveGame
        || ogame->IsOnPause() || !ogame->GetShowPlayerStatus() || zCMenu::GetActive() || player->IsDead()
        || player->inventory2.IsOpen()
        || player->IsInGlobalCutscene() || !oCInformationManager::GetInformationManager().HasFinished()) return false;
    for (auto* node = oCItemContainer::contList.GetNextInList(); node; node = node->GetNextInList()) {
        auto* container = node->GetData();
        if (container && container->IsOpen()) return false;
    }
    return true;
}

bool IsInstantConsumable(oCItem& item) {
    constexpr int consumables = ITM_CAT_FOOD | ITM_CAT_POTION;
    // Items with a longer interaction sequence still need the native state machine.
    return (item.mainflag & consumables) && ActionFor(item) == HotbarAction::Use
        && item.GetStateEffectFunc(0) > 0 && item.GetStateEffectFunc(1) <= 0
        && item.GetStateEffectFunc(2) <= 0 && item.GetStateEffectFunc(3) <= 0;
}

void ConsumeInstantly(oCNpc& npc, oCItem& item) {
    if (!parser || npc.IsDead() || npc.GetInteractMob() || npc.interactItem
        || !npc.GetHomeWorld() || item.amount <= 0 || (item.flags & ITM_FLAG_ACTIVE)) return;

    const int body = npc.GetBodyState();
    const int state = body & BS_ONLY_STATE;
    if ((body & (BS_MOD_TRANSFORMED | BS_MOD_CONTROLLED | BS_MOD_CONTROLLING))
        || (state > BS_SPRINT && state != BS_INVENTORY && state != BS_STUMBLE
            && (state < BS_AIMNEAR || state > BS_CASTING))) return;

    auto* scripts = parser;
    const int callback = item.GetStateEffectFunc(0);
    const auto* effect = scripts->GetSymbol(callback);
    const int selfIndex = scripts->GetIndex(zSTRING("SELF"));
    const int itemIndex = scripts->GetIndex(zSTRING("ITEM"));
    auto* selfSymbol = scripts->GetSymbol(selfIndex);
    auto* itemSymbol = scripts->GetSymbol(itemIndex);
    if (!effect || effect->type != zPAR_TYPE_FUNC || effect->ele != 0
        || !selfSymbol || selfSymbol->type != zPAR_TYPE_INSTANCE
        || !itemSymbol || itemSymbol->type != zPAR_TYPE_INSTANCE || !npc.CanUse(&item)) return;

    auto* previousSelf = selfSymbol->GetInstanceAdr();
    auto* previousItem = itemSymbol->GetInstanceAdr();
    auto* consumed = npc.RemoveFromInv(&item, 1);
    if (!consumed) return;
    consumed->AddRef();

    // The animated use event binds SELF and ITEM before calling on_state[0].
    scripts->SetInstance(selfIndex, &npc);
    scripts->SetInstance(itemIndex, consumed);
    scripts->CallFunc(callback);
    scripts->SetInstance(selfIndex, previousSelf);
    scripts->SetInstance(itemIndex, previousItem == consumed ? nullptr : previousItem);

    if (auto* world = consumed->GetHomeWorld()) world->RemoveVob(consumed);
    consumed->Release();
}

void UseHotbar(int slot) {
    if (!player->GetAnictrl() || player->GetInteractMob() || player->interactItem) return;
    auto& inv = player->inventory2;
    const int instance = history.Hotbar()[slot];
    auto stock = StockFor(inv, instance);
    if (!stock.item) {
        MaterializeHotbarItem(inv, instance);
        stock = StockFor(inv, instance);
    }
    if (!stock.item) return;
    if (IsInstantConsumable(*stock.item)) {
        ConsumeInstantly(*player, *stock.item);
        return;
    }

    const int state = player->GetBodyState() & BS_ONLY_STATE;
    if (state < BS_STAND || state > BS_SPRINT) return;
    const auto action = ActionFor(*stock.item);
    if (action == HotbarAction::None || (action == HotbarAction::Equip && (stock.item->flags & ITM_FLAG_ACTIVE))) return;
    auto* events = player->GetEM();
    if (!events || !events->IsEmpty(TRUE)) return;
    if (action == HotbarAction::Torch) {
        player->UseItem(stock.item);
        return;
    }
    auto* symbol = parser ? parser->GetSymbol(instance) : nullptr;
    if (!symbol) return;
    // The inventory can change before the native event runs. Let the engine
    // look up the current stack by its script instance when it handles the event.
    const auto type = action == HotbarAction::Equip ? oCMsgManipulate::EV_EQUIPITEM : oCMsgManipulate::EV_USEITEMTOSTATE;
    events->OnMessage(new oCMsgManipulate(type, symbol->name, 0), player);
}

class HotbarPanel {
    zCView panel{0, 0, 8192, 8192};
    zCView solid{0, 0, 8, 8};
    zCView text{0, 0, 8192, 8192};
    zCView preview{0, 0, 8, 8};
    zCWorld* world = nullptr;
    oCNpcInventory* previousInventory = nullptr;
    std::string previousPack;
    std::array<int, HotbarSlots> previousBindings{};

    void Fill(Box box, unsigned long color) {
        Place(solid, box);
        solid.SetColor(Ink(color));
        solid.Blit();
    }

    void Text(Box cell, const std::string& label, unsigned long color, bool amount = false) {
        const zSTRING value(label.c_str());
        const int width = text.GetFont()->GetFontX(value);
        const int x = (amount ? cell.Right() - width - 3 : cell.x + 3) - text.pposx;
        const int y = (amount ? cell.Bottom() - text.GetFont()->GetFontY() - 2 : cell.y + 2) - text.pposy;
        if (width > cell.width - 6) return;
        auto ink = Ink(color);
        auto shadow = Ink(0xFF0A0907);
        text.CreateText(text.anx(x), text.any(y), value, 0, ink, FALSE, TRUE);
        text.CreateText(text.anx(x + 1), text.any(y + 1), value, 0, shadow, FALSE, TRUE);
    }

    void Render(oCItem& item, oCNpcInventory& inv, Box box) {
        if (!inv.rndWorld && !world) {
            world = new zCWorld();
            world->m_bIsInventoryWorld = TRUE;
        }
        Place(preview, box);
        DrawItemPreview(item, inv.rndWorld ? inv.rndWorld : world, preview);
    }

public:
    HotbarPanel() {
        InitSprite(panel, "G2NRI_PANEL.TGA", 248);
        InitSprite(solid, "G2NRI_SOLID.TGA");
    }

    ~HotbarPanel() { if (world) world->Release(); }

    void Draw(oCNpcInventory& inv, bool editing, std::optional<POINT> point, oCItem* dragged) {
        if (!panel.backTex || !solid.backTex) return;
        const auto& bindings = history.Hotbar();
        const std::string_view pack(inv.packString.ToChar());
        if (!editing && (previousInventory != &inv || previousPack != pack || previousBindings != bindings)) {
            for (int instance : bindings) MaterializeHotbarItem(inv, instance);
            previousInventory = &inv;
            previousPack = inv.packString.ToChar();
            previousBindings = bindings;
        }
        std::array<HotbarStock, HotbarSlots> stock{};
        for (int i = 0; i < HotbarSlots; ++i) stock[i] = StockFor(inv, bindings[i]);
        const auto layout = InventoryLayout::PlaceHotbar(screen->psizex, screen->psizey);
        const auto hover = point ? layout.Hit(point->x, point->y) : -1;
        Place(panel, layout.bounds);
        Place(text, layout.bounds);
        text.SetFont(screen->GetFont());
        text.ClrPrintwin();
        const auto alpha = zrenderer->GetAlphaBlendSource();
        zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
        panel.Blit();
        for (int i = 0; i < HotbarSlots; ++i) {
            const auto cell = layout.Cell(i);
            const bool equipped = stock[i].item && (stock[i].item->flags & ITM_FLAG_ACTIVE);
            Fill(cell, i == hover ? Bright : equipped ? Equipped : Rule);
            Fill({cell.x + 1, cell.y + 1, cell.width - 2, cell.height - 2},
                 i == hover && dragged ? 0xFF35351F : equipped ? 0xFF292D1E : 0xFF1D2018);
        }
        zrenderer->SetAlphaBlendSource(alpha);
        oCItem::SetLightingSwell(FALSE);
        for (int i = 0; i < HotbarSlots; ++i) {
            const auto cell = layout.Cell(i);
            if (stock[i].item) {
                Render(*stock[i].item, inv, {cell.x + 6, cell.y + 8, cell.width - 12, cell.height - 12});
            }
            const bool missing = bindings[i] >= 0 && !stock[i].item;
            Text(cell, "F" + std::to_string(i + 1), i == hover ? Bright : missing ? Missing : Bronze);
            if (stock[i].amount > 1 || missing) Text(cell, std::to_string(stock[i].amount), missing ? Missing : Bright, true);
        }
        text.DrawItems();
        if (dragged && point) {
            const int size = layout.cellSize;
            const int x = std::clamp(static_cast<int>(point->x) + 15, 1, screen->psizex - size - 1);
            const int y = std::clamp(static_cast<int>(point->y) + 15, 1, screen->psizey - size - 1);
            zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);
            Fill({x, y, size, size}, Bronze);
            Fill({x + 1, y + 1, size - 2, size - 2}, 0xED202318);
            zrenderer->SetAlphaBlendSource(alpha);
            Render(*dragged, inv, {x + 4, y + 4, size - 8, size - 8});
        }
        oCItem::SetLightingSwell(TRUE);
    }
};
