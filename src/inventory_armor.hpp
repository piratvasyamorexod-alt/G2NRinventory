#pragma once

using InventoryHistory::ArmorLook;

bool IsBodyArmor(const oCItem& item) {
    return (item.mainflag & ITM_CAT_ARMOR) && !(item.flags & ITM_FLAG_SHIELD);
}

enum class ArmorAction { Full, Armor, Skin };

struct ArmorVisualPass {
    zSTRING mesh;
    int texture;
    zCModel* model = nullptr;
};

ArmorVisualPass* armorVisualPass = nullptr;
oCNpc* appearanceOwner = nullptr;
const zCVisual* appearanceVisual = nullptr;
const zCWorld* appearanceWorld = nullptr;
bool appearanceDirty = false;
bool appearanceOverridden = false;

bool InventoryAvailable();

void InvalidateArmorAppearance() { appearanceDirty = true; }

bool CanStyleHero(oCNpc* npc) {
    constexpr int alteredBody = BS_MOD_TRANSFORMED | BS_MOD_CONTROLLED | BS_MOD_CONTROLLING;
    return npc && npc == player && ogame && InventoryAvailable() && !trackingPaused && !ogame->inLoadSaveGame
        && npc->GetHomeWorld() && npc->GetVisual()
        && npc->IsHuman() && !(npc->GetBodyState() & alteredBody);
}

int __fastcall ApplyArmorMesh(zCModel*, void*, const zSTRING&);
auto ArmorMeshHook = Union::CreateHook(
    SIGNATURE_OF(&zCModel::ApplyMeshLib), &ApplyArmorMesh, Union::HookType::Hook_Detours);

int __fastcall ApplyArmorMesh(zCModel* model, void* edx, const zSTRING& mesh) {
    // This call inside InitModel selects the hero's body mesh.
    if (!armorVisualPass || _ReturnAddress() != reinterpret_cast<void*>(0x0073858E)) {
        return ArmorMeshHook(model, edx, mesh);
    }
    armorVisualPass->model = model;
    return ArmorMeshHook(model, edx, armorVisualPass->mesh);
}

int __fastcall ApplyArmorTexture(zCModel*, void*, const zSTRING&, int, int, zSTRING*);
auto ArmorTextureHook = Union::CreateHook(
    SIGNATURE_OF(&zCModel::SetMeshLibTexture), &ApplyArmorTexture, Union::HookType::Hook_Detours);

int __fastcall ApplyArmorTexture(zCModel* model, void* edx, const zSTRING& mesh, int channel, int variant, zSTRING* texture) {
    if (armorVisualPass && armorVisualPass->model == model) {
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        // Keep the native skin color and head variants. The third call sets the armor variant.
        if (caller == 0x007385CA || caller == 0x00738615 || caller == 0x00738659) {
            const int selected = caller == 0x00738659 ? armorVisualPass->texture : variant;
            return ArmorTextureHook(model, edx, armorVisualPass->mesh, channel, selected, texture);
        }
    }
    return ArmorTextureHook(model, edx, mesh, channel, variant, texture);
}

void __fastcall InitArmorModel(oCNpc*, void*);
auto InitArmorModelHook = Union::CreateHook(
    SIGNATURE_OF(&oCNpc::InitModel), &InitArmorModel, Union::HookType::Hook_Detours);

void __fastcall InitArmorModel(oCNpc* npc, void* edx) {
    if (npc != player || !InventoryAvailable()) {
        InitArmorModelHook(npc, edx);
        return;
    }
    const auto& look = history.GetArmorLook();
    std::optional<ArmorVisualPass> pass;
    if (look && CanStyleHero(npc)) {
        const bool body = look->mesh.empty();
        const char* name = body ? npc->body_visualName.ToChar() : look->mesh.c_str();
        if (name && *name) pass.emplace(ArmorVisualPass{zSTRING(name), body ? npc->body_TexVarNr : look->texture});
    }
    auto* outer = std::exchange(armorVisualPass, pass ? &*pass : nullptr);
    InitArmorModelHook(npc, edx);
    armorVisualPass = outer;
    if (npc && npc == player) {
        appearanceOwner = npc;
        appearanceVisual = npc->GetVisual();
        appearanceWorld = npc->GetHomeWorld();
        appearanceOverridden = pass && pass->model;
        appearanceDirty = false;
    }
}

void SyncArmorAppearance() {
    const bool hasLook = history.GetArmorLook().has_value();
    if (!hasLook && (!appearanceDirty || !appearanceOverridden)) return;
    if (!CanStyleHero(player)) {
        InvalidateArmorAppearance();
        return;
    }
    const bool newModel = appearanceOwner != player || appearanceVisual != player->GetVisual()
        || appearanceWorld != player->GetHomeWorld();
    if (!hasLook && newModel) {
        appearanceDirty = appearanceOverridden = false;
        return;
    }
    if (appearanceDirty || newModel) {
        player->InitModel();
    }
}

std::optional<ArmorLook> ItemArmorLook(oCItem& item) {
    if (item.visual_change.IsEmpty()) return std::nullopt;
    const auto mesh = item.GetVisualChange();
    if (mesh.IsEmpty()) return std::nullopt;
    return ArmorLook{mesh.ToChar(), item.visual_skin};
}

void ApplyArmorAction(oCNpc& npc, oCItem& item, ArmorAction action) {
    if (!CanStyleHero(&npc) || !IsBodyArmor(item)) return;
    auto look = history.GetArmorLook();
    if (action == ArmorAction::Skin) {
        look = ItemArmorLook(item);
        if (!look) return;
    } else {
        auto* equipped = npc.GetEquippedArmor();
        if (action == ArmorAction::Full) look.reset();
        else if (!look) {
            if (equipped) look = ItemArmorLook(*equipped);
            if (!look) look = ArmorLook{};
        }
        if (equipped != &item) npc.EquipArmor(&item);
        // EquipArmor performs the native requirements check and can reject the item.
        if (&npc != player || npc.GetEquippedArmor() != &item) return;
    }
    history.SetArmorLook(std::move(look));
    InvalidateArmorAppearance();
    SyncArmorAppearance();
}
