#pragma once

#include <vector>

template < typename Key, typename Value, typename Handle >
class SynchronousCache
{
private:

	struct Entry
	{
		Key		key;
		int		age;
		Handle	handle;
		Value	value;

		Entry() {}
		Entry( Key const & k, Value const & v ) : key( k ), value( v ), age( 0 ) {}
	};

	typedef std::vector< Entry >		EntryVector;

public:

	typedef	Key		key_type;
	typedef	Value	value_type;
	typedef Handle	handle_type;

	SynchronousCache()
	{
		m_entries.reserve( NUMBER_OF_ENTRIES );
	}

	virtual ~SynchronousCache()
	{
		for ( EntryVector::iterator it = m_entries.begin(); it != m_entries.end(); ++it )
		{
			Flush( it->handle, it->value );
		}
	}

	Value & operator()( Key const & key )
	{
		// Bump the ages

		for ( EntryVector::iterator it = m_entries.begin(); it != m_entries.end(); ++it )
		{
			++it->age;
		}

		// Find the desired entry
		// If the entry is not in the cache, then load it into an empty entry if the cache is
		// not full yet or into the oldest entry if it is.

		EntryVector::iterator pEntry = Find( key );

		if ( pEntry == m_entries.end() )
		{
			if ( m_entries.size() < NUMBER_OF_ENTRIES )
			{
				Entry	entry;

				entry.key = key;
				entry.handle = Fetch( key, &entry.value );
				m_entries.push_back( entry );
				pEntry = m_entries.end()-1;
			}
			else
			{
				// Find the oldest

				EntryVector::iterator pOldest = m_entries.begin();
				for ( EntryVector::iterator it = m_entries.begin(); it != m_entries.end(); ++it )
				{
					if ( it->age > pOldest->age )
					{
						pOldest = it;
					}
				}

				// Flush the previous contents

				Flush( pOldest->handle, pOldest->value );

				// Load the new contents

				pOldest->key = key;
				pOldest->handle = Fetch( key, &pOldest->value );

				pEntry = pOldest;
			}
		}
		
		// Reset the age

		pEntry->age = 0;

		// Return the cached value

		return pEntry->value;
	}

	void Lock( Key const & key )
	{
		EntryVector::iterator pEntry = Find( key );
		if ( pEntry != m_entries.end() )
		{
			pEntry->age += 0x80000000;
		}
	}

	void Unlock( Key const & key )
	{
		EntryVector::iterator pEntry = Find( key );
		if ( pEntry != m_entries.end() )
		{
			pEntry->age = 0;
		}
	}


private:

	static int const	NUMBER_OF_ENTRIES	= 8;

	virtual Handle Fetch( Key const & key, Value * pValue ) const	= 0;
	virtual void Flush( Handle const & handle, Value const & value ) const {};

	typename EntryVector::iterator Find( Key const & key )
	{
		EntryVector::iterator pEntry = m_entries.begin();
		while ( pEntry != m_entries.end() && pEntry->key != key )
		{
			++pEntry;
		}

		return pEntry;
	}

	EntryVector	m_entries;
};
