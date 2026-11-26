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

std::optional<InventoryHistory::DocumentState> DocumentReadingState(oCItem& item) {
    if (!ReadableDocument(item)) return std::nullopt;
    return history.ReadingState(item.GetInstance());
}

std::optional<int> DocumentScriptInt(const char* name) {
    auto* symbol = parser ? parser->GetSymbol(zSTRING(name)) : nullptr;
    if (!symbol || symbol->type != zPAR_TYPE_INT || !symbol->ele) return std::nullopt;
    int value = 0;
    symbol->GetValue(value, 0);
    return value;
}

void ObserveOwnedDocuments(oCNpcInventory& inv) {
    const auto ironbeardProgress = DocumentScriptInt("TAMORAEINSIEDLERBRIEFGELESEN");
    for (auto* node = inv.inventory.GetNextInList(); node; node = node->GetNextInList()) {
        auto* item = node->GetData();
        const int instance = DocumentInstance(item);
        if (instance < 0) continue;
        history.ObserveDocument(instance);
        if (!ironbeardProgress || history.ReadingState(instance) == InventoryHistory::DocumentState::Read) continue;

        const auto* itemSymbol = parser->GetSymbol(instance);
        const auto* readerSymbol = parser->GetSymbol(item->GetStateEffectFunc(0));
        if (!itemSymbol || itemSymbol->name.IsEmpty() || !readerSymbol || readerSymbol->name.IsEmpty()) continue;
        auto itemName = itemSymbol->name;
        auto readerName = readerSymbol->name;
        if (OdysseeDocumentReads::HasReadEvidence(itemName.Upper().ToChar(), readerName.Upper().ToChar(), *ironbeardProgress,
                                                [](const char* flag) { return DocumentScriptInt(flag) == 1; })) {
            history.MarkRead(instance);
        }
    }
}

std::filesystem::path HistoryPath(oCSavegameManager* manager, int slot) {
    if (!zoptions || !manager || slot < 0) return {};
    const auto& directory = zoptions->GetDirString(DIR_SAVEGAMES);
    const auto name = manager->GetSlotDirName(slot);
    if (directory.IsEmpty() || name.IsEmpty()) return {};
    return std::filesystem::path(directory.ToChar()) / name.ToChar() / L"G2NRinventory.dat";
}

void ReadHistory(const std::filesystem::path& path) {
    history = {};
    if (path.empty()) return;
    std::error_code error;
    if (std::filesystem::file_size(path, error) > 1024 * 1024) return;
    std::ifstream in(path, std::ios::binary);
    history.Load(in, [](const std::string& name) { return parser->GetIndex(zSTRING(name.c_str())); });
}

void WriteHistory(const std::filesystem::path& path) {
    if (path.empty()) return;
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream out(temporary, std::ios::binary);
    if (!out) return;
    history.Save(out, [](int index) -> std::string {
        if (auto* symbol = parser->GetSymbol(index)) {
            return symbol->name.ToChar();
        }
        return {};
    });
    out.close();
    if (!out || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
    }
}

void __fastcall SaveHistory(oCSavegameManager*, void*, int, oCSavegameInfo*);
auto SaveHistoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCSavegameManager::SetAndWriteSavegame), &SaveHistory, Union::HookType::Hook_Detours);

void __fastcall SaveHistory(oCSavegameManager* manager, void* edx, int slot, oCSavegameInfo* info) {
    SaveHistoryHook(manager, edx, slot, info);
    if (player) WriteHistory(HistoryPath(manager, slot));
}

void __fastcall LoadHistory(oCGame*, void*, int, int);
auto LoadHistoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::LoadSavegame), &LoadHistory, Union::HookType::Hook_Detours);

void __fastcall LoadHistory(oCGame* game, void* edx, int slot, int globals) {
    const bool previous = std::exchange(trackingPaused, true);
    LoadHistoryHook(game, edx, slot, globals);
    trackingPaused = previous;
    if (globals && slot >= 0) {
        ReadHistory(HistoryPath(game->savegameManager, slot));
        if (player) ObserveOwnedDocuments(player->inventory2);
    }
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
    const bool document = ReadableDocument(*item);
    const bool reserved = history.ForSale(instance) > 0;
    bool alreadyOwned = false;
    int owned = 0;
    for (auto* node = inv->inventory.GetNextInList(); node; node = node->GetNextInList()) {
        auto* existing = node->GetData();
        if (existing == item) return InsertHook(inv, edx, item);
        if (!existing || (!document && !reserved) || existing->GetInstance() != instance || existing->amount <= 0) continue;
        alreadyOwned = true;
        if (reserved && !(existing->flags & ITM_FLAG_ACTIVE)) {
            owned += std::clamp(existing->amount, 0, INT_MAX - owned);
        }
    }
    const bool unpacked = (document || reserved) && inv->GetNumItemsInPackString() == 0;
    if (reserved && unpacked) history.CapForSale(instance, owned);
    auto* result = InsertHook(inv, edx, item);
    if (result) {
        history.Acquire(instance);
        if (document) history.ObserveDocument(instance, unpacked && !alreadyOwned);
    }
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
