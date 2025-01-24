//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef C_MOVIE_DISPLAY_H
#define C_MOVIE_DISPLAY_H

#include "cbase.h"

class C_MovieDisplay : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_MovieDisplay, C_BaseEntity );
	DECLARE_CLIENTCLASS();
#ifdef MAPBASE
	DECLARE_DATADESC();
#endif

	C_MovieDisplay();
	~C_MovieDisplay();

#ifdef MAPBASE
	void OnDataChanged( DataUpdateType_t type );

	void UpdateVideoFrame( int nFrame ) { m_nVideoFrame = nFrame; }

	void OnSave();
	void OnRestore();
#endif

	bool IsEnabled( void ) const { return m_bEnabled; }
	bool IsLooping( void ) const { return m_bLooping; }
	bool IsMuted(void) const { return m_bMuted; }

#ifdef MAPBASE
	bool IsPaused(void) const { return m_bPaused; }

	float GetTargetTime(void) const { return m_flTargetTime; }
	bool HasNewTargetTime() const { return m_bNewTargetTime; }
	void ResetTargetTime() { m_bNewTargetTime = false; m_flOldTargetTime = -1.0f; }

	float GetTargetFrame( void ) const { return m_nTargetFrame; }
	bool HasNewTargetFrame() const { return m_bNewTargetFrame; }
	void ResetTargetFrame() { m_bNewTargetFrame = false; m_nTargetFrame = -1; }
#endif

	const char *GetMovieFilename( void ) const { return m_szMovieFilename; }
	const char *GetGroupName( void ) const { return m_szGroupName; }

private:
	bool	m_bEnabled;
	bool	m_bLooping;
	bool	m_bMuted;
	char	m_szMovieFilename[128];
	char	m_szGroupName[128];

#ifdef MAPBASE
	bool	m_bPaused = false;

	bool	m_bNewTargetTime = false;
	float	m_flTargetTime = 0.0f;
	float	m_flOldTargetTime = 0.0f;

	bool	m_bNewTargetFrame = false;
	int		m_nTargetFrame = 0;
	int		m_nOldTargetFrame = 0;

	// Updated by screen, restored to screen after save load
	int		m_nVideoFrame;
#endif
};

#endif //C_MOVIE_DISPLAY_H