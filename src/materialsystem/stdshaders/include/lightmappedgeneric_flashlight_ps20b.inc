// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// !$WORLDVERTEXTRANSITION && $NORMALMAP2
// !$NORMALMAP && $NORMALMAP2
// !$DETAILTEXTURE && ( $DETAIL_BLEND_MODE != 0 )
// !$PHONG && $PHONGMASK
// $BASETEXTURETRANSFORM2 && !$WORLDVERTEXTRANSITION
// $BASETEXTURETRANSFORM2 && $SEAMLESS
// $BASETEXTURETRANSFORM2 && $PHONG
// defined $HDRTYPE && defined $HDRENABLED && !$HDRTYPE && $HDRENABLED
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $HDRTYPE && $LIGHTING_PREVIEW && $HDRTYPE != 0
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
// defined $HDRTYPE && defined $HDRENABLED && !$HDRTYPE && $HDRENABLED
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $HDRTYPE && $LIGHTING_PREVIEW && $HDRTYPE != 0
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW

#pragma once
#include "shaderlib/cshader.h"
class lightmappedgeneric_flashlight_ps20b_Static_Index
{
	unsigned int m_nNORMALMAP : 2;
	unsigned int m_nNORMALMAP2 : 2;
	unsigned int m_nWORLDVERTEXTRANSITION : 2;
	unsigned int m_nFANCY_BLENDING : 2;
	unsigned int m_nSEAMLESS : 2;
	unsigned int m_nDETAILTEXTURE : 2;
	unsigned int m_nDETAIL_BLEND_MODE : 2;
	unsigned int m_nFLASHLIGHTDEPTHFILTERMODE : 2;
	unsigned int m_nPHONG : 2;
	unsigned int m_nPHONGMASK : 3;
	unsigned int m_nBASETEXTURETRANSFORM2 : 2;
#ifdef _DEBUG
	bool m_bNORMALMAP : 1;
	bool m_bNORMALMAP2 : 1;
	bool m_bWORLDVERTEXTRANSITION : 1;
	bool m_bFANCY_BLENDING : 1;
	bool m_bSEAMLESS : 1;
	bool m_bDETAILTEXTURE : 1;
	bool m_bDETAIL_BLEND_MODE : 1;
	bool m_bFLASHLIGHTDEPTHFILTERMODE : 1;
	bool m_bPHONG : 1;
	bool m_bPHONGMASK : 1;
	bool m_bBASETEXTURETRANSFORM2 : 1;
#endif	// _DEBUG
public:
	void SetNORMALMAP( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nNORMALMAP = i;
#ifdef _DEBUG
		m_bNORMALMAP = true;
#endif	// _DEBUG
	}

	void SetNORMALMAP2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAP2 = i;
#ifdef _DEBUG
		m_bNORMALMAP2 = true;
#endif	// _DEBUG
	}

	void SetWORLDVERTEXTRANSITION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDVERTEXTRANSITION = i;
#ifdef _DEBUG
		m_bWORLDVERTEXTRANSITION = true;
#endif	// _DEBUG
	}

	void SetFANCY_BLENDING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFANCY_BLENDING = i;
#ifdef _DEBUG
		m_bFANCY_BLENDING = true;
#endif	// _DEBUG
	}

	void SetSEAMLESS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS = i;
#ifdef _DEBUG
		m_bSEAMLESS = true;
#endif	// _DEBUG
	}

	void SetDETAILTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAILTEXTURE = i;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
#endif	// _DEBUG
	}

	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif	// _DEBUG
	}

	void SetPHONG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONG = i;
#ifdef _DEBUG
		m_bPHONG = true;
#endif	// _DEBUG
	}

	void SetPHONGMASK( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nPHONGMASK = i;
#ifdef _DEBUG
		m_bPHONGMASK = true;
#endif	// _DEBUG
	}

	void SetBASETEXTURETRANSFORM2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURETRANSFORM2 = i;
#ifdef _DEBUG
		m_bBASETEXTURETRANSFORM2 = true;
#endif	// _DEBUG
	}

	lightmappedgeneric_flashlight_ps20b_Static_Index(  )
	{
		m_nNORMALMAP = 0;
		m_nNORMALMAP2 = 0;
		m_nWORLDVERTEXTRANSITION = 0;
		m_nFANCY_BLENDING = 0;
		m_nSEAMLESS = 0;
		m_nDETAILTEXTURE = 0;
		m_nDETAIL_BLEND_MODE = 0;
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
		m_nPHONG = 0;
		m_nPHONGMASK = 0;
		m_nBASETEXTURETRANSFORM2 = 0;
#ifdef _DEBUG
		m_bNORMALMAP = false;
		m_bNORMALMAP2 = false;
		m_bWORLDVERTEXTRANSITION = false;
		m_bFANCY_BLENDING = false;
		m_bSEAMLESS = false;
		m_bDETAILTEXTURE = false;
		m_bDETAIL_BLEND_MODE = false;
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
		m_bPHONG = false;
		m_bPHONGMASK = false;
		m_bBASETEXTURETRANSFORM2 = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bNORMALMAP && m_bNORMALMAP2 && m_bWORLDVERTEXTRANSITION && m_bFANCY_BLENDING && m_bSEAMLESS && m_bDETAILTEXTURE && m_bDETAIL_BLEND_MODE && m_bFLASHLIGHTDEPTHFILTERMODE && m_bPHONG && m_bPHONGMASK && m_bBASETEXTURETRANSFORM2 );
		AssertMsg( !( !m_nWORLDVERTEXTRANSITION && m_nNORMALMAP2 ), "Invalid combo combination ( !WORLDVERTEXTRANSITION && NORMALMAP2 )" );
		AssertMsg( !( !m_nNORMALMAP && m_nNORMALMAP2 ), "Invalid combo combination ( !NORMALMAP && NORMALMAP2 )" );
		AssertMsg( !( !m_nDETAILTEXTURE && ( m_nDETAIL_BLEND_MODE != 0 ) ), "Invalid combo combination ( !DETAILTEXTURE && ( DETAIL_BLEND_MODE != 0 ) )" );
		AssertMsg( !( !m_nPHONG && m_nPHONGMASK ), "Invalid combo combination ( !PHONG && PHONGMASK )" );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && !m_nWORLDVERTEXTRANSITION ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && !WORLDVERTEXTRANSITION )" );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && m_nSEAMLESS ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && SEAMLESS )" );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && m_nPHONG ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && PHONG )" );
		return ( 4 * m_nNORMALMAP ) + ( 12 * m_nNORMALMAP2 ) + ( 24 * m_nWORLDVERTEXTRANSITION ) + ( 48 * m_nFANCY_BLENDING ) + ( 96 * m_nSEAMLESS ) + ( 192 * m_nDETAILTEXTURE ) + ( 384 * m_nDETAIL_BLEND_MODE ) + ( 768 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 2304 * m_nPHONG ) + ( 4608 * m_nPHONGMASK ) + ( 18432 * m_nBASETEXTURETRANSFORM2 ) + 0;
	}
};

#define shaderStaticTest_lightmappedgeneric_flashlight_ps20b psh_forgot_to_set_static_NORMALMAP + psh_forgot_to_set_static_NORMALMAP2 + psh_forgot_to_set_static_WORLDVERTEXTRANSITION + psh_forgot_to_set_static_FANCY_BLENDING + psh_forgot_to_set_static_SEAMLESS + psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_PHONG + psh_forgot_to_set_static_PHONGMASK + psh_forgot_to_set_static_BASETEXTURETRANSFORM2


class lightmappedgeneric_flashlight_ps20b_Dynamic_Index
{
	unsigned int m_nPIXELFOGTYPE : 2;
	unsigned int m_nFLASHLIGHTSHADOWS : 2;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE : 1;
	bool m_bFLASHLIGHTSHADOWS : 1;
#endif	// _DEBUG
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif	// _DEBUG
	}

	lightmappedgeneric_flashlight_ps20b_Dynamic_Index(  )
	{
		m_nPIXELFOGTYPE = 0;
		m_nFLASHLIGHTSHADOWS = 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = false;
		m_bFLASHLIGHTSHADOWS = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bPIXELFOGTYPE && m_bFLASHLIGHTSHADOWS );
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};

#define shaderDynamicTest_lightmappedgeneric_flashlight_ps20b psh_forgot_to_set_dynamic_PIXELFOGTYPE + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS

