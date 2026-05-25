//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose : Singleton manager for color correction on the client
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "tier0/vprof.h"
#include "colorcorrectionmgr.h"
#ifdef MAPBASE
#include "clientmode_shared.h" //"clientmode.h" // From Alien Swarm SDK

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"
#endif


//------------------------------------------------------------------------------
// Singleton access
//------------------------------------------------------------------------------
static CColorCorrectionMgr s_ColorCorrectionMgr;
CColorCorrectionMgr *g_pColorCorrectionMgr = &s_ColorCorrectionMgr;

#ifdef MAPBASE // From Alien Swarm SDK
static ConVar mat_colcorrection_editor( "mat_colcorrection_editor", "0" );

static CUtlVector<C_ColorCorrection *> g_ColorCorrectionList;
static CUtlVector<C_ColorCorrectionVolume *> g_ColorCorrectionVolumeList;

extern int g_nColCorrectExcludeMask;
#endif


//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
CColorCorrectionMgr::CColorCorrectionMgr()
{
	m_nActiveWeightCount = 0;
#ifdef MAPBASE
	m_nFirstFreeSlot = ColCorrectExcludeDefinition_t::END_OF_FREE_LIST;
#endif
}


//------------------------------------------------------------------------------
// Creates, destroys color corrections
//------------------------------------------------------------------------------
ClientCCHandle_t CColorCorrectionMgr::AddColorCorrection( const char *pName, const char *pFileName )
{
	if ( !pFileName )
	{
		pFileName = pName;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ColorCorrectionHandle_t ccHandle = pRenderContext->AddLookup( pName );
	if ( ccHandle )
	{
		pRenderContext->LockLookup( ccHandle );
		pRenderContext->LoadLookup( ccHandle, pFileName );
		pRenderContext->UnlockLookup( ccHandle );
	}
	else
	{
		Warning("Cannot find color correction lookup file: '%s'\n", pFileName );
	}

	return (ClientCCHandle_t)ccHandle;
}

void CColorCorrectionMgr::RemoveColorCorrection( ClientCCHandle_t h )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->RemoveLookup( ccHandle );
	}
}

#ifdef MAPBASE // From Alien Swarm SDK
ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionEntity( C_ColorCorrection *pEntity, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		Assert(g_ColorCorrectionList.Find(pEntity) == -1);
		g_ColorCorrectionList.AddToTail(pEntity);
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionEntity( C_ColorCorrection *pEntity, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	g_ColorCorrectionList.FindAndFastRemove(pEntity);
}

ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionVolume( C_ColorCorrectionVolume *pVolume, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		Assert(g_ColorCorrectionVolumeList.Find(pVolume) == -1);
		g_ColorCorrectionVolumeList.AddToTail(pVolume);
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionVolume( C_ColorCorrectionVolume *pVolume, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	g_ColorCorrectionVolumeList.FindAndFastRemove(pVolume);
}
#endif

//------------------------------------------------------------------------------
// Modify color correction weights
//------------------------------------------------------------------------------
#ifdef MAPBASE // From Alien Swarm SDK
ConVar mat_colcorrection_mask( "mat_colcorrection_mask", "1" );
ConVar mat_colcorrection_mask_always_use( "mat_colcorrection_mask_always_use", "0" );
ConVar mat_colcorrection_mask_always_invert( "mat_colcorrection_mask_always_invert", "0" );

void CColorCorrectionMgr::SetColorCorrectionWeight( ClientCCHandle_t h, float flWeight, bool bExclusive, bool bUseMask, bool bInvertMask )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		if ( mat_colcorrection_mask_always_use.GetBool() )
			bUseMask = true;
		else if ( !mat_colcorrection_mask.GetBool() )
			bUseMask = false;

		if ( mat_colcorrection_mask_always_invert.GetBool() )
			bInvertMask = true;

		SetWeightParams_t params = { h, flWeight, bExclusive, bUseMask, bInvertMask };
		m_colorCorrectionWeights.AddToTail( params );
		if ( bExclusive && flWeight > m_flExclusiveWeight )
		{
			m_bHaveExclusiveWeight = true;
			m_flExclusiveWeight = flWeight;
		}
	}
}

#ifdef MAPBASE
struct CCSortableParams_t
{
	ClientCCHandle_t handle;
	float flWeight;
};

int CCHandleSort( const CCSortableParams_t *a, const CCSortableParams_t *b )
{
	// Choose the higher weight
	if ( a->flWeight > b->flWeight )
		return -1;
	else if ( a->flWeight < b->flWeight )
		return 1;

	return 0;
}
#endif

void CColorCorrectionMgr::CommitColorCorrectionWeights()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

#ifdef MAPBASE
	// Need these to be in order so that they can translate to engine_post
	CUtlVector< CCSortableParams_t > vecActiveLookups;
#endif

	for ( int i = 0; i < m_colorCorrectionWeights.Count(); i++ )
	{
		ColorCorrectionHandle_t ccHandle = reinterpret_cast<ColorCorrectionHandle_t>( m_colorCorrectionWeights[i].handle );
		float flWeight = m_colorCorrectionWeights[i].flWeight;
		if ( !m_colorCorrectionWeights[i].bExclusive )
		{
			flWeight = (1.0f - m_flExclusiveWeight ) * m_colorCorrectionWeights[i].flWeight;
		}
		pRenderContext->SetLookupWeight( ccHandle, flWeight );

		// FIXME: NOTE! This doesn't work if the same handle has
		// its weight set twice with no intervening calls to ResetColorCorrectionWeights
		// which, at the moment, is true
		if ( flWeight != 0.0f )
		{
			++m_nActiveWeightCount;
#ifdef MAPBASE
			if ( m_colorCorrectionWeights[i].bUseMask )
				++m_nActiveMaskWeightCount;

			int idx = vecActiveLookups.AddToTail();
			vecActiveLookups[idx].flWeight = flWeight;
			vecActiveLookups[idx].handle = m_colorCorrectionWeights[i].handle;
#endif
		}
	}

#ifdef MAPBASE
	g_nColCorrectExcludeMask = 0;
	if ( m_bDrawingColCorrectExclude )
	{
		vecActiveLookups.Sort( CCHandleSort );

		for ( int i = 0; i < m_colorCorrectionWeights.Count(); i++ )
		{
			if ( m_colorCorrectionWeights[i].bUseMask )
			{
				// engine_post counts from 0
				int j = 0;
				for (; j < vecActiveLookups.Count(); j++)
				{
					if ( vecActiveLookups[j].handle == m_colorCorrectionWeights[i].handle )
						break;
				}

				if ( j < vecActiveLookups.Count() )
				{
					g_nColCorrectExcludeMask |= (1 << j);

					if ( m_colorCorrectionWeights[i].bInvertMask )
						g_nColCorrectExcludeMask |= (1 << (16 + j));
				}
			}
		}

		// Clean up any invalid definitions
		for ( int i = m_ColCorrectExcludeDefs.Count()-1; i >= 0; i-- )
		{
			if ( !m_ColCorrectExcludeDefs[i].m_hEntity || m_ColCorrectExcludeDefs[i].m_hEntity->IsMarkedForDeletion() )
			{
				UnregisterExclusionObject( i );
			}
		}
	}
#endif

	m_colorCorrectionWeights.RemoveAll();
}

void CColorCorrectionMgr::LevelShutdownPreEntity()
{
	//Clean up the vectors when shuting down a level
	//will keep dangling pointers inside of the vector causing a nullptr crash
	if (g_ColorCorrectionVolumeList.Base())
	{
		g_ColorCorrectionVolumeList.Purge();
	}

	if (g_ColorCorrectionList.Base())
	{
		g_ColorCorrectionList.Purge();
	}
}

#else
void CColorCorrectionMgr::SetColorCorrectionWeight( ClientCCHandle_t h, float flWeight )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->SetLookupWeight( ccHandle, flWeight );

		// FIXME: NOTE! This doesn't work if the same handle has
		// its weight set twice with no intervening calls to ResetColorCorrectionWeights
		// which, at the moment, is true
		if ( flWeight != 0.0f )
		{
			++m_nActiveWeightCount;
		}
	}
}
#endif

void CColorCorrectionMgr::ResetColorCorrectionWeights()
{
	VPROF_("ResetColorCorrectionWeights", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0);
	// FIXME: Where should I put this? It needs to happen prior to SimulateEntities()
	// which is where the client thinks for c_colorcorrection + c_colorcorrectionvolumes
	// update the color correction weights.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ResetLookupWeights();
	m_nActiveWeightCount = 0;
#ifdef MAPBASE // From Alien Swarm SDK
	m_bHaveExclusiveWeight = false;
	m_flExclusiveWeight = 0.0f;
	m_colorCorrectionWeights.RemoveAll();

	m_nActiveMaskWeightCount = 0;
#endif
}

void CColorCorrectionMgr::SetResetable( ClientCCHandle_t h, bool bResetable )
{
	// NOTE: Setting stuff to be not resettable doesn't work when in queued mode
	// because the logic that sets m_nActiveWeightCount to 0 in ResetColorCorrectionWeights
	// is no longer valid when stuff is not resettable.
	Assert( bResetable || !g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED );
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->SetResetable( ccHandle, bResetable );
	}
}


//------------------------------------------------------------------------------
// Is color correction active?
//------------------------------------------------------------------------------
#ifdef MAPBASE // From Alien Swarm SDK
bool CColorCorrectionMgr::HasNonZeroColorCorrectionWeights() const
{
	return ( m_nActiveWeightCount != 0 ) || mat_colcorrection_editor.GetBool();
}

void CColorCorrectionMgr::UpdateColorCorrection()
{
	ResetColorCorrectionWeights();
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	IClientMode *pClientMode = GetClientModeNormal(); //GetClientMode();

	Assert( pClientMode );
	if ( !pPlayer || !pClientMode )
	{
		return;
	}

	pClientMode->OnColorCorrectionWeightsReset();
	float ccScale = pClientMode->GetColorCorrectionScale();

	UpdateColorCorrectionEntities( pPlayer, ccScale, g_ColorCorrectionList.Base(), g_ColorCorrectionList.Count() );
	UpdateColorCorrectionVolumes( pPlayer, ccScale, g_ColorCorrectionVolumeList.Base(), g_ColorCorrectionVolumeList.Count() );
	CommitColorCorrectionWeights();
}
#else
bool CColorCorrectionMgr::HasNonZeroColorCorrectionWeights() const
{
	return ( m_nActiveWeightCount != 0 );
}
#endif
