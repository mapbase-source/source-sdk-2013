//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_VEC_H
#define SQDBG_VEC_H

void *sqdbg_malloc( unsigned int size );
void *sqdbg_realloc( void *p, unsigned int oldsize, unsigned int size );
void sqdbg_free( void *p, unsigned int size );

template < typename T >
class CDefaultAllocator
{
public:
	#pragma pack(push, 4)
	struct memory_t
	{
		void *base;
		int size;
	};
	#pragma pack(pop)

	memory_t memory;

	CDefaultAllocator() : memory()
	{
	}

	T &operator[]( int i ) const
	{
		Assert( memory.size > 0 );
		Assert( i >= 0 && i < memory.size );
		return *( (T*)memory.base + i );
	}

	T *Base()
	{
		return (T*)memory.base;
	}

	int Size() const
	{
		return memory.size;
	}

	void Free()
	{
		if ( memory.base )
		{
			sqdbg_free( memory.base, memory.size );
			memory.base = 0;
			memory.size = 0;
		}
	}

	void Alloc( int count )
	{
		Assert( count > 0 || memory.size == 0 );

		if ( count == memory.size )
			return;

		if ( memory.base )
		{
			memory.base = (T*)sqdbg_realloc( memory.base, memory.size * sizeof(T), count * sizeof(T) );
		}
		else
		{
			memory.base = (T*)sqdbg_malloc( count * sizeof(T) );
		}

		memory.size = count;
	}

	void Ensure( int newcount )
	{
		int oldcount = memory.size;

		if ( newcount <= oldcount )
			return;

		if ( oldcount != 0 )
		{
			oldcount = ALIGN( oldcount, 4 );

			while ( ( oldcount <<= 1 ) < newcount )
			{
				// overflow
				if ( oldcount < 0 )
				{
					oldcount = 0x7fffffff;
					AssertMsg1( 0, "cannot allocate %d elements", newcount );
				}
			}

			newcount = oldcount;
		}
		else
		{
			if ( newcount < 4 )
				newcount = 4;
		}

		Alloc( newcount );
	}
};

template < typename T, class CAllocator = CDefaultAllocator< T > >
class vector
{
public:
	CAllocator _base;
	int _size;

	vector() : _size(0)
	{
	}

	vector( int count ) : _size(0)
	{
		_base.Alloc( count );
	}

	vector( const vector< T > &src )
	{
		_base.Alloc( src._base.Size() );
		_size = src._size;

		for ( int i = 0; i < _size; i++ )
			new( _base.Base() + i ) T( src._base[i] );
	}

	~vector()
	{
		Assert( _size <= _base.Size() );

		for ( int i = 0; i < _size; i++ )
			_base[i].~T();

		_base.Free();
	}

	T &operator[]( int i ) const
	{
		Assert( _size > 0 );
		Assert( i >= 0 && i < _size );
		return _base[i];
	}

	int size() const
	{
		return _size;
	}

	int capacity() const
	{
		return _base.Size();
	}

	T &top() const
	{
		Assert( _size > 0 );
		return _base[ _size - 1 ];
	}

	void pop()
	{
		Assert( _size > 0 );
		_base[ --_size ].~T();
	}

	T &append()
	{
		_base.Ensure( ++_size );
		return *( new( _base.Base() + _size - 1 ) T() );
	}

	void append( const T &src )
	{
		_base.Ensure( ++_size );
		new( _base.Base() + _size - 1 ) T( src );
	}

	void remove( int i )
	{
		Assert( _size > 0 );
		Assert( i >= 0 && i < _size );

		_base[i].~T();

		if ( i != _size - 1 )
			memmove( (char*)( _base.Base() + i ), _base.Base() + i + 1, ( _size - i - 1 ) * sizeof(T) );

		_size--;
	}

	void clear()
	{
		for ( int i = 0; i < _size; i++ )
			_base[i].~T();

		_size = 0;
	}

	void sort( int (*fn)(const T *, const T *) )
	{
		qsort( _base.Base(), _size, sizeof(T), (int (*)(const void *, const void *))fn );
	}

	void reserve( int count )
	{
		Assert( count >= 0 );
		Assert( _size <= _base.Size() );

		if ( count == 0 )
			count = 4;

		if ( count == _base.Size() )
			return;

		for ( int i = count; i < _size; i++ )
			_base[i].~T();

		_base.Alloc( count );
	}

	void purge()
	{
		Assert( _size <= _base.Size() );

		for ( int i = 0; i < _size; i++ )
			_base[i].~T();

		_base.Free();
		_size = 0;
	}
};

#endif // SQDBG_VEC_H
