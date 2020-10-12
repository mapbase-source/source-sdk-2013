#include "shaderlib/cshader.h"
class flow_water_vs30_Static_Index
{
private:
	int m_nBASETEXTURE;
#ifdef _DEBUG
	bool m_bBASETEXTURE;
#endif
public:
	void SetBASETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE = i;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
	void SetBASETEXTURE( bool i )
	{
		m_nBASETEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
private:
	int m_nMULTITEXTURE;
#ifdef _DEBUG
	bool m_bMULTITEXTURE;
#endif
public:
	void SetMULTITEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMULTITEXTURE = i;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
	void SetMULTITEXTURE( bool i )
	{
		m_nMULTITEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
private:
	int m_nFLASHLIGHT;
#ifdef _DEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nLIGHTMAPWATERFOG;
#ifdef _DEBUG
	bool m_bLIGHTMAPWATERFOG;
#endif
public:
	void SetLIGHTMAPWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTMAPWATERFOG = i;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = true;
#endif
	}
	void SetLIGHTMAPWATERFOG( bool i )
	{
		m_nLIGHTMAPWATERFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = true;
#endif
	}
private:
	int m_nFLOWMAP;
#ifdef _DEBUG
	bool m_bFLOWMAP;
#endif
public:
	void SetFLOWMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLOWMAP = i;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
	void SetFLOWMAP( bool i )
	{
		m_nFLOWMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
public:
	flow_water_vs30_Static_Index( )
	{
#ifdef _DEBUG
		m_bBASETEXTURE = false;
#endif // _DEBUG
		m_nBASETEXTURE = 0;
#ifdef _DEBUG
		m_bMULTITEXTURE = false;
#endif // _DEBUG
		m_nMULTITEXTURE = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = false;
#endif // _DEBUG
		m_nLIGHTMAPWATERFOG = 0;
#ifdef _DEBUG
		m_bFLOWMAP = false;
#endif // _DEBUG
		m_nFLOWMAP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBASETEXTURE && m_bMULTITEXTURE && m_bFLASHLIGHT && m_bLIGHTMAPWATERFOG && m_bFLOWMAP;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBASETEXTURE ) + ( 2 * m_nMULTITEXTURE ) + ( 4 * m_nFLASHLIGHT ) + ( 8 * m_nLIGHTMAPWATERFOG ) + ( 16 * m_nFLOWMAP ) + 0;
	}
};
#define shaderStaticTest_flow_water_vs30 vsh_forgot_to_set_static_BASETEXTURE + vsh_forgot_to_set_static_MULTITEXTURE + vsh_forgot_to_set_static_FLASHLIGHT + vsh_forgot_to_set_static_LIGHTMAPWATERFOG + vsh_forgot_to_set_static_FLOWMAP + 0
class flow_water_vs30_Dynamic_Index
{
public:
	flow_water_vs30_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_flow_water_vs30 0
