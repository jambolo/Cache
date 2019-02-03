#pragma once

#if !defined(CACHE_CACHE_H)
#define CACHE_CACHE_H

#include <map>

//! An entry in the generic cache.
//!
//! This is used internally by the cache to store the data. It should not be used directly.
//!
//! @internal

template <typename T>
struct CacheEntry
{
    T    value;         // Value
    bool locked;        // True if the entry is locked in the cache
    bool dirty;         // True if the entry's value has been updated.
#if defined(FEATURE_ASYNCHRONOUS)
    bool prefetched;    // True if the entry is being prefetched
#endif
};

//! A generic cached proxy.
//!
//! This generic cache is a proxy for some source of data. The cache provides a method for accessing
//! the values in the source using a key whose form is completely determined by the user. The cache is a
//! write-back cache and uses fully associative mapping. 
//!
//! The cache also provides support methods: a method to flush a value, a method to prevent an value from being
//! flushed, and a method that pre-fetches an value from the source. Pre-fetch is asynchronous (in contrast
//! the normal accessor, which blocks until the value is in the cache).
//!
//! @param  Key         Type of the key used to reference a value in the cache.
//! @param  Value,           Type of value contained in the cache
//! @param  Compare     Used to sort the values in the cache. Default: std::less<Key>
//! @param  Allocator   Used by the cache's container to allocate container nodes.
//!                     Default: std::allocator<std::pair<Key, CacheEntry<Value>>>
//!
//! @note   This class is an abstract base class. The following methods must be overridden:
//!             - read
//!             - write
//!             - full
//!             - condemn
//!             - readAsync
//!             - waitForAsyncRead

template< typename Key,
          typename Value,
          typename Compare   = std::less<Key>,
          typename Allocator = std::allocator<std::pair<Key const, CacheEntry<Value>>> >
class Cache
{
public:

    using key_type = Key;
    using value_type = Value;
    using compare_type = Compare;
    using allocator_type = Allocator;

    using Entry = CacheEntry<value_type>;

    //! Constructor.
    Cache() = default;

    //! Destructor.
    virtual ~Cache()
    {
        clear();
    }

    //! Accesses a value in the cache.
    //!
    //! If the value is not in the cache, it is loaded into the cache first.
    //!
    //! @param    key        The value to access
    //!
    //! @note    If the value does not exist, it is created.
    value_type & operator[]( key_type const & key )
    {
        Entry & entry = get( key );
        return entry.value;
    }

    //! Accesses a value in the cache.
    //!
    //! If the value is not in the cache, it is loaded into the cache first.
    //!
    //! @param    key        The value to access
    //!
    //! @note    If the value does not exist, it is created.
    value_type operator[]( key_type const & key ) const
    {
        Entry & entry = get( key );
        return entry.value;
    }

    //! Returns true if the cache is empty.
    bool empty() const
    {
        return container_.empty();
    }

    //! Returns the number of values in the cache.
    int count() const
    {
        return (int)container_.size();
    }

    //! Flushes a value in the cache.
    //!
    //! This function writes out a value to the source if it has been marked as dirty.
    //!
    //! @param    key   Which value to flush
    //!
    void flush( key_type const & key )
    {
        typename Container::iterator p = container_.find( key ); // Point to the value in the cache
        if (p != container_.end())
            flush(p);
    }

    //! Flushes the entire cache.
    //!
    //! This function writes back all values to the source that have been marked as dirty.
    void flush()
    {
        for (typename Container::iterator p = container_.begin(); p != container_.end(); ++p)
        {
            flush(p);
        }
    }

    //! Locks a value in the cache.
    //!
    //! This function prevents an value from being removed from the cache before any unlocked values. It does
    //! not guarantee that the value will never be removed from the cache.
    //!
    //! @param  key         The key of the value to mark
    //! @param  locked      The new lock setting. Default: true
    //!
    //! @note    If the value is not in the cache, it is loaded into the cache first.
    void lock( key_type const & key, bool locked = true )
    {
        Entry &    entry    = get( key );
        entry.locked = locked;
    }

    //! Marks a value as dirty.
    //!
    //! A dirty value is eventually written back to the source.
    //!
    //! @param  key     The key of the value to mark
    //!
    //! @note    If the value is not in the cache, it is loaded into the cache first.
    void dirty( key_type const & key )
    {
        Entry & entry = get( key );
        entry.dirty = true;
    }

    //! Marks a value as invalid.
    //!
    //! An invalid value is out-of-date w.r.t. the source.
    //!
    //! @param  key     The key of the value to mark
    //!
    //! @warning    Invalidating a value marked as dirty results in a loss
    void invalidate( key_type const & key )
    {
        // Instead of being marked, the value is just removed from the cache, forcing it to be loaded from the source
        // the next time it is accessed.

        typename Container::iterator const    p    = container_.find( key );
        if ( p != container_.end() )
        {
            assert(!p->second.dirty); // Invalidating dirty data results in a loss
            container_.erase(p);
        }
    }

    //! Removes a value from the cache.
    //!
    //! If the value is marked as dirty, it is written back to the source first. If the value is not in the cache,
    //! nothing happens.
    //!
    //! @param    key        The value to remove
    void purge( key_type const & key )
    {
        typename Container::iterator const    p    = container_.find( key );
        if ( p != container_.end() )
        {
            purge( p );
        }
    }

    //! Purges all values in the cache.
    //!
    //! Any values that are marked as dirty are written back to the source first.
    //!
    void clear()
    {
        for ( typename Container::iterator p = container_.begin(); p != container_.end(); ++p )
        {
            purge( p );
        }
    }

#if defined(FEATURE_ASYNCHRONOUS)
    //! Prefetches a value.
    //!
    //! This function loads a value into the cache asynchronously. Normally, if a value is not in the cache,
    //! it is loaded when it is accessed, and the accessor waits until the value is loaded before continuing.
    //!
    //! @param    key        The value to prefetch
    void prefetch( key_type const & key )
    {
        typename Container::iterator p = container_.find( key ); // Point to the value in the cache

        // If the value or its stand-in is not already in the cache, then get it from the source
        if ( p == container_.end() )
        {
            p = insert( key, value_type(), true );
            asyncRead( key, &p->second.value );
        }
    }
#endif // defined(FEATURE_ASYNCHRONOUS)

protected:

    typedef std::map< key_type, Entry, compare_type, allocator_type >        Container;

    //! Returns the cache's underlying container.
    Container & container() { return container_; }

    //! Returns the cache's underlying container.
    Container const & container() const { return container_; }

    //! Returns true if the cache has enough room to hold the value.
    //!
    //! @param  key     The value being added to the cache
    //!
    //! @return     true if the cache is full
    //!
    //! @note    This function must be overridden.
    virtual bool full( key_type const & key ) const = 0;

    //! Returns a value from the source.
    //!
    //! @param  key     Which value to retrieve
    //!
    //! @return     Value retrieved from the source
    //!
    //! @note    This function must be overridden.
    //! @note    If the value doesn't exist in the source, it must be created.
    virtual value_type read( key_type const & key ) const = 0;

    //! Writes a value to the source.
    //!
    //! @param  key     Which value to write
    //! @param  value   Value to write
    //!
    //! @note    This function must be overridden
    virtual void write( key_type const & key, value_type const & value ) const = 0;

    //! Returns an iterator pointing to the entry in the cache with the lowest priority.
    //!
    //! The cache uses this function to determine which entry to overwrite when a value is being inserted and
    //! the cache is full. Note that the priority of any entry whose locked member is true is higher than every
    //! entry whose locked member is false.
    //!
    //! @param  hint    Key of the value being inserted into the cache. The function can use this value to help
    //!                 determine which value to overwrite.
    //!
    //! @note    This function must be overridden
    virtual typename Container::iterator condemn( key_type const & hint ) = 0;

#if defined(FEATURE_ASYNCHRONOUS)
    //! Retrieves a value from the source, but returns immediately, potentially before the value is in the cache.
    //!
    //! @param  key         Value being read into the cache
    //! @param  pValue      Where to put the value
    //!
    //! @note    This function must be overridden
    virtual void readAsync( key_type const & key, value_type * pValue ) const = 0;

    //! Waits for a value to be retrieved from the source via a previous readAsync call.
    //!
    //! @param  key         Value being retrieved
    //! @param  pValue      Where to put the value
    //!
    //! @note    This function must be overridden
    virtual void waitForAsyncRead( key_type const & key ) const = 0;
#endif // defined(FEATURE_ASYNCHRONOUS)

private:

    // Retrieves a value from the cache, loading it from the source if necessary.
    Entry & get( key_type const & key )
    {
        typename Container::iterator p = container_.find( key );

        // If the value is not in the cache, then it must be loaded now.
        if ( p == container_.end())
        {
            p = insert(key, read(key));
        }
#if defined(FEATURE_ASYNCHRONOUS)
        else
        {
            synchronize( key );
        }
#endif // defined(FEATURE_ASYNCHRONOUS)

        return p->second;
    }

    // Removes an entry after writing back to the source if dirty
    void purge( typename Container::iterator const & p )
    {
        // Make sure the source has been updated
        flush( p );

        // Remove it
        container_.erase( p );
    }

    // Writes an entry back to the source if it is marked as dirty
    void flush( typename Container::iterator const & p )
    {
        key_type const & key   = p->first;
        Entry &          entry = p->second;

        // If the entry is dirty, write it out and mark it as not dirty.
        if ( entry.dirty )
        {
            write( key, entry.value );
            entry.dirty = false;
        }
    }

    // Inserts a new value into the cache. Returns the container iterator to the inserted entry.
    typename Container::iterator insert( key_type const & key, value_type const & value) const
    {
        // Make sure there is enough room in the cache for the new value
        if ( full( key ) )
        {
            purge( condemn( key ) );
        }

        // Load the value into the cache
        Entry entry{ value, false, false };

        return container_.insert({ key, entry }).first;
    }

#if defined(FEATURE_ASYNCHRONOUS)
    // Inserts a new value into the cache. Returns the container iterator to the inserted entry.
    typename Container::iterator insert( key_type const & key, value_type const & value, bool prefetched ) const
    {
        // Make sure there is enough room in the cache for the new value
        if ( full( key ) )
        {
            purge( condemn( key ) );
        }

        // Load the value into the cache
        Entry entry{ false, false, prefetched, value };

        return container_.insert({ key, entry }).first;
    }

    // Synchronizes an entry that is dirty or is being prefetched
    void synchronize( key_type const & key )
    {
        Entry & entry = get(key);
        assert(!entry.dirty || !entry.prefetched); // That would be bad

        if ( entry.prefetched )
        {
            waitForAsyncRead( key );
            entry.prefetched = false;
        }
        else if (entry.dirty)
        {
            write( key, entry.value );
            entry.dirty = false;
        }
    }
#endif // defined(FEATURE_ASYNCHRONOUS)

    mutable Container    container_;    // The underlying container
};

#endif // !defined(CACHE_CACHE_H)
