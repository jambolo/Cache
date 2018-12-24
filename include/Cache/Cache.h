/** @file *//********************************************************************************************************

                                                       Cache.h

						                    Copyright 2004, John J. Bolton
	--------------------------------------------------------------------------------------------------------------

	$Header: //depot/Libraries/Cache/Cache.h#9 $

	$NoKeywords: $

 ********************************************************************************************************************/

#pragma once

#include <map>
#include <algorithm>
#include <boost/noncopyable.hpp>

//! A generic cache element
//
//! This struct is used by the cache to store the data. It should not be used directly.
//!
//! @internal

template < typename _Ty >
struct _Cache_element
{
	typedef _Ty		Value;

	bool	locked;		// True if the element is locked in the cache
	bool	dirty;		// True if the element is not synchronized.
	bool	prefetched;	// True if the element is being prefetched
	Value	value;		// Element value

	_Cache_element() {}
	_Cache_element( bool locked_, bool dirty_, bool prefetched_, Value const & value_ )
		: locked( locked_ ),
		dirty( dirty_ ),
		prefetched( prefetched_ ),
		value( value_ )
	{
	}
};

//! A generic cache proxy
//
//! This generic cache is a proxy for some sort of dataset. The cache provides a method for accessing
//! the values in the dataset using a key whose form is completely determined by the user. The cache is a
//! write-back cache and uses fully associative mapping. 
//!
//! The cache also provides support methods: a method to flush a value, a method to prevent an value from being
//! flushed, and a method that pre-fetches an value from the dataset. Pre-fetch is asynchronous (in contrast
//! the normal accessor, which blocks until the value is in the cache).
//!
//!	@param	_Kty		Type of the key used to reference a value in the cache.
//! @param	_Ty			Type of value contained in the cache
//! @param	_Pr			Functor used to sort the values in the cache. Default: std::less< _Kty >
//! @param	_Alloc		Functor used by the cache's container to allocate container nodes.
//!							Default: std::allocator< std::pair< _Kty, _Cache_element< _Ty > > >
//!
//! @note		This class is an abstract base class. The following methods must be overridden:
//!					- Cache<>::Read
//!					- Cache<>::Write
//!					- Cache<>::IsFull
//!					- Cache<>::Condemn
//!					- Cache<>::AsyncRead
//!					- Cache<>::WaitForAsyncRead

template< typename _Kty,
		  typename _Ty,
		  typename _Pr			= std::less< _Kty >,
		  typename _Alloc		= std::allocator< std::pair< _Kty const, _Cache_element< _Ty > > > >
class Cache
		: public boost::noncopyable
{
public:

	typedef _Kty					Key;
	typedef _Ty						Value;
	typedef _Pr						Comparator;
	typedef _Alloc					Allocator;
	typedef _Cache_element< _Ty >	Element;

	//! Destructor
	virtual	~Cache()
	{
		Clear();
	}

	//! Returns @c true if the cache is empty
	//
	//!
	//! @return		@c true if the cache is empty

	bool IsEmpty() const
	{
		return m_Container.empty();
	}

	//! Returns the number of values in the cache
	//
	//!
	//! @return		number of values in the cache

	int Count() const
	{
		return (int)m_Container.size();
	}
	
	//! Flushes a value in the cache
	//
	//! This function writes out a value if it has been changed. The value remains in the cache after being
	//! flushed.
	//!
	//! @param	key		The value to flush
	//!
	//! @note	If the value is not in the cache, it is loaded into the cache first.
	//! @note	If the value does not exist, it is created.

	void Flush( Key const & key )
	{
		Element &	element	= Get( key );

		// If the element is dirty, write it out and mark it as not dirty.
		
		if ( element.dirty )
		{
			Write( key, element.value );
			element.dirty = false;
		}
	}

	//! Locks a value in the cache
	//
	//! This function prevents an value from being discarded from the cache before any unlocked values. It does
	//! not guarantee that the value will never be discarded from the cache.
	//!
	//! @param	key		The value to lock
	//! @param	lock	The new lock setting. Default: true
	//!
	//! @note	If the value is not in the cache, it is loaded into the cache first.
	//! @note	If the value does not exist, it is created.

	void Lock( Key const & key, bool lock = true )
	{
		Element &	element	= Get( key );

		element.locked = lock;
	}

	//! Prefetches a value
	//
	//! This function loads an value into the cache asynchronously. Normally, if a value is not in the cache,
	//! it is loaded when it is accessed, and the accessor waits until the value is loaded before continuing.
	//!
	//! @param	key		The value to prefetch

	void Prefetch( Key const & key )
	{
		Container::iterator	it	= m_Container.find( key );		// Point to the value in the cache

		// If the value or its stand-in is not already in the cache, then get it from the dataset

		if ( it == m_Container.end() )
		{
			it = Insert( key, Value(), true );
			AsyncRead( key, &it->second.value );
		}
	}

	//! Marks an value as dirty
	//
	//! A dirty value is written back to the dataset when it is discarded from the cache.
	//!
	//! @param	key		The value to mark
	//!
	//! @note	If the value does not exist, it is created.

	void Invalidate( Key const & key )
	{
		Element &	element	= Get( key );

		element.dirty = true;
	}

	//! Accesses a value in the dataset through the cache
	//
	//! If the value is not in the cache, it is loaded into the cache first.
	//!
	//! @param	key		The value to access
	//!
	//! @note	If the value does not exist, it is created.

	Value & operator[]( Key const & key )
	{
		Element &	element	= Get( key );

		return element.value;
	}

	//! Discards a value from the cache
	//
	//! If the value is "dirty", it is written back first. If the value is not in the cache, nothing happens.
	//!
	//! @param	key		The value to discard

	void Discard( Key const & key )
	{
		Container::iterator const	it	= m_Container.find( key );

		if ( it != m_Container.end() )
		{
			Discard( it );
		}
	}

	//! Discards all values in the cache
	//
	//! Any values that are "dirty" are written back to the dataset first.
	//!
	
	void Clear()
	{
		for ( Container::iterator it = m_Container.begin(); it != m_Container.end(); ++it )
		{
            Discard( it );
		}
	}

protected:

	typedef std::map< Key, Element, Comparator, Allocator >		Container;

	//! Returns the cache's underlying container
	Container & GetContainer() { return m_Container; }

	//! Returns the cache's underlying container
	Container const & GetContainer() const { return m_Container; }

	//! Returns @c true if the cache has enough room to hold the value
	//
	//! @param	key		The value being added to the cache
	//!
	//! @return		@c true if the cache is full
	//!
	//! @note	This function must be overridden.

	virtual bool IsFull( Key const & key ) const = 0;

	//! Returns a value from the dataset
	//
	//! @param	key		Which value to retrieve
	//!
	//! @return		Value retrieved from the dataset
	//!
	//! @note	This function must be overridden.
	//! @note	If the value doesn't exist in the dataset, it is created.

	virtual Value & Read( Key const & key ) const = 0;

	//! Writes a value to the dataset
	//
	//! @param	key		Which value to write
	//! @param	value	Value to write
	//!
	//! @note	This function must be overridden

	virtual void Write( Key const & key, Value const & value ) const = 0;

	//! Returns an iterator pointing to the value in the cache with the lowest priority
	//
	//! @param	key		Value being inserted into the cache. The function uses this value to help
	//!					determine which value to choose.
	//!
	//! @note	This function must be overridden

	virtual typename Container::iterator Condemn( Key const & key ) = 0;

	//! Retrieves a value from the dataset, but returns immediately, before the value is in the cache
	//
	//! @param	key			Value being read into the cache
	//! @param	pElement	Where to put the value
	//!
	//! @note	This function must be overridden

	virtual void AsyncRead( Key const & key, Value * pValue ) const = 0;

	//! Waits for a value to be retrieved a from the dataset.
	//
	//! @param	key			Value being retrieved
	//! @param	pElement	Where to put the value
	//!
	//! @note	This function must be overridden

	virtual void WaitForAsyncRead( Key const & key, Value * pValue ) const = 0;

private:

	// Retrieves a value from the cache, loading it from the dataset if necessary.

	Element & Get( Key const & key )
	{
		// Point to the value in the cache

		typename Container::iterator	it			= m_Container.find( key );
		Element &						element		= it->second;

		// If the value is not in the cache, then it must be loaded now.
		// Otherwise, if it was prefetched, wait for it.

		if ( it == m_Container.end() )
		{
			Insert( key, Read( key ), false );
		}
		else
		{
			Synchronize( key, element );
		}

		return element;
	}

	// Inserts a new element into the cache. Returns the container iterator to the inserted element.

	typename Container::iterator Insert( Key const & key, Value const & value, bool prefetched )
	{
		// Make sure there is enough room in the cache for the new value

		if ( IsFull( key ) )
		{
			Discard( Condemn( key ) );
		}

		// Load the value into the cache

		Element const								e( false, false, prefetched, value );
		typename Container::value_type const		cvt( key, e );

		return m_Container.insert( cvt ).first;
	}

	// Discards an element

	void Discard( typename Container::iterator const & it )
	{
		Key const &		key			= it->first;
		Element &		element		= it->second;

		// If the element is being prefetched, then wait for it to be finished before erasing it.

		Synchronize( key, element );

		// Make sure the element is no longer dirty

		Flush( it );

		// Remove it

		m_Container.erase( it );
	}

	// Synchronizes an element being prefetched

	void Synchronize( Key const & key, Element & element )
	{
		if ( element.prefetched )
		{
			WaitForAsyncRead( key, &element.value );
			element.dirty		= false;
			element.prefetched	= false;
		}
	}

	// Flushes an element

	void Flush( typename Container::iterator const & it )
	{
		Key const &		key			= it->first;
		Element &		element		= it->second;

		// If the element is dirty, write it out and mark it as not dirty.
		
		if ( element.dirty )
		{
			Write( key, element.value );
			element.dirty = false;
		}
	}

	Container	m_Container;	// The underlying container
};
