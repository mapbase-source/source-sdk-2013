//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "entity_roundwin.h"
#include "teamplayroundbased_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef MAPBASE
// Matches up with WINREASON_ enum, but with WINREASON_ removed for comparison reasons
static const char *g_pszWinReasons[] = {
	"NONE",
	"ALL_POINTS_CAPTURED",
	"OPPONENTS_DEAD",
	"FLAG_CAPTURE_LIMIT",
	"DEFEND_UNTIL_TIME_LIMIT",
	"STALEMATE",
	"TIMELIMIT",
	"WINLIMIT",
	"WINDIFFLIMIT",
#ifdef TF_DLL
	"RD_REACTOR_CAPTURED",
	"RD_CORES_COLLECTED",
	"RD_REACTOR_RETURNED",
	"PD_POINTS",
	"SCORED",
	"STOPWATCH_WATCHING_ROUNDS",
	"STOPWATCH_WATCHING_FINAL_ROUND",
	"STOPWATCH_PLAYING_ROUNDS",
#endif
};

// Add any new win reasons to the array above
COMPILE_TIME_ASSERT( ARRAYSIZE( g_pszWinReasons ) == WINREASON_COUNT );
#endif

//=============================================================================
//
// CTeamplayRoundWin tables.
//
BEGIN_DATADESC( CTeamplayRoundWin )

	DEFINE_KEYFIELD( m_bForceMapReset, FIELD_BOOLEAN, "force_map_reset" ),
	DEFINE_KEYFIELD( m_bSwitchTeamsOnWin, FIELD_BOOLEAN, "switch_teams" ),
	DEFINE_KEYFIELD( m_iWinReason, FIELD_INTEGER, "win_reason" ),

	// Inputs.
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetTeam", InputSetTeam ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundWin", InputRoundWin ),

	// Outputs.
	DEFINE_OUTPUT( m_outputOnRoundWin, "OnRoundWin" ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( game_round_win, CTeamplayRoundWin );

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CTeamplayRoundWin::CTeamplayRoundWin()
{
	// default win reason for map-fired event (map may change it)
	m_iWinReason = WINREASON_DEFEND_UNTIL_TIME_LIMIT;	
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTeamplayRoundWin::KeyValue( const char *szKeyName, const char *szValue )
{
#ifdef MAPBASE
	if ( FStrEq( szKeyName, "win_reason" ) )
	{
		// Allow string input
		if ( szValue[0] == 'W' )
		{
			// Strip the WINREASON_ so that we only compare the part that matters
			const char *pszCompareValue = szValue + 10;
			for ( int i = 0; i < ARRAYSIZE( g_pszWinReasons ); i++ )
			{
				if ( FStrEq( pszCompareValue, g_pszWinReasons[i] ) )
				{
					m_iWinReason = i;
					return true;
				}
			}

			// Win reason string does not match any known value
			Warning( "%s: Invalid win reason \"%s\"", GetDebugName(), szValue );
		}

		// Fall through to base class for integer handling
	}
#endif

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTeamplayRoundWin::RoundWin( void )
{
    CTeamplayRoundBasedRules *pGameRules = dynamic_cast<CTeamplayRoundBasedRules *>( GameRules() );

	if ( pGameRules )
	{
		int iTeam = GetTeamNumber();

		if ( iTeam > LAST_SHARED_TEAM )
		{
			if ( !m_bForceMapReset )
			{
				pGameRules->SetWinningTeam( iTeam, m_iWinReason, m_bForceMapReset );
			}
			else
			{
				pGameRules->SetWinningTeam( iTeam, m_iWinReason, m_bForceMapReset, m_bSwitchTeamsOnWin );
			}
		}
		else
		{
			pGameRules->SetStalemate( STALEMATE_TIMER, m_bForceMapReset, m_bSwitchTeamsOnWin );
		}
	}

	// Output.
	m_outputOnRoundWin.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTeamplayRoundWin::InputRoundWin( inputdata_t &inputdata )
{
	RoundWin();
}

