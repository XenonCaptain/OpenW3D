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
#include "dlgevaobjectivestab.h"
#include "dlgevamaptab.h"
#include "dlgevadatatab.h"
#include "dlgevacharacterstab.h"
#include "dlgevaweaponstab.h"
#include "dlgevavehiclestab.h"
#include "dlgevabuildingstab.h"
#include "cnetwork.h"
#include "gamemode.h"
#include "wolgmode.h"
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
#include "cstextobj.h"
#include "wwaudio.h"
#include "dlghelpscreen.h"
#include "crandom.h"
#include "slavemaster.h"
#include "string_ids.h"
#include "translatedb.h"


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
		TABCTRL_ADD_TAB (tab_ctrl, EvaObjectivesTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, EvaMapTabClass);
		TABCTRL_ADD_TAB (tab_ctrl, EvaDataTabClass);
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

	//
	//	Remove the save/load options for multiplay
	//

	if (!IS_MISSION) {
		Get_Dlg_Item (IDC_MENU_LOAD_SP_GAME_BUTTON)->Show (false);
		Get_Dlg_Item (IDC_MENU_LOAD_SP_GAME_BUTTON)->Enable (false);
		Get_Dlg_Item (IDC_MENU_SAVE_SP_GAME_BUTTON)->Show (false);
		Get_Dlg_Item (IDC_MENU_SAVE_SP_GAME_BUTTON)->Enable (false);
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
	bool allow_default_processing = true;

	switch (ctrl_id)
	{
		case IDC_CREATIVE_RESPAWN:
			if (cGod::Creative_Respawn ()) {
				On_Command (IDC_MENU_BACK_BUTTON, 0, 0);
			}
			return;

		case IDC_HELP_BUTTON:
			START_DIALOG (HelpScreenDialogClass);
			break;

		case IDC_MENU_MAIN_MENU_BUTTON:
			Prompt_User ();
			allow_default_processing = false;
			break;

		case IDCANCEL:
			ctrl_id = IDC_MENU_BACK_BUTTON;
		case IDC_MENU_BACK_BUTTON:
			GameInitMgrClass::Continue_Game();
			break;
	}

	//
	//	Allow the base class to process the message (if necessary)
	//
	if (allow_default_processing) {
		MenuDialogClass::On_Command (ctrl_id, message_id, param);
	}

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


////////////////////////////////////////////////////////////////
//
//	Prompt_User
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::Prompt_User (void)
{
	//
	//	Display the message box
	//
	DlgMsgBox::DoDialog (TRANSLATE (IDS_MENU_TEXT054), TRANSLATE (IDS_EXIT_GAME_VERIFICATION), DlgMsgBox::YesNo, this);	
	return ;
}


////////////////////////////////////////////////////////////////
//
//	HandleNotification
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::HandleNotification (DlgMsgBoxEvent &event)
{
	if (event.Event () == DlgMsgBoxEvent::Yes) {
		Exit_Game ();
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Exit_Game
//
////////////////////////////////////////////////////////////////
void
CreativeModeMenuClass::Exit_Game (void)
{
	bool stop = false;
	if (GameModeManager::Find("WOL")->Is_Active() || GameModeManager::Find("LAN")->Is_Active()) {
		if (cNetwork::I_Am_Server() && The_Game() && The_Game()->IsDedicated.Is_True() && SlaveMaster.Am_I_Slave()) {
			stop = true;
		}
	}

	//
	//	Close the dialog
	//
	End_Dialog ();

	GameInitMgrClass::End_Game();

	//
	// Dedicated slave servers should just quit here.
	//
	if (stop) {
		extern void Stop_Main_Loop (int exitCode);
		Stop_Main_Loop (EXIT_SUCCESS);
	}
	GameInitMgrClass::Display_End_Game_Menu();
	return ;
}
