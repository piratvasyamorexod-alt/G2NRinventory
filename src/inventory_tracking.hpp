#pragma once

InventoryHistory::History history;
bool trackingPaused = false;
bool usingItemState = false;
int directDocument = -1;

bool ReadableDocument(oCItem& item) {
    return (item.mainflag & ITM_CAT_DOCS) && item.GetStateEffectFunc(0) >= 0;
}

int DocumentInstance(oCItem* item) {
    return item && ReadableDocument(*item) ? item->GetInstance() : -1;
}

std::filesystem::path FindSaveFolder(oCSavegameManager* manager, int slot) {
    if (!zoptions || !manager || slot < 0) return {};
    const auto slotName = manager->GetSlotDirName(slot);
    auto* info = manager->GetSavegame(slot);
    const std::array<std::filesystem::path, 2> names{info ? info->m_Dir.ToChar() : "", slotName.ToChar()};
    const auto* optionPath = zoptions->GetDirString(DIR_SAVEGAMES).ToChar();
    auto* nativeDirectory = zoptions->GetDir(DIR_SAVEGAMES);
    const auto nativePath = nativeDirectory ? nativeDirectory->GetDir() : zSTRING();
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    const std::array<std::filesystem::path, 3> roots{optionPath, nativePath.ToChar(), current};
    for (const auto& root : roots) {
        if (root.empty()) continue;
        for (const auto& name : names) {
            if (name.empty()) continue;
            const auto folder = root / name;
            if (std::filesystem::is_regular_file(folder / L"SAVEHDR.SAV", error)) return folder;
        }
    }
    return {};
}

bool ReadHistory(const std::filesystem::path& path) {
    if (path.empty() || !parser) return false;
    std::error_code error;
    if (std::filesystem::file_size(path, error) > 1024 * 1024 || error) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    InventoryHistory::History restored;
    if (!restored.Load(in, [](const std::string& name) { return parser->GetIndex(zSTRING(name.c_str())); })) return false;
    history = std::move(restored);
    return true;
}

bool WriteHistory(const std::filesystem::path& path) {
    if (path.empty() || !parser) return false;
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream out(temporary, std::ios::binary);
    if (!out) return false;
    history.Save(out, [](int index) -> std::string {
        if (auto* symbol = parser->GetSymbol(index)) {
            return symbol->name.ToChar();
        }
        return {};
    });
    out.close();
    if (!out || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

void SaveInventoryFor(oCSavegameManager* manager, int slot) {
    const auto directory = FindSaveFolder(manager, slot);
    if (!directory.empty() && WriteHistory(directory / L"G2NRINVENTORY.SAV")) return;
    OutputDebugStringW(L"G2NRinventory: could not save inventory data to the game slot.\n");
}

void LoadInventoryFor(oCSavegameManager* manager, int slot) {
    history = {};
    const auto directory = FindSaveFolder(manager, slot);
    if (directory.empty()) return;
    if (!ReadHistory(directory / L"G2NRINVENTORY.SAV")) ReadHistory(directory / L"G2NRinventory.dat");
}

bool managerSaving = false, managerLoading = false;

void __fastcall SaveFromManager(CGameManager*, void*, int);
auto SaveManagerHook = Union::CreateHook(
    SIGNATURE_OF(&CGameManager::Write_Savegame), &SaveFromManager, Union::HookType::Hook_Detours);

void __fastcall SaveFromManager(CGameManager* manager, void* edx, int slot) {
    const bool previous = std::exchange(managerSaving, true);
    SaveManagerHook(manager, edx, slot);
    managerSaving = previous;
    if (!previous && slot >= 0 && player) SaveInventoryFor(manager->savegameManager, slot);
}

void __fastcall SaveHistory(oCGame*, void*, int, int);
auto SaveHistoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::WriteSavegame), &SaveHistory, Union::HookType::Hook_Detours);

void __fastcall SaveHistory(oCGame* game, void* edx, int slot, int globals) {
    SaveHistoryHook(game, edx, slot, globals);
    if (!managerSaving && globals && slot >= 0 && player) SaveInventoryFor(game->savegameManager, slot);
}

void __fastcall LoadFromManager(CGameManager*, void*, int);
auto LoadManagerHook = Union::CreateHook(
    SIGNATURE_OF(&CGameManager::Read_Savegame), &LoadFromManager, Union::HookType::Hook_Detours);

void __fastcall LoadFromManager(CGameManager* manager, void* edx, int slot) {
    const bool previous = std::exchange(managerLoading, true);
    const bool wasTrackingPaused = std::exchange(trackingPaused, true);
    LoadManagerHook(manager, edx, slot);
    trackingPaused = wasTrackingPaused;
    managerLoading = previous;
    if (!previous && slot >= 0 && player) LoadInventoryFor(manager->savegameManager, slot);
}

void __fastcall LoadHistory(oCGame*, void*, int, int);
auto LoadHistoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::LoadSavegame), &LoadHistory, Union::HookType::Hook_Detours);

void __fastcall LoadHistory(oCGame* game, void* edx, int slot, int globals) {
    const bool previous = std::exchange(trackingPaused, true);
    LoadHistoryHook(game, edx, slot, globals);
    trackingPaused = previous;
    if (!managerLoading && globals && slot >= 0 && player) LoadInventoryFor(game->savegameManager, slot);
}

void __fastcall NewGame(oCGame*, void*, int, const zSTRING&);
auto NewGameHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::LoadGame), &NewGame, Union::HookType::Hook_Detours);

void __fastcall NewGame(oCGame* game, void* edx, int slot, const zSTRING& world) {
    history = {};
    NewGameHook(game, edx, slot, world);
}

void __fastcall UnpackInventory(oCNpcInventory*, void*);
auto UnpackInventoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::UnpackItemsInCategory), &UnpackInventory, Union::HookType::Hook_Detours);

void __fastcall UnpackInventory(oCNpcInventory* inv, void* edx) {
    const bool previous = std::exchange(trackingPaused, true);
    UnpackInventoryHook(inv, edx);
    trackingPaused = previous;
}

oCItem* __fastcall TrackInsert(oCNpcInventory*, void*, oCItem*);
auto InsertHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpcInventory::Insert), &TrackInsert, Union::HookType::Hook_Detours);

oCItem* __fastcall TrackInsert(oCNpcInventory* inv, void* edx, oCItem* item) {
    if (trackingPaused || !player || inv != &player->inventory2 || !item || item->amount <= 0
        || item == player->interactItem) return InsertHook(inv, edx, item);
    const int instance = item->GetInstance();
    const bool reconcileSale = history.ForSale(instance) > 0 && inv->GetNumItemsInPackString() == 0;
    int owned = 0;
    for (auto* node = inv->inventory.GetNextInList(); node; node = node->GetNextInList()) {
        auto* existing = node->GetData();
        if (existing == item) return InsertHook(inv, edx, item);
        if (reconcileSale && existing && !(existing->flags & ITM_FLAG_ACTIVE) && existing->GetInstance() == instance) {
            owned += std::clamp(existing->amount, 0, INT_MAX - owned);
        }
    }
    if (reconcileSale) history.CapForSale(instance, owned);
    auto* result = InsertHook(inv, edx, item);
    if (result) history.Acquire(instance);
    return result;
}

int __fastcall TrackUse(oCNpc*, void*, oCItem*);
auto TrackUseHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpc::UseItem), &TrackUse, Union::HookType::Hook_Detours);

int __fastcall TrackUse(oCNpc* npc, void* edx, oCItem* item) {
    const int previous = std::exchange(directDocument, npc == player ? DocumentInstance(item) : -1);
    const int result = TrackUseHook(npc, edx, item);
    directDocument = previous;
    return result;
}

int __fastcall TrackUseToState(oCNpc*, void*, oCMsgManipulate*);
auto TrackUseToStateHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpc::EV_UseItemToState), &TrackUseToState, Union::HookType::Hook_Detours);

int __fastcall TrackUseToState(oCNpc* npc, void* edx, oCMsgManipulate* message) {
    const bool previous = std::exchange(usingItemState, npc == player);
    const int result = TrackUseToStateHook(npc, edx, message);
    usingItemState = previous;
    return result;
}

void MarkDocumentShown() {
    if (trackingPaused || !player) return;
    const int instance = directDocument >= 0 ? directDocument : usingItemState ? DocumentInstance(player->interactItem) : -1;
    history.MarkRead(instance);
}

void __fastcall TrackDocument(oCDocumentManager*, int);
auto TrackDocumentHook = Union::CreateHook(
    SIGNATURE_OF(&oCDocumentManager::Show), &TrackDocument, Union::HookType::Hook_Detours);

void __fastcall TrackDocument(oCDocumentManager* manager, int id) {
    if ((usingItemState || directDocument >= 0) && manager->GetDocumentView(id)) MarkDocumentShown();
    TrackDocumentHook(manager, id);
}

void __fastcall TrackLegacyDocument(oCDoc*, void*);
auto TrackLegacyDocumentHook = Union::CreateHook(
    SIGNATURE_OF(&oCDoc::Show), &TrackLegacyDocument, Union::HookType::Hook_Detours);

void __fastcall TrackLegacyDocument(oCDoc* document, void* edx) {
    MarkDocumentShown();
    TrackLegacyDocumentHook(document, edx);
}
