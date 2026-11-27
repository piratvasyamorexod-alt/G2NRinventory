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

constexpr auto InventorySaveFile = L"G2NRINVENTORY.SAV";

std::filesystem::path InventorySaveFolder(oCSavegameManager* manager, int slot) {
    if (!zoptions || !manager) return {};
    const auto& root = zoptions->GetDirString(DIR_SAVEGAMES);
    const auto slotName = manager->GetSlotDirName(slot);
    if (root.IsEmpty() || slotName.IsEmpty()) return {};
    return std::filesystem::path(root.ToChar()) / slotName.ToChar();
}

bool RestoreHistory(std::istream& input) {
    InventoryHistory::History restored;
    if (!restored.Load(input, [](const std::string& name) { return parser->GetIndex(zSTRING(name.c_str())); })) return false;
    history = std::move(restored);
    return true;
}

bool ReadHistory(const std::filesystem::path& path) {
    if (!parser) return false;
    std::error_code error;
    if (std::filesystem::file_size(path, error) > 1024 * 1024 || error) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string header;
    std::getline(in, header);
    in.clear();
    in.seekg(0);
    if (header != "ZenGin Archive" && header != "ZenGin Archive\r") return RestoreHistory(in);
    in.close();

    auto* archive = zarcFactory->CreateArchiverRead(zSTRING(path.string().c_str()), 0);
    if (!archive) return false;
    const int count = archive->ReadInt("recordCount");
    const bool valid = count > 0 && count <= 16384;
    std::stringstream records;
    if (valid) {
        for (int i = 0; i < count; ++i) {
            zSTRING line;
            archive->ReadString("record", line);
            records << line.ToChar() << '\n';
        }
    }
    archive->Close();
    archive->Release();
    return valid && RestoreHistory(records);
}

bool WriteHistory(const std::filesystem::path& path) {
    if (!parser) return false;
    std::stringstream records;
    history.Save(records, [](int index) -> std::string {
        if (auto* symbol = parser->GetSymbol(index)) return symbol->name.ToChar();
        return {};
    });
    std::vector<std::string> lines;
    for (std::string line; std::getline(records, line);) lines.push_back(std::move(line));
    if (lines.size() > 16384) return false;

    auto temporary = path;
    temporary += L".TMP";
    auto* archive = zarcFactory->CreateArchiverWrite(
        zSTRING(temporary.string().c_str()), zARC_MODE_BINARY_SAFE, TRUE, 0);
    if (!archive) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    archive->WriteInt("recordCount", static_cast<int>(lines.size()));
    for (const auto& line : lines) archive->WriteString("record", zSTRING(line.c_str()));
    archive->Close();
    archive->Release();

    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temporary.c_str());
    return false;
}

void SaveInventory(oCSavegameManager* manager, int slot) {
    const auto directory = InventorySaveFolder(manager, slot);
    if (directory.empty() || !WriteHistory(directory / InventorySaveFile)) {
        OutputDebugStringW(L"G2NRinventory: could not write G2NRINVENTORY.SAV.\n");
        if (zerr) zerr->Warning(zSTRING("G2NRinventory: could not write G2NRINVENTORY.SAV"));
    }
}

void RestoreInventory(oCSavegameManager* manager, int slot) {
    history = {};
    const auto directory = InventorySaveFolder(manager, slot);
    if (!directory.empty() && !ReadHistory(directory / InventorySaveFile)) {
        ReadHistory(directory / L"G2NRinventory.dat");
    }
}

const int* unionSaveSlot = nullptr;

void __cdecl OnUnionSaveEnd() {
    if (ogame && unionSaveSlot && *unionSaveSlot >= 0) SaveInventory(ogame->savegameManager, *unionSaveSlot);
}

void __cdecl OnUnionLoadEnd() {
    if (ogame && unionSaveSlot && *unionSaveSlot >= 0) RestoreInventory(ogame->savegameManager, *unionSaveSlot);
}

void __cdecl UnusedUnionSaveEvent() {}

bool RegisterUnionSaveEvents() {
    // Legacy Union exposes its application callbacks and current slot through Shw32.
    // Resolve them at runtime so the native engine hooks can still serve other loaders.
    if (GetGameVersion() != Engine_G2A) return false;
    const auto core = GetModuleHandleW(L"Shw32.dll");
    if (!core) return false;
    using Event = void (__cdecl*)();
    using Register = void* (__cdecl*)(
        Event, Event, Event, Event, Event, Event, Event,
        Event, Event, Event, Event, Event, Event, Event,
        Event, Event, Event, Event, Event, Event, Event);
    const auto install = reinterpret_cast<Register>(GetProcAddress(core,
        "?CreateRefApplication@CApplication@UnionCore@@SAPAV12@P6AXXZ00000000000000000000@Z"));
    unionSaveSlot = reinterpret_cast<const int*>(GetProcAddress(core,
        "?SaveLoadGameInfo@UnionCore@@3UTSaveLoadGameInfo@1@A"));
    if (!install || !unionSaveSlot) return false;
    constexpr Event idle = UnusedUnionSaveEvent;
    return install(
        idle, idle, idle, idle, idle, idle, idle, // Entry through MenuLoop
        idle, OnUnionSaveEnd,
        idle, idle, // New game
        idle, OnUnionLoadEnd, // Load a saved game
        idle, idle, idle, idle, idle, idle, idle, idle) != nullptr;
}

const bool useUnionSaveEvents = RegisterUnionSaveEvents();

void __fastcall SaveInventoryWithSlot(oCSavegameManager*, void*, int);
auto SaveInventoryWithSlotHook = Union::CreateHook(
    useUnionSaveEvents ? nullptr : SIGNATURE_OF(&oCSavegameManager::CopyFromCurrent),
    &SaveInventoryWithSlot, Union::HookType::Hook_Detours);

void __fastcall SaveInventoryWithSlot(oCSavegameManager* manager, void* edx, int slot) {
    if (slot >= 0) SaveInventory(manager, -1);
    SaveInventoryWithSlotHook(manager, edx, slot);
}

void __fastcall LoadHistory(oCGame*, void*, int, int);
auto LoadHistoryHook = Union::CreateHook(
    SIGNATURE_OF(&oCGame::LoadSavegame), &LoadHistory, Union::HookType::Hook_Detours);

void __fastcall LoadHistory(oCGame* game, void* edx, int slot, int globals) {
    const bool previous = std::exchange(trackingPaused, true);
    LoadHistoryHook(game, edx, slot, globals);
    trackingPaused = previous;
    if (!useUnionSaveEvents && slot >= 0 && globals && player) {
        RestoreInventory(game->savegameManager, -1);
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
