// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $BASETEXTURETRANSFORM2 && !$WORLDVERTEXTRANSITION
// $BASETEXTURETRANSFORM2 && $SEAMLESS
// $BASETEXTURETRANSFORM2 && $PHONG
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#pragma once
#include "shaderlib/cshader.h"
class lightmappedgeneric_flashlight_vs30_Static_Index
{
	unsigned int m_nNORMALMAP : 2;
	unsigned int m_nWORLDVERTEXTRANSITION : 2;
	unsigned int m_nSEAMLESS : 2;
	unsigned int m_nDETAIL : 2;
	unsigned int m_nPHONG : 2;
	unsigned int m_nBASETEXTURETRANSFORM2 : 2;
#ifdef _DEBUG
	bool m_bNORMALMAP : 1;
	bool m_bWORLDVERTEXTRANSITION : 1;
	bool m_bSEAMLESS : 1;
	bool m_bDETAIL : 1;
	bool m_bPHONG : 1;
	bool m_bBASETEXTURETRANSFORM2 : 1;
#endif	// _DEBUG
public:
	void SetNORMALMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAP = i;
#ifdef _DEBUG
		m_bNORMALMAP = true;
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

	void SetSEAMLESS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS = i;
#ifdef _DEBUG
		m_bSEAMLESS = true;
#endif	// _DEBUG
	}

	void SetDETAIL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL = i;
#ifdef _DEBUG
		m_bDETAIL = true;
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

	void SetBASETEXTURETRANSFORM2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURETRANSFORM2 = i;
#ifdef _DEBUG
		m_bBASETEXTURETRANSFORM2 = true;
#endif	// _DEBUG
	}

	lightmappedgeneric_flashlight_vs30_Static_Index(  )
	{
		m_nNORMALMAP = 0;
		m_nWORLDVERTEXTRANSITION = 0;
		m_nSEAMLESS = 0;
		m_nDETAIL = 0;
		m_nPHONG = 0;
		m_nBASETEXTURETRANSFORM2 = 0;
#ifdef _DEBUG
		m_bNORMALMAP = false;
		m_bWORLDVERTEXTRANSITION = false;
		m_bSEAMLESS = false;
		m_bDETAIL = false;
		m_bPHONG = false;
		m_bBASETEXTURETRANSFORM2 = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bNORMALMAP && m_bWORLDVERTEXTRANSITION && m_bSEAMLESS && m_bDETAIL && m_bPHONG && m_bBASETEXTURETRANSFORM2 );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && !m_nWORLDVERTEXTRANSITION ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && !WORLDVERTEXTRANSITION )" );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && m_nSEAMLESS ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && SEAMLESS )" );
		AssertMsg( !( m_nBASETEXTURETRANSFORM2 && m_nPHONG ), "Invalid combo combination ( BASETEXTURETRANSFORM2 && PHONG )" );
		return ( 2 * m_nNORMALMAP ) + ( 4 * m_nWORLDVERTEXTRANSITION ) + ( 8 * m_nSEAMLESS ) + ( 16 * m_nDETAIL ) + ( 32 * m_nPHONG ) + ( 64 * m_nBASETEXTURETRANSFORM2 ) + 0;
	}
};

#define shaderStaticTest_lightmappedgeneric_flashlight_vs30 vsh_forgot_to_set_static_NORMALMAP + vsh_forgot_to_set_static_WORLDVERTEXTRANSITION + vsh_forgot_to_set_static_SEAMLESS + vsh_forgot_to_set_static_DETAIL + vsh_forgot_to_set_static_PHONG + vsh_forgot_to_set_static_BASETEXTURETRANSFORM2


class lightmappedgeneric_flashlight_vs30_Dynamic_Index
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

	lightmappedgeneric_flashlight_vs30_Dynamic_Index(  )
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

#define shaderDynamicTest_lightmappedgeneric_flashlight_vs30 vsh_forgot_to_set_dynamic_DOWATERFOG

