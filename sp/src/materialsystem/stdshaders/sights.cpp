//===================== File of the LUX Shader Project =====================//
//
//	Initial D.	:	24.03.2025 DMY
//	Last Change :	07.04.2025 DMY
//
//	Purpose of this File :	LUX_SIGHTS Shader for Holosights
//
//==========================================================================//

#include "BaseVSShader.h"

// Includes for Shaderfiles...
#include "extra_sights_vs30.inc"
#include "extra_sights_ps30.inc"

//==========================================================================//
// Shader Start
//==========================================================================//
BEGIN_VS_SHADER(LUX_SIGHTS, "A Shader of the LUX Project")

BEGIN_SHADER_PARAMS
SHADER_PARAM(RECTICLESIZE, SHADER_PARAM_TYPE_FLOAT, "", "")
SHADER_PARAM(DETAIL, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "", "")
SHADER_PARAM(DETAILTEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "", "")
SHADER_PARAM(DETAILTINT, SHADER_PARAM_TYPE_COLOR, "", "")
SHADER_PARAM(DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "", "")
END_SHADER_PARAMS

SHADER_INIT_PARAMS()
{
	if (!params[RECTICLESIZE]->IsDefined())
		params[RECTICLESIZE]->SetFloatValue(1.0f);

	if (!params[DETAILSCALE]->IsDefined())
		params[DETAILSCALE]->SetFloatValue(4.0f);

	SET_FLAGS(MATERIAL_VAR_MODEL);
	SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);             // Required for skinning
	SET_FLAGS2(MATERIAL_VAR2_LIGHTING_VERTEX_LIT);              // Required for dynamic lighting
	SET_FLAGS2(MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS);   // Required for ambient cube
	SET_FLAGS2(MATERIAL_VAR2_NEEDS_TANGENT_SPACES);             // Required for dynamic lighting
	SET_FLAGS2(MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL);         // Required for dynamic lighting
}

SHADER_FALLBACK
{
	if (g_pHardwareConfig->GetDXSupportLevel() < 90)
	{
		Warning("Game run at DXLevel < 90 \n");
		return "Wireframe";
	}
	return 0;
}

SHADER_INIT
{
	LoadTexture(BASETEXTURE, TEXTUREFLAGS_SRGB);
	LoadTexture(DETAIL, 0);
}

SHADER_DRAW
{
	// We assume a Model Material with Compressed Vertices

	// Make sure we are not rendering a Projected Texture, Undesired!
	if (IsSnapshotting() && CShader_IsFlag2Set(params, MATERIAL_VAR2_USE_FLASHLIGHT))
	{
		Draw(false);
		return;
	}
	else if (s_pShaderAPI && s_pShaderAPI->InFlashlightMode())
	{
		Draw(false);
		return;
	}

// Textures
bool bHasBaseTexture = params[BASETEXTURE]->IsTexture();
bool bHasDetailTexture = params[DETAIL]->IsTexture();

// Handle Opaqueness. Spoiler its transparent
bool bIsOpaque = false;

//==========================================================================//
// Snapshotting State
// This gets setup once, after that, using Dynamic State.
//==========================================================================//
if (IsSnapshotting())
{
	//==========================================================================//
	// General Rendering Setup
	//==========================================================================//

	// This handles : $IgnoreZ, $Decal, $Nocull, $Znearer, $Wireframe, $AllowAlphaToCoverage
	// Source : OrangeBox/ASW Code which have BaseShader.cpp
	SetInitialShadowState();

	// Has to be called before SetDefaultAlphaBlending
	// This handles : $Additive, $Translucent, and sets $Additive for when the Flashlight is on
//		SetDefaultBlendingShadowState(BASETEXTURE, true);
//		SetDefaultAlphaBlending(BASETEXTURE);
		EnableAlphaBlending(SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA);

		// Not required for this Shader
		// This is now handled in the flashlight sampler setup
//		DefaultFog(); 

		// We always need this
		pShaderShadow->EnableAlphaWrites(bIsOpaque);

		// Weird name, what it actually means : We output linear values
		pShaderShadow->EnableSRGBWrite(true);

		// Alphatest Support
//		pShaderShadow->EnableAlphaTest(bAlphatest);
//		if (GetFloat(ALPHATESTREFERENCE) > 0.0f) // 0 is default.
//		{
//			pShaderShadow->AlphaFunc(SHADER_ALPHAFUNC_GEQUAL, GetFloat(ALPHATESTREFERENCE));
//		}

		//==========================================================================//
		// VertexFormat
		//==========================================================================//
		// Don't need VERTEX_TANGENTS, Models get them either way in the Compressed Stream
		unsigned int nFlags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;

		// Only have one of them
		int nTexCoords = 1;

		//		if (g_pHardwareConfig->HasFastVertexTextures() && IS_FLAG_SET(MATERIAL_VAR_DECAL))
		//			nTexCoords = 3;

		//		int pTexCoordDim[3] = { 2, 0, 3 };

				pShaderShadow->VertexShaderVertexFormat(nFlags, nTexCoords, 0, 0); // UserData probably only required for Uncompressed Verts..

				//==========================================================================//
				// Enable Samplers
				//==========================================================================//
				 // We always have a basetexture, and yes they should always be sRGB
				pShaderShadow->EnableTexture(SHADER_SAMPLER0, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);

				// No sRGB
				if (bHasDetailTexture)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER1, true);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER1, false);
				}

				//==========================================================================//
				// Declare Static Shaders
				//==========================================================================//
				DECLARE_STATIC_VERTEX_SHADER(extra_sights_vs30);
				SET_STATIC_VERTEX_SHADER_COMBO(DETAILUV, bHasDetailTexture);
				SET_STATIC_VERTEX_SHADER(extra_sights_vs30);

				DECLARE_STATIC_PIXEL_SHADER(extra_sights_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(DETAILTEXTURE, bHasDetailTexture);
				SET_STATIC_PIXEL_SHADER(extra_sights_ps30);
			}
			else // End of Snapshotting ------------------------------------------------------------------------------------------------------------------------------------------------------------------
			{
	//==========================================================================//
	// Bind Textures
	//==========================================================================//
	if (bHasBaseTexture)
	{
		BindTexture(SHADER_SAMPLER0, BASETEXTURE, FRAME);
	}
	else
	{
		pShaderAPI->BindStandardTexture(SHADER_SAMPLER0, TEXTURE_WHITE);
	}

	if (bHasDetailTexture)
	{
		BindTexture(SHADER_SAMPLER1, DETAIL, DETAILFRAME);
	}

	//==========================================================================//
	// Setup Constant Registers
	//==========================================================================//

	// Prepare boolean array, yes we need to use BOOL
//		BOOL BBools[16] = { false };

		// Always having this
		SetVertexShaderTextureTransform(223, BASETEXTURETRANSFORM);

		if (bHasDetailTexture)
			SetVertexShaderTextureScaledTransform(225, DETAILTEXTURETRANSFORM, DETAILSCALE);

		// Doing this the old-school way, for NBC ( Mapbase and Stock SDK Support )
		float f4EyePos[4];
		pShaderAPI->GetWorldSpaceCameraPosition(f4EyePos);
		f4EyePos[3] = 1.0f / params[RECTICLESIZE]->GetFloatValue();

		// c0
		pShaderAPI->SetPixelShaderConstant(1, f4EyePos);

			float f4DetailTint[4];
		params[DETAILTINT]->GetVecValue(f4DetailTint, 3);
		pShaderAPI->SetPixelShaderConstant(2, f4DetailTint);

		float f4ColorTint[4];
		ComputeModulationColor(f4ColorTint);
		pShaderAPI->SetPixelShaderConstant(3, f4ColorTint);

		//		bool bRadialFog = false;
		//		lux_radialfog.GetBool()

				DECLARE_DYNAMIC_VERTEX_SHADER(extra_sights_vs30);
				SET_DYNAMIC_VERTEX_SHADER_COMBO(SKINNING, pShaderAPI->GetCurrentNumBones() > 0 ? true : false);
				SET_DYNAMIC_VERTEX_SHADER(extra_sights_vs30);

				DECLARE_DYNAMIC_PIXEL_SHADER(extra_sights_vs30);
				SET_DYNAMIC_PIXEL_SHADER(extra_sights_vs30);
			}

// ShiroDkxtro2:	I had it happen a bunch of times...
//		Draw(); MUST BE above the final }
//		It is very easy to mess this up ( done so several times )
//		Game will just crash and debug leads you to a bogus function.
Draw();
}
END_SHADER