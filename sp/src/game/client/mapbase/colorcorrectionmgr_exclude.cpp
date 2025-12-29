//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		Color correction mask rendering
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "tier0/vprof.h"
#include "colorcorrectionmgr.h"
#include "view_shared.h"
#include "viewpostprocess.h"
#include "model_types.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// Color Correction Mask
//------------------------------------------------------------------------------
int CColorCorrectionMgr::RegisterExclusionObject( C_BaseEntity *pEntity, Vector *vecColor, float flAlpha )
{
	int nIndex = -1;
	for ( int i = 0; i < m_ColCorrectExcludeDefs.Count(); i++ )
	{
		if ( m_ColCorrectExcludeDefs[i].m_hEntity == pEntity )
		{
			nIndex = i;
			break;
		}
	}

	if ( nIndex == -1 )
	{
		if (m_nFirstFreeSlot == ColCorrectExcludeDefinition_t::END_OF_FREE_LIST)
		{
			nIndex = m_ColCorrectExcludeDefs.AddToTail();
		}
		else
		{
			nIndex = m_nFirstFreeSlot;
			m_nFirstFreeSlot = m_ColCorrectExcludeDefs[nIndex].m_nNextFreeSlot;
		}
	}

	m_ColCorrectExcludeDefs[nIndex].m_hEntity = pEntity;
	m_ColCorrectExcludeDefs[nIndex].m_bStudio = (modelinfo->GetModelType( pEntity->GetModel() ) == mod_studio);
	m_ColCorrectExcludeDefs[nIndex].m_vecColor = vecColor ? *vecColor : Vector( 1.0f, 1.0f, 1.0f );
	m_ColCorrectExcludeDefs[nIndex].m_flAlpha = flAlpha;
	m_ColCorrectExcludeDefs[nIndex].m_nNextFreeSlot = ColCorrectExcludeDefinition_t::ENTRY_IN_USE;

	return nIndex;
}

void CColorCorrectionMgr::UnregisterExclusionObject( C_BaseEntity *pEntity )
{
	for ( int i = 0; i < m_ColCorrectExcludeDefs.Count(); i++ )
	{
		if ( m_ColCorrectExcludeDefs[i].m_hEntity == pEntity )
		{
			UnregisterExclusionObject( i );
			break;
		}
	}
}

void CColorCorrectionMgr::UnregisterExclusionObject( int nGlowObjectHandle )
{
	if ( m_ColCorrectExcludeDefs.Count() >= nGlowObjectHandle )
		return;

	Assert( !m_ColCorrectExcludeDefs[nGlowObjectHandle].IsUnused() );

	m_ColCorrectExcludeDefs[nGlowObjectHandle].m_nNextFreeSlot = m_nFirstFreeSlot;
	m_ColCorrectExcludeDefs[nGlowObjectHandle].m_hEntity = NULL;
	m_nFirstFreeSlot = nGlowObjectHandle;
}

// This was copied directly from glow_outline_effect.
// TODO: Generalized stencil functions?
struct CCShaderStencilState_t
{
	bool m_bEnable;
	StencilOperation_t m_FailOp;
	StencilOperation_t m_ZFailOp;
	StencilOperation_t m_PassOp;
	StencilComparisonFunction_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

	CCShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = STENCILOPERATION_KEEP;
		m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;
	}

	void SetStencilState( CMatRenderContextPtr &pRenderContext  )
	{
		pRenderContext->SetStencilEnable( m_bEnable );
		pRenderContext->SetStencilFailOperation( m_FailOp );
		pRenderContext->SetStencilZFailOperation( m_ZFailOp );
		pRenderContext->SetStencilPassOperation( m_PassOp );
		pRenderContext->SetStencilCompareFunction( m_CompareFunc );
		pRenderContext->SetStencilReferenceValue( m_nReferenceValue );
		pRenderContext->SetStencilTestMask( m_nTestMask );
		pRenderContext->SetStencilWriteMask( m_nWriteMask );
	}
};

extern bool g_bDumpRenderTargets;

void CColorCorrectionMgr::ColCorrectExcludeDefinition_t::DrawModel()
{
	if ( m_hEntity )
	{
		// UNDONE: Figure out a way to draw non-studio models in white
		if ( m_bStudio )
		{
			m_hEntity->DrawModel( STUDIO_RENDER );
		}
		else
		{
			m_hEntity->DrawModel( STUDIO_RENDER );
		}

		/*if ( m_pRenderable->GetIClientUnknown() && m_pRenderable->GetIClientUnknown()->GetBaseEntity() )
		{
			C_BaseEntity *pAttachment = m_pRenderable->GetIClientUnknown()->GetBaseEntity()->FirstMoveChild();

			while ( pAttachment != NULL )
			{
				if ( !g_GlowObjectManager.HasGlowEffect( pAttachment ) && pAttachment->ShouldDraw() && !pAttachment->IgnoresZBuffer() )
				{
					pAttachment->DrawModel( STUDIO_RENDER );
				}
				pAttachment = pAttachment->NextMovePeer();
			}
		}*/
	}
}

extern ConVar mat_colcorrection_mask;

void CColorCorrectionMgr::RenderExclusionObjects( const CViewSetup *pSetup )
{
	if ( m_nActiveMaskWeightCount > 0 && g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_0() )
	{
		if ( mat_colcorrection_mask.GetBool() )
		{
			CMatRenderContextPtr pRenderContext( materials );

			int nX, nY, nWidth, nHeight;
			pRenderContext->GetViewport( nX, nY, nWidth, nHeight );

			PIXEvent _pixEvent( pRenderContext, "ColCorrectExclusionObjects" );
			ApplyColCorrectExclusionObjects( pSetup, pRenderContext, nX, nY, nWidth, nHeight );
		}
	}
}

void CColorCorrectionMgr::RenderExclusionModels( ITexture *pRenderTarget, const CViewSetup *pSetup, CMatRenderContextPtr &pRenderContext )
{
	//==========================================================================================//
	// This renders solid pixels with the correct coloring for each object that needs the glow.	//
	// After this function returns, this image will then be blurred and added into the frame	//
	// buffer with the objects stenciled out.													//
	//==========================================================================================//
	ITexture *pRtFullFrame1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );
	pRenderContext->CopyRenderTargetToTexture( pRtFullFrame1 );

	// Save modulation color and blend
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );
	float flOrigBlend = render->GetBlend();

	//SetRenderTargetAndViewPort( pRenderTarget );

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	// Set override material for glow color
	IMaterial *pMatGlowColor = NULL;

	pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	CCShaderStencilState_t stencilState;
	stencilState.m_bEnable = false;
	stencilState.m_nReferenceValue = 0;
	stencilState.m_nTestMask = 0xFF;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_KEEP;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;

	stencilState.SetStencilState( pRenderContext );

	//==================//
	// Draw the objects //
	//==================//
	for ( int i = 0; i < m_ColCorrectExcludeDefs.Count(); ++ i )
	{
		if ( m_ColCorrectExcludeDefs[i].IsUnused() || !m_ColCorrectExcludeDefs[i].ShouldDraw() )
			continue;

		render->SetColorModulation( m_ColCorrectExcludeDefs[i].m_vecColor.Base() ); // This only sets rgb, not alpha
		render->SetBlend( m_ColCorrectExcludeDefs[i].m_flAlpha );

		pRenderContext->OverrideDepthEnable( true, false );

		m_ColCorrectExcludeDefs[i].DrawModel();
	}	

	if ( g_bDumpRenderTargets )
	{
		DumpTGAofRenderTarget( pSetup->width, pSetup->height, "ColCorrectMask" );
	}

	g_pStudioRender->ForcedMaterialOverride( NULL );
	render->SetColorModulation( vOrigColor.Base() );
	render->SetBlend( flOrigBlend );
	
	CCShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	stencilStateDisable.SetStencilState( pRenderContext );

	// Reset depth override
	pRenderContext->OverrideDepthEnable( false, false );

	// Copy what we just rendered to FullFrame
	pRenderContext->CopyRenderTargetToTexture( pRenderTarget );

	// Restore our backup
	pRenderContext->CopyTextureToRenderTargetEx( 0, pRtFullFrame1, NULL );
}

void CColorCorrectionMgr::ApplyColCorrectExclusionObjects( const CViewSetup *pSetup, CMatRenderContextPtr &pRenderContext, int x, int y, int w, int h )
{
	//=======================================================//
	// Render objects into stencil buffer					 //
	//=======================================================//
	// Set override shader to the same simple shader we use to render the glow models
	IMaterial *pMatGlowColor = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );

	CCShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	float flSavedBlend = render->GetBlend();

	// Set alpha to 0 so we don't touch any color pixels
	render->SetBlend( 0.0f );
	pRenderContext->OverrideDepthEnable( true, false );

	int iNumGlowObjects = 0;

	for ( int i = 0; i < m_ColCorrectExcludeDefs.Count(); ++ i )
	{
		if ( m_ColCorrectExcludeDefs[i].IsUnused() || !m_ColCorrectExcludeDefs[i].ShouldDraw() )
			continue;

		CCShaderStencilState_t stencilState;
		stencilState.m_bEnable = true;
		stencilState.m_nReferenceValue = 2;
		stencilState.m_nTestMask = 0x1;
		stencilState.m_nWriteMask = 0x3;
		stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
		stencilState.m_PassOp = STENCILOPERATION_INCRSAT;
		stencilState.m_FailOp = STENCILOPERATION_KEEP;
		stencilState.m_ZFailOp = STENCILOPERATION_KEEP;

		stencilState.SetStencilState( pRenderContext );

		m_ColCorrectExcludeDefs[i].DrawModel();

		iNumGlowObjects++;
	}

	pRenderContext->OverrideDepthEnable( false, false );
	render->SetBlend( flSavedBlend );
	stencilStateDisable.SetStencilState( pRenderContext );
	g_pStudioRender->ForcedMaterialOverride( NULL );

	// If there aren't any objects to glow, don't do all this other stuff
	// this fixes a bug where if there are glow objects in the list, but none of them are glowing,
	// the whole screen blooms.
	if ( iNumGlowObjects <= 0 )
	{
		m_bDrawingColCorrectExclude = false;
		return;
	}

	m_bDrawingColCorrectExclude = true;

	//=============================================
	// Render the glow colors to _rt_FullFrameFB 
	//=============================================
	{
		PIXEvent pixEvent( pRenderContext, "RenderExclusionModels" );
		ITexture *pRenderTarget = materials->FindTexture( "_rt_ColCorrectMask", TEXTURE_GROUP_RENDER_TARGET );
		RenderExclusionModels( pRenderTarget, pSetup, pRenderContext );
	}
}
