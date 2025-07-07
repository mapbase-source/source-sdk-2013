//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Generic custom timer entity
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#ifdef CLIENT_DLL
#include "c_baseentity.h"
#else
#include "filters.h"
#endif

#ifdef CLIENT_DLL
DECLARE_AUTO_LIST( IGameTimerAutoList );

#define CGameTimer C_GameTimer
#endif

#define MAX_GAME_TIMER_CAPTION	32

//-----------------------------------------------------------------------------
// Purpose: Displays a custom timer
//-----------------------------------------------------------------------------
class CGameTimer : public CBaseEntity
#ifdef CLIENT_DLL
	, public IGameTimerAutoList
#endif
{
public:
	DECLARE_CLASS( CGameTimer, CBaseEntity );
	DECLARE_NETWORKCLASS();
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

	CGameTimer();
	~CGameTimer();

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	void Spawn();
#ifndef CLIENT_DLL
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
#endif

	// Based on teamplay_round_timer
	virtual float GetTimeRemaining( void );
	virtual float GetTimerMaxLength( void );
	virtual bool StartPaused( void ) { return m_bStartPaused; }
	bool ShowTimeRemaining( void ) { return m_bShowTimeRemaining; }
	float GetWarnTime( void );

	const char *GetTimerCaption( void ) { return m_szTimerCaption.Get(); }

	int GetProgressBarMaxSegments( void ) { return m_nProgressBarMaxSegments; }
	int GetProgressBarOverride( void ) { return m_nProgressBarOverride; }

	bool OverridesPosition( void ) { return m_flOverrideX != -1.0f || m_flOverrideY != -1.0f; }
	void GetPositionOverride( float &flX, float &flY ) { flX = m_flOverrideX; flY = m_flOverrideY; }

	bool IsDisabled( void ) { return m_bIsDisabled; }
	bool IsTimerPaused( void ) { return m_bTimerPaused; }

#ifndef CLIENT_DLL
	virtual void SetTimeRemaining( float flTime ); // Set the initial length of the timer
	virtual void AddTimerSeconds( float flTimeToAdd ); // Add time to an already running ( or paused ) timer
	virtual void OnTimerFinished();
	virtual void OnTimerWarned();
	virtual void PauseTimer( CBaseEntity *pActivator = NULL );
	virtual void ResumeTimer( CBaseEntity *pActivator = NULL );

	void SetProgressBarMaxSegments( int nSegments ) { m_nProgressBarMaxSegments = nSegments; }
	void SetProgressBarOverride( int nSegments ) { m_nProgressBarOverride = nSegments; }

	int UpdateTransmitState();

	// Inputs
	void InputEnable( inputdata_t &input );
	void InputDisable( inputdata_t &input );
	void InputPause( inputdata_t &input );
	void InputResume( inputdata_t &input );
	void InputSetTime( inputdata_t &input );
	void InputAddTime( inputdata_t &input );
	void InputRemoveTime( inputdata_t &input );
	void InputRestart( inputdata_t &input );
	void InputSetMaxTime( inputdata_t &input );
	void InputSetTimerCaption( inputdata_t &input );
	void InputSetProgressBarMaxSegments( inputdata_t &input );
	void InputSetProgressBarOverride( inputdata_t &input );
	void InputSetX( inputdata_t &input );
	void InputSetY( inputdata_t &input );
	void InputGetTimeRemaining( inputdata_t &input );
	void InputSetPlayerFilter( inputdata_t &inputdata );
#endif

private:

#ifdef CLIENT_DLL
	void	OnDataChanged( DataUpdateType_t updateType );
	void	UpdateOnRemove( void );
#else
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	void TimerThink( void );
#endif

private:

	CNetworkVar( bool, m_bIsDisabled );
	CNetworkVar( float, m_flTimerInitialLength );
	CNetworkVar( float, m_flTimerMaxLength );
	CNetworkVar( float, m_flTimerEndTime );
	CNetworkVar( float, m_flTimeRemaining );
	CNetworkVar( float, m_flWarnTime );				// Time at which timer turns red, starts ticking loudly, etc.
	CNetworkVar( int, m_nProgressBarMaxSegments );	// Overrides maximum segments in progress bar if greater than -1
	CNetworkVar( int, m_nProgressBarOverride );		// Overrides progress bar value if greater than -1
	CNetworkVar( float, m_flOverrideX );
	CNetworkVar( float, m_flOverrideY );
	CNetworkVar( bool, m_bTimerPaused );
	CNetworkVar( bool, m_bStartPaused );
	CNetworkVar( bool, m_bShowTimeRemaining );
	CNetworkString( m_szTimerCaption, MAX_GAME_TIMER_CAPTION );

#ifdef CLIENT_DLL
	bool	m_bOldDisabled;
#else
	bool	m_bStartedWarn;
	bool	m_bDisableOnFinish;
	bool	m_bShowInHUD;	// TODO: ShowInHUD input? Would require client to know it shouldn't show the timer anymore

	string_t				m_iszPlayerFilterName;
	CHandle<CBaseFilter> 	m_hPlayerFilter;

	// Outputs
	COutputEvent	m_OnFinished;
	COutputEvent	m_OnPaused;
	COutputEvent	m_OnResumed;
	COutputEvent	m_OnWarned;
	COutputInt		m_OnTick;
	COutputFloat	m_OnGetTimeRemaining;
#endif
};
