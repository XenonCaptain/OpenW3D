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
		TABCTRL_ADD_TAB (tab_ctrl, EvaCharactersTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, EvaWeaponsTabClass);
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
