/***********************************************************************************************
 *                                                                                             *
 *                 Project Name : OpenW3D                                                      *
 *                                                                                             *
 *                     File Name : /Code/Commando/dlgcreativemode.cpp                  		  $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "dlgcreativemode.h"
#include "dialogresource.h"
#include "tabctrl.h"
#include "childdialog.h"
#include "listctrl.h"
#include "objectives.h"
#include "translatedb.h"
#include "string_ids.h"
#include <wchar.h>
#include "dlgevacharacterstab.h"
#include "dlgevaweaponstab.h"
#include "dlgevavehiclestab.h"
#include "dlgevabuildingstab.h"
#include "cnetwork.h"
#include "gamemode.h"
#include "dialogmgr.h"
#include "gameinitmgr.h"
#include "gametype.h"
#include "god.h"
#include "input.h"
#include "directinput.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "combat.h"
#include "ccamera.h"
#include "win.h"
#include "soldier.h"
#include "definitionmgr.h"
#include "objlibrary.h"
#include "phys.h"
#include "pscene.h"
#include "physcoltest.h"
#include "assets.h"
#include "rendobj.h"
#include "viewerctrl.h"
#include "dialogtext.h"
#include "checkboxctrl.h"
#include "weapons.h"
#include "weaponbag.h"
#include "creativeammo.h"
#include "stylemgr.h"



namespace {
RenderObjClass *InfantryPreview = NULL;
PhysicsSceneClass *PreviewScene = NULL;
StringClass InfantryPreset;
float InfantryDistance = 5.0F;
float InfantryHeight = 0.0F;
float InfantryInitialFacing = 0.0F;
int InfantryRotationSteps = 0;
bool PlacementArmed = false;
bool InfantrySpawnRequested = false;
bool SuppressPlacementInput = false;
Matrix3D InfantryTransform(1);

SoldierGameObjDef *Find_Soldier_Definition(int id)
{
	DefinitionClass *definition = DefinitionMgrClass::Find_Definition(id, false);
	return definition != nullptr && definition->Get_Class_ID() == CLASSID_GAME_OBJECT_DEF_SOLDIER
		? static_cast<SoldierGameObjDef *>(definition) : nullptr;
}

void Cancel_Infantry_Placement()
{
    if (InfantryPreview != NULL) {
        PreviewScene->Remove_Render_Object(InfantryPreview);
        PreviewScene->Release_Ref();
        InfantryPreview->Release_Ref();
    }
    InfantryPreview = NULL;
    PreviewScene = NULL;
    InfantryPreset = "";
    InfantryHeight = 0.0F;
    PlacementArmed = false;
    InfantrySpawnRequested = false;
}

bool Is_Soldier_Definition(DefinitionClass *definition)
{
    if (definition == NULL) return false;
    const uint32 class_id = definition->Get_Class_ID();
    return class_id == CLASSID_GAME_OBJECT_DEF_SOLDIER ||
        class_id == CLASSID_GAME_OBJECT_DEF_MENDOZA_BOSS ||
        class_id == CLASSID_GAME_OBJECT_DEF_RAVESHAW_BOSS;
}

bool Can_Select_Character()
{
    return cNetwork::I_Am_Server() && COMBAT_STAR != NULL &&
        COMBAT_STAR->Get_Vehicle() == NULL && The_Game() != NULL &&
        The_Game()->IsIntermission.Is_False();
}
}

// A separate resource preserves the normal encyclopedia and the existing viewer bounds.
class CreativeCharactersTabClass : public EvaViewerTabClass
{
public:
    CreativeCharactersTabClass() : EvaViewerTabClass(IDD_CREATIVE_CHARACTERS_TAB) {}
    void On_Init_Dialog()
    {
        Set_Encyclopedia_Type(EncyclopediaMgrClass::TYPE_CHARACTER);
        Set_List_Ctrl((ListCtrlClass *)Get_Dlg_Item(IDC_LIST_CTRL));
        Set_Description_Ctrl(NULL);
        Set_Affiliation_Ctrl((DialogTextClass *)Get_Dlg_Item(IDC_AFFILIATION_STATIC));
        Set_Viewer_Ctrl((ViewerCtrlClass *)Get_Dlg_Item(IDC_VIEWER_CTRL));
        EvaViewerTabClass::On_Init_Dialog();
        ListCtrl->Sort(AlphabeticalSortCallback, 0);
        if (ListCtrl->Get_Entry_Count() > 0) {
            ListCtrl->Set_Curr_Sel(0);
            View_Entry(0);
        }
    }
    StringClass Preset_Model(SoldierGameObjDef *soldier)
    {
        DefinitionClass *physics = DefinitionMgrClass::Find_Definition(soldier->Get_Phys_Def_ID());
        StringClass model_name;
        if (physics != NULL) {
            // Physics presets store a .w3d filename, while the viewer and
            // placement loader expect the render object's name without its path
            // or extension.
            Strip_Path_From_Filename(model_name, ((PhysDefClass *)physics)->Get_Model_Name());
            const int length = model_name.Get_Length();
            if (length >= 4 && stricmp(model_name.Peek_Buffer() + length - 4, ".w3d") == 0) {
                model_name.Erase(length - 4, 4);
            }
        }
        return model_name;
    }
    static int CALLBACK AlphabeticalSortCallback(ListCtrlClass *list, int first, int second, uint32)
    {
        EvaViewerObjectClass *a = (EvaViewerObjectClass *)list->Get_Entry_Data(first, 0);
        EvaViewerObjectClass *b = (EvaViewerObjectClass *)list->Get_Entry_Data(second, 0);
        const int result = wcsicmp(a->Get_Name(), b->Get_Name());
        // Retain distinct presets sharing the same translated display name.
        return result != 0 ? result : stricmp(a->Get_Definition_Name(), b->Get_Definition_Name());
    }
    void Build_Object_List()
    {
        ObjectList.Delete_All();
        // Every loaded soldier definition gets its own row, including mission
        // variants and soldier-derived bosses, regardless of encyclopedia reveal.
		for (DefinitionClass *definition = DefinitionMgrClass::Get_First(CLASSID_GAME_OBJECT_DEF_SOLDIER);
             definition != NULL; definition = DefinitionMgrClass::Get_Next(definition)) {
            if (!Is_Soldier_Definition(definition) || stricmp(definition->Get_Name(), "Soldier_Presets") == 0) continue;
            SoldierGameObjDef *soldier = (SoldierGameObjDef *)definition;
            WideStringClass name;
            const uint32 name_id = soldier->Get_Translated_Name_ID();
            if (name_id != 0 && TranslateDBClass::Find_Object(name_id) != NULL) {
                const WCHAR *translated = TRANSLATE(name_id);
                if (translated != NULL) name = translated;
            }
            if (name.Get_Length() == 0) name.Convert_From(definition->Get_Name());
            EvaViewerObjectClass entry;
            entry.Set_ID(definition->Get_ID());
            entry.Set_Definition_Name(definition->Get_Name());
            entry.Set_Player_Type(soldier->Get_Default_Player_Type());
            entry.Set_Model_Name(Preset_Model(soldier));
            entry.Set_Name(name);
            entry.Set_Affiliation(name);
            ObjectList.Add(entry);
        }
    }
    bool Is_Entry_Visible(const EvaViewerObjectClass &) { return true; }
    void On_ViewerCtrl_Model_Loaded(ViewerCtrlClass *viewer, int, RenderObjClass *model)
    {
        Prepare_Model(model);
        viewer->Set_Interface_Mode(ViewerCtrlClass::Z_ROTATION, 30.0f);
        Update_Display_Facing();
    }
    void Update_Display_Facing()
    {
        if (ViewerCtrl->Peek_Model() != NULL) {
            Matrix3D facing(1);
            facing.Rotate_Z(DEG_TO_RADF(45.0F));
            ViewerCtrl->Peek_Model()->Set_Transform(facing);
        }
    }
    void On_Frame_Update()
    {
        Update_Display_Facing();
        Get_Dlg_Item(IDC_CREATIVE_CHARACTER_SELECT)->Enable(Can_Select_Character() && ListCtrl->Get_Curr_Sel() >= 0);
        EvaViewerTabClass::On_Frame_Update();
    }
    void On_Command(int ctrl_id, int message_id, DWORD param)
    {
        if (ctrl_id != IDC_CREATIVE_CHARACTER_SELECT) {
            EvaViewerTabClass::On_Command(ctrl_id, message_id, param);
            return;
        }
        if (!Can_Select_Character() || ListCtrl->Get_Curr_Sel() < 0) return;
        EvaViewerObjectClass *entry = (EvaViewerObjectClass *)ListCtrl->Get_Entry_Data(ListCtrl->Get_Curr_Sel(), 0);
        SoldierGameObjDef *soldier = Find_Soldier_Definition(entry->Get_ID());
        if (!soldier) return;
        Cancel_Infantry_Placement();
        if (((CheckBoxCtrlClass *)Get_Dlg_Item(IDC_CREATIVE_SPAWN_INFANTRY))->Get_Check()) {
            InfantryPreview = WW3DAssetManager::Get_Instance()->Create_Render_Obj(entry->Get_Model_Name());
            if (InfantryPreview == NULL) return;
            InfantryPreset = soldier->Get_Name();
            InfantryDistance = 5.0F;
            InfantryHeight = 0.0F;
            InfantryInitialFacing = COMBAT_STAR->Get_Transform().Get_Z_Rotation();
            InfantryRotationSteps = 0;
            PreviewScene = COMBAT_SCENE;
            PreviewScene->Add_Ref();
            PreviewScene->Add_Render_Object(InfantryPreview);
        } else {
            const Matrix3D transform = COMBAT_STAR->Get_Transform();
            const int team = COMBAT_STAR->Get_Player_Type();
            COMBAT_STAR->Re_Init(*soldier);
            COMBAT_STAR->Set_Player_Type(team);
            COMBAT_STAR->Set_Transform(transform);
        }
        SuppressPlacementInput = true;
        CreativeModeMenuClass::Get_Instance()->On_Command(IDC_MENU_BACK_BUTTON, 0, 0);
    }
};

void CreativeModeMenuClass::Update_Placement_Input()
{
    const bool left = (DirectInput::Get_Mouse_Button(DirectInput::BUTTON_MOUSE_LEFT) & DirectInput::DI_BUTTON_HELD) != 0;
    const bool right = (DirectInput::Get_Mouse_Button(DirectInput::BUTTON_MOUSE_RIGHT) & DirectInput::DI_BUTTON_HELD) != 0;
    if (InfantryPreview != NULL || SuppressPlacementInput) {
        Input::Suppress_Creative_Placement_Actions();
        if (COMBAT_STAR != NULL) {
            COMBAT_STAR->Get_Control().Set_Boolean(ControlClass::BOOLEAN_WEAPON_FIRE_PRIMARY, false);
            COMBAT_STAR->Get_Control().Set_Boolean(ControlClass::BOOLEAN_WEAPON_FIRE_SECONDARY, false);
        }
    }
    if (InfantryPreview == NULL) {
        if (!left && !right) SuppressPlacementInput = false;
        return;
    }
    if (!Can_Select_Character() || COMBAT_SCENE != PreviewScene ||
        !GameInFocus || Input::Is_Console_Enabled() || DialogMgrClass::Get_Dialog_Count() != 0 ||
        COMBAT_CAMERA == NULL || COMBAT_CAMERA->Is_In_Cinematic()) {
        Cancel_Infantry_Placement();
        return;
    }
    if (!left && !right) PlacementArmed = true;
    if (!PlacementArmed) return;
    if (right) { Cancel_Infantry_Placement(); return; }
    if (DirectInput::Get_Keyboard_Button(DIK_R) & DirectInput::DI_BUTTON_HIT) {
        InfantryRotationSteps = (InfantryRotationSteps + 1) % 8;
    }
    if (DirectInput::Get_Keyboard_Button(DIK_ADD) & DirectInput::DI_BUTTON_HIT) {
        InfantryHeight += 0.5F;
    }
    if (DirectInput::Get_Keyboard_Button(DIK_SUBTRACT) & DirectInput::DI_BUTTON_HIT) {
        InfantryHeight -= 0.5F;
    }
    InfantryDistance = WWMath::Clamp(InfantryDistance - DirectInput::Get_Mouse_Axis(DirectInput::MOUSE_Z_AXIS) / 120.0F, 1.0F, 50.0F);
    if (left) InfantrySpawnRequested = true;
}

// Run after combat/camera updates, immediately before rendering. The preview has
// no physics, AI, or network interpolation to fight the placement transform.
void CreativeModeMenuClass::Update_Placement_Preview()
{
    if (InfantryPreview == NULL || COMBAT_CAMERA == NULL || COMBAT_STAR == NULL) return;
    Vector3 forward = -COMBAT_CAMERA->Get_Transform().Get_Z_Vector();
    forward.Z = 0;
    if (forward.Length2() < 0.0001F) forward = COMBAT_STAR->Get_Transform().Get_X_Vector();
    forward.Normalize();
    Vector3 position;
    COMBAT_STAR->Get_Position(&position);
    position += forward * InfantryDistance;
    CastResultStruct result;
    PhysRayCollisionTestClass ray(LineSegClass(position + Vector3(0, 0, 3), position - Vector3(0, 0, 100)), &result, 0);
    ray.CheckDynamicObjs = false;
    COMBAT_SCENE->Cast_Ray(ray);
    if (result.Fraction < 1.0F) position.Z = position.Z + 3.0F - 103.0F * result.Fraction;
    position.Z += InfantryHeight;
    InfantryTransform.Make_Identity();
    InfantryTransform.Rotate_Z(InfantryInitialFacing + DEG_TO_RADF(45.0F * InfantryRotationSteps));
    InfantryTransform.Set_Translation(position);
    InfantryPreview->Set_Transform(InfantryTransform);
    if (InfantrySpawnRequested) {
        PhysicalGameObj *object = ObjectLibraryManager::Create_Object(InfantryPreset);
        if (object != NULL) {
            object->Set_Transform(InfantryTransform);
            if (object->As_SoldierGameObj() != NULL) object->As_SoldierGameObj()->Innate_Enable();
        }
        Cancel_Infantry_Placement();
    }
}

class CreativeWeaponsTabClass : public EvaViewerTabClass
{
public:
    CreativeWeaponsTabClass() : EvaViewerTabClass(IDD_CREATIVE_WEAPONS_TAB) {}
    static StringClass Preview_Model_Name(const char *filename)
    {
        StringClass name;
        Strip_Path_From_Filename(name, filename);
        const char *slash = strrchr(name, '/');
        if (slash != NULL) { StringClass basename(slash + 1); name = basename; }
        if (name.Get_Length() >= 4 && stricmp(name.Peek_Buffer() + name.Get_Length() - 4, ".w3d") == 0) {
            name.Erase(name.Get_Length() - 4, 4);
        }
        return name;
    }
    void Load_Preview_Fallback()
    {
        if (ViewerCtrl->Peek_Model() != NULL || ListCtrl->Get_Curr_Sel() < 0) return;
        EvaViewerObjectClass *entry = (EvaViewerObjectClass *)ListCtrl->Get_Entry_Data(ListCtrl->Get_Curr_Sel(), 0);
        const WeaponDefinitionClass *weapon = WeaponManager::Find_Weapon_Definition(entry->Get_ID());
        if (weapon == NULL) return;
        const char *models[] = { weapon->Model, weapon->FirstPersonModel, weapon->BackModel };
        for (int i = 0; i < 3 && ViewerCtrl->Peek_Model() == NULL; ++i) {
            StringClass name = Preview_Model_Name(models[i]);
            if (!name.Is_Empty()) ViewerCtrl->Set_Model(name);
        }
        // The standalone preview does not depend on a soldier's gun animation.
        ViewerCtrl->Set_Animation("");
    }
    void On_ListCtrl_Sel_Change(ListCtrlClass *list, int ctrl_id, int old_index, int new_index)
    {
        EvaViewerTabClass::On_ListCtrl_Sel_Change(list, ctrl_id, old_index, new_index);
        Load_Preview_Fallback();
    }
    void On_Init_Dialog()
    {
        Set_Encyclopedia_Type(EncyclopediaMgrClass::TYPE_WEAPON);
        Set_List_Ctrl((ListCtrlClass *)Get_Dlg_Item(IDC_LIST_CTRL));
        Set_Viewer_Ctrl((ViewerCtrlClass *)Get_Dlg_Item(IDC_VIEWER_CTRL));
        Set_INI_Filename("weapons.ini");
        EvaViewerTabClass::On_Init_Dialog();
        Load_Preview_Fallback();
        const uint32 color = StyleMgrClass::Get_Text_Color();
        const Vector3 yellow(((color >> 16) & 255) / 255.0F, ((color >> 8) & 255) / 255.0F, (color & 255) / 255.0F);
        for (int row = 0; row < ListCtrl->Get_Entry_Count(); ++row) ListCtrl->Set_Entry_Color(row, 0, yellow);
        ((CheckBoxCtrlClass *)Get_Dlg_Item(IDC_CREATIVE_INFINITE_AMMO))->Set_Check(Get_Creative_Infinite_Ammo());
    }
    void Build_Object_List()
    {
        EvaViewerTabClass::Build_Object_List();
        DynamicVectorClass<EvaViewerObjectClass> encyclopedia = ObjectList;
        ObjectList.Delete_All();
        for (DefinitionClass *def = DefinitionMgrClass::Get_First(CLASSID_DEF_WEAPON);
             def != NULL; def = DefinitionMgrClass::Get_Next(def, CLASSID_DEF_WEAPON)) {
            WeaponDefinitionClass *weapon = (WeaponDefinitionClass *)def;
            EvaViewerObjectClass entry;
            entry.Set_ID(def->Get_ID());
            entry.Set_Definition_Name(def->Get_Name());
            StringClass model = weapon->Model;
            if (model.Is_Empty()) model = weapon->FirstPersonModel;
            if (model.Is_Empty()) {
                for (int i = 0; i < encyclopedia.Count(); ++i) {
                    if (stricmp(encyclopedia[i].Get_Definition_Name(), def->Get_Name()) == 0 ||
                        (weapon->IconNameID != 0 && wcsicmp(encyclopedia[i].Get_Name(), TRANSLATE(weapon->IconNameID)) == 0)) {
                        model = encyclopedia[i].Get_Model_Name();
                        break;
                    }
                }
            }
            entry.Set_Model_Name(Preview_Model_Name(model));
            entry.Set_Anim_Name("");
            if (weapon->IconNameID != 0) entry.Set_Name(TRANSLATE(weapon->IconNameID));
            if (entry.Get_Name()[0] == 0) {
                WideStringClass fallback;
                fallback.Convert_From(def->Get_Name());
                entry.Set_Name(fallback);
            }
            // Neutral entries use the viewer's alphabetical name sort.
            ObjectList.Add(entry);
        }
    }
    bool Is_Entry_Visible(const EvaViewerObjectClass &) { return true; }
    void On_ViewerCtrl_Model_Loaded(ViewerCtrlClass *viewer, int, RenderObjClass *model)
    {
        Prepare_Model(model);
        viewer->Set_Interface_Mode(ViewerCtrlClass::Z_ROTATION, 30.0F);
    }
    void On_Frame_Update()
    {
        const bool enabled = Can_Select_Character();
        Get_Dlg_Item(IDC_CREATIVE_WEAPON_SELECT)->Enable(enabled && ListCtrl->Get_Curr_Sel() >= 0);
        CheckBoxCtrlClass *checkbox = (CheckBoxCtrlClass *)Get_Dlg_Item(IDC_CREATIVE_INFINITE_AMMO);
        checkbox->Enable(enabled);
        if (enabled && Is_Visible() && checkbox->Get_Check() != Get_Creative_Infinite_Ammo()) {
            Set_Creative_Infinite_Ammo(checkbox->Get_Check());
            WeaponClass *weapon = COMBAT_STAR->Get_Weapon();
            if (checkbox->Get_Check() && weapon != NULL && weapon->Get_Clip_Rounds() == 0) {
                weapon->Set_Clip_Rounds(MAX(1, (int)weapon->Get_Definition()->ClipSize));
                COMBAT_STAR->Get_Weapon_Bag()->Force_Changed();
            }
        }
        EvaViewerTabClass::On_Frame_Update();
    }
    void On_Command(int ctrl_id, int message_id, DWORD param)
    {
        if (ctrl_id != IDC_CREATIVE_WEAPON_SELECT) {
            EvaViewerTabClass::On_Command(ctrl_id, message_id, param);
            return;
        }
        if (!Can_Select_Character() || ListCtrl->Get_Curr_Sel() < 0) return;
        EvaViewerObjectClass *entry = (EvaViewerObjectClass *)ListCtrl->Get_Entry_Data(ListCtrl->Get_Curr_Sel(), 0);
        const WeaponDefinitionClass *definition = WeaponManager::Find_Weapon_Definition(entry->Get_ID());
        if (definition == NULL) return;
        WeaponBagClass *bag = COMBAT_STAR->Get_Weapon_Bag();
        WeaponClass *weapon = bag->Add_Weapon(definition, MAX(1, (int)definition->ClipSize), true);
        if (weapon == NULL) return;
        weapon->Set_Clip_Rounds(MAX(1, (int)definition->ClipSize));
        weapon->Set_Inventory_Rounds(definition->MaxInventoryRounds);
        bag->Select_Weapon(weapon);
        bag->Force_Changed();
        Set_Creative_Infinite_Ammo(((CheckBoxCtrlClass *)Get_Dlg_Item(IDC_CREATIVE_INFINITE_AMMO))->Get_Check());
        SuppressPlacementInput = true;
        CreativeModeMenuClass::Get_Instance()->On_Command(IDC_MENU_BACK_BUTTON, 0, 0);
    }
};

// Creative Mode uses its own tab so the normal EVA objective view is unchanged.
class CreativeObjectivesTabClass : public ChildDialogClass
{
public:
    CreativeObjectivesTabClass() : ChildDialogClass(IDD_CREATIVE_OBJECTIVES_TAB) {}

    void On_Init_Dialog()
    {
        ListCtrlClass *list = Get_List();
        if (list != NULL) {
            list->Add_Column(TRANSLATE(IDS_OBJHDR_PRIORITY), 0.25F, Vector3(1, 1, 1));
            list->Add_Column(TRANSLATE(IDS_OBJHDR_OBJECTIVE), 0.5F, Vector3(1, 1, 1));
            list->Add_Column(TRANSLATE(IDS_OBJHDR_STATUS), 0.25F, Vector3(1, 1, 1));
        }
        Refresh_List();
        ChildDialogClass::On_Init_Dialog();
    }

    void On_Frame_Update()
    {
        Refresh_List();
        ChildDialogClass::On_Frame_Update();
    }

    void On_ListCtrl_Sel_Change(ListCtrlClass *, int ctrl_id, int, int)
    {
        if (ctrl_id == IDC_OBJECTIVES_LIST_CTRL) {
            Update_Buttons();
        }
    }

    void On_Command(int ctrl_id, int message_id, DWORD param)
    {
        if (ctrl_id == IDC_CREATIVE_OBJECTIVE_COMPLETE || ctrl_id == IDC_CREATIVE_OBJECTIVE_FAIL) {
            Objective *objective = Selected_Objective();
            if (Can_Edit() && objective != NULL) {
                const int status = ctrl_id == IDC_CREATIVE_OBJECTIVE_COMPLETE ?
                    ObjectiveManager::STATUS_ACCOMPLISHED : ObjectiveManager::STATUS_FAILED;
                if (objective->Status != status) {
                    // The manager sorts its list after a status change. Use the
                    // objective's persistent ID, never its current array index.
                    ObjectiveManager::Set_Objective_Status(objective->ID, status);
                    Refresh_List();
                }
            }
            return;
        }
        ChildDialogClass::On_Command(ctrl_id, message_id, param);
    }

private:
    void Update_Text(ListCtrlClass *list, int row, int column, const WCHAR *text)
    {
        if (wcscmp(list->Get_Entry_Text(row, column), text) != 0) {
            list->Set_Entry_Text(row, column, text);
        }
    }

    ListCtrlClass *Get_List()
    {
        return (ListCtrlClass *)Get_Dlg_Item(IDC_OBJECTIVES_LIST_CTRL);
    }

    bool Can_Edit()
    {
        return cNetwork::I_Am_Server() && The_Game() != NULL &&
            IS_MISSION && The_Game()->IsIntermission.Is_False();
    }

    Objective *Selected_Objective()
    {
        ListCtrlClass *list = Get_List();
        if (list == NULL || list->Get_Curr_Sel() < 0) {
            return NULL;
        }
        const uint32 id = list->Get_Entry_Data(list->Get_Curr_Sel(), 0);
        for (int i = 0; i < ObjectiveManager::Get_Objective_Count(); ++i) {
            Objective *objective = ObjectiveManager::Get_Objective(i);
            if (objective != NULL && (uint32)objective->ID == id) {
                return objective;
            }
        }
        return NULL;
    }

    void Update_Buttons()
    {
        Objective *objective = Selected_Objective();
        const bool enabled = Can_Edit() && objective != NULL;
        Get_Dlg_Item(IDC_CREATIVE_OBJECTIVE_COMPLETE)->Enable(enabled &&
            objective->Status != ObjectiveManager::STATUS_ACCOMPLISHED);
        Get_Dlg_Item(IDC_CREATIVE_OBJECTIVE_FAIL)->Enable(enabled &&
            objective->Status != ObjectiveManager::STATUS_FAILED);
    }

    void Refresh_List()
    {
        ListCtrlClass *list = Get_List();
        if (list == NULL) {
            return;
        }
        // Keep rows keyed by ID as scripts add/remove objectives or the manager sorts.
        for (int row = list->Get_Entry_Count() - 1; row >= 0; --row) {
            bool found = false;
            for (int i = 0; i < ObjectiveManager::Get_Objective_Count(); ++i) {
                Objective *objective = ObjectiveManager::Get_Objective(i);
                if (objective != NULL && (uint32)objective->ID == list->Get_Entry_Data(row, 0)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                list->Delete_Entry(row);
            }
        }
        for (int i = 0; i < ObjectiveManager::Get_Objective_Count(); ++i) {
            Objective *objective = ObjectiveManager::Get_Objective(i);
            if (objective == NULL) {
                continue;
            }
            int row = 0;
            for (; row < list->Get_Entry_Count(); ++row) {
                if (list->Get_Entry_Data(row, 0) == (uint32)objective->ID) {
                    break;
                }
            }
            if (row == list->Get_Entry_Count()) {
                row = list->Insert_Entry(row, objective->Type_To_Name());
                if (row < 0) {
                    continue;
                }
                list->Set_Entry_Data(row, 0, (uint32)objective->ID);
            }
            WideStringClass text = TRANSLATE(objective->ShortDescriptionID);
            if (text.Get_Length() > 0 && text[text.Get_Length() - 1] == L'\n') {
                text.Erase(text.Get_Length() - 1, 1);
            }
            if (text.Get_Length() == 0) {
                text.Format(L"Objective %d", objective->ID);
            }
            const bool type_changed = wcscmp(list->Get_Entry_Text(row, 0), objective->Type_To_Name()) != 0;
            // New rows have no objective text yet; color them once, then only
            // recolor if a mission script changes their priority.
            if (type_changed || list->Get_Entry_Text(row, 1)[0] == 0) {
                for (int col = 0; col < 3; ++col) {
                    list->Set_Entry_Color(row, col, objective->Type_To_Color());
                }
            }
            Update_Text(list, row, 0, objective->Type_To_Name());
            Update_Text(list, row, 1, text);
            Update_Text(list, row, 2, objective->Status_To_Name());
        }
        if (list->Get_Curr_Sel() < 0 && list->Get_Entry_Count() > 0) {
            list->Set_Curr_Sel(0);
        }
        Update_Buttons();
    }
};

////////////////////////////////////////////////////////////////
//	Static member initialization
////////////////////////////////////////////////////////////////
CreativeModeMenuClass *	CreativeModeMenuClass::_TheInstance = NULL;
int								CreativeModeMenuClass::_NextTabIndex = -4;

// Poll once per frame so Delete also works while solo combat is suspended.
void CreativeModeMenuClass::Update_Toggle (void)
{
	static bool delete_held = false;
	const bool held = (DirectInput::Get_Keyboard_Button (DIK_DELETE) &
		DirectInput::DI_BUTTON_HELD) != 0;
	const bool pressed = held && !delete_held;
	delete_held = held;
	if (!pressed || !GameInFocus || Input::Is_Console_Enabled () ||
		DialogMgrClass::Is_Flushing_Dialogs () ||
		DialogMgrClass::Peek_Transitioning_Dialog () != NULL) {
		return;
	}

	GameModeClass *combat = GameModeManager::Find ("Combat");
	GameModeClass *menu = GameModeManager::Find ("Menu");
	if (The_Game () == NULL || combat == NULL || menu == NULL ||
		The_Game ()->IsIntermission.Is_True ()) {
		return;
	}

	if (_TheInstance != NULL) {
		// Leave child menus and confirmation popups in control of their input.
		if (_TheInstance->Is_Active_Menu () &&
			DialogMgrClass::Get_Active_Dialog () == _TheInstance) {
			CreativeModeMenuClass *dialog = _TheInstance;
			dialog->Add_Ref ();
			dialog->On_Command (IDC_MENU_BACK_BUTTON, 0, 0);
			dialog->Release_Ref ();
		}
		return;
	}

	if (!combat->Is_Active () || !menu->Is_Inactive () ||
		DialogMgrClass::Get_Dialog_Count () != 0 ||
		COMBAT_STAR == NULL || COMBAT_CAMERA == NULL ||
		COMBAT_CAMERA->Is_In_Cinematic ()) {
		return;
	}
	if (IS_SOLOPLAY) {
		combat->Suspend ();
	}
	Display ();
	menu->Activate ();
}


////////////////////////////////////////////////////////////////
//
//	CreativeModeMenuClass
//
////////////////////////////////////////////////////////////////
CreativeModeMenuClass::CreativeModeMenuClass (void)	:
	MenuDialogClass (IDD_MENU_CREATIVE_MODE)
{
	_TheInstance = this;
	return ;
}


////////////////////////////////////////////////////////////////
//
//	CreativeModeMenuClass
//
////////////////////////////////////////////////////////////////
CreativeModeMenuClass::~CreativeModeMenuClass (void)
{
	_TheInstance = NULL;
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Init_Dialog
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::On_Init_Dialog (void)
{
	TabCtrlClass *tab_ctrl = (TabCtrlClass *)Get_Dlg_Item (IDC_GENERIC_TABCTRL);
	if (tab_ctrl != NULL) {

		//
		//	Add the tabs to the control
		//
		TABCTRL_ADD_TAB (tab_ctrl, CreativeObjectivesTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, CreativeCharactersTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, CreativeWeaponsTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, EvaVehiclesTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, EvaBuildingsTabClass);

		//
		//	Determine which tab to show be default
		//
		int tab_index = _NextTabIndex;
		if (_NextTabIndex < 0) {
			tab_index = 0;
		}

		tab_ctrl->Set_Curr_Tab (tab_index);
	}

	Get_Dlg_Item (IDC_CREATIVE_RESPAWN)->Enable (cGod::Can_Creative_Respawn ());

	MenuDialogClass::On_Init_Dialog ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Destroy
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::On_Destroy (void)
{
	TabCtrlClass *tab_ctrl = (TabCtrlClass *)Get_Dlg_Item (IDC_GENERIC_TABCTRL);
	if (tab_ctrl == NULL) {
		return ;
	}

	//
	//	Remember what tab the user left on...
	//
	if (_NextTabIndex >= 0) {
		_NextTabIndex = tab_ctrl->Get_Curr_Tab ();
	}

	MenuDialogClass::On_Destroy ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Command
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::On_Command (int ctrl_id, int message_id, DWORD param)
{
	switch (ctrl_id)
	{
		case IDC_CREATIVE_RESPAWN:
			if (cGod::Creative_Respawn ()) {
				On_Command (IDC_MENU_BACK_BUTTON, 0, 0);
			}
			return;

		case IDC_CREATIVE_REFILL:
		case IDC_CREATIVE_FREEZE:
		case IDC_CREATIVE_LOCK_DEFS:
		case IDC_CREATIVE_GM_FOR_ALLY:
		case IDC_CREATIVE_SPAWN:
			// Placeholders: consume clicks without invoking global menu commands.
			return;

		case IDCANCEL:
			ctrl_id = IDC_MENU_BACK_BUTTON;
		case IDC_MENU_BACK_BUTTON:
			GameInitMgrClass::Continue_Game();
			break;
	}

	//
	//	Allow the base class to process the message (if necessary)
	//
	MenuDialogClass::On_Command (ctrl_id, message_id, param);

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Display
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::Display (TAB_ID tab_id)
{
	//
	//	Create the dialog if necessary, otherwise simply bring it to the front
	//
	if (_TheInstance == NULL) {
		if (tab_id != TAB_NONE) {
			_NextTabIndex = tab_id;
		}
		START_DIALOG (CreativeModeMenuClass);
	} else {
		if (_TheInstance->Is_Active_Menu () == false) {
			DialogMgrClass::Rollback (_TheInstance);
		}
	}

	return ;
}
