//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_JSON_H
#define SQDBG_JSON_H

#include <new>
#include <squirrel.h>
#include "vec.h"
#include "str.h"

typedef enum
{
	JSON_NULL			= 0x0000,
	JSON_INTEGER		= 0x0001,
	JSON_FLOAT			= 0x0002,
	JSON_STRING			= 0x0004,
#ifdef SQUNICODE
	JSON_WSTRING		= 0x0008,
#endif
	JSON_TABLE			= 0x0010,
	JSON_ARRAY			= 0x0020,
	JSON_BOOL			= 0x0040,
	_JSON_ALLOCATED		= 0x0080,
	_JSON_QUOTED		= 0x0100
} JSONTYPE;

class json_table_t;
class json_array_t;
struct json_value_t;

static inline json_table_t *CreateTable( int reserve );
static inline json_array_t *CreateArray( int reserve );
static inline void DeleteTable( json_table_t *p );
static inline void DeleteArray( json_array_t *p );
static inline void FreeValue( json_value_t *val );

struct json_value_t
{
	json_value_t() {}

	union
	{
		string_t _string;
#ifdef SQUNICODE
		sqstring_t _wstring;
#endif
		int _integer;
		SQFloat _float;
		json_table_t *_table;
		json_array_t *_array;
	};

	int type;

	int IsType( int t ) const
	{
		return type & t;
	}

	int GetType() const
	{
		return type & ~(_JSON_ALLOCATED | _JSON_QUOTED);
	}

	string_t &AsString()
	{
		Assert( type & JSON_STRING );
		return _string;
	}

	json_table_t &AsTable()
	{
		Assert( type & JSON_TABLE );
		return *_table;
	}
};

struct json_field_t
{
	string_t key;
	json_value_t val;
};

class json_array_t
{
private:
	typedef vector< json_value_t > array_t;
	array_t value;

	void clear()
	{
		for ( int i = value.size(); i--; )
		{
			json_value_t &elem = value._base[i];

			if ( elem.IsType( _JSON_ALLOCATED ) )
			{
				FreeValue( &elem );
			}

			value.pop();
		}
	}

public:
	json_array_t() {}

	json_array_t( int reserve )
	{
		if ( reserve )
			value.reserve( reserve );
	}

	~json_array_t()
	{
		clear();
	}

	int size() const
	{
		return value.size();
	}

	int capacity() const
	{
		return value.capacity();
	}

	void remove( int i )
	{
		value.remove(i);
	}

	void reserve( int i )
	{
		if ( i > value.capacity() )
			value.reserve(i);
	}

	json_value_t *Get( int i )
	{
		Assert( i >= 0 && i < value.size() );
		return &value._base[i];
	}

	json_value_t *NewElement()
	{
		json_value_t &ret = value.append();
		memzero( &ret );
		return &ret;
	}

	json_table_t &AppendTable( int reserve = 0 )
	{
		json_value_t &elem = value.append();

		elem.type = JSON_TABLE | _JSON_ALLOCATED;
		elem._table = CreateTable( reserve );

		return *elem._table;
	}

	void Append( json_table_t &t )
	{
		json_value_t &elem = value.append();
		elem.type = JSON_TABLE;
		elem._table = &t;
	}

	void Append( int val )
	{
		json_value_t &elem = value.append();
		elem.type = JSON_INTEGER;
		elem._integer = val;
	}

	void Append( const string_t &val )
	{
		json_value_t &elem = value.append();
		elem.type = JSON_STRING;
		elem._string = val;
	}

#ifdef SQUNICODE
	void Append( const sqstring_t &val )
	{
		json_value_t &elem = value.append();
		elem.type = JSON_WSTRING;
		elem._wstring = val;
	}
#endif
};

class json_table_t
{
private:
	typedef vector< json_field_t > table_t;
	table_t value;

	void clear()
	{
		for ( int i = value.size(); i--; )
		{
			json_value_t &val = value._base[i].val;

			if ( val.IsType( _JSON_ALLOCATED ) )
			{
				FreeValue( &val );
			}

			value.pop();
		}
	}

public:
	json_table_t() {}

	json_table_t( int reserve )
	{
		if ( reserve )
			value.reserve( reserve );
	}

	~json_table_t()
	{
		clear();
	}

	int size() const
	{
		return value.size();
	}

	int capacity() const
	{
		return value.capacity();
	}

	void reserve( int i )
	{
		if ( i > value.capacity() )
			value.reserve(i);
	}

	json_field_t *Get( int i )
	{
		Assert( i >= 0 && i < value.size() );
		return &value._base[i];
	}

	json_value_t *Get( const string_t &key )
	{
		for ( int i = 0; i < value.size(); i++ )
		{
			json_field_t *kv = &value._base[i];
			if ( kv->key.IsEqualTo( key ) )
				return &kv->val;
		}

		return NULL;
	}

	json_value_t *Get( const string_t &key ) const
	{
		return const_cast< json_table_t * >( this )->Get( key );
	}

	json_value_t *GetOrCreate( const string_t &key )
	{
		json_value_t *val = Get( key );

		if ( !val )
		{
			json_field_t *kv = NewElement();
			kv->key = key;
			val = &kv->val;
		}
		else if ( val->IsType( _JSON_ALLOCATED ) )
		{
			FreeValue( val );
		}

		return val;
	}

	json_field_t *NewElement()
	{
		json_field_t &ret = value.append();
		memzero( &ret );
		return &ret;
	}

	bool GetBool( const string_t &key, bool *out ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_BOOL ) )
		{
			*out = kval->_integer;
			return true;
		}

		*out = false;
		return false;
	}

	bool GetInt( const string_t &key, int *out, int defaultVal = 0 ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_INTEGER ) )
		{
			*out = kval->_integer;
			return true;
		}

		*out = defaultVal;
		return false;
	}

	bool GetFloat( const string_t &key, float *out, float defaultVal = 0.0f ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_FLOAT ) )
		{
			*out = kval->_float;
			return true;
		}

		*out = defaultVal;
		return false;
	}

	bool GetString( const string_t &key, string_t *out, const char *defaultVal = "" ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_STRING ) )
		{
			*out = kval->_string;
			return true;
		}

		out->Assign( defaultVal, strlen(defaultVal) );
		return false;
	}

	bool GetTable( const string_t &key, json_table_t **out ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_TABLE ) )
		{
			*out = kval->_table;
			return true;
		}

		*out = NULL;
		return false;
	}

	bool GetArray( const string_t &key, json_array_t **out ) const
	{
		json_value_t *kval = Get( key );

		if ( kval && ( kval->type & JSON_ARRAY ) )
		{
			*out = kval->_array;
			return true;
		}

		*out = NULL;
		return false;
	}

	void SetNull( const string_t &key )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_NULL;
	}

	void SetBool( const string_t &key, bool val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_BOOL;
		kval->_integer = val;
	}

	template < typename T >
	void SetInt( const string_t &key, T *val )
	{
		SetInt( key, (int)val );
	}

	void SetInt( const string_t &key, int val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_INTEGER;
		kval->_integer = val;
	}

	void SetFloat( const string_t &key, float val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_FLOAT;
		kval->_float = val;
	}

	void SetIntString( const string_t &key, int val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_INTEGER | _JSON_QUOTED;
		kval->_integer = val;
	}

	template < int size >
	void SetString( const string_t &key, const char (&val)[size] )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_STRING;
		kval->_string.Assign( val, size - 1 );
	}

	void SetString( const string_t &key, SQString *val, bool quote = false )
	{
		SetStringNoCopy( key, { val->_val, (int)val->_len }, quote );
	}

	void SetString( const string_t &key, const conststring_t &val )
	{
		SetStringNoCopy( key, val );
	}

	void SetString( const string_t &key, const string_t &val )
	{
		Assert( val.ptr );

		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_STRING | _JSON_ALLOCATED;
		CopyString( val, &kval->_string );
	}

	void SetStringNoCopy( const string_t &key, const string_t &val, bool quote = false )
	{
		Assert( val.ptr );

		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_STRING;
		kval->_string = val;

		if ( quote )
			kval->type |= _JSON_QUOTED;
	}

	// Calls FreeString on release,
	// assumes len + 1 bytes allocated
	void SetStringExternal( const string_t &key, const string_t &val )
	{
		Assert( val.ptr );

		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_STRING | _JSON_ALLOCATED;
		kval->_string = val;
	}

#ifdef SQUNICODE
	void SetStringNoCopy( const string_t &key, const sqstring_t &val, bool quote = false )
	{
		Assert( val.ptr );

		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_WSTRING;
		kval->_wstring = val;

		if ( quote )
			kval->type |= _JSON_QUOTED;
	}

	void SetString( const string_t &key, const sqstring_t &val )
	{
		Assert( val.ptr );

		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_STRING | _JSON_ALLOCATED;
		CopyString( val, &kval->_string );
	}
#endif

	json_table_t &SetTable( const string_t &key, int reserve = 0 )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_TABLE | _JSON_ALLOCATED;
		kval->_table = CreateTable( reserve );

		return *kval->_table;
	}

	json_array_t &SetArray( const string_t &key, int reserve = 0 )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_ARRAY | _JSON_ALLOCATED;
		kval->_array = CreateArray( reserve );

		return *kval->_array;
	}

	void SetTable( const string_t &key, json_table_t &val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_TABLE;
		kval->_table = &val;
	}

	void SetArray( const string_t &key, json_array_t &val )
	{
		json_value_t *kval = GetOrCreate( key );

		kval->type = JSON_ARRAY;
		kval->_array = &val;
	}

	void Set( const string_t &key, bool val ) { SetBool( key, val ); }
	void Set( const string_t &key, int val ) { SetInt( key, val ); }
	void Set( const string_t &key, unsigned int val ) { SetInt( key, val ); }
	void Set( const string_t &key, float val ) { SetFloat( key, val ); }
	void Set( const string_t &key, const string_t &val ) { SetString( key, val ); }

	bool Get( const string_t &key, bool *out ) const { return GetBool( key, out ); }
	bool Get( const string_t &key, int *out, int defaultVal = 0 ) const { return GetInt( key, out, defaultVal ); }
	bool Get( const string_t &key, float *out, float defaultVal = 0.0f ) const { return GetFloat( key, out, defaultVal ); }
	bool Get( const string_t &key, string_t *out, const char *defaultVal = "" ) const { return GetString( key, out, defaultVal ); }
	bool Get( const string_t &key, json_table_t **out ) const { return GetTable( key, out ); }
	bool Get( const string_t &key, json_array_t **out ) const { return GetArray( key, out ); }

private:
	template < typename T > void Set( const string_t &, T* );
	template < typename T > void Set( const string_t &, T );
	template < typename T > void SetBool( const string_t &, T );
};

inline json_table_t *CreateTable( int reserve )
{
	json_table_t *ret = (json_table_t *)sqdbg_malloc( sizeof(json_table_t) );
	new (ret) json_table_t( reserve );
	return ret;
}

inline json_array_t *CreateArray( int reserve )
{
	json_array_t *ret = (json_array_t *)sqdbg_malloc( sizeof(json_array_t) );
	new (ret) json_array_t( reserve );
	return ret;
}

inline void DeleteTable( json_table_t *p )
{
	p->~json_table_t();
	sqdbg_free( p, sizeof(json_table_t) );
}

inline void DeleteArray( json_array_t *p )
{
	p->~json_array_t();
	sqdbg_free( p, sizeof(json_array_t) );
}

inline void FreeValue( json_value_t *val )
{
	Assert( val->IsType( _JSON_ALLOCATED ) );

	switch ( val->GetType() )
	{
		case JSON_STRING:
		{
			FreeString( &val->_string );
			break;
		}
#ifdef SQUNICODE
		case JSON_WSTRING:
		{
			FreeString( &val->_wstring );
			break;
		}
#endif
		case JSON_TABLE:
		{
			DeleteTable( val->_table );
			break;
		}
		case JSON_ARRAY:
		{
			DeleteArray( val->_array );
			break;
		}
		default: UNREACHABLE();
	}
}

inline int GetJSONStringSize( const json_value_t &obj )
{
	int len;

	switch ( obj.GetType() )
	{
		case JSON_STRING:
		{
			len = 2 + obj._string.len; // "val"

			if ( obj.IsType( _JSON_QUOTED ) )
				len += 4;

			const char *c = obj._string.ptr;
			for ( int i = obj._string.len; i--; c++ )
			{
				switch ( *c )
				{
					case '\"': case '\\': case '\b':
					case '\f': case '\n': case '\r': case '\t':
						len++;
						if ( obj.IsType( _JSON_QUOTED ) )
						{
							len++;
							if ( *c == '\"' || *c == '\\' )
								len++;
						}
						break;
					default:
						if ( !IN_RANGE_CHAR( *(unsigned char*)c, 0x20, 0x7E ) )
						{
							int ret = IsValidUTF8( (unsigned char*)c, i + 1 );
							if ( ret != 0 )
							{
								i -= ret - 1;
								c += ret - 1;
							}
							else
							{
								if ( !obj.IsType( _JSON_QUOTED ) )
								{
									len += sizeof(uint16_t) * 2 + 1;
								}
								else
								{
									len += sizeof(SQChar) * 2 + 2;
								}
							}
						}
				}
			}

			break;
		}
#ifdef SQUNICODE
		case JSON_WSTRING:
		{
			if ( !obj.IsType( _JSON_QUOTED ) )
			{
				len = 2 + UTF8Length< kUTFEscapeJSON >( obj._wstring.ptr, obj._wstring.len );
			}
			else
			{
				len = 2 + UTF8Length< kUTFEscapeQuoted >( obj._wstring.ptr, obj._wstring.len );
			}

			break;
		}
#endif
		case JSON_INTEGER:
		{
			if ( obj._integer > 0 )
			{
				len = countdigits( obj._integer );
			}
			else if ( obj._integer )
			{
				len = countdigits( -obj._integer ) + 1;
			}
			else
			{
				len = 1; // 0
			}

			if ( obj.IsType( _JSON_QUOTED ) )
				len += 2;

			break;
		}
		case JSON_FLOAT:
		{
			char tmp[ FMT_FLT_LEN + 1 ];
			len = snprintf( tmp, sizeof(tmp), "%f", obj._float );
			break;
		}
		case JSON_BOOL:
		{
			len = obj._integer ? STRLEN("true") : STRLEN("false");
			break;
		}
		case JSON_NULL:
		{
			len = STRLEN("null");
			break;
		}
		case JSON_TABLE:
		{
			len = 2; // {}

			if ( obj._table->size() )
			{
				for ( int i = 0; i < obj._table->size(); i++ )
				{
					const json_field_t *kv = obj._table->Get(i);
					len += 2 + kv->key.len + 1 + GetJSONStringSize( kv->val ) + 1; // "key":val,
				}

				len--; // trailing comma
			}

			break;
		}
		case JSON_ARRAY:
		{
			len = 2; // []

			if ( obj._array->size() )
			{
				for ( int i = 0; i < obj._array->size(); i++ )
					len += GetJSONStringSize( *obj._array->Get(i) ) + 1;

				len--; // trailing comma
			}

			break;
		}
		default: UNREACHABLE();
	}

	return len;
}

inline int JSONStringify( const json_value_t &obj, char *mem, int size, int idx )
{
	switch ( obj.GetType() )
	{
		case JSON_STRING:
		{
			mem[idx++] = '\"';

			if ( obj.IsType( _JSON_QUOTED ) )
			{
				mem[idx++] = '\\';
				mem[idx++] = '\"';
			}

			const char *c = obj._string.ptr;
			for ( int i = obj._string.len; i--; c++ )
			{
				mem[idx++] = *c;

				switch ( *c )
				{
					case '\"':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
						{
							mem[idx++] = '\\';
							mem[idx++] = '\\';
						}
						mem[idx++] = '\"';
						break;
					case '\\':
						if ( obj.IsType( _JSON_QUOTED ) )
						{
							mem[idx++] = '\\';
							mem[idx++] = '\\';
						}
						mem[idx++] = '\\';
						break;
					case '\b':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
							mem[idx++] = '\\';
						mem[idx++] = 'b';
						break;
					case '\f':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
							mem[idx++] = '\\';
						mem[idx++] = 'f';
						break;
					case '\n':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
							mem[idx++] = '\\';
						mem[idx++] = 'n';
						break;
					case '\r':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
							mem[idx++] = '\\';
						mem[idx++] = 'r';
						break;
					case '\t':
						mem[idx-1] = '\\';
						if ( obj.IsType( _JSON_QUOTED ) )
							mem[idx++] = '\\';
						mem[idx++] = 't';
						break;
					default:
						if ( !IN_RANGE_CHAR( *(unsigned char*)c, 0x20, 0x7E ) )
						{
							int ret = IsValidUTF8( (unsigned char*)c, i + 1 );
							if ( ret != 0 )
							{
								memcpy( mem + idx, c + 1, ret - 1 );
								idx += ret - 1;
								i -= ret - 1;
								c += ret - 1;
							}
							else
							{
								mem[idx-1] = '\\';

								if ( !obj.IsType( _JSON_QUOTED ) )
								{
									mem[idx++] = 'u';
									idx += printhex< true, false >( mem + idx, size, (uint16_t)*(unsigned char*)c );
								}
								else
								{
									mem[idx++] = '\\';
									mem[idx++] = 'x';
									idx += printhex< true, false >( mem + idx, size, (SQChar)*(unsigned char*)c );
								}
							}
						}
				}
			}

			if ( obj.IsType( _JSON_QUOTED ) )
			{
				mem[idx++] = '\\';
				mem[idx++] = '\"';
			}

			mem[idx++] = '\"';
			break;
		}
#ifdef SQUNICODE
		case JSON_WSTRING:
		{
			mem[idx++] = '\"';

			if ( !obj.IsType( _JSON_QUOTED ) )
			{
				idx += SQUnicodeToUTF8< kUTFEscapeJSON >( mem + idx, size, obj._wstring.ptr, obj._wstring.len );
			}
			else
			{
				idx += SQUnicodeToUTF8< kUTFEscapeQuoted >( mem + idx, size, obj._wstring.ptr, obj._wstring.len );
			}

			mem[idx++] = '\"';
			break;
		}
#endif
		case JSON_INTEGER:
		{
			if ( !obj.IsType( _JSON_QUOTED ) )
			{
				idx += printint( mem + idx, size, obj._integer );
			}
			else
			{
				mem[idx++] = '\"';
				idx += printint( mem + idx, size, obj._integer );
				mem[idx++] = '\"';
			}

			break;
		}
		case JSON_FLOAT:
		{
			idx += sprintf( mem + idx, "%f", obj._float );
			break;
		}
		case JSON_BOOL:
		{
			if ( obj._integer )
			{
				memcpy( mem + idx, "true", STRLEN("true") );
				idx += STRLEN("true");
			}
			else
			{
				memcpy( mem + idx, "false", STRLEN("false") );
				idx += STRLEN("false");
			}
			break;
		}
		case JSON_NULL:
		{
			memcpy( mem + idx, "null", STRLEN("null") );
			idx += STRLEN("null");
			break;
		}
		case JSON_TABLE:
		{
			mem[idx++] = '{';
			if ( obj._table->size() )
			{
				for ( int i = 0; i < obj._table->size(); i++ )
				{
					const json_field_t *kv = obj._table->Get(i);
					mem[idx++] = '\"';
					memcpy( mem + idx, kv->key.ptr, kv->key.len );
					idx += kv->key.len;
					mem[idx++] = '\"';
					mem[idx++] = ':';
					idx = JSONStringify( kv->val, mem, size, idx );
					mem[idx++] = ',';
				}
				idx--; // trailing comma
			}
			mem[idx++] = '}';
			break;
		}
		case JSON_ARRAY:
		{
			mem[idx++] = '[';
			if ( obj._array->size() )
			{
				for ( int i = 0; i < obj._array->size(); i++ )
				{
					idx = JSONStringify( *obj._array->Get(i), mem, size, idx );
					mem[idx++] = ',';
				}
				idx--; // trailing comma
			}
			mem[idx++] = ']';
			break;
		}
		default: UNREACHABLE();
	}
	Assert( idx < size );
	return idx;
}

inline int GetJSONStringSize( json_table_t *obj )
{
	json_value_t v;
	v.type = JSON_TABLE;
	v._table = obj;
	return GetJSONStringSize( v );
}

inline int JSONStringify( json_table_t *obj, char *mem, int size, int idx )
{
	json_value_t v;
	v.type = JSON_TABLE;
	v._table = obj;
	return JSONStringify( v, mem, size, idx );
}

class JSONParser
{
private:
	char *m_cur;
	char *m_end;
	char m_error[48];

	enum
	{
		Token_Error = 0,
		Token_String,
		Token_Integer,
		Token_Float,
		Token_False,
		Token_True = Token_False + 1,
		Token_Null,
		Token_Table = '{',
		Token_Array = '[',
	};

public:
	JSONParser( char *ptr, int len, json_table_t *pTable )
	{
		m_error[0] = 0;

		m_cur = ptr;
		m_end = ptr + len + 1;

		string_t token;
		int type = NextToken( token );

		if ( type == '{' )
		{
			ParseTable( pTable, token );
		}
		else
		{
			SetError( "invalid token, expected '%c'", '{' );
		}
	}

	const char *GetError() const
	{
		return m_error[0] ? m_error : NULL;
	}

private:
	void SetError( const char *fmt, ... )
	{
		if ( m_error[0] )
			return;

		va_list va;
		va_start( va, fmt );
		int len = vsnprintf( m_error, sizeof(m_error), fmt, va );
		va_end( va );

		if ( len < 0 || len > (int)sizeof(m_error)-1 )
			len = (int)sizeof(m_error)-1;

		if ( (int)sizeof(m_error) - len > 4 )
		{
			// negative offset
			m_error[len++] = '@';
			m_error[len++] = '-';
			len += printint( m_error + len, sizeof(m_error) - len, m_end - m_cur - 1 );
			m_error[len] = 0;
		}
	}

	int NextToken( string_t &token )
	{
		while ( m_cur < m_end )
		{
			switch ( *m_cur )
			{
				case 0x20: case 0x0A: case 0x0D: case 0x09:
					m_cur++;
					break;

				case '\"':
					return ParseString( token );

				case '-':
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					return ParseNumber( token );

				case ':': case ',':
				case '{': case '}':
				case '[': case ']':
					return *m_cur++;

				case 't':
					if ( m_cur + 4 >= m_end ||
							m_cur[1] != 'r' || m_cur[2] != 'u' || m_cur[3] != 'e' )
					{
						SetError( "invalid token, expected %s", "\"true\"" );
						return Token_Error;
					}

					m_cur += 4;
					return Token_True;

				case 'f':
					if ( m_cur + 5 >= m_end ||
							m_cur[1] != 'a' || m_cur[2] != 'l' || m_cur[3] != 's' || m_cur[4] != 'e' )
					{
						SetError( "invalid token, expected %s", "\"false\"" );
						return Token_Error;
					}

					m_cur += 5;
					return Token_False;

				case 'n':
					if ( m_cur + 4 >= m_end ||
							m_cur[1] != 'u' || m_cur[2] != 'l' || m_cur[3] != 'l' )
					{
						SetError( "invalid token, expected %s", "\"null\"" );
						return Token_Error;
					}

					m_cur += 4;
					return Token_Null;

				default:
					if ( IN_RANGE_CHAR( *(unsigned char*)m_cur, 0x20, 0x7E ) )
					{
						SetError( "invalid token '%c'", *m_cur );
					}
					else
					{
						SetError( "invalid token '0x%02x'", *(unsigned char*)m_cur );
					}

					return Token_Error;
			}
		}

		return Token_Error;
	}

	int ParseString( string_t &token )
	{
		char *pStart = ++m_cur;
		bool bEscape = false;

		for (;;)
		{
			if ( m_cur >= m_end )
			{
				SetError( "unfinished string" );
				return Token_Error;
			}

			// end
			if ( *m_cur == '\"' )
			{
				*m_cur = 0;
				token.Assign( pStart, m_cur - pStart );
				m_cur++;
				break;
			}

			if ( *m_cur != '\\' )
			{
				m_cur++;
				continue;
			}

			if ( m_cur + 1 >= m_end )
			{
				SetError( "unfinished string" );
				return Token_Error;
			}

			m_cur++;
			bEscape = true;

			// Defer unescape until the end of the string is found
			switch ( *m_cur )
			{
				case '\"': case '\\': case '/':
				case 'b': case 'f':
				case 'n': case 'r': case 't':
					m_cur++;
					break;

				case 'u':
					if ( m_cur + 4 >= m_end ||
							!_isxdigit( m_cur[1] ) || !_isxdigit( m_cur[2] ) ||
							!_isxdigit( m_cur[3] ) || !_isxdigit( m_cur[4] ) )
					{
						SetError( "invalid hex escape" );
						return Token_Error;
					}

					m_cur += 4;
					break;

				default:
					SetError( "invalid escape char '0x%02x'", *(unsigned char*)m_cur );
					return Token_Error;
			}
		}

		if ( bEscape )
		{
			char *cur = pStart;
			char *end = pStart + token.len;

			do
			{
				if ( cur[0] != '\\' )
				{
					cur++;
					continue;
				}

#define _shift( bytesWritten, bytesRead ) \
	memmove( cur + (bytesWritten), cur + (bytesRead), end - ( cur + (bytesRead) ) ); \
	cur += (bytesWritten); \
	end -= (bytesRead) - (bytesWritten);

				switch ( cur[1] )
				{
					case '\\':
shift_one:
						_shift( 0, 1 );
						cur++;
						break;
					case '\"': goto shift_one;
					case '/': goto shift_one;
					case 'b': cur[1] = '\b'; goto shift_one;
					case 'f': cur[1] = '\f'; goto shift_one;
					case 'n': cur[1] = '\n'; goto shift_one;
					case 'r': cur[1] = '\r'; goto shift_one;
					case 't': cur[1] = '\t'; goto shift_one;
					case 'u':
					{
						unsigned int val;
						atox( { cur + 2, 4 }, &val );

						if ( val <= 0xFF )
						{
							cur[0] = (char)val;

							_shift( 1, 6 );
							break;
						}
						else if ( val <= 0x7FF )
						{
							UTF8_2_FROM_UTF32( (unsigned char*)cur, val );

							_shift( 2, 6 );
							break;
						}
						else if ( UTF_SURROGATE(val) )
						{
							if ( UTF_SURROGATE_LEAD(val) )
							{
								if ( cur + 11 < end &&
										cur[6] == '\\' && cur[7] == 'u' &&
										_isxdigit( cur[8] ) && _isxdigit( cur[9] ) &&
										_isxdigit( cur[10] ) && _isxdigit( cur[11] ) )
								{
									unsigned int low;
									atox( { cur + 8, 4 }, &low );

									if ( UTF_SURROGATE_TRAIL( low ) )
									{
										val = UTF32_FROM_UTF16_SURROGATE( val, low );
										UTF8_4_FROM_UTF32( (unsigned char*)cur, val );

										_shift( 4, 12 );
										break;
									}
								}
							}
						}

						UTF8_3_FROM_UTF32( (unsigned char*)cur, val );

						_shift( 3, 6 );
						break;
					}
					default: UNREACHABLE();
				}

#undef _shift
			}
			while ( cur < end );

			token.len = end - pStart;
			token.ptr[token.len] = 0;
		}

		return Token_String;
	}

	int ParseNumber( string_t &token )
	{
		const char *pStart = m_cur;
		int type;

		if ( *m_cur == '-' )
		{
			m_cur++;
			if ( m_cur >= m_end )
				goto err_eof;
		}

		if ( *m_cur == '0' )
		{
			m_cur++;
			if ( m_cur >= m_end )
				goto err_eof;
		}
		else if ( IN_RANGE_CHAR( *(unsigned char*)m_cur, '1', '9' ) )
		{
			do
			{
				m_cur++;
				if ( m_cur >= m_end )
					goto err_eof;
			}
			while ( IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) );
		}
		else
		{
			SetError( "unexpected char '0x%02x' in number", *(unsigned char*)m_cur );
			return Token_Error;
		}

		type = Token_Integer;

		if ( *m_cur == '.' )
		{
			type = Token_Float;
			m_cur++;

			while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) )
				m_cur++;

			if ( m_cur >= m_end )
				goto err_eof;
		}

		if ( *m_cur == 'e' || *m_cur == 'E' )
		{
			type = Token_Float;
			m_cur++;

			if ( m_cur >= m_end )
				goto err_eof;

			if ( *m_cur == '-' || *m_cur == '+' )
			{
				m_cur++;

				if ( m_cur >= m_end )
					goto err_eof;
			}

			while ( m_cur < m_end && IN_RANGE_CHAR( *(unsigned char*)m_cur, '0', '9' ) )
				m_cur++;
		}

		token.Assign( pStart, m_cur - pStart );
		return type;

	err_eof:
		SetError( "unexpected eof" );
		return Token_Error;
	}

	int ParseTable( json_table_t *pTable, string_t &token )
	{
		if ( *m_cur == '}' )
		{
			m_cur++;
			return Token_Table;
		}

		int type;
		do
		{
			type = NextToken( token );

			if ( type != Token_String )
			{
				SetError( "invalid token, expected %s", "string" );
				return Token_Error;
			}

			type = NextToken( token );

			if ( type != ':' )
			{
				SetError( "invalid token, expected '%c'", ':' );
				return Token_Error;
			}

			Assert( !pTable->Get( token ) );

			json_field_t *kv = pTable->NewElement();
			kv->key = token;

			type = NextToken( token );
			type = ParseValue( type, token, &kv->val );

			if ( type == Token_Error )
				return Token_Error;

			type = NextToken( token );

			if ( type != ',' && type != '}' )
			{
				SetError( "invalid token, expected '%c'", '}' );
				return Token_Error;
			}
		} while ( type != '}' );

		return Token_Table;
	}

	int ParseArray( json_array_t *pArray, string_t &token )
	{
		if ( *m_cur == ']' )
		{
			m_cur++;
			return Token_Array;
		}

		int type;
		do
		{
			type = NextToken( token );

			if ( type == Token_Error )
				return Token_Error;

			json_value_t *val = pArray->NewElement();
			type = ParseValue( type, token, val );

			if ( type == Token_Error )
				return Token_Error;

			type = NextToken( token );

			if ( type != ',' && type != ']' )
			{
				SetError( "invalid token, expected '%c'", ']' );
				return Token_Error;
			}
		} while ( type != ']' );

		return Token_Array;
	}

	int ParseValue( int type, string_t &token, json_value_t *value )
	{
		switch ( type )
		{
			case Token_Integer:
				value->type = JSON_INTEGER;
				atoi( token, &value->_integer );
				return type;
			case Token_Float:
			{
				char cEnd = token.ptr[token.len];
				token.ptr[token.len] = 0;
				value->type = JSON_FLOAT;
				value->_float = (SQFloat)strtod( token.ptr, NULL );
				token.ptr[token.len] = cEnd;
				return type;
			}
			case Token_String:
				value->type = JSON_STRING;
				value->_string = token;
				return type;
			case '{':
				value->type = JSON_TABLE | _JSON_ALLOCATED;
				value->_table = CreateTable(0);
				return ParseTable( value->_table, token );
			case '[':
				value->type = JSON_ARRAY | _JSON_ALLOCATED;
				value->_array = CreateArray(0);
				return ParseArray( value->_array, token );
			case Token_False:
			case Token_True:
				value->type = JSON_BOOL;
				value->_integer = type - Token_False;
				return type;
			case Token_Null:
				value->type = JSON_NULL;
				return type;
			default:
				SetError( "unrecognised token" );
				return Token_Error;
		}
	}
};

#endif // SQDBG_JSON_H
