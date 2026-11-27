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
constexpr unsigned long MaximumInventoryArchiveSize = 1024 * 1024;

std::filesystem::path PendingInventorySave(std::filesystem::path path) {
    path += L".TMP";
    return path;
}

std::filesystem::path GameSystemDirectory() {
    wchar_t executable[32768]{};
    const auto length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (!length || length >= std::size(executable)) return {};
    return std::filesystem::path(executable).parent_path();
}

std::filesystem::path InventorySaveFolder(oCSavegameManager* manager, int slot) {
    if (!zoptions || !manager) return {};
    const auto& configuredRoot = zoptions->GetDirString(DIR_SAVEGAMES);
    const auto slotName = manager->GetSlotDirName(slot);
    if (configuredRoot.IsEmpty() || slotName.IsEmpty()) return {};

    std::filesystem::path root(configuredRoot.ToChar());
    if (root.is_relative()) {
        // Menus and the engine change the working directory. Anchor relative
        // save paths to the game root for both writing and loading.
        const auto& engineRoot = zFILE::s_rootDirString;
        auto gameRoot = std::filesystem::path(engineRoot.ToChar());
        if (!gameRoot.is_absolute()) gameRoot = GameSystemDirectory().parent_path();
        if (gameRoot.empty()) return {};
        root = gameRoot / root;
    }
    return (root / slotName.ToChar()).make_preferred();
}

bool RestoreHistory(std::istream& input) {
    InventoryHistory::History restored;
    if (!restored.Load(input, [](const std::string& name) { return parser->GetIndex(zSTRING(name.c_str())); })) return false;
    history = std::move(restored);
    return true;
}

bool ReadHistoryRecords(const std::filesystem::path& path, std::string& records, std::string& status) {
    // VFILE::Open can show a fatal error on missing files. Our sidecar is
    // optional, including when a save was created without the plugin.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        status = "file unavailable";
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaximumInventoryArchiveSize)) {
        status = "invalid size=" + std::to_string(static_cast<std::streamoff>(size));
        return false;
    }
    file.seekg(0);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        status = "short read";
        return false;
    }

    if (bytes.compare(0, 14, "ZenGin Archive") == 0) {
        if (InventoryArchive::ExtractRecords(bytes, records)) return true;
        status = "archive layout unrecognized, bytes=" + std::to_string(static_cast<std::streamoff>(size));
        return false;
    }
    records = std::move(bytes);
    return true;
}

bool ReadHistory(const std::filesystem::path& path, std::string& status) {
    std::string records;
    if (!ReadHistoryRecords(path, records, status)) return false;
    if (!parser) {
        status = "no script parser";
        return false;
    }
    std::istringstream input(records);
    if (RestoreHistory(input)) return true;
    status = "inventory records invalid";
    return false;
}

bool WriteHistory(const std::filesystem::path& path) {
    if (!parser) return false;
    std::ostringstream records;
    history.Save(records, [](int index) -> std::string {
        if (auto* symbol = parser->GetSymbol(index)) return symbol->name.ToChar();
        return {};
    });
    const auto bytes = records.str();
    if (!records || bytes.size() > MaximumInventoryArchiveSize) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    return static_cast<bool>(file);
}

std::size_t BoundHotbarSlots() {
    const auto& slots = history.Hotbar();
    return std::count_if(slots.begin(), slots.end(), [](int instance) { return instance >= 0; });
}

void LogInventorySave(std::string_view operation, int slot, std::string_view outcome) {
    const auto system = GameSystemDirectory();
    if (system.empty()) return;
    const auto path = system / L"G2NRinventory-save.log";
    std::error_code error;
    const auto length = std::filesystem::file_size(path, error);
    const auto mode = !error && length >= 65536 ? std::ios::trunc : std::ios::app;
    std::ofstream log(path, mode);
    if (log) {
        log << "0.1.21 " << operation << " slot=" << slot << " " << outcome
            << " bindings=" << BoundHotbarSlots() << '\n';
    }
}

void SaveInventory(oCSavegameManager* manager, int slot) {
    const auto directory = InventorySaveFolder(manager, slot);
    const auto path = directory / InventorySaveFile;
    const bool written = !directory.empty() && WriteHistory(path);
    std::string detail = written ? "archive verified" : "write failed";
    std::string records;
    if (written && !ReadHistoryRecords(path, records, detail)) detail = "after write: " + detail;
    LogInventorySave("save", slot, detail);
    if (!written && zerr) zerr->Warning(zSTRING("G2NRinventory: could not write optional G2NRINVENTORY.SAV"));
}

void RestoreInventory(oCSavegameManager* manager, int slot) {
    history = {};
    const auto directory = InventorySaveFolder(manager, slot);
    if (directory.empty()) {
        LogInventorySave("load", slot, "no directory");
        return;
    }
    const auto path = directory / InventorySaveFile;
    std::string archiveStatus, pendingStatus, legacyStatus;
    if (ReadHistory(path, archiveStatus)) {
        LogInventorySave("load", slot, "SAV");
    } else if (ReadHistory(PendingInventorySave(path), pendingStatus)) {
        LogInventorySave("load", slot, "TMP");
    } else if (ReadHistory(directory / L"G2NRinventory.dat", legacyStatus)) {
        LogInventorySave("load", slot, "legacy");
    } else {
        LogInventorySave("load", slot, "SAV=" + archiveStatus + ", TMP=" + pendingStatus + ", legacy=" + legacyStatus);
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
