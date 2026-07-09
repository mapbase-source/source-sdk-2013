// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $REFRACT || $CORECOLORTEXTURE ) && $CUBEMAP
// defined $HDRTYPE && defined $HDRENABLED && !$HDRTYPE && $HDRENABLED
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $HDRTYPE && $LIGHTING_PREVIEW && $HDRTYPE != 0
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW

#pragma once
#include "shaderlib/cshader.h"
class core_ps20b_Static_Index
{
	unsigned int m_nCONVERT_TO_SRGB : 1;
	unsigned int m_nCUBEMAP : 2;
	unsigned int m_nFLOWMAP : 2;
	unsigned int m_nCORECOLORTEXTURE : 2;
	unsigned int m_nREFRACT : 2;
#ifdef _DEBUG
	bool m_bCUBEMAP : 1;
	bool m_bFLOWMAP : 1;
	bool m_bCORECOLORTEXTURE : 1;
	bool m_bREFRACT : 1;
#endif	// _DEBUG
public:
	void SetCONVERT_TO_SRGB( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nCONVERT_TO_SRGB = i;
	}

	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif	// _DEBUG
	}

	void SetFLOWMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLOWMAP = i;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif	// _DEBUG
	}

	void SetCORECOLORTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCORECOLORTEXTURE = i;
#ifdef _DEBUG
		m_bCORECOLORTEXTURE = true;
#endif	// _DEBUG
	}

	void SetREFRACT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACT = i;
#ifdef _DEBUG
		m_bREFRACT = true;
#endif	// _DEBUG
	}

	core_ps20b_Static_Index(  )
	{
		m_nCONVERT_TO_SRGB = g_pHardwareConfig->NeedsShaderSRGBConversion();
		m_nCUBEMAP = 0;
		m_nFLOWMAP = 0;
		m_nCORECOLORTEXTURE = 0;
		m_nREFRACT = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
		m_bFLOWMAP = false;
		m_bCORECOLORTEXTURE = false;
		m_bREFRACT = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bCUBEMAP && m_bFLOWMAP && m_bCORECOLORTEXTURE && m_bREFRACT );
		AssertMsg( !( ( m_nREFRACT || m_nCORECOLORTEXTURE ) && m_nCUBEMAP ), "Invalid combo combination ( ( REFRACT || CORECOLORTEXTURE ) && CUBEMAP )" );
		return ( 2 * m_nCONVERT_TO_SRGB ) + ( 2 * m_nCUBEMAP ) + ( 4 * m_nFLOWMAP ) + ( 8 * m_nCORECOLORTEXTURE ) + ( 16 * m_nREFRACT ) + 0;
	}
};

#define shaderStaticTest_core_ps20b psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_FLOWMAP + psh_forgot_to_set_static_CORECOLORTEXTURE + psh_forgot_to_set_static_REFRACT


class core_ps20b_Dynamic_Index
{
	unsigned int m_nPIXELFOGTYPE : 2;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE : 1;
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

	core_ps20b_Dynamic_Index(  )
	{
		m_nPIXELFOGTYPE = 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bPIXELFOGTYPE );
		return ( 1 * m_nPIXELFOGTYPE ) + 0;
	}
};

#define shaderDynamicTest_core_ps20b psh_forgot_to_set_dynamic_PIXELFOGTYPE

