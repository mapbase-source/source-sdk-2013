// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $PARALLAXCORRECT && !$CUBEMAP
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
class shatteredglass_ps20b_Static_Index
{
	unsigned int m_nCUBEMAP : 2;
	unsigned int m_nVERTEXCOLOR : 2;
	unsigned int m_nENVMAPMASK : 2;
	unsigned int m_nBASEALPHAENVMAPMASK : 2;
	unsigned int m_nHDRTYPE : 2;
	unsigned int m_nPARALLAXCORRECT : 2;
#ifdef _DEBUG
	bool m_bCUBEMAP : 1;
	bool m_bVERTEXCOLOR : 1;
	bool m_bENVMAPMASK : 1;
	bool m_bBASEALPHAENVMAPMASK : 1;
	bool m_bHDRTYPE : 1;
	bool m_bPARALLAXCORRECT : 1;
#endif	// _DEBUG
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif	// _DEBUG
	}

	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif	// _DEBUG
	}

	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif	// _DEBUG
	}

	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif	// _DEBUG
	}

	void SetHDRTYPE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nHDRTYPE = i;
#ifdef _DEBUG
		m_bHDRTYPE = true;
#endif	// _DEBUG
	}

	void SetPARALLAXCORRECT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPARALLAXCORRECT = i;
#ifdef _DEBUG
		m_bPARALLAXCORRECT = true;
#endif	// _DEBUG
	}

	shatteredglass_ps20b_Static_Index(  )
	{
		m_nCUBEMAP = 0;
		m_nVERTEXCOLOR = 0;
		m_nENVMAPMASK = 0;
		m_nBASEALPHAENVMAPMASK = 0;
		m_nHDRTYPE = 0;
		m_nPARALLAXCORRECT = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
		m_bVERTEXCOLOR = false;
		m_bENVMAPMASK = false;
		m_bBASEALPHAENVMAPMASK = false;
		m_bHDRTYPE = false;
		m_bPARALLAXCORRECT = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bCUBEMAP && m_bVERTEXCOLOR && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bHDRTYPE && m_bPARALLAXCORRECT );
		AssertMsg( !( m_nPARALLAXCORRECT && !m_nCUBEMAP ), "Invalid combo combination ( PARALLAXCORRECT && !CUBEMAP )" );
		return ( 2 * m_nCUBEMAP ) + ( 4 * m_nVERTEXCOLOR ) + ( 8 * m_nENVMAPMASK ) + ( 16 * m_nBASEALPHAENVMAPMASK ) + ( 32 * m_nHDRTYPE ) + ( 96 * m_nPARALLAXCORRECT ) + 0;
	}
};

#define shaderStaticTest_shatteredglass_ps20b psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_HDRTYPE + psh_forgot_to_set_static_PARALLAXCORRECT


class shatteredglass_ps20b_Dynamic_Index
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

	shatteredglass_ps20b_Dynamic_Index(  )
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

#define shaderDynamicTest_shatteredglass_ps20b psh_forgot_to_set_dynamic_PIXELFOGTYPE

