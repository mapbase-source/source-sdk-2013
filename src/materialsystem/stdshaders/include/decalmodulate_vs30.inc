// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#pragma once
#include "shaderlib/cshader.h"
class decalmodulate_vs30_Static_Index
{
	unsigned int m_nVERTEXCOLOR : 2;
	unsigned int m_nLIGHTING_PREVIEW : 2;
	unsigned int m_nFLASHLIGHT : 2;
#ifdef _DEBUG
	bool m_bVERTEXCOLOR : 1;
	bool m_bLIGHTING_PREVIEW : 1;
	bool m_bFLASHLIGHT : 1;
#endif	// _DEBUG
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif	// _DEBUG
	}

	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif	// _DEBUG
	}

	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif	// _DEBUG
	}

	decalmodulate_vs30_Static_Index(  )
	{
		m_nVERTEXCOLOR = 0;
		m_nLIGHTING_PREVIEW = 0;
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = false;
		m_bLIGHTING_PREVIEW = false;
		m_bFLASHLIGHT = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bVERTEXCOLOR && m_bLIGHTING_PREVIEW && m_bFLASHLIGHT );
		return ( 16 * m_nVERTEXCOLOR ) + ( 32 * m_nLIGHTING_PREVIEW ) + ( 64 * m_nFLASHLIGHT ) + 0;
	}
};

#define shaderStaticTest_decalmodulate_vs30 vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_LIGHTING_PREVIEW + vsh_forgot_to_set_static_FLASHLIGHT


class decalmodulate_vs30_Dynamic_Index
{
	unsigned int m_nDOWATERFOG : 2;
	unsigned int m_nCOMPRESSED_VERTS : 2;
	unsigned int m_nSKINNING : 2;
	unsigned int m_nMORPHING : 2;
#ifdef _DEBUG
	bool m_bDOWATERFOG : 1;
	bool m_bCOMPRESSED_VERTS : 1;
	bool m_bSKINNING : 1;
	bool m_bMORPHING : 1;
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

	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = true;
#endif	// _DEBUG
	}

	void SetSKINNING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSKINNING = i;
#ifdef _DEBUG
		m_bSKINNING = true;
#endif	// _DEBUG
	}

	void SetMORPHING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMORPHING = i;
#ifdef _DEBUG
		m_bMORPHING = true;
#endif	// _DEBUG
	}

	decalmodulate_vs30_Dynamic_Index(  )
	{
		m_nDOWATERFOG = 0;
		m_nCOMPRESSED_VERTS = 0;
		m_nSKINNING = 0;
		m_nMORPHING = 0;
#ifdef _DEBUG
		m_bDOWATERFOG = false;
		m_bCOMPRESSED_VERTS = false;
		m_bSKINNING = false;
		m_bMORPHING = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bDOWATERFOG && m_bCOMPRESSED_VERTS && m_bSKINNING && m_bMORPHING );
		return ( 1 * m_nDOWATERFOG ) + ( 2 * m_nCOMPRESSED_VERTS ) + ( 4 * m_nSKINNING ) + ( 8 * m_nMORPHING ) + 0;
	}
};

#define shaderDynamicTest_decalmodulate_vs30 vsh_forgot_to_set_dynamic_DOWATERFOG + vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_MORPHING

