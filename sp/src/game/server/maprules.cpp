//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains entities for implementing/changing game rules dynamically within each BSP.
//
//=============================================================================//

#include "cbase.h"
#include "datamap.h"
#include "gamerules.h"
#include "maprules.h"
#include "player.h"
#include "entitylist.h"
#include "ai_hull.h"
#include "entityoutput.h"
#ifdef MAPBASE
#include "eventqueue.h"
#include "saverestore_utlvector.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CRuleEntity : public CBaseEntity
{
public:
	DECLARE_CLASS( CRuleEntity, CBaseEntity );

	void	Spawn( void );

	DECLARE_DATADESC();

	void	SetMaster( string_t iszMaster ) { m_iszMaster = iszMaster; }

protected:
	bool	CanFireForActivator( CBaseEntity *pActivator );

private:
	string_t	m_iszMaster;
};

BEGIN_DATADESC( CRuleEntity )

	DEFINE_KEYFIELD( m_iszMaster, FIELD_STRING, "master" ),

END_DATADESC()



void CRuleEntity::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
}


bool CRuleEntity::CanFireForActivator( CBaseEntity *pActivator )
{
	if ( m_iszMaster != NULL_STRING )
	{
		if ( UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
			return true;
		else
			return false;
	}
	
	return true;
}

// 
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CRulePointEntity, CRuleEntity );

	int		m_Score;
	void		Spawn( void );
};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CRulePointEntity )

	DEFINE_FIELD( m_Score,	FIELD_INTEGER ),

END_DATADESC()


void CRulePointEntity::Spawn( void )
{
	BaseClass::Spawn();
	SetModelName( NULL_STRING );
	m_Score = 0;
}

// 
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
public:
	DECLARE_CLASS( CRuleBrushEntity, CRuleEntity );

	void		Spawn( void );

private:
};

void CRuleBrushEntity::Spawn( void )
{
	SetModel( STRING( GetModelName() ) );
	BaseClass::Spawn();
}


// CGameScore / game_score	-- award points to player / team 
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

#define SF_SCORE_NEGATIVE			0x0001
#define SF_SCORE_TEAM				0x0002

class CGameScore : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameScore, CRulePointEntity );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	inline	int		Points( void ) { return m_Score; }
	inline	bool	AllowNegativeScore( void ) { return m_spawnflags & SF_SCORE_NEGATIVE; }
	inline	int		AwardToTeam( void ) { return (m_spawnflags & SF_SCORE_TEAM); }

	inline	void	SetPoints( int points ) { m_Score = points; }

	void InputApplyScore( inputdata_t &inputdata );

private:
};

LINK_ENTITY_TO_CLASS( game_score, CGameScore );

BEGIN_DATADESC( CGameScore )
	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ApplyScore", InputApplyScore ),
END_DATADESC()

void CGameScore::Spawn( void )
{
	int iScore = Points();
	BaseClass::Spawn();
	SetPoints( iScore );
}


bool CGameScore::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "points"))
	{
		SetPoints( atoi(szValue) );
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

void CGameScore::InputApplyScore( inputdata_t &inputdata )
{
	CBaseEntity *pActivator = inputdata.pActivator;

	if ( pActivator == NULL )
		 return;

	if ( CanFireForActivator( pActivator ) == false )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}

void CGameScore::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	// Only players can use this
	if ( pActivator->IsPlayer() )
	{
		if ( AwardToTeam() )
		{
			pActivator->AddPointsToTeam( Points(), AllowNegativeScore() );
		}
		else
		{
			pActivator->AddPoints( Points(), AllowNegativeScore() );
		}
	}
}


// CGameEnd / game_end	-- Ends the game in MP

class CGameEnd : public CRulePointEntity
{
	DECLARE_CLASS( CGameEnd, CRulePointEntity );

public:
	DECLARE_DATADESC();

	void	InputGameEnd( inputdata_t &inputdata );
#ifdef MAPBASE
	void	InputGameEndSP( inputdata_t &inputdata );
#endif
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
private:
};

BEGIN_DATADESC( CGameEnd )

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "EndGame", InputGameEnd ),
#ifdef MAPBASE
	DEFINE_INPUTFUNC( FIELD_STRING, "EndGameSP", InputGameEndSP ),
#endif

END_DATADESC()

LINK_ENTITY_TO_CLASS( game_end, CGameEnd );


void CGameEnd::InputGameEnd( inputdata_t &inputdata )
{
	g_pGameRules->EndMultiplayerGame();
}

#ifdef MAPBASE
void CGameEnd::InputGameEndSP( inputdata_t &inputdata )
{
	// This basically just acts as a shortcut for the "startupmenu force"/disconnection command.
	// Things like mapping competitions could change this code based on given strings for specific endings (e.g. background maps).
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (pPlayer)
		engine->ClientCommand(pPlayer->edict(), "startupmenu force");
}
#endif

void CGameEnd::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	g_pGameRules->EndMultiplayerGame();
}


//
// CGameText / game_text	-- NON-Localized HUD Message (use env_message to display a titles.txt message)
//	Flag: All players					SF_ENVTEXT_ALLPLAYERS
//
#define SF_ENVTEXT_ALLPLAYERS			0x0001


class CGameText : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameText, CRulePointEntity );

	bool	KeyValue( const char *szKeyName, const char *szValue );

	DECLARE_DATADESC();

	inline	bool	MessageToAll( void ) { return (m_spawnflags & SF_ENVTEXT_ALLPLAYERS); }
	inline	void	MessageSet( const char *pMessage ) { m_iszMessage = AllocPooledString(pMessage); }
	inline	const char *MessageGet( void )	{ return STRING( m_iszMessage ); }

	void InputDisplay( inputdata_t &inputdata );
	void Display( CBaseEntity *pActivator );
#ifdef MAPBASE
	void InputSetText ( inputdata_t &inputdata );
	void SetText( const char* pszStr );

	void InputSetFont( inputdata_t &inputdata ) { m_strFont = inputdata.value.StringID(); }
#endif

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		Display( pActivator );
	}

private:

	string_t m_iszMessage;
	hudtextparms_t	m_textParms;

#ifdef MAPBASE
	string_t m_strFont;
	bool m_bAutobreak;
#endif
};

LINK_ENTITY_TO_CLASS( game_text, CGameText );

// Save parms as a block.  Will break save/restore if the structure changes, but this entity didn't ship with Half-Life, so
// it can't impact saved Half-Life games.
BEGIN_DATADESC( CGameText )

	DEFINE_KEYFIELD( m_iszMessage, FIELD_STRING, "message" ),

	DEFINE_KEYFIELD( m_textParms.channel, FIELD_INTEGER, "channel" ),
	DEFINE_KEYFIELD( m_textParms.x, FIELD_FLOAT, "x" ),
	DEFINE_KEYFIELD( m_textParms.y, FIELD_FLOAT, "y" ),
	DEFINE_KEYFIELD( m_textParms.effect, FIELD_INTEGER, "effect" ),
	DEFINE_KEYFIELD( m_textParms.fadeinTime, FIELD_FLOAT, "fadein" ),
	DEFINE_KEYFIELD( m_textParms.fadeoutTime, FIELD_FLOAT, "fadeout" ),
	DEFINE_KEYFIELD( m_textParms.holdTime, FIELD_FLOAT, "holdtime" ),
	DEFINE_KEYFIELD( m_textParms.fxTime, FIELD_FLOAT, "fxtime" ),

#ifdef MAPBASE
	DEFINE_KEYFIELD( m_strFont, FIELD_STRING, "font" ),
	DEFINE_KEYFIELD( m_bAutobreak, FIELD_BOOLEAN, "autobreak" ),
#endif

	DEFINE_ARRAY( m_textParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Display", InputDisplay ),
#ifdef MAPBASE
	DEFINE_INPUTFUNC( FIELD_STRING, "SetText", InputSetText ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetFont", InputSetFont ),
#endif

END_DATADESC()



bool CGameText::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "color"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, szValue );
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
	}
	else if (FStrEq(szKeyName, "color2"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, szValue );
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
	}
#ifdef MAPBASE
	else if (FStrEq( szKeyName, "message" ))
	{
		// Needed for newline support
		SetText( szValue );
	}
#endif
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


void CGameText::InputDisplay( inputdata_t &inputdata )
{
	Display( inputdata.pActivator );
}

void CGameText::Display( CBaseEntity *pActivator )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( MessageToAll() )
	{
#ifdef MAPBASE
		UTIL_HudMessageAll( m_textParms, MessageGet(), STRING(m_strFont), m_bAutobreak );
#else
		UTIL_HudMessageAll( m_textParms, MessageGet() );
#endif
	}
	else
	{
		// If we're in singleplayer, show the message to the player.
		if ( gpGlobals->maxClients == 1 )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#ifdef MAPBASE
			UTIL_HudMessage( pPlayer, m_textParms, MessageGet(), STRING(m_strFont), m_bAutobreak );
#else
			UTIL_HudMessage( pPlayer, m_textParms, MessageGet() );
#endif
		}
		// Otherwise show the message to the player that triggered us.
		else if ( pActivator && pActivator->IsNetClient() )
		{
#ifdef MAPBASE
			UTIL_HudMessage( ToBasePlayer( pActivator ), m_textParms, MessageGet(), STRING(m_strFont), m_bAutobreak );
#else
			UTIL_HudMessage( ToBasePlayer( pActivator ), m_textParms, MessageGet() );
#endif
		}
	}
}

#ifdef MAPBASE
void CGameText::InputSetText( inputdata_t &inputdata )
{
	SetText( inputdata.value.String() );
}

void CGameText::SetText( const char* pszStr )
{
	// Replace /n with \n
	if (Q_strstr( pszStr, "/n" ))
	{
		CUtlStringList vecLines;
		Q_SplitString( pszStr, "/n", vecLines );

		char szMsg[512];
		Q_strncpy( szMsg, vecLines[0], sizeof( szMsg ) );

		for (int i = 1; i < vecLines.Count(); i++)
		{
			Q_strncat( szMsg, "\n", sizeof( szMsg ) );
			Q_strncat( szMsg, vecLines[i], sizeof( szMsg ) );
		}
		m_iszMessage = AllocPooledString( szMsg );
	}
	else
	{
		m_iszMessage = AllocPooledString( pszStr );
	}
}
#endif


/* TODO: Replace with an entity I/O version
//
// CGameTeamSet / game_team_set	-- Changes the team of the entity it targets to the activator's team
// Flag: Fire once
// Flag: Clear team				-- Sets the team to "NONE" instead of activator

#define SF_TEAMSET_FIREONCE			0x0001
#define SF_TEAMSET_CLEARTEAM		0x0002

class CGameTeamSet : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGameTeamSet, CRulePointEntity );

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_TEAMSET_FIREONCE) ? true : false; }
	inline bool ShouldClearTeam( void ) { return (m_spawnflags & SF_TEAMSET_CLEARTEAM) ? true : false; }
	void InputTrigger( inputdata_t &inputdata );

private:
	COutputEvent m_OnTrigger;
};

LINK_ENTITY_TO_CLASS( game_team_set, CGameTeamSet );


void CGameTeamSet::InputTrigger( inputdata_t &inputdata )
{
	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	if ( ShouldClearTeam() )
	{
		// clear the team of our target
	}
	else
	{
		// set the team of our target to our activator's team
	}

	m_OnTrigger.FireOutput(pActivator, this);

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/


//
// CGamePlayerZone / game_player_zone -- players in the zone fire my target when I'm fired
//
// Needs master?
class CGamePlayerZone : public CRuleBrushEntity
{
public:
	DECLARE_CLASS( CGamePlayerZone, CRuleBrushEntity );
	void InputCountPlayersInZone( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	COutputEvent m_OnPlayerInZone;
	COutputEvent m_OnPlayerOutZone;

	COutputInt m_PlayersInCount;
	COutputInt m_PlayersOutCount;
};

LINK_ENTITY_TO_CLASS( game_zone_player, CGamePlayerZone );
BEGIN_DATADESC( CGamePlayerZone )

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "CountPlayersInZone", InputCountPlayersInZone),

	// Outputs
	DEFINE_OUTPUT(m_OnPlayerInZone, "OnPlayerInZone"),
	DEFINE_OUTPUT(m_OnPlayerOutZone, "OnPlayerOutZone"),
	DEFINE_OUTPUT(m_PlayersInCount, "PlayersInCount"),
	DEFINE_OUTPUT(m_PlayersOutCount, "PlayersOutCount"),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Counts all the players in the zone. Fires one output per player
//			in the zone, one output per player out of the zone, and outputs
//			with the total counts of players in and out of the zone.
//-----------------------------------------------------------------------------
void CGamePlayerZone::InputCountPlayersInZone( inputdata_t &inputdata )
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if ( !CanFireForActivator( inputdata.pActivator ) )
		return;

	CBaseEntity *pPlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			trace_t		trace;
			Hull_t		hullType;

			hullType = HULL_HUMAN;
			if ( pPlayer->GetFlags() & FL_DUCKING )
			{
				hullType = HULL_SMALL_CENTERED;
			}

			UTIL_TraceModel( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), NAI_Hull::Mins(hullType), 
				NAI_Hull::Maxs(hullType), this, COLLISION_GROUP_NONE, &trace );

			if ( trace.startsolid )
			{
				playersInCount++;
				m_OnPlayerInZone.FireOutput(pPlayer, this);
			}
			else
			{
				playersOutCount++;
				m_OnPlayerOutZone.FireOutput(pPlayer, this);
			}
		}
	}

	m_PlayersInCount.Set(playersInCount, inputdata.pActivator, this);
	m_PlayersOutCount.Set(playersOutCount, inputdata.pActivator, this);
}


/*
// Disable.  Eventually will be replace by new activator filter entities.  (LHL)
//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once

#define SF_PKILL_FIREONCE			0x0001
class CGamePlayerHurt : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerHurt, CRulePointEntity );

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PKILL_FIREONCE) ? true : false; }

	DECLARE_DATADESC();

private:
	
	float m_flDamage;		// Damage to inflict, negative values give health.

	COutputEvent m_OnUse;
};

LINK_ENTITY_TO_CLASS( game_player_hurt, CGamePlayerHurt );


BEGIN_DATADESC( CGamePlayerHurt )

	DEFINE_KEYFIELD( m_flDamage, FIELD_FLOAT, "dmg" ),

END_DATADESC()



void CGamePlayerHurt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( m_flDamage < 0 )
		{
			pActivator->TakeHealth( -m_flDamage, DMG_GENERIC );
		}
		else
		{
			pActivator->TakeDamage( this, this, m_flDamage, DMG_GENERIC );
		}
	}
	
	SUB_UseTargets( pActivator, useType, value );
	m_OnUse.FireOutput(pActivator, this); // dvsents2: handle useType and value here - they are passed through

	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}
*/

//
// CGamePlayerEquip / game_playerequip	-- Sets the default player equipment
// Flag: USE Only

#define SF_PLAYEREQUIP_USEONLY			0x0001
#define MAX_EQUIP		32

class CGamePlayerEquip : public CRulePointEntity
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CGamePlayerEquip, CRulePointEntity );

	bool		KeyValue( const char *szKeyName, const char *szValue );
	void		Touch( CBaseEntity *pOther );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline bool	UseOnly( void ) { return (m_spawnflags & SF_PLAYEREQUIP_USEONLY) ? true : false; }

private:

	void		EquipPlayer( CBaseEntity *pPlayer );

	string_t	m_weaponNames[MAX_EQUIP];
	int			m_weaponCount[MAX_EQUIP];
};

LINK_ENTITY_TO_CLASS( game_player_equip, CGamePlayerEquip );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CGamePlayerEquip )

	DEFINE_AUTO_ARRAY( m_weaponNames,		FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_weaponCount,		FIELD_INTEGER ),

END_DATADESC()




bool CGamePlayerEquip::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue( szKeyName, szValue ) )
	{
		for ( int i = 0; i < MAX_EQUIP; i++ )
		{
			if ( !m_weaponNames[i] )
			{
				char tmp[128];

				UTIL_StripToken( szKeyName, tmp );

				m_weaponNames[i] = AllocPooledString(tmp);
				m_weaponCount[i] = atoi(szValue);
				m_weaponCount[i] = MAX(1,m_weaponCount[i]);
				return true;
			}
		}
	}

	return false;
}


void CGamePlayerEquip::Touch( CBaseEntity *pOther )
{
	if ( !CanFireForActivator( pOther ) )
		return;

	if ( UseOnly() )
		return;

	EquipPlayer( pOther );
}

void CGamePlayerEquip::EquipPlayer( CBaseEntity *pEntity )
{
	CBasePlayer *pPlayer = ToBasePlayer(pEntity);

	if ( !pPlayer )
		return;

	for ( int i = 0; i < MAX_EQUIP; i++ )
	{
		if ( !m_weaponNames[i] )
			break;
		for ( int j = 0; j < m_weaponCount[i]; j++ )
		{
 			pPlayer->GiveNamedItem( STRING(m_weaponNames[i]) );
		}
	}
}


void CGamePlayerEquip::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EquipPlayer( pActivator );
}


//
// CGamePlayerTeam / game_player_team	-- Changes the team of the player who fired it
// Flag: Fire once
// Flag: Kill Player
// Flag: Gib Player

#define SF_PTEAM_FIREONCE			0x0001
#define SF_PTEAM_KILL    			0x0002
#define SF_PTEAM_GIB     			0x0004

class CGamePlayerTeam : public CRulePointEntity
{
public:
	DECLARE_CLASS( CGamePlayerTeam, CRulePointEntity );

	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	inline bool RemoveOnFire( void ) { return (m_spawnflags & SF_PTEAM_FIREONCE) ? true : false; }
	inline bool ShouldKillPlayer( void ) { return (m_spawnflags & SF_PTEAM_KILL) ? true : false; }
	inline bool ShouldGibPlayer( void ) { return (m_spawnflags & SF_PTEAM_GIB) ? true : false; }
	
	const char *TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator );
};

LINK_ENTITY_TO_CLASS( game_player_team, CGamePlayerTeam );


const char *CGamePlayerTeam::TargetTeamName( const char *pszTargetName, CBaseEntity *pActivator )
{
	CBaseEntity *pTeamEntity = NULL;

	while ((pTeamEntity = gEntList.FindEntityByName( pTeamEntity, pszTargetName, NULL, pActivator )) != NULL)
	{
		if ( FClassnameIs( pTeamEntity, "game_team_master" ) )
			return pTeamEntity->TeamID();
	}

	return NULL;
}


void CGamePlayerTeam::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !CanFireForActivator( pActivator ) )
		return;

	if ( pActivator->IsPlayer() )
	{
		const char *pszTargetTeam = TargetTeamName( STRING(m_target), pActivator );
		if ( pszTargetTeam )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
			g_pGameRules->ChangePlayerTeam( pPlayer, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer() );
		}
	}
	
	if ( RemoveOnFire() )
	{
		UTIL_Remove( this );
	}
}


#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Displays a custom number menu for player(s)
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( game_menu, CGameMenu );

BEGIN_DATADESC( CGameMenu )

	DEFINE_UTLVECTOR( m_ActivePlayers, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_ActivePlayerTimes, FIELD_TIME ),

	DEFINE_KEYFIELD( m_flDisplayTime, FIELD_FLOAT, "holdtime" ),

	DEFINE_KEYFIELD( m_iszTitle, FIELD_STRING, "Title" ),

	DEFINE_KEYFIELD( m_iszOption[0], FIELD_STRING, "Case01" ),
	DEFINE_KEYFIELD( m_iszOption[1], FIELD_STRING, "Case02" ),
	DEFINE_KEYFIELD( m_iszOption[2], FIELD_STRING, "Case03" ),
	DEFINE_KEYFIELD( m_iszOption[3], FIELD_STRING, "Case04" ),
	DEFINE_KEYFIELD( m_iszOption[4], FIELD_STRING, "Case05" ),
	DEFINE_KEYFIELD( m_iszOption[5], FIELD_STRING, "Case06" ),
	DEFINE_KEYFIELD( m_iszOption[6], FIELD_STRING, "Case07" ),
	DEFINE_KEYFIELD( m_iszOption[7], FIELD_STRING, "Case08" ),
	DEFINE_KEYFIELD( m_iszOption[8], FIELD_STRING, "Case09" ),
	DEFINE_KEYFIELD( m_iszOption[9], FIELD_STRING, "Case10" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ShowMenu", InputShowMenu ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HideMenu", InputHideMenu ),
	DEFINE_INPUTFUNC( FIELD_VOID, "__DoRestore", InputDoRestore ),

	// Outputs
	DEFINE_OUTPUT( m_OnCase[0], "OnCase01" ),
	DEFINE_OUTPUT( m_OnCase[1], "OnCase02" ),
	DEFINE_OUTPUT( m_OnCase[2], "OnCase03" ),
	DEFINE_OUTPUT( m_OnCase[3], "OnCase04" ),
	DEFINE_OUTPUT( m_OnCase[4], "OnCase05" ),
	DEFINE_OUTPUT( m_OnCase[5], "OnCase06" ),
	DEFINE_OUTPUT( m_OnCase[6], "OnCase07" ),
	DEFINE_OUTPUT( m_OnCase[7], "OnCase08" ),
	DEFINE_OUTPUT( m_OnCase[8], "OnCase09" ),
	DEFINE_OUTPUT( m_OnCase[9], "OnCase10" ),
	DEFINE_OUTPUT( m_OutValue, "OutValue" ),
	DEFINE_OUTPUT( m_OnTimeout, "OnTimeout" ),

	DEFINE_THINKFUNC( TimeoutThink ),

END_DATADESC()

IMPLEMENT_AUTO_LIST( IGameMenuAutoList );

static const char *s_pTimeoutContext = "TimeoutContext";

#define	MENU_INFINITE_TIME	-1.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGameMenu::CGameMenu()
{
	m_flDisplayTime = 5.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnRestore()
{
	BaseClass::OnRestore();

	// Do this a bit after we restore since the HUD might not be ready yet
	g_EventQueue.AddEvent( this, "__DoRestore", 0.4f, this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::InputDoRestore( inputdata_t &inputdata )
{
	// Check if we should restore the menu on anyone
	FOR_EACH_VEC_BACK( m_ActivePlayers, i )
	{
		if (m_ActivePlayers[i].Get())
		{
			if (m_ActivePlayerTimes[i] > gpGlobals->curtime || m_ActivePlayerTimes[i] == MENU_INFINITE_TIME)
			{
				CRecipientFilter filter;
				filter.AddRecipient( static_cast<CBasePlayer*>( m_ActivePlayers[i].Get() ) );

				ShowMenu( filter, m_ActivePlayerTimes[i] - gpGlobals->curtime );
				continue;
			}
		}

		// Remove this player since it's no longer valid
		m_ActivePlayers.Remove( i );
		m_ActivePlayerTimes.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::TimeoutThink()
{
	float flNextLowestTime = FLT_MAX;
	FOR_EACH_VEC( m_ActivePlayerTimes, i )
	{
		// If the player is still in our list, then they must not have selected an option
		if (m_ActivePlayerTimes[i] != MENU_INFINITE_TIME)
		{
			if (m_ActivePlayerTimes[i] <= gpGlobals->curtime)
			{
				m_OnTimeout.FireOutput( m_ActivePlayers[i], this );

				// Remove this player since it's no longer valid
				m_ActivePlayers.Remove( i );
				m_ActivePlayerTimes.Remove( i );
				break;
			}
			else if (m_ActivePlayerTimes[i] < flNextLowestTime)
			{
				flNextLowestTime = m_ActivePlayerTimes[i];
			}
		}
	}

	if (flNextLowestTime < FLT_MAX)
	{
		SetNextThink( flNextLowestTime, s_pTimeoutContext );
	}
	else
	{
		SetContextThink( NULL, TICK_NEVER_THINK, s_pTimeoutContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::ShowMenu( CRecipientFilter &filter, float flDisplayTime )
{
	// Before showing the menu, check each menu to see if there's already one being shown to one of our recipients
	for ( int i = 0; i < IGameMenuAutoList::AutoList().Count(); i++ )
	{
		CGameMenu *pMenu = static_cast<CGameMenu*>( IGameMenuAutoList::AutoList()[i] );
		if ( pMenu != this && pMenu->IsActive() )
		{
			for ( int j = 0; j < filter.GetRecipientCount(); j++ )
			{
				CBaseEntity *ent = CBaseEntity::Instance( filter.GetRecipientIndex( j ) );
				if ( pMenu->IsActiveOnTarget( ent ) )
				{
					Msg( "%s overriding menu %s for player %i\n", GetDebugName(), pMenu->GetDebugName(), j );
					pMenu->RemoveTarget( ent );
				}
			}
		}
	}

	if (flDisplayTime == 0.0f)
	{
		flDisplayTime = m_flDisplayTime;
	}

	char szString[512] = { 0 };
	int nBitsValidSlots = 0;

	if (m_iszTitle != NULL_STRING)
	{
		V_strncat( szString, STRING( m_iszTitle ), sizeof( szString ) );
	}
	else
	{
		// Insert space to tell menu code to skip
		V_strncat( szString, " ", sizeof( szString ) );
	}

	// Insert newline even if there's no string
	V_strncat( szString, "\n", sizeof( szString ) );

	// Populate the options
	for (int i = 0; i < MAX_MENU_OPTIONS; i++)
	{
		if (m_iszOption[i] != NULL_STRING)
		{
			nBitsValidSlots |= (1 << i);

			V_strncat( szString, STRING( m_iszOption[i] ), sizeof( szString ) );
		}
		else
		{
			// Insert space to tell menu code to skip
			V_strncat( szString, " ", sizeof( szString ) );
		}

		// Insert newline even if there's no string
		V_strncat( szString, "\n", sizeof( szString ) );
	}

	if (nBitsValidSlots <= 0 && m_iszTitle == NULL_STRING)
	{
		Warning( "%s ShowMenu: Can't show menu with no options or title\n", GetDebugName() );
		return;
	}

	UserMessageBegin( filter, "ShowMenuComplex" );
		WRITE_WORD( nBitsValidSlots );
		WRITE_FLOAT( flDisplayTime );
		WRITE_BYTE( 0 );
		WRITE_STRING( szString );
	MessageEnd();

	float flMenuTime;
	if (flDisplayTime <= 0.0f)
	{
		flMenuTime = MENU_INFINITE_TIME;
	}
	else
	{
		flMenuTime = gpGlobals->curtime + flDisplayTime;
	}

	for ( int j = 0; j < filter.GetRecipientCount(); j++ )
	{
		CBaseEntity *ent = CBaseEntity::Instance( filter.GetRecipientIndex( j ) );

		// Check if we already track this player. If not, make a new one
		bool bFound = false;
		FOR_EACH_VEC( m_ActivePlayers, i )
		{
			if (m_ActivePlayers[i].Get() == ent)
			{
				m_ActivePlayerTimes[i] = flMenuTime;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			m_ActivePlayers.AddToTail( ent );
			m_ActivePlayerTimes.AddToTail( flMenuTime );
		}
	}

	if (GetNextThink( s_pTimeoutContext ) == TICK_NEVER_THINK)
	{
		SetContextThink( &CGameMenu::TimeoutThink, gpGlobals->curtime + flDisplayTime, s_pTimeoutContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::HideMenu( CRecipientFilter &filter )
{
	UserMessageBegin( filter, "ShowMenuComplex" );
		WRITE_WORD( -1 );
		WRITE_FLOAT( 0.0f );
		WRITE_BYTE( 0 );
		WRITE_STRING( "" );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::MenuSelected( int nSlot, CBaseEntity *pActivator )
{
	if (nSlot <= 0 || nSlot > MAX_MENU_OPTIONS)
	{
		Warning( "%s: Invalid slot %i\n", GetDebugName(), nSlot );
		return;
	}

	m_OnCase[nSlot-1].FireOutput( pActivator, this );
	m_OutValue.Set( nSlot, pActivator, this );

	RemoveTarget( pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGameMenu::IsActive()
{
	FOR_EACH_VEC_BACK( m_ActivePlayers, i )
	{
		if (m_ActivePlayers[i].Get())
		{
			if (m_ActivePlayerTimes[i] > gpGlobals->curtime || m_ActivePlayerTimes[i] == MENU_INFINITE_TIME)
				return true;
		}

		// Remove this player since it's no longer valid
		m_ActivePlayers.Remove( i );
		m_ActivePlayerTimes.Remove( i );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGameMenu::IsActiveOnTarget( CBaseEntity *pPlayer )
{
	FOR_EACH_VEC_BACK( m_ActivePlayers, i )
	{
		if (m_ActivePlayers[i].Get() == pPlayer)
		{
			if (m_ActivePlayerTimes[i] > gpGlobals->curtime || m_ActivePlayerTimes[i] == MENU_INFINITE_TIME)
				return true;

			// Remove this player since it's no longer valid
			m_ActivePlayers.Remove( i );
			m_ActivePlayerTimes.Remove( i );
			return false;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::RemoveTarget( CBaseEntity *pPlayer )
{
	FOR_EACH_VEC_BACK( m_ActivePlayers, i )
	{
		if (m_ActivePlayers[i].Get() == pPlayer)
		{
			m_ActivePlayers.Remove( i );
			m_ActivePlayerTimes.Remove( i );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::InputShowMenu( inputdata_t &inputdata )
{
	if (HasSpawnFlags( SF_GAMEMENU_ALLPLAYERS ))
	{
		CRecipientFilter filter;
		filter.AddAllPlayers();

		ShowMenu( filter );
	}
	else
	{
		CBasePlayer *pPlayer = NULL;

		// If we're in singleplayer, show the message to the player.
		if ( gpGlobals->maxClients == 1 )
		{
			pPlayer = UTIL_GetLocalPlayer();
		}
		// Otherwise show the message to the player that triggered us.
		else if ( inputdata.pActivator && inputdata.pActivator->IsNetClient() )
		{
			pPlayer = ToBasePlayer( inputdata.pActivator );
		}

		if (pPlayer)
		{
			CRecipientFilter filter;
			filter.AddRecipient( pPlayer );

			ShowMenu( filter );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::InputHideMenu( inputdata_t &inputdata )
{
	if (HasSpawnFlags( SF_GAMEMENU_ALLPLAYERS ))
	{
		CRecipientFilter filter;

		FOR_EACH_VEC_BACK( m_ActivePlayers, i )
		{
			// Select all players in our list who are still active, and remove them
			if (m_ActivePlayerTimes[i] > gpGlobals->curtime || m_ActivePlayerTimes[i] == MENU_INFINITE_TIME)
			{
				filter.AddRecipient( static_cast<CBasePlayer*>(m_ActivePlayers[i].Get()) );
			}

			m_ActivePlayers.Remove( i );
			m_ActivePlayerTimes.Remove( i );
		}

		if (filter.GetRecipientCount() <= 0)
			return;

		HideMenu( filter );
	}
	else
	{
		CBasePlayer *pPlayer = NULL;

		// If we're in singleplayer, show the message to the player.
		if ( gpGlobals->maxClients == 1 )
		{
			pPlayer = UTIL_GetLocalPlayer();
		}
		// Otherwise show the message to the player that triggered us.
		else if ( inputdata.pActivator && inputdata.pActivator->IsNetClient() )
		{
			pPlayer = ToBasePlayer( inputdata.pActivator );
		}

		if (!pPlayer)
			return;

		// Verify that this player is in our list
		CRecipientFilter filter;
		FOR_EACH_VEC( m_ActivePlayers, i )
		{
			if (m_ActivePlayers[i].Get() == pPlayer && (m_ActivePlayerTimes[i] > gpGlobals->curtime || m_ActivePlayerTimes[i] == MENU_INFINITE_TIME))
			{
				filter.AddRecipient( pPlayer );

				// Remove since the player won't have the menu anymore
				m_ActivePlayers.Remove( i );
				m_ActivePlayerTimes.Remove( i );
				break;
			}
		}

		if (filter.GetRecipientCount() <= 0)
			return;

		HideMenu( filter );
	}
}
#endif


