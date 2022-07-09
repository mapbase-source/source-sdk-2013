//==========================================================================//
//
// Purpose: VGUI Keypad made by Maestro Fenix 2015
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"


#include "point_keypad.h"

#include "baseentity.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "recipientfilter.h"
#include "usermessages.h" //For user messages at keypad


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(point_keypad, CPointKeypad);

BEGIN_DATADESC(CPointKeypad)
DEFINE_FIELD(s_code, FIELD_STRING),
DEFINE_FIELD(s_frame_label, FIELD_STRING),
DEFINE_FIELD(b_hide_pass, FIELD_BOOLEAN),

//Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Enable", Enable),

//Outputs
DEFINE_OUTPUT(OnCorrectCode, "OnCorrectCode"),
DEFINE_OUTPUT(OnIncorrectCode, "OnIncorrectCode")
END_DATADESC()



void CPointKeypad::Spawn(void)
{

	Precache();

	SetSolid(SOLID_NONE);
	SetMoveType(MOVETYPE_NONE);

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointKeypad::Precache(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: Cache user-entity-field values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : Returns true if handled, false if not.
//-----------------------------------------------------------------------------
bool CPointKeypad::KeyValue(const char *szKeyName, const char *szValue)
{
	if (FStrEq(szKeyName, "code"))
	{
		s_code = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "label"))
	{
		s_frame_label = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "hide_pass"))
	{
		b_hide_pass = (atoi(szValue) != 0);
	}
	else
	{
		return CPointEntity::KeyValue(szKeyName, szValue);
	}
	return true;
}

void CPointKeypad::Enable(inputdata_t &inputdata)
{

	CBasePlayer *pPlayer = NULL;
	if (inputdata.pActivator && inputdata.pActivator->IsPlayer())
	{
		pPlayer = ToBasePlayer(inputdata.pActivator);
	}
	else
	{
		pPlayer = UTIL_GetLocalPlayer();
	}

	CSingleUserRecipientFilter user(pPlayer);
	//user.MakeReliable(); Needed?
	UserMessageBegin(user, "KeypadHandler");
	WRITE_STRING(STRING(s_code));//password
	WRITE_STRING(STRING(s_frame_label));//what's displayed on LCD screen
	WRITE_BOOL(b_hide_pass);//hide password option
	MessageEnd();
}


//-----------------------------------------------------------------------------
// Purpose: this function is callled when the user introduced the correct code and vgui sent a command(keypad_codematch) and
// the command(keypad_codematch) checks if there is a keypad that needs that code(see player.cpp)
//-----------------------------------------------------------------------------
void CPointKeypad::FireTarget(void)
{
	OnCorrectCode.FireOutput(this, this);
}


//-----------------------------------------------------------------------------
// Purpose: this function is callled when the user introduced the incorrect code and vgui sent a command(keypad_codedismatch) and
// the command(keypad_codedismatch) checks if there is a keypad that needs that code(see player.cpp)
//-----------------------------------------------------------------------------
void CPointKeypad::WrongCode(void)
{
	OnIncorrectCode.FireOutput(this, this);
}