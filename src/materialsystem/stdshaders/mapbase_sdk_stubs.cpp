//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
// 
// Purpose: This file converts SDK_ shaders to the original names to adapt to the
//			new shader override capability
// 
//=============================================================================//

#include "BaseVSShader.h"

#define SHADER_SDK_LEGACY_STUB( _name, _fallback ) \
BEGIN_VS_SHADER_FLAGS( _name, "", SHADER_NOT_EDITABLE ) \
	BEGIN_SHADER_PARAMS \
	END_SHADER_PARAMS \
	SHADER_INIT {} \
	SHADER_FALLBACK { return _fallback; } \
	SHADER_DRAW {} \
END_SHADER \

//----------------------------------------------------------------------

SHADER_SDK_LEGACY_STUB( SDK_LightmappedGeneric,	"LightmappedGeneric" )
SHADER_SDK_LEGACY_STUB( SDK_VertexLitGeneric,	"VertexLitGeneric" )
SHADER_SDK_LEGACY_STUB( SDK_UnlitGeneric,		"UnlitGeneric" )

SHADER_SDK_LEGACY_STUB( SDK_WorldVertexTransition,	"WorldVertexTransition" )
SHADER_SDK_LEGACY_STUB( SDK_Water,					"Water" )
SHADER_SDK_LEGACY_STUB( SDK_Sprite,					"Sprite" )
SHADER_SDK_LEGACY_STUB( SDK_Refract,					"Refract" )
SHADER_SDK_LEGACY_STUB( SDK_LightmappedReflective,	"LightmappedReflective" )
SHADER_SDK_LEGACY_STUB( SDK_WorldTwoTextureBlend,	"WorldTwoTextureBlend" )
SHADER_SDK_LEGACY_STUB( SDK_EyeRefract,				"EyeRefract" )
SHADER_SDK_LEGACY_STUB( SDK_Eyes,					"Eyes" )
SHADER_SDK_LEGACY_STUB( SDK_EyeGlint,				"EyeGlint" )
SHADER_SDK_LEGACY_STUB( SDK_Teeth,					"Teeth" )
SHADER_SDK_LEGACY_STUB( SDK_Cable,					"Cable" )
SHADER_SDK_LEGACY_STUB( SDK_DepthWrite,				"DepthWrite" )
SHADER_SDK_LEGACY_STUB( SDK_DecalModulate,			"DecalModulate" )
SHADER_SDK_LEGACY_STUB( SDK_UnlitTwoTexture,			"UnlitTwoTexture" )
SHADER_SDK_LEGACY_STUB( SDK_MonitorScreen,			"MonitorScreen" )
SHADER_SDK_LEGACY_STUB( SDK_ShatteredGlass,			"ShatteredGlass" )
SHADER_SDK_LEGACY_STUB( SDK_WindowImposter,			"WindowImposter" )
SHADER_SDK_LEGACY_STUB( SDK_Engine_Post,			"Engine_Post" )
