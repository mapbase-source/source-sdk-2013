// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#pragma once
#include "shaderlib/cshader.h"
class shatteredglass_vs20_Static_Index
{
	unsigned int m_nENVMAP_MASK : 2;
#ifdef _DEBUG
	bool m_bENVMAP_MASK : 1;
#endif	// _DEBUG
public:
	void SetENVMAP_MASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAP_MASK = i;
#ifdef _DEBUG
		m_bENVMAP_MASK = true;
#endif	// _DEBUG
	}

	shatteredglass_vs20_Static_Index(  )
	{
		m_nENVMAP_MASK = 0;
#ifdef _DEBUG
		m_bENVMAP_MASK = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bENVMAP_MASK );
		return ( 2 * m_nENVMAP_MASK ) + 0;
	}
};

#define shaderStaticTest_shatteredglass_vs20 vsh_forgot_to_set_static_ENVMAP_MASK


class shatteredglass_vs20_Dynamic_Index
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

	shatteredglass_vs20_Dynamic_Index(  )
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

#define shaderDynamicTest_shatteredglass_vs20 vsh_forgot_to_set_dynamic_DOWATERFOG

