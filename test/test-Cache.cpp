#include "Cache/Cache.h"

#include "gtest/gtest.h"

#include <string>

class TestCache : public Cache<std::string, int>
{
    static size_t constexpr MAX_ENTRIES = 100;

    virtual bool full(key_type const & key) const override
    {
        return container().size() >= MAX_ENTRIES;
    };

    virtual value_type read( key_type const & key ) const override
    {
        return 0; // always 0 for now
    }

    virtual void write( key_type const & key, value_type const & value ) const override
    {
        // do nothing for now
    }

    virtual typename Container::iterator condemn( key_type const & hint ) override
    {
        // FIFO
        Container & c = container();
        for (Container::iterator p = c.begin(); p != c.end(); ++p)
        {
            if (!p->second.locked)
                return p;
        }
        // All locked, just return the first
        return c.begin();
    }
#if defined(FEATURE_ASYNCHRONOUS)
    virtual void readAsync( key_type const & key, value_type * pValue ) const override
    {
        // no waiting for now
        *pValue = 1;
    }

    virtual void waitForAsyncRead( key_type const & ) const override
    {
        // no waiting for now
    }
#endif // defined(FEATURE_ASYNCHRONOUS)
};

TEST(CacheTest, Placeholder)
{
    TestCache cache;
    EXPECT_TRUE(false);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int rv = RUN_ALL_TESTS();
    return rv;
}
