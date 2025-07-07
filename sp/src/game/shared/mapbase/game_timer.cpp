//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Generic custom timer entity
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "game_timer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool IsInCommentaryMode( void );

ConVar	sv_game_menu_default_warn_frac( "sv_game_menu_default_warn_frac", "0.25", FCVAR_REPLICATED );

LINK_ENTITY_TO_CLASS( game_timer, CGameTimer );

IMPLEMENT_NETWORKCLASS_ALIASED( GameTimer, DT_GameTimer )

BEGIN_NETWORK_TABLE_NOBASE( CGameTimer, DT_GameTimer )
#ifdef CLIENT_DLL

	RecvPropBool( RECVINFO( m_bTimerPaused ) ),
	RecvPropTime( RECVINFO( m_flTimerInitialLength ) ),
	RecvPropTime( RECVINFO( m_flTimerMaxLength ) ),
	RecvPropTime( RECVINFO( m_flTimeRemaining ) ),
	RecvPropTime( RECVINFO( m_flTimerEndTime ) ),
	RecvPropTime( RECVINFO( m_flWarnTime ) ),
	RecvPropBool( RECVINFO( m_bIsDisabled ) ),
	RecvPropBool( RECVINFO( m_bStartPaused ) ),
	RecvPropBool( RECVINFO( m_bShowTimeRemaining ) ),
	RecvPropInt( RECVINFO( m_nProgressBarMaxSegments ) ),
	RecvPropInt( RECVINFO( m_nProgressBarOverride ) ),
	RecvPropFloat( RECVINFO( m_flOverrideX ) ),
	RecvPropFloat( RECVINFO( m_flOverrideY ) ),
	RecvPropString( RECVINFO( m_szTimerCaption ) ),

	RecvPropInt( RECVINFO( m_iTeamNum ) ),

#else

	SendPropBool( SENDINFO( m_bTimerPaused ) ),
	SendPropTime( SENDINFO( m_flTimerInitialLength ) ),
	SendPropTime( SENDINFO( m_flTimerMaxLength ) ),
	SendPropTime( SENDINFO( m_flTimeRemaining ) ),
	SendPropTime( SENDINFO( m_flTimerEndTime ) ),
	SendPropTime( SENDINFO( m_flWarnTime ) ),
	SendPropBool( SENDINFO( m_bIsDisabled ) ),
	SendPropBool( SENDINFO( m_bStartPaused ) ),
	SendPropBool( SENDINFO( m_bShowTimeRemaining ) ),
	SendPropInt( SENDINFO( m_nProgressBarMaxSegments ) ),
	SendPropInt( SENDINFO( m_nProgressBarOverride ) ),
	SendPropFloat( SENDINFO( m_flOverrideX ) ),
	SendPropFloat( SENDINFO( m_flOverrideY ) ),
	SendPropString( SENDINFO( m_szTimerCaption ) ),

	SendPropInt( SENDINFO( m_iTeamNum ), TEAMNUM_NUM_BITS, 0 ),

#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
BEGIN_DATADESC( CGameTimer )

	DEFINE_KEYFIELD( m_flTimerInitialLength,		FIELD_FLOAT,	"timer_length" ),
	DEFINE_KEYFIELD( m_flTimerMaxLength,			FIELD_FLOAT,	"max_length" ),
	DEFINE_KEYFIELD( m_flWarnTime,				FIELD_FLOAT,	"warn_time" ),
	DEFINE_KEYFIELD( m_bIsDisabled,				FIELD_BOOLEAN,	"StartDisabled" ),
	DEFINE_KEYFIELD( m_bStartPaused,			FIELD_BOOLEAN,	"start_paused" ),
	DEFINE_KEYFIELD( m_bShowTimeRemaining,		FIELD_BOOLEAN,	"show_time_remaining" ),
	DEFINE_KEYFIELD( m_bDisableOnFinish,		FIELD_BOOLEAN,	"disable_on_finish" ),
	DEFINE_KEYFIELD( m_bShowInHUD,				FIELD_BOOLEAN,	"show_in_hud" ),
	DEFINE_KEYFIELD( m_nProgressBarMaxSegments, FIELD_INTEGER,	"progress_bar_max" ),
	DEFINE_KEYFIELD( m_nProgressBarOverride,	FIELD_INTEGER,	"progress_bar_override" ),
	DEFINE_KEYFIELD( m_flOverrideX,				FIELD_FLOAT,	"x" ),
	DEFINE_KEYFIELD( m_flOverrideY,				FIELD_FLOAT,	"y" ),
	DEFINE_AUTO_ARRAY( m_szTimerCaption, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_iszPlayerFilterName,		FIELD_STRING,	"PlayerFilter"			),
	DEFINE_FIELD( m_hPlayerFilter, FIELD_EHANDLE ),

	DEFINE_FIELD( m_flTimerEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTimeRemaining, FIELD_FLOAT ),
	DEFINE_FIELD( m_bTimerPaused, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bStartedWarn, FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_VOID,		"Enable",						InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Disable",						InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Pause",						InputPause ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Resume",						InputResume ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetTime",						InputSetTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"AddTime",						InputAddTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"RemoveTime",					InputRemoveTime ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Restart",						InputRestart ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetMaxTime",					InputSetMaxTime ),
	DEFINE_INPUTFUNC( FIELD_STRING,		"SetTimerCaption",				InputSetTimerCaption ),
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"SetProgressBarMaxSegments",	InputSetProgressBarMaxSegments ),
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"SetProgressBarOverride",		InputSetProgressBarOverride ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetX",							InputSetX ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetY",							InputSetY ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"GetTimeRemaining",				InputGetTimeRemaining ),
	DEFINE_INPUTFUNC( FIELD_STRING,		"SetPlayerFilter",				InputSetPlayerFilter ),

	DEFINE_OUTPUT(	m_OnFinished,			"OnFinished" ),
	DEFINE_OUTPUT(	m_OnPaused,				"OnPaused" ),
	DEFINE_OUTPUT(	m_OnResumed,			"OnResumed" ),
	DEFINE_OUTPUT(	m_OnWarned,				"OnWarned" ),
	DEFINE_OUTPUT(	m_OnTick,				"OnTick" ),
	DEFINE_OUTPUT(	m_OnGetTimeRemaining,	"OnGetTimeRemaining" ),

	DEFINE_THINKFUNC( TimerThink ),

END_DATADESC();
#endif

#ifdef CLIENT_DLL
IMPLEMENT_AUTO_LIST( IGameTimerAutoList );
#else
#define TIMER_THINK			"GameTimerThink"
#endif

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CGameTimer::CGameTimer( void )
{
	m_bIsDisabled = true;
	m_bTimerPaused = true;
	m_flTimeRemaining = 0;
	m_flTimerEndTime = 0;
	m_flWarnTime = -1.0f;
	m_bStartPaused = false;
	m_bShowTimeRemaining = true;
	m_flTimerInitialLength = 0;
	m_flTimerMaxLength = 0;
	m_nProgressBarMaxSegments = -1;
	m_nProgressBarOverride = -1;
	m_flOverrideX = m_flOverrideY = -1.0f;
#ifndef CLIENT_DLL
	m_bDisableOnFinish = true;
	m_bShowInHUD = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CGameTimer::~CGameTimer( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::Spawn( void )
{
#ifndef CLIENT_DLL
	if ( IsDisabled() )  // we need to get the data initialized before actually become disabled
	{
		m_bIsDisabled = false;
		SetTimeRemaining( m_flTimerInitialLength );
		m_bIsDisabled = true;
	}
	else
	{
		SetTimeRemaining( m_flTimerInitialLength );
		if ( !m_bStartPaused )
			ResumeTimer();
	}

	if ( m_iszPlayerFilterName != NULL_STRING )
	{
		m_hPlayerFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( NULL, m_iszPlayerFilterName, this ));
	}
#endif

	BaseClass::Spawn();
}

#ifndef CLIENT_DLL

bool CGameTimer::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "timer_caption" ) )
	{
		Q_strncpy( m_szTimerCaption.GetForModify(), szValue, MAX_GAME_TIMER_CAPTION );
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

bool CGameTimer::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "timer_caption" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%s", m_szTimerCaption.Get() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Gets the seconds left on the timer, paused or not.
//-----------------------------------------------------------------------------
float CGameTimer::GetTimeRemaining( void )
{
	float flSecondsRemaining;

	if ( m_bTimerPaused )
	{
		flSecondsRemaining = m_flTimeRemaining;
	}
	else
	{
		flSecondsRemaining = m_flTimerEndTime - gpGlobals->curtime;
	}

	if ( flSecondsRemaining < 0 )
	{
		flSecondsRemaining = 0.0f;
	}

	return flSecondsRemaining;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the timer's warning time
//-----------------------------------------------------------------------------
float CGameTimer::GetWarnTime( void )
{
	if ( m_flWarnTime < 0 )
	{
		// TODO: All of the default warning stuff is on the client!!!
		return GetTimerMaxLength() * sv_game_menu_default_warn_frac.GetFloat();
	}

	return m_flWarnTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CGameTimer::GetTimerMaxLength( void )
{
	if ( m_flTimerMaxLength )
		return m_flTimerMaxLength;

	return m_flTimerInitialLength;
}

#ifdef CLIENT_DLL

bool g_bAnyGameTimerActive = false;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GameTimer::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( !m_bIsDisabled )
	{
		g_bAnyGameTimerActive = true;
	}
	else if ( !m_bOldDisabled )
	{
		// Go through all of the timers and mark when one is active
		g_bAnyGameTimerActive = false;
		for ( int i = 0; i < IGameTimerAutoList::AutoList().Count(); i++ )
		{
			C_GameTimer *pTimer = static_cast<C_GameTimer *>( IGameTimerAutoList::AutoList()[i] );
			if ( !pTimer->IsDisabled() && !pTimer->IsMarkedForDeletion() )
			{
				g_bAnyGameTimerActive = true;
				break;
			}
		}
	}

	m_bOldDisabled = m_bIsDisabled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GameTimer::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	// Update timer presence state
	g_bAnyGameTimerActive = false;
	for ( int i = 0; i < IGameTimerAutoList::AutoList().Count(); i++ )
	{
		C_GameTimer *pTimer = static_cast<C_GameTimer *>( IGameTimerAutoList::AutoList()[i] );
		if ( pTimer != this && !pTimer->IsDisabled() && !pTimer->IsMarkedForDeletion() )
		{
			g_bAnyGameTimerActive = true;
			break;
		}
	}
}

#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::TimerThink( void )
{
	if ( IsDisabled() /*|| IsInCommentaryMode() || gpGlobals->eLoadType == MapLoad_Background*/ )
	{
		SetContextThink( &CGameTimer::TimerThink, gpGlobals->curtime + 0.05, TIMER_THINK );
		return;
	}

	float flTime = GetTimeRemaining();

	int nTick = (int)floorf( flTime );
	if (nTick != m_OnTick.Get())
	{
		m_OnTick.Set( nTick, this, this );
	}

	if ( flTime <= 0.0f )
	{
		OnTimerFinished();
		PauseTimer();
		if (m_bDisableOnFinish)
			m_bIsDisabled = true;
		return;
	}
	else if ( flTime <= GetWarnTime() && !m_bStartedWarn)
	{
		OnTimerWarned();
		m_bStartedWarn = true;
	}

	SetContextThink( &CGameTimer::TimerThink, gpGlobals->curtime + 0.05, TIMER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: To set the initial timer duration
//-----------------------------------------------------------------------------
void CGameTimer::SetTimeRemaining( float flTime )
{
	// make sure we don't go over our max length
	flTime = m_flTimerMaxLength > 0 ? MIN( flTime, m_flTimerMaxLength ) : flTime;

	m_flTimeRemaining = flTime;
	m_flTimerEndTime = gpGlobals->curtime + m_flTimeRemaining;

	if ( m_flTimeRemaining > m_flWarnTime )
	{
		m_bStartedWarn = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::OnTimerFinished()
{
	m_OnFinished.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::OnTimerWarned()
{
	m_OnWarned.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: Timer is paused at round end, stops the countdown
//-----------------------------------------------------------------------------
void CGameTimer::PauseTimer( CBaseEntity *pActivator )
{
	if ( IsDisabled() )
		return;

	if ( m_bTimerPaused == false )
	{
		m_bTimerPaused = true;
		m_flTimeRemaining = m_flTimerEndTime - gpGlobals->curtime;

		m_OnPaused.FireOutput( pActivator ? pActivator : this, this );

		SetContextThink( NULL, TICK_NEVER_THINK, TIMER_THINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: To start or re-start the timer after a pause
//-----------------------------------------------------------------------------
void CGameTimer::ResumeTimer( CBaseEntity *pActivator )
{
	if ( IsDisabled() )
		return;

	if ( m_bTimerPaused == true )
	{
		m_bTimerPaused = false;
		m_flTimerEndTime = gpGlobals->curtime + m_flTimeRemaining;

		m_OnResumed.FireOutput( pActivator ? pActivator : this, this );

		TimerThink();

		SetContextThink( &CGameTimer::TimerThink, gpGlobals->curtime + 0.05, TIMER_THINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add seconds to the timer while it is running or paused
//-----------------------------------------------------------------------------
void CGameTimer::AddTimerSeconds( float flTimeToAdd )
{
	if ( IsDisabled() )
		return;

	if ( m_flTimerMaxLength > 0 )
	{
		// will adding this many seconds push us over our max length?
		if ( GetTimeRemaining() + flTimeToAdd > m_flTimerMaxLength)
		{
			// adjust to only add up to our max length
			flTimeToAdd = m_flTimerMaxLength - GetTimeRemaining();
		}
	}

	if ( m_bTimerPaused )
	{
		m_flTimeRemaining += flTimeToAdd;
	}
	else
	{
		m_flTimerEndTime += flTimeToAdd;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Should we transmit it to the client?
//-----------------------------------------------------------------------------
int CGameTimer::UpdateTransmitState()
{
	if ( !m_bShowInHUD )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	if ( m_hPlayerFilter || GetTeamNumber() > TEAM_UNASSIGNED )
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Which clients should we be transmitting to?
//-----------------------------------------------------------------------------
int CGameTimer::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	int result = BaseClass::ShouldTransmit( pInfo );

	if ( result != FL_EDICT_DONTSEND )
	{
		CBaseEntity *pClient = (CBaseEntity *)(pInfo->m_pClientEnt->GetUnknown());
		if ( pClient )
		{
			if ( m_hPlayerFilter )
			{
				// Don't send to players who don't pass our filter
				if ( !m_hPlayerFilter->PassesFilter( this, pClient ) )
					return FL_EDICT_DONTSEND;
			}
			else if ( GetTeamNumber() > TEAM_UNASSIGNED )
			{
				// If we don't have an explicit filter, then just check if it's on the same team as us
				if ( pClient->GetTeamNumber() != GetTeamNumber() )
					return FL_EDICT_DONTSEND;
			}

			return FL_EDICT_ALWAYS;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputPause( inputdata_t &input )
{
	PauseTimer( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputResume( inputdata_t &input )
{
	ResumeTimer( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetTime( inputdata_t &input )
{
	float flTime = input.value.Float();
	SetTimeRemaining( flTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputAddTime( inputdata_t &input )
{
	float flTime = input.value.Float();
	AddTimerSeconds( flTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputRemoveTime( inputdata_t &input )
{
	float flTime = input.value.Float();
	AddTimerSeconds( -flTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetMaxTime( inputdata_t &input )
{
	float flTime = input.value.Float();
	m_flTimerMaxLength = flTime;

	if (m_flTimerMaxLength > 0)
	{
		// make sure our current time is not above the max length
		if (GetTimeRemaining() > m_flTimerMaxLength)
		{
			SetTimeRemaining( m_flTimerMaxLength );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetTimerCaption( inputdata_t &input )
{
	Q_strncpy( m_szTimerCaption.GetForModify(), input.value.String(), MAX_GAME_TIMER_CAPTION );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetProgressBarMaxSegments( inputdata_t &input )
{
	m_nProgressBarMaxSegments = input.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetProgressBarOverride( inputdata_t &input )
{
	m_nProgressBarOverride = input.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetX( inputdata_t &input )
{
	m_flOverrideX = input.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetY( inputdata_t &input )
{
	m_flOverrideY = input.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputRestart( inputdata_t &input )
{
	SetTimeRemaining( m_flTimerInitialLength );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputGetTimeRemaining( inputdata_t &input )
{
	m_OnGetTimeRemaining.Set( GetTimeRemaining(), input.pActivator, this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputEnable( inputdata_t &input )
{ 
	if (GetTimeRemaining() == 0.0f)
		SetTimeRemaining( m_flTimerInitialLength );

	m_bIsDisabled = false;
	if ( !m_bStartPaused )
		ResumeTimer( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputDisable( inputdata_t &input )
{ 
	PauseTimer( input.pActivator );
	m_bIsDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameTimer::InputSetPlayerFilter( inputdata_t &inputdata )
{
	m_iszPlayerFilterName = inputdata.value.StringID();
	if ( m_iszPlayerFilterName != NULL_STRING )
	{
		m_hPlayerFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( NULL, m_iszPlayerFilterName, this ));
	}
	else
	{
		m_hPlayerFilter = NULL;
	}
}


#endif
