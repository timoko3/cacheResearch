#include "cache/cache2Q.h"
#include "cache_tests_tools.h"

namespace tests {

// The base cache loader contract returns a reference to a live backing page.
inline void lookupUpdateTest(cache::Cache2Q<uint32_t, int>& c,
                             const std::vector<int>& requests, std::string_view expected) {
    ASSERT_EQ(requests.size(), expected.size());
    uint32_t loadedPage = 0;
    for (std::size_t index = 0; index < requests.size(); ++index) {
        bool loaderCalled = false;
        auto slow = [&](int key) -> uint32_t& {
            EXPECT_EQ(key, requests[index]);
            loaderCalled = true;
            loadedPage = hashInt(key);
            return loadedPage;
        };
        EXPECT_EQ(c.lookupUpdate(requests[index], slow), hashInt(requests[index]));
        EXPECT_EQ(loaderCalled, expected[index] == 'M') << "request index = " << index;
    }
    checkCacheStats(c, requests, expected);
}

TEST(Cache2QTrace, RepeatedOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(Cache2QTrace, AlternatingTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMMHHH");
}

TEST(Cache2QTrace, CycleThree) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMMMH");
}

TEST(Cache2QTrace, CycleFour) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMMMMM");
}

TEST(Cache2QTrace, HotOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMMMHMH");
}

TEST(Cache2QTrace, MixedA) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMMMMMHH");
}

TEST(Cache2QTrace, ScanFive) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMMMMMMMMM");
}

TEST(Cache2QTrace, MixedB) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMMMMHHMH");
}

TEST(Cache2QTrace, HotTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMMHMMHMMM");
}

TEST(Cache2QTrace, ReuseAfterScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMMMMMMMMM");
}

TEST(Cache2QTrace, TwoHotThenScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMHMMHMMHHMMH");
}

TEST(Cache2QTrace, HotTwoLong) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMMMMHMHHMMH");
}

TEST(Cache2QFocused, MetadataAndListsStartEmpty) {
    cache::Cache2Q<int, int> c(4, cache::cacheLevel_t::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::cacheLevel_t::L2);
    EXPECT_TRUE(c.getA1in().empty());
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());
    EXPECT_TRUE(c.getHash().empty());
}

TEST(Cache2QFocused, CapacityBelowTwoIsRejected) {
    EXPECT_THROW((cache::Cache2Q<int, int>(0)), std::invalid_argument);
    EXPECT_THROW((cache::Cache2Q<int, int>(1)), std::invalid_argument);
}

TEST(Cache2QFocused, FirstInsertGoesToA1in) {
    cache::Cache2Q<int, int> c(4);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key * 10;
        return loadedPage;
    };

    EXPECT_EQ(c.lookupUpdate(5, slow), 50);
    ASSERT_EQ(c.getA1in().size(), 1u);
    EXPECT_EQ(c.getA1in().front().key, 5);
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());
}

TEST(Cache2QFocused, OverflowOfA1inCreatesGhostEntry) {
    cache::Cache2Q<int, int> c(4); // KIn = 1.
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);

    ASSERT_EQ(c.getA1in().size(), 1u);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1in().front().key, 2);
    EXPECT_EQ(c.getA1out().front().key, 1);
}

TEST(Cache2QFocused, GhostHitIsMissAndPromotesPageToAm) {
    cache::Cache2Q<int, int> c(4);
    int calls = 0;
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        ++calls;
        loadedPage = key * 10;
        return loadedPage;
    };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    ASSERT_EQ(calls, 2);

    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    EXPECT_EQ(calls, 3);
    ASSERT_FALSE(c.getAm().empty());
    EXPECT_EQ(c.getAm().front().key, 1);

    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    EXPECT_EQ(calls, 3);
}

TEST(Cache2QFocused, AmHitMovesEntryToFront) {
    cache::Cache2Q<int, int> c(8);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    c.lookupUpdate(3, slow);
    c.lookupUpdate(1, slow);
    c.lookupUpdate(4, slow);
    c.lookupUpdate(2, slow);

    ASSERT_GE(c.getAm().size(), 2u);
    EXPECT_EQ(c.getAm().front().key, 2);

    c.lookupUpdate(1, slow);
    EXPECT_EQ(c.getAm().front().key, 1);
}

TEST(Cache2QFocused, A1outNeverExceedsConfiguredGhostLimit) {
    cache::Cache2Q<int, int> c(4);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };

    for (int key = 1; key <= 20; ++key) {
        c.lookupUpdate(key, slow);
        EXPECT_LE(c.getA1out().size(), 2u);
    }
}

TEST(Cache2QFocused, ImmediateA1inReuseIsAHit) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {9, 9, 9, 10, 10}, "MHHMH");
}

TEST(Cache2Q, ReloadedPageHasNewValue) {
    cache::Cache2Q<int, int> c(2);
    int loads = 0;
    int loadedPage = 0;
    auto slow = [&](int) -> int& {
        loadedPage = ++loads;
        return loadedPage;
    };

    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(loads, 3);
}

TEST(Cache2Q, RetryFailedLoad) {
    cache::Cache2Q<int, int> c(3);
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int& { throw std::runtime_error("load failed"); }),
                 std::runtime_error);

    int loads = 0;
    int loadedPage = 420;
    auto slow = [&](int) -> int& {
        ++loads;
        return loadedPage;
    };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(Cache2Q, FailedGhostLoadDoesNotAffectNextRequest) {
    cache::Cache2Q<int, int> c(4);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key * 10;
        return loadedPage;
    };
    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);

    EXPECT_THROW(c.lookupUpdate(1, [](int) -> int& {
        throw std::runtime_error("ghost load failed");
    }), std::runtime_error);

    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front().key, 1);
    EXPECT_FALSE(c.getA1out().front().page.has_value());
    EXPECT_TRUE(c.getAm().empty());

    EXPECT_EQ(c.lookupUpdate(3, slow), 30);
    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    ASSERT_EQ(c.getAm().size(), 1u);
    EXPECT_EQ(c.getAm().front().key, 1);
    EXPECT_EQ(c.getHash().size(),
              c.getA1in().size() + c.getA1out().size() + c.getAm().size());
}

TEST(Cache2Q, VectorPage) {
    cache::Cache2Q<std::vector<int>, int> c(3);
    int loads = 0;
    std::vector<int> loadedPage;
    auto slow = [&](int key) -> std::vector<int>& {
        ++loads;
        loadedPage = {key, key + 1};
        return loadedPage;
    };

    auto page = c.lookupUpdate(7, slow);
    ASSERT_EQ(page.size(), 2u);
    page[0] = -1;
    page[1] = -2;
    EXPECT_EQ(page, (std::vector<int>{-1, -2}));
    EXPECT_EQ(c.lookupUpdate(7, slow), (std::vector<int>{7, 8}));
    EXPECT_EQ(loads, 1);
}

TEST(Cache2Q, StringKeys) {
    cache::Cache2Q<int, std::string> c(3);
    int loads = 0;
    int loadedPage = 0;
    auto slow = [&](const std::string& key) -> int& {
        ++loads;
        loadedPage = static_cast<int>(key.size());
        return loadedPage;
    };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

namespace {
struct CopyCheckedPage {
    int value = 0;
    bool failCopy = false;

    CopyCheckedPage() = default;
    CopyCheckedPage(const CopyCheckedPage& source)
        : value(source.value), failCopy(source.failCopy) {
        if (failCopy) {
            throw std::runtime_error("page copy failed");
        }
    }
};
} // namespace

TEST(Cache2Q, FailedNewPageCopyPreservesResidentPage) {
    cache::Cache2Q<CopyCheckedPage, int> c(4);
    CopyCheckedPage loadedPage;
    auto slow = [&](int key) -> CopyCheckedPage& {
        loadedPage.value = key;
        return loadedPage;
    };
    c.lookupUpdate(1, slow);

    loadedPage.failCopy = true;
    EXPECT_THROW(c.lookupUpdate(2, slow), std::runtime_error);

    ASSERT_EQ(c.getA1in().size(), 1u);
    EXPECT_EQ(c.getA1in().front().key, 1);
    EXPECT_EQ(c.getA1in().front().page->value, 1);
    EXPECT_EQ(c.getHash().size(), 1u);
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());

    loadedPage.failCopy = false;
    EXPECT_EQ(c.lookupUpdate(2, slow).value, 2);
}

TEST(Cache2Q, FailedGhostPageCopyPreservesHotPageAndGhost) {
    cache::Cache2Q<CopyCheckedPage, int> c(2);
    CopyCheckedPage loadedPage;
    auto slow = [&](int key) -> CopyCheckedPage& {
        loadedPage.value = key;
        return loadedPage;
    };
    for (int key : {1, 2, 1, 3}) {
        c.lookupUpdate(key, slow);
    }

    loadedPage.failCopy = true;
    EXPECT_THROW(c.lookupUpdate(2, slow), std::runtime_error);

    ASSERT_EQ(c.getAm().size(), 1u);
    EXPECT_EQ(c.getAm().front().key, 1);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front().key, 2);
    EXPECT_FALSE(c.getA1out().front().page.has_value());
    EXPECT_EQ(c.getHash().size(), 3u);

    loadedPage.failCopy = false;
    EXPECT_EQ(c.lookupUpdate(2, slow).value, 2);
    EXPECT_EQ(c.getAm().front().key, 2);
}

} // namespace tests
