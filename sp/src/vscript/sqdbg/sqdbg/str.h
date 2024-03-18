//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_STRING_H
#define SQDBG_STRING_H

#include "debug.h"

#define STRLEN(s) (sizeof(s) - 1)

#define FMT_UINT32_LEN STRLEN("4294967295")

#ifdef _SQ64
	#define FMT_INT_LEN STRLEN("-9223372036854775808")
	#define FMT_PTR_LEN 18
	#define FMT_OCT_LEN STRLEN("01777777777777777777777")

	#if defined(_WIN32) || SQUIRREL_VERSION_NUMBER > 223
		#define FMT_INT "%lld"
		#define FMT_PTR "0x%016llX"
	#else
		#define FMT_INT "%ld"
		#define FMT_PTR "0x%016lX"
	#endif
#else
	#define FMT_INT_LEN STRLEN("-2147483648")
	#define FMT_PTR_LEN 10
	#define FMT_OCT_LEN STRLEN("017777777777")

	#define FMT_INT "%d"
	#define FMT_PTR "0x%08X"
#endif

#ifdef SQUNICODE
	#define FMT_STR "%ls"
	#define FMT_CSTR "%hs"
	#define FMT_VCSTR "%.*hs"
	#define FMT_VSTR "%.*ls"
#else
	#define FMT_STR "%s"
	#define FMT_CSTR "%s"
	#define FMT_VCSTR "%.*s"
	#define FMT_VSTR "%.*s"
#endif

#ifdef SQUSEDOUBLE
	#define FMT_FLT "%lf"
	#define FMT_FLT_LEN ( 1 + DBL_MAX_10_EXP + 1 + 1 + FLT_DIG )
#else
	#define FMT_FLT "%f"
	#define FMT_FLT_LEN ( 1 + FLT_MAX_10_EXP + 1 + 1 + FLT_DIG )
#endif

struct string_t;
#ifndef SQUNICODE
typedef string_t sqstring_t;
#else
struct sqstring_t;
#endif
struct stringbufbase_t;
template < int BUFSIZE > struct stringbuf_t;
typedef stringbufbase_t stringbufext_t;

template < typename C, typename I >
int printint( C *buf, int size, I value );

template < bool padding = true, bool prefix = true, bool uppercase = true, typename C, typename I >
int printhex( C *buf, int size, I value );

template < typename C, typename I >
inline int printoct( C *buf, int size, I value );

template < typename I >
bool atoi( string_t str, I *out );

template < typename I >
bool atox( string_t str, I *out );

template < typename I >
bool atoo( string_t str, I *out );


#ifdef SQUNICODE
void CopyString( const string_t &src, sqstring_t *dst );
void CopyString( const sqstring_t &src, string_t *dst );
#endif
template < typename T > void CopyString( const T &src, T *dst );
template < typename T > void FreeString( T *dst );

#define IN_RANGE(c, min, max) \
	((uint32_t)((uint32_t)(c) - (uint32_t)(min)) <= (uint32_t)((max)-(min)))

#define IN_RANGE_CHAR(c, min, max) \
	((unsigned char)((unsigned char)(c) - (unsigned char)(min)) <= (unsigned char)((max)-(min)))

#define UTF16_NONCHAR(cp) IN_RANGE(cp, 0xFDD0, 0xFDEF)
#define UTF_NONCHAR(cp) ( ( (cp) & 0xFFFE ) == 0xFFFE )

// [0xD800, 0xDFFF]
#define UTF_SURROGATE(cp) ( ( (cp) & 0xFFFFF800 ) == 0x0000D800 )

// [0xD800, 0xDBFF]
#define UTF_SURROGATE_LEAD(cp) ( ( (cp) & 0xFFFFFC00 ) == 0x0000D800 )

// [0xDC00, 0xDFFF]
#define UTF_SURROGATE_TRAIL(cp) ( ( (cp) & 0xFFFFFC00 ) == 0x0000DC00 )

// 10xxxxxx
#define UTF8_TRAIL(c) ( ( (c) & 0xC0 ) == 0x80 )

// 110xxxxx 10xxxxxx
#define UTF8_2_LEAD(c) ( ( (c) & 0xE0 ) == 0xC0 )

// 1110xxxx 10xxxxxx 10xxxxxx
#define UTF8_3_LEAD(c) ( ( (c) & 0xF0 ) == 0xE0 )

// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
#define UTF8_4_LEAD(c) ( ( (c) & 0xF8 ) == 0xF0 )

#define UTF8_2(len, c0, src) \
	( (len) > 1 && UTF8_TRAIL((src)[1]) )

#define UTF8_3(len, c0, src) \
	( (len) > 2 && UTF8_3_ISVALID(c0, (src)[1]) && UTF8_TRAIL((src)[2]) )

#define UTF8_4(len, c0, src) \
	( (len) > 3 && UTF8_4_ISVALID(c0, (src)[1]) && UTF8_TRAIL((src)[2]) && UTF8_TRAIL((src)[3]) )

#define UTF8_3_ISVALID(c0, c1) \
	( ( c0 == 0xE0 && IN_RANGE(c1, 0xA0, 0xBF) ) || \
	  ( c0 == 0xED && IN_RANGE(c1, 0x80, 0x9F) ) || \
	  ( UTF8_TRAIL(c1) && (IN_RANGE(c0, 0xE1, 0xEC) || IN_RANGE(c0, 0xEE, 0xEF)) ) )

#define UTF8_4_ISVALID(c0, c1) \
	( ( c0 == 0xF0 && IN_RANGE(c1, 0x90, 0xBF) ) || \
	  ( c0 == 0xF4 && IN_RANGE(c1, 0x80, 0x8F) ) || \
	  ( UTF8_TRAIL(c1) && IN_RANGE(c0, 0xF1, 0xF3) ) )

#define UTF32_FROM_UTF8_2(c0, c1) \
	( ( ( (c0) & 0x1F ) << 6 ) | \
	    ( (c1) & 0x3F ) )

#define UTF32_FROM_UTF8_3(c0, c1, c2) \
	( ( ( (c0) & 0x0F ) << 12 ) | \
	  ( ( (c1) & 0x3F ) << 6 ) | \
	    ( (c2) & 0x3F ) )

#define UTF32_FROM_UTF8_4(c0, c1, c2, c3) \
	( ( ( (c0) & 0x07 ) << 18 ) | \
	  ( ( (c1) & 0x3F ) << 12 ) | \
	  ( ( (c2) & 0x3F ) << 6 ) | \
	    ( (c3) & 0x3F ) )

#define UTF32_FROM_UTF16_SURROGATE(lead, trail) \
	( ( ( ( (lead) & 0x3FF ) << 10 ) | ( (trail) & 0x3FF ) ) + 0x10000 )

#define UTF16_SURROGATE_FROM_UTF32(dst, cp) \
	(dst)[0] = 0xD800 | ( (cp - 0x10000) >> 10 ); \
	(dst)[1] = 0xDC00 | ( (cp - 0x10000) & 0x3FF );

#define UTF8_2_FROM_UTF32(mbc, cp) \
	(mbc)[0] = 0xC0 | ( (cp) >> 6 ); \
	(mbc)[1] = 0x80 | ( (cp) & 0x3F );

#define UTF8_3_FROM_UTF32(mbc, cp) \
	(mbc)[0] = 0xE0 | ( (cp) >> 12 ); \
	(mbc)[1] = 0x80 | ( ( (cp) >> 6 ) & 0x3F ); \
	(mbc)[2] = 0x80 | ( (cp) & 0x3F ); \

#define UTF8_4_FROM_UTF32(mbc, cp) \
	(mbc)[0] = 0xF0 | ( (cp) >> 18 ); \
	(mbc)[1] = 0x80 | ( ( (cp) >> 12 ) & 0x3F ); \
	(mbc)[2] = 0x80 | ( ( (cp) >> 6 ) & 0x3F ); \
	(mbc)[3] = 0x80 | ( (cp) & 0x3F );

typedef enum
{
	kUTFNoEscape = 0,
	// Use 'x' as hex escape, escape invalid unicode
	kUTFEscape = 1,
	// Same as kUTFEscape, escape all backslashes as well, quote the whole input
	kUTFEscapeQuoted,
	// Use 'u' as hex escape
	kUTFEscapeJSON,
} EUTFEscape;

inline int IsValidUTF8( unsigned char *src, int srclen );
#ifdef SQUNICODE
inline int IsValidUnicode( const SQChar *src, int srclen );
template < bool undoEscape = false >
inline int UTF8ToSQUnicode( SQChar *dst, int destSize, const char *src, int srclen );
template < EUTFEscape escape = kUTFNoEscape >
inline int SQUnicodeToUTF8( char *dst, int destSize, const SQChar *src, int srclen );

// Returns character length
inline int SQUnicodeLength( const char *src, int srclen )
{
	return UTF8ToSQUnicode( NULL, 0, src, srclen );
}

// Returns byte length
template < EUTFEscape escape = kUTFNoEscape >
inline int UTF8Length( const SQChar *src, int srclen )
{
	return SQUnicodeToUTF8< escape >( NULL, 0, src, srclen );
}
#endif

inline int scstombs( char *dst, int destSize, const SQChar *src, int srclen )
{
#ifdef SQUNICODE
	return SQUnicodeToUTF8( dst, destSize, src, srclen );
#else
	int len = min( srclen, destSize );
	memcpy( dst, src, len );
	return len;
#endif
}

#define STR_EXPAND(s) (s).len, (s).ptr

struct string_t
{
	char *ptr;
	int len;

	string_t() {}

	string_t( const char *src, int size ) :
		ptr((char*)src),
		len(size)
	{
	}

	string_t( const stringbufbase_t &src );

#ifndef SQUNICODE
	string_t( SQString *src ) :
		ptr(src->_val),
		len(src->_len)
	{
	}
#endif

	template < int size >
	string_t( const char (&src)[size] ) :
		ptr((char*)src),
		len(size-1)
	{
		// input wasn't a string literal,
		// call ( src, size ) constructor instead
		Assert( (int)strlen(src) == len );
	}

	void Strip()
	{
		char *end = ptr + len;

		for ( char *c = ptr; c < end; c++ )
		{
			if ( *c == ' ' || *c == '\t' || *c == '\n' )
			{
				ptr++;
				len--;
			}
			else
			{
				break;
			}
		}

		for ( char *c = end - 1; c >= ptr; c-- )
		{
			if ( *c == ' ' || *c == '\t' || *c == '\n' )
			{
				len--;
			}
			else
			{
				break;
			}
		}
	}

	char *Find( char ch ) const
	{
		char *p = ptr;
		char *end = ptr + len;

		for ( ; p < end; p++ )
		{
			if ( *p == ch )
				return p;
		}

		return NULL;
	}

	template < int size >
	bool StartsWith( const char (&other)[size] ) const
	{
		if ( size-1 <= len && *ptr == *other )
			return !memcmp( ptr, other, size-1 );

		return false;
	}

	bool StartsWith( const string_t &other ) const
	{
		if ( other.len <= len && *ptr == *other.ptr )
			return !memcmp( ptr, other.ptr, other.len );

		return false;
	}

	template < int size >
	bool IsEqualTo( const char (&other)[size] ) const
	{
		if ( size-1 == len && *ptr == *other )
			return !memcmp( ptr, other, size-1 );

		return false;
	}

	bool IsEqualTo( const char *other, int size ) const
	{
		if ( len == size && *ptr == *other )
			return !memcmp( ptr, other, size );

		return false;
	}

	bool IsEqualTo( const string_t &other ) const
	{
		if ( len == other.len && *ptr == *other.ptr )
			return !memcmp( ptr, other.ptr, len );

		return false;
	}

	bool IsEqualTo( const SQString *other ) const
	{
#ifdef SQUNICODE
		if ( len == other->_len )
		{
			Assert( len );

			int i = 0;
			do
			{
				// Used for comparing against locals and outers,
				// implement unicode conversion if locals can have unicode characters
				if ( other->_val[i] > 0x7E || (char)other->_val[i] != ptr[i] )
				{
					AssertMsg( other->_val[i] <= 0x7E, "not implemented" );
					return false;
				}
			}
			while ( ++i < len );

			return true;
		}
#else
		if ( len == other->_len && *ptr == *other->_val )
			return !memcmp( ptr, other->_val, len );
#endif
		return false;
	}

	bool IsEmpty() const
	{
		return !len;
	}

	bool Contains( char ch ) const
	{
		return ( memchr( ptr, ch, len ) != NULL );
	}

	template < int size >
	void Assign( const char (&src)[size] )
	{
		ptr = (char*)src;
		len = size - 1;
		Assert( (int)strlen(src) == len );
	}

	void Assign( const char *src, int size )
	{
		ptr = (char*)src;
		len = size;
	}

#ifndef SQUNICODE
	void Assign( const SQString *src )
	{
		ptr = (SQChar*)src->_val;
		len = src->_len;
	}
#endif

private:
	operator const char*();
	operator char*();
	string_t &operator=( const char *src );
};

struct conststring_t : string_t
{
	template < int size >
	conststring_t( const char (&src)[size] ) : string_t(src) {}
};

#ifdef SQUNICODE
struct sqstring_t
{
	SQChar *ptr;
	int len;

	sqstring_t() {}

	sqstring_t( const SQString *src ) :
		ptr((SQChar*)src->_val),
		len(src->_len)
	{
	}

	sqstring_t( const SQChar *src, int size ) :
		ptr((SQChar*)src),
		len(size)
	{
	}

	template < int size >
	sqstring_t( const SQChar (&src)[size] ) :
		ptr((SQChar*)src),
		len(size-1)
	{
		Assert( (int)scstrlen(src) == len );
	}

	bool StartsWith( const string_t &other ) const
	{
		if ( other.len <= len )
		{
			Assert( other.len );

			int i = 0;
			do
			{
				if ( ptr[i] > 0x7E || other.ptr[i] != (char)ptr[i] )
				{
					AssertMsg( ptr[i] <= 0x7E, "not implemented" );
					return false;
				}
			}
			while ( ++i < other.len );

			return true;
		}

		return false;
	}

	bool IsEqualTo( const sqstring_t &other ) const
	{
		if ( len == other.len && *ptr == *other.ptr )
			return !memcmp( ptr, other.ptr, sq_rsl(len) );

		return false;
	}

	bool IsEqualTo( const SQString *other ) const
	{
		if ( len == other->_len && *ptr == *other->_val )
			return !memcmp( ptr, other->_val, sq_rsl(len) );

		return false;
	}

	bool IsEmpty() const
	{
		return !len;
	}

	template < int size >
	void Assign( const SQChar (&src)[size] )
	{
		ptr = (SQChar*)src;
		len = size - 1;
		Assert( (int)scstrlen(src) == len );
	}

	void Assign( const SQChar *src, int size )
	{
		ptr = (SQChar*)src;
		len = size;
	}

	void Assign( const SQString *src )
	{
		ptr = (SQChar*)src->_val;
		len = src->_len;
	}
};
#endif

struct stringbufbase_t
{
	char *ptr;
	int len;
	const int size;

	stringbufbase_t( char *src, int size ) :
		ptr(src),
		len(0),
		size(size)
	{
	}

	stringbufbase_t &operator=( const stringbufbase_t & );

	int BufSize()
	{
		return size;
	}

	int BytesLeft()
	{
		return BufSize() - len;
	}

	template < int SIZE >
	void Puts( const char (&psz)[SIZE] )
	{
		int amt = min( BytesLeft(), (SIZE-1) );

		memcpy( ptr + len, psz, amt );
		len += amt;
	}

	void Puts( const string_t &str )
	{
		int amt = min( BytesLeft(), str.len );

		memcpy( ptr + len, str.ptr, amt );
		len += amt;
	}

#ifdef SQUNICODE
	void Puts( const sqstring_t &str )
	{
		len += SQUnicodeToUTF8( ptr + len, BytesLeft(), str.ptr, str.len );
	}
#endif

	void Put( char ch )
	{
		if ( BufSize() >= len + 1 )
		{
			ptr[len++] = ch;
		}
	}

	void Term()
	{
		if ( len > BufSize()-1 )
			len = BufSize()-1;

		ptr[len] = 0;
		Assert( (int)strlen(ptr) == len );
	}

	template < typename I >
	void PutInt( I value )
	{
		int space = BytesLeft();

		if ( space < 1 )
			return;

		len += printint( ptr + len, space, value );
	}

	template < typename I >
	void PutHex( I value, bool padding = true )
	{
		int space = BytesLeft();

		if ( space < 3 )
			return;

		if ( padding )
		{
			len += printhex< true >( ptr + len, space, value );
		}
		else
		{
			len += printhex< false >( ptr + len, space, value );
		}
	}
};

string_t::string_t( const stringbufbase_t &src ) :
	ptr(src.ptr),
	len(src.len)
{
}

template < int BUFSIZE >
struct stringbuf_t : stringbufbase_t
{
	char ptr[BUFSIZE];

	stringbuf_t() : stringbufbase_t( ptr, BUFSIZE )
	{
	}
};

#define _isdigit( c ) \
	IN_RANGE_CHAR(c, '0', '9')

#define _isxdigit( c ) \
	( IN_RANGE_CHAR(c, '0', '9') || \
	  IN_RANGE_CHAR(c, 'A', 'F') || \
	  IN_RANGE_CHAR(c, 'a', 'f') )

#define _isalpha( c ) \
	( IN_RANGE_CHAR(c, 'A', 'Z') || IN_RANGE_CHAR(c, 'a', 'z') )

#define _isalnum( c ) \
	( _isalpha(c) || _isdigit(c) )

template < int BASE = 10, typename I >
inline int countdigits( I input )
{
	int i = 0;

	do
	{
		input /= BASE;
		i++;
	}
	while ( input );

	return i;
}

template < typename C, typename I >
inline int printint( C *buf, int size, I value )
{
	Assert( size > 0 );

	if ( !value )
	{
		if ( size >= 1 )
		{
			buf[0] = '0';
			return 1;
		}

		return 0;
	}

	bool neg;
	int len;

	if ( value >= 0 )
	{
		len = countdigits( value );
		neg = false;
	}
	else
	{
		value = -value;
		len = countdigits( value ) + 1;
		buf[0] = '-';

		neg = ( value < 0 ); // value == INT_MIN
	}

	if ( len > size )
		len = size;

	int i = len - 1;

	do
	{
		C c = value % 10;
		value /= 10;
		buf[i--] = !neg ? ( '0' + c ) : ( '0' - c );
	}
	while ( value && i >= 0 );

	return len;
}

template < bool padding, bool prefix, bool uppercase, typename C, typename I >
inline int printhex( C *buf, int size, I value )
{
	Assert( size > 0 );

	int len = ( prefix ? 2 : 0 ) + ( padding ? sizeof(I) * 2 : countdigits<16>( value ) );

	if ( len > size )
		len = size;

	int i = len - 1;

	do
	{
		C c = value & 0xf;
		value >>= 4;
		buf[i--] = c + ( ( c < 10 ) ? '0' : ( ( uppercase ? 'A' : 'a' ) - 10 ) );
	}
	while ( value && i >= 0 );

	if ( padding )
	{
		while ( i >= ( prefix ? 2 : 0 ) )
			buf[i--] = '0';

		if ( prefix )
		{
			buf[i--] = 'x';
			buf[i--] = '0';
		}
	}
	else
	{
		if ( prefix )
		{
			buf[0] = '0';
			buf[1] = 'x';
			i--;
			i--;
		}
	}

	Assert( i == -1 );
	return len;
}

template < typename C, typename I >
inline int printoct( C *buf, int size, I value )
{
	Assert( size > 0 );

	int len = countdigits<8>( value ) + 1;

	if ( len > size )
		len = size;

	int i = len - 1;

	do
	{
		C c = value & 0x7;
		value >>= 3;
		buf[i--] = '0' + c;
	}
	while ( value && i >= 0 );

	if ( i >= 0 )
		buf[i--] = '0';

	Assert( i == -1 );
	return len;
}

template < typename I >
inline bool atoi( string_t str, I *out )
{
	Assert( str.ptr && str.len > 0 );

	I val = 0;
	bool neg = ( *str.ptr == '-' );

	if ( neg )
	{
		str.ptr++;
		str.len--;
	}

	for ( ; str.len--; str.ptr++ )
	{
		unsigned char ch = *str.ptr;

		if ( IN_RANGE_CHAR(ch, '0', '9') )
		{
			val *= 10;
			val += ch - '0';
		}
		else
		{
			*out = 0;
			return false;
		}
	}

	*out = !neg ? val : -val;
	return true;
}

template < typename I >
inline bool atox( string_t str, I *out )
{
	if ( str.StartsWith("0x") )
	{
		str.ptr += 2;
		str.len -= 2;
	}

	I val = 0;

	for ( ; str.len--; str.ptr++ )
	{
		unsigned char ch = *str.ptr;

		if ( IN_RANGE_CHAR(ch, '0', '9') )
		{
			val <<= 4;
			val += ch - '0';
		}
		else if ( IN_RANGE_CHAR(ch, 'A', 'F') )
		{
			val <<= 4;
			val += ch - 'A' + 10;
		}
		else if ( IN_RANGE_CHAR(ch, 'a', 'f') )
		{
			val <<= 4;
			val += ch - 'a' + 10;
		}
		else
		{
			*out = 0;
			return false;
		}
	}

	*out = val;
	return true;
}

template < typename I >
inline bool atoo( string_t str, I *out )
{
	I val = 0;

	for ( ; str.len--; str.ptr++ )
	{
		unsigned char ch = *str.ptr;

		if ( IN_RANGE_CHAR(ch, '0', '7') )
		{
			val <<= 3;
			val += ch - '0';
		}
		else
		{
			*out = 0;
			return false;
		}
	}

	*out = val;
	return true;
}

// Returns byte count of valid UTF8 sequences
// Returns 0 for control characters
// Returns 0 for noncharacters
inline int IsValidUTF8( unsigned char *src, int srclen )
{
	unsigned char cp = src[0];

	if ( cp <= 0x7E )
	{
		if ( cp >= 0x20 )
			return 1;

		return 0;
	}
	else if ( IN_RANGE_CHAR(cp, 0xC2, 0xF4) )
	{
		if ( UTF8_2_LEAD(cp) )
		{
			if ( UTF8_2( srclen, cp, src ) )
			{
				return 2;
			}
		}
		else if ( UTF8_3_LEAD(cp) )
		{
			if ( UTF8_3( srclen, cp, src ) )
			{
				return 3;
			}
		}
		else if ( UTF8_4_LEAD(cp) )
		{
			if ( UTF8_4( srclen, cp, src ) )
			{
				return 4;
			}
		}
	}
	// Look behind
	// Unused, there is no condition where strings aren't processed linearly from the start
#if 0
	else if ( UTF8_TRAIL(cp) )
	{
		int lim = srcindex - 3;
		if ( lim < 0 )
			lim = 0;

		while ( srcindex-- > lim )
		{
			cp = *(--src);
			srclen++;

			if ( !UTF8_TRAIL(cp) )
			{
				if ( IN_RANGE_CHAR(cp, 0xC2, 0xF4) )
					goto check;

				return 0;
			}
		}
	}
#endif
	// else [0x7F, 0xC2) & (0xF4, 0xFF]

	return 0;
}

#ifdef SQUNICODE
// Returns code unit count for valid unicode
// Returns 0 for control characters
// Returns -1 if the invalid code unit is larger than 1 byte
// Noncharacters and private use areas are valid
inline int IsValidUnicode( const SQChar *src, int srclen )
{
	uint32_t cp = (uint32_t)src[0];

	if ( cp <= 0x7E )
	{
		if ( cp >= 0x20 )
			return 1;

		return 0;
	}
	else if ( cp <= 0xFF )
	{
		if ( IN_RANGE(cp, 0xC2, 0xF4) )
		{
			if ( UTF8_2_LEAD(cp) )
			{
				if ( UTF8_2( srclen, cp, src ) )
				{
					return 2;
				}
			}
			else if ( UTF8_3_LEAD(cp) )
			{
				if ( UTF8_3( srclen, cp, src ) )
				{
					return 3;
				}
			}
			else if ( UTF8_4_LEAD(cp) )
			{
				if ( UTF8_4( srclen, cp, src ) )
				{
					return 4;
				}
			}
		}
		// else [0x7F, 0xC2) & (0xF4, 0xFF]

		return 0;
	}

	if ( cp <= 0xFFFF )
	{
		if ( UTF_SURROGATE(cp) )
		{
			if ( srclen > 1 && UTF_SURROGATE_LEAD(cp) && UTF_SURROGATE_TRAIL(src[1]) )
			{
				return 2;
			}

			return -1;
		}

		return 1;
	}
	else if ( cp <= 0x10FFFF )
	{
		return 2;
	}
	else
	{
		return -1;
	}
}

template < bool undoEscape >
inline int UTF8ToSQUnicode( SQChar *dst, int destSize, const char *src, int srclen )
{
	uint32_t cp;
	const char *end = src + srclen;
	int count = 0;

	for ( ; src < end; src++ )
	{
		cp = (uint32_t)((unsigned char*)src)[0];

		if ( cp <= 0x7E )
		{
			if ( undoEscape )
			{
				if ( cp == '\\' && src + 1 < end )
				{
					switch ( ((unsigned char*)src)[1] )
					{
						case '\"': cp = '\"'; src++; break;
						case '\\': src++; break;
						case 'b': cp = '\b'; src++; break;
						case 'f': cp = '\f'; src++; break;
						case 'n': cp = '\n'; src++; break;
						case 'r': cp = '\r'; src++; break;
						case 't': cp = '\t'; src++; break;
						case 'x':
						{
							if ( src + sizeof(SQChar) * 2 + 1 < end )
							{
								Verify( atox( { src + 2, (int)sizeof(SQChar) * 2 }, &cp ) );
								src += sizeof(SQChar) * 2 + 1;
							}

							break;
						}
						case 'u':
						{
							if ( src + sizeof(uint16_t) * 2 + 1 < end )
							{
								Verify( atox( { src + 2, (int)sizeof(uint16_t) * 2 }, &cp ) );
								src += sizeof(uint16_t) * 2 + 1;
							}

							break;
						}
					}
				}
			}

			goto xffff;
		}
		else if ( IN_RANGE(cp, 0xC2, 0xF4) )
		{
			if ( UTF8_2_LEAD(cp) )
			{
				if ( UTF8_2( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_2( cp, src[1] );
					src += 1;
					goto xffff;
				}
			}
			else if ( UTF8_3_LEAD(cp) )
			{
				if ( UTF8_3( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_3( cp, src[1], src[2] );
					src += 2;
					goto xffff;
				}
			}
			else if ( UTF8_4_LEAD(cp) )
			{
				if ( UTF8_4( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_4( cp, src[1], src[2], src[3] );
					src += 3;
					goto supplementary;
				}
			}

			goto xffff;
		}
		else // [0x7F, 0xC2) & (0xF4, 0xFF]
		{
			goto xffff;
		}

#if WCHAR_SIZE == 4
xffff:
supplementary:
		if ( dst )
		{
			if ( destSize >= (int)sizeof(SQChar) )
			{
				*dst++ = cp;
				destSize -= sizeof(SQChar);
				count += 1;
			}
			else
			{
				// out of space
				break;
			}
		}
		else
		{
			count += 1;
		}

		continue;
#else // WCHAR_SIZE == 2
xffff:
		if ( dst )
		{
			if ( destSize >= (int)sizeof(SQChar) )
			{
				*dst++ = (SQChar)cp;
				destSize -= sizeof(SQChar);
				count += 1;
			}
			else
			{
				// out of space
				break;
			}
		}
		else
		{
			count += 1;
		}

		continue;

supplementary:
		if ( dst )
		{
			if ( destSize > (int)sizeof(SQChar) )
			{
				UTF16_SURROGATE_FROM_UTF32( dst, cp );
				dst += 2;
				destSize -= 2 * sizeof(SQChar);
				count += 2;
			}
			else
			{
				cp = 0xFFFD;
				goto xffff;
			}
		}
		else
		{
			count += 2;
		}

		continue;
#endif
	}

	return count;
}

// SQUnicode can be UTF16 or UTF32
template < EUTFEscape escape >
inline int SQUnicodeToUTF8( char *dst, int destSize, const SQChar *src, int srclen )
{
	uint32_t cp;
	const SQChar *end = src + srclen;
	unsigned char mbc[ escape != 0 ?
		( escape == kUTFEscapeJSON ?
		  6 : // "\u0000"
		  ( escape == kUTFEscapeQuoted ?
			14 : // "\\uD800\\uDC00"
			// kUTFEscape
			12 ) ) : // "\uD800\uDC00"
		4 ];
	int count = 0;
	int bytes;

	if ( escape == kUTFEscapeQuoted )
	{
		mbc[0] = '\\';
		mbc[1] = '\"';
		bytes = 2;

		if ( dst )
		{
			if ( bytes <= destSize )
			{
				memcpy( dst, mbc, bytes );
				dst += bytes;
				destSize -= bytes;
				count += bytes;
			}
			else
			{
				// out of space
				return count;
			}
		}
		else
		{
			count += bytes;
		}
	}

	for ( ; src < end; src++ )
	{
		cp = (uint32_t)src[0];

		if ( cp <= 0xFF )
		{
			if ( escape )
			{
				bytes = 0;

				switch ( cp )
				{
					case '\"':
						if ( escape == kUTFEscapeQuoted )
						{
							mbc[bytes++] = '\\';
							mbc[bytes++] = '\\';
						}
						mbc[bytes++] = '\\';
						mbc[bytes++] = '\"';
						goto write;
					case '\\':
						if ( escape == kUTFEscapeQuoted )
						{
							mbc[bytes++] = '\\';
							mbc[bytes++] = '\\';
						}
						mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						goto write;
					case '\b':
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						mbc[bytes++] = 'b';
						goto write;
					case '\f':
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						mbc[bytes++] = 'f';
						goto write;
					case '\n':
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						mbc[bytes++] = 'n';
						goto write;
					case '\r':
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						mbc[bytes++] = 'r';
						goto write;
					case '\t':
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';
						mbc[bytes++] = '\\';
						mbc[bytes++] = 't';
						goto write;

					default:

					if ( !IN_RANGE_CHAR(cp, 0x20, 0x7E) )
					{
						// While UTF8 bytes are valid UTF16, converting them will
						// make distinct SQ strings indistinguishable to the client
#ifndef SQDBG_ESCAPE_UTF8_BYTES_IN_UTF16
						if ( IN_RANGE(cp, 0xC2, 0xF4) )
						{
							if ( UTF8_2_LEAD(cp) )
							{
								if ( UTF8_2( end - src, cp, src ) )
								{
									mbc[0] = (unsigned char)cp;
									mbc[1] = (unsigned char)src[1];
									bytes = 2;
									src += 1;
									goto write;
								}
							}
							else if ( UTF8_3_LEAD(cp) )
							{
								if ( UTF8_3( end - src, cp, src ) )
								{
									mbc[0] = (unsigned char)cp;
									mbc[1] = (unsigned char)src[1];
									mbc[2] = (unsigned char)src[2];
									bytes = 3;
									src += 2;
									goto write;
								}
							}
							else if ( UTF8_4_LEAD(cp) )
							{
								if ( UTF8_4( end - src, cp, src ) )
								{
									mbc[0] = (unsigned char)cp;
									mbc[1] = (unsigned char)src[1];
									mbc[2] = (unsigned char)src[2];
									mbc[3] = (unsigned char)src[3];
									bytes = 4;
									src += 3;
									goto write;
								}
							}
						}
#endif
						// [0x7F, 0xC2) & (0xF4, 0xFF]
						if ( escape == kUTFEscapeQuoted )
							mbc[bytes++] = '\\';

						mbc[bytes++] = '\\';

						if ( escape == kUTFEscapeJSON )
						{
							mbc[bytes++] = 'u';
							bytes += printhex< true, false >( mbc + bytes, sizeof(mbc) - bytes, (uint16_t)cp );
						}
						else
						{
							mbc[bytes++] = 'x';
							bytes += printhex< true, false >( mbc + bytes, sizeof(mbc) - bytes, (SQChar)cp );
						}

						goto write;
					}
				}
			}

			mbc[0] = (unsigned char)cp;
			bytes = 1;
		}
		else if ( cp <= 0x7FF )
		{
			UTF8_2_FROM_UTF32( mbc, cp );
			bytes = 2;
		}
		else if ( cp <= 0xFFFF )
		{
			if ( UTF_SURROGATE(cp) )
			{
				if ( src + 1 < end && UTF_SURROGATE_LEAD(cp) && UTF_SURROGATE_TRAIL(src[1]) )
				{
					cp = UTF32_FROM_UTF16_SURROGATE( cp, (uint32_t)src[1] );
					src++;
					goto supplementary;
				}

				if ( escape && escape != kUTFEscapeJSON )
				{
					bytes = 0;

					if ( escape == kUTFEscapeQuoted )
						mbc[bytes++] = '\\';

					mbc[bytes++] = '\\';
					mbc[bytes++] = 'x';
					bytes = bytes + printhex< true, false >( mbc + bytes, sizeof(mbc) - bytes, (SQChar)cp );
					goto write;
				}
			}

			UTF8_3_FROM_UTF32( mbc, cp );
			bytes = 3;
		}
		else
		{
			if ( cp > 0x10FFFF && escape && escape != kUTFEscapeJSON )
			{
				// "\\uD800\\uDC00"
				uint16_t s[2];
				UTF16_SURROGATE_FROM_UTF32( s, cp );

				bytes = 0;

				if ( escape == kUTFEscapeQuoted )
					mbc[bytes++] = '\\';

				mbc[bytes++] = '\\';
				mbc[bytes++] = 'u';
				bytes += printhex< true, false >( mbc + bytes, sizeof(mbc) - bytes, s[0] );

				if ( escape == kUTFEscapeQuoted )
					mbc[bytes++] = '\\';

				mbc[bytes++] = '\\';
				mbc[bytes++] = 'u';
				bytes += printhex< true, false >( mbc + bytes, sizeof(mbc) - bytes, s[1] );
				goto write;
			}

supplementary:
			UTF8_4_FROM_UTF32( mbc, cp );
			bytes = 4;
		}
write:
		if ( dst )
		{
			if ( bytes <= destSize )
			{
				memcpy( dst, mbc, bytes );
				dst += bytes;
				destSize -= bytes;
				count += bytes;
			}
			else
			{
				// out of space
				break;
			}
		}
		else
		{
			count += bytes;
		}
	}

	if ( escape == kUTFEscapeQuoted )
	{
		mbc[0] = '\\';
		mbc[1] = '\"';
		bytes = 2;

		if ( dst )
		{
			if ( bytes <= destSize )
			{
				memcpy( dst, mbc, bytes );
				dst += bytes;
				destSize -= bytes;
				count += bytes;
			}
			else
			{
				// out of space
				return count;
			}
		}
		else
		{
			count += bytes;
		}
	}

	return count;
}
#endif

#endif // SQDBG_STRING_H
