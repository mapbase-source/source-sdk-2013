// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#pragma once
#include "shaderlib/cshader.h"
class windowimposter_vs20_Static_Index
{
	unsigned int m_nPARALLAXCORRECT : 2;
#ifdef _DEBUG
	bool m_bPARALLAXCORRECT : 1;
#endif	// _DEBUG
public:
	void SetPARALLAXCORRECT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPARALLAXCORRECT = i;
#ifdef _DEBUG
		m_bPARALLAXCORRECT = true;
#endif	// _DEBUG
	}

	windowimposter_vs20_Static_Index(  )
	{
		m_nPARALLAXCORRECT = 0;
#ifdef _DEBUG
		m_bPARALLAXCORRECT = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bPARALLAXCORRECT );
		return ( 2 * m_nPARALLAXCORRECT ) + 0;
	}
};

#define shaderStaticTest_windowimposter_vs20 vsh_forgot_to_set_static_PARALLAXCORRECT


class windowimposter_vs20_Dynamic_Index
{
	unsigned int m_nDOWATERFOG : 2;
#ifdef _DEBUG
	bool m_bDOWATERFOG : 1;
#endif	// _DEBUG
public:
	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif	// _DEBUG
	}

	windowimposter_vs20_Dynamic_Index(  )
	{
		m_nDOWATERFOG = 0;
#ifdef _DEBUG
		m_bDOWATERFOG = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bDOWATERFOG );
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};

#define shaderDynamicTest_windowimposter_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG

