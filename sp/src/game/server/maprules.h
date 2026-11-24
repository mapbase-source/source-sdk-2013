//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef MAPRULES_H
#define MAPRULES_H

#ifdef MAPBASE
#define MAX_MENU_OPTIONS	10
#define LAST_MENU_OPTION	9

#define SF_GAMEMENU_ALLPLAYERS			(1 << 0)
#define SF_GAMEMENU_CENTERED			(1 << 1)
#define SF_GAMEMENU_CENTER_FILL			(1 << 2)
#define SF_GAMEMENU_BIG_FONTS			(1 << 3)
#define SF_GAMEMENU_NO_FLASH			(1 << 4)
#define SF_GAMEMENU_WORD_WRAP			(1 << 5)

//-----------------------------------------------------------------------------
// Purpose: Displays a custom number menu for player(s)
//-----------------------------------------------------------------------------
DECLARE_AUTO_LIST( IGameMenuAutoList );
class CGameMenu : public CLogicalEntity, public IGameMenuAutoList
{
public:
	DECLARE_CLASS( CGameMenu, CLogicalEntity );
	DECLARE_DATADESC();
	CGameMenu();

	void OnRestore();
	void InputDoRestore( inputdata_t &inputdata );

	void TimeoutThink();

	void ShowMenu( CRecipientFilter &filter, float flDisplayTime = 0.0f );
	void HideMenu( CRecipientFilter &filter );
	void MenuSelected( int nSlot, CBaseEntity *pActivator );

	bool IsActive();
	bool IsActiveOnTarget( CBaseEntity *pPlayer );
	void RemoveTarget( CBaseEntity *pPlayer );

	// Inputs
	void InputShowMenu( inputdata_t &inputdata );
	void InputHideMenu( inputdata_t &inputdata );

	void InputSetCase01( inputdata_t &inputdata ) { m_iszOption[0] = inputdata.value.StringID(); }
	void InputSetCase02( inputdata_t &inputdata ) { m_iszOption[1] = inputdata.value.StringID(); }
	void InputSetCase03( inputdata_t &inputdata ) { m_iszOption[2] = inputdata.value.StringID(); }
	void InputSetCase04( inputdata_t &inputdata ) { m_iszOption[3] = inputdata.value.StringID(); }
	void InputSetCase05( inputdata_t &inputdata ) { m_iszOption[4] = inputdata.value.StringID(); }
	void InputSetCase06( inputdata_t &inputdata ) { m_iszOption[5] = inputdata.value.StringID(); }
	void InputSetCase07( inputdata_t &inputdata ) { m_iszOption[6] = inputdata.value.StringID(); }
	void InputSetCase08( inputdata_t &inputdata ) { m_iszOption[7] = inputdata.value.StringID(); }
	void InputSetCase09( inputdata_t &inputdata ) { m_iszOption[8] = inputdata.value.StringID(); }
	void InputSetCase10( inputdata_t &inputdata ) { m_iszOption[9] = inputdata.value.StringID(); }

private:

	CUtlVector<EHANDLE>	m_ActivePlayers;
	CUtlVector<float>	m_ActivePlayerTimes;

	float			m_flDisplayTime;

	bool			m_bSkipEmptyCases;	// Gaps between cases are filled in (e.g. if case02 is empty and case03 is not, then case03 acts like case02)

	string_t		m_iszTitle;
	string_t		m_iszOption[MAX_MENU_OPTIONS];

	// Outputs
	COutputEvent	m_OnCase[MAX_MENU_OPTIONS];		// Fired for the option chosen
	COutputInt		m_OutValue;						// Outputs the option chosen
	COutputEvent	m_OnTimeout;					// Fires when no option was chosen in time
};
#endif

#endif		// MAPRULES_H

