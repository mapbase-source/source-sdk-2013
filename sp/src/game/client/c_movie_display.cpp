//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//
//=====================================================================================//
#include "cbase.h"
#include "c_movie_display.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_MovieDisplay, DT_MovieDisplay, CMovieDisplay )
	RecvPropBool( RECVINFO( m_bEnabled ) ),
	RecvPropBool( RECVINFO( m_bLooping ) ),
	RecvPropBool( RECVINFO( m_bMuted ) ),
	RecvPropString( RECVINFO( m_szMovieFilename ) ),
	RecvPropString( RECVINFO( m_szGroupName ) ),
#ifdef MAPBASE
	RecvPropBool( RECVINFO( m_bPaused ) ),
	RecvPropFloat( RECVINFO( m_flTargetTime ) ),
	RecvPropInt( RECVINFO( m_nTargetFrame ) ),
#endif
END_RECV_TABLE()

#ifdef MAPBASE
BEGIN_DATADESC( C_MovieDisplay )

	DEFINE_FIELD( m_nVideoFrame, FIELD_INTEGER ),

END_DATADESC()
#endif

C_MovieDisplay::C_MovieDisplay()
{
}

C_MovieDisplay::~C_MovieDisplay()
{
}

#ifdef MAPBASE
void C_MovieDisplay::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	OnRestore();

	if (type == DATA_UPDATE_DATATABLE_CHANGED)
	{
		if (m_flTargetTime != m_flOldTargetTime)
		{
			m_bNewTargetTime = true;
			m_flOldTargetTime = m_flTargetTime;
		}
		if (m_nTargetFrame != m_nOldTargetFrame)
		{
			m_bNewTargetFrame = true;
			m_nOldTargetFrame = m_nTargetFrame;
		}
	}
}

//-----------------------------------------------------------------------------
// handler to do stuff before you are saved
//-----------------------------------------------------------------------------
void C_MovieDisplay::OnSave()
{
	BaseClass::OnSave();
}

//-----------------------------------------------------------------------------
// handler to do stuff after you are restored
//-----------------------------------------------------------------------------
void C_MovieDisplay::OnRestore()
{
	BaseClass::OnRestore();

	m_bNewTargetFrame = true;
	m_nOldTargetFrame = m_nTargetFrame = m_nVideoFrame;
}
#endif
