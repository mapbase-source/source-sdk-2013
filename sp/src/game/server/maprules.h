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

#define SF_GAMEMENU_ALLPLAYERS			0x0001

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

private:

	CUtlVector<EHANDLE>	m_ActivePlayers;
	CUtlVector<float>	m_ActivePlayerTimes;

	float			m_flDisplayTime;

	string_t		m_iszTitle;
	string_t		m_iszOption[MAX_MENU_OPTIONS];

	// Outputs
	COutputEvent	m_OnCase[MAX_MENU_OPTIONS];		// Fired for the option chosen
	COutputInt		m_OutValue;						// Outputs the option chosen
	COutputEvent	m_OnTimeout;					// Fires when no option was chosen in time
};
#endif

#endif		// MAPRULES_H

