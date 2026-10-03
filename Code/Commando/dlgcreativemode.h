/***********************************************************************************************
 *                                                                                             *
 *                 Project Name : OpenW3D                                                      *
 *                                                                                             *
 *                     File Name : /Code/Commando/dlgcreativemode.h                    		  $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __DLGCREATIVEMODE_H
#define __DLGCREATIVEMODE_H

#include "menudialog.h"
#include "resource.h"


////////////////////////////////////////////////////////////////
//
//	CreativeModeMenuClass
//
////////////////////////////////////////////////////////////////
class CreativeModeMenuClass : public MenuDialogClass
{
public:
	
	////////////////////////////////////////////////////////////////
	//	Public constants
	////////////////////////////////////////////////////////////////	
	typedef enum
	{
		TAB_NONE				= -1,
		TAB_OBJECTIVES		= 0,
		TAB_CHARACTERS,
		TAB_WEAPONS,
		TAB_VEHICLES,
		TAB_BUILDINGS
	} TAB_ID;
	
	////////////////////////////////////////////////////////////////
	//	Public constructors/destructors
	////////////////////////////////////////////////////////////////	
	CreativeModeMenuClass (void);
	~CreativeModeMenuClass (void);

	////////////////////////////////////////////////////////////////
	//	Public methods
	////////////////////////////////////////////////////////////////
	static void Update_Toggle (void);
	static void Update_Placement_Input (void);
	static void Update_Placement_Preview (void);
	static void Cancel_Placement (void);
	void		On_Init_Dialog (void);
	void		On_Destroy (void);
	void		On_Command (int ctrl_id, int mesage_id, DWORD param);

	//
	//	Singleton access
	//
	static void									Display (TAB_ID tab_id = TAB_NONE);
	static CreativeModeMenuClass *	Get_Instance (void)	{ return _TheInstance; }

private:

	////////////////////////////////////////////////////////////////
	//	Private member data
	////////////////////////////////////////////////////////////////	
	static CreativeModeMenuClass *	_TheInstance;
	static int									_NextTabIndex;
};


#endif //__DLGCREATIVEMODE_H
