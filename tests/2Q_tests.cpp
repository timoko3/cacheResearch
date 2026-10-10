#include <random>
#include <type_traits>

#include "cache/cache2Q.h"
#include "cache_tests_tools.h"

struct HashCheckedKey {
    int value;
    inline static int failingValue = -1;
    bool operator==(const HashCheckedKey& other) const noexcept { return value == other.value; }
};

namespace std {
template <>
struct hash<HashCheckedKey> {
    size_t operator()(const HashCheckedKey& key) const {
        if (key.value == HashCheckedKey::failingValue) {
            throw runtime_error("key hashing failed");
        }
        return hash<int>{}(key.value);
    }
};
} // namespace std

namespace tests {

// The base cache loader contract returns a reference to a live backing page.
TEST(Cache2QTrace, RepeatedOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(Cache2QTrace, AlternatingTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMHHHH");
}

TEST(Cache2QTrace, CycleThree) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMHHH");
}

TEST(Cache2QTrace, CycleFour) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMHHHH");
}

TEST(Cache2QTrace, HotOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHHH");
}

TEST(Cache2QTrace, MixedA) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMHMHHHH");
}

TEST(Cache2QTrace, ScanFive) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMMMMMH");
}

TEST(Cache2QTrace, MixedB) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMHHMHHHH");
}

TEST(Cache2QTrace, HotTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMMMMMH");
}

TEST(Cache2QTrace, ReuseAfterScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMHMMMMMHH");
}

TEST(Cache2QTrace, TwoHotThenScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMHHMMHHMMMMMH");
}

TEST(Cache2QTrace, HotTwoLong) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMHHMHMHMHHH");
}

TEST(Cache2QFocused, MetadataAndListsStartEmpty) {
    cache::Cache2Q<int, int> c(4, cache::CacheLevel::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::CacheLevel::L2);
    EXPECT_TRUE(c.getA1in().empty());
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());
    EXPECT_EQ(c.getIndexedCount(), 0u);
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
    cache::Cache2Q<int> c(4);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate(key, slow);
    EXPECT_EQ(c.getResidentCount(), 4u);
    EXPECT_TRUE(c.getA1out().empty());

    c.lookupUpdate(5, slow);
    EXPECT_EQ(c.getResidentCount(), 4u);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front(), 1);
}

TEST(Cache2QFocused, GhostHitIsMissAndPromotesPageToAm) {
    cache::Cache2Q<int> c(4);
    int calls = 0;
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        ++calls;
        loadedPage = key * 10;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4, 5})
        c.lookupUpdate(key, slow);

    const int& restored = c.lookupUpdate(1, slow);
    EXPECT_EQ(restored, 10);
    EXPECT_NE(&restored, &loadedPage);
    EXPECT_EQ(calls, 6);
    ASSERT_EQ(c.getAm().size(), 1u);
    EXPECT_EQ(&restored, &c.getAm().front().page);
    EXPECT_EQ(c.getAm().front().key, 1);
    EXPECT_EQ(c.getA1out().front(), 2);
    EXPECT_EQ(c.getResidentCount(), 4u);
    loadedPage = 99;
    EXPECT_EQ(restored, 10);
    EXPECT_EQ(&c.lookupUpdate(1, slow), &restored);
    EXPECT_EQ(calls, 6);
}

TEST(Cache2QFocused, AmHitMovesEntryToFront) {
    cache::Cache2Q<int> c(8);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };
    for (int key = 1; key <= 10; ++key)
        c.lookupUpdate(key, slow);
    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    ASSERT_EQ(c.getAm().size(), 2u);
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
    cache::Cache2Q<int> c(2);
    int loads = 0;
    int loadedPage = 0;
    auto slow = [&](int) -> int& {
        loadedPage = ++loads;
        return loadedPage;
    };
    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(3, slow), 3);
    EXPECT_EQ(c.lookupUpdate(1, slow), 4);
    EXPECT_EQ(c.lookupUpdate(1, slow), 4);
    EXPECT_EQ(loads, 4);
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
    cache::Cache2Q<int> c(4);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key * 10;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4, 5})
        c.lookupUpdate(key, slow);
    EXPECT_THROW(
        c.lookupUpdate(1, [](int) -> int& { throw std::runtime_error("ghost load failed"); }),
        std::runtime_error);

    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front(), 1);
    EXPECT_EQ(c.getResidentCount(), 4u);
    EXPECT_TRUE(c.getAm().empty());
    EXPECT_EQ(c.lookupUpdate(6, slow), 60);
    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    ASSERT_EQ(c.getAm().size(), 1u);
    EXPECT_EQ(c.getAm().front().key, 1);
    EXPECT_EQ(c.getIndexedCount(), c.getResidentCount() + c.getA1out().size());
}

TEST(Cache2Q, VectorPage) {
    cache::Cache2Q<std::vector<int>, int> c(3);
    int loads = 0;
    std::vector<int> loadedPage;
    auto slow = [&](int key) -> std::vector<int>& {
        ++loads;
        loadedPage = std::vector<int>{key, key + 1};
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
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate(key, slow);

    loadedPage.failCopy = true;
    EXPECT_THROW(c.lookupUpdate(5, slow), std::runtime_error);

    ASSERT_EQ(c.getA1in().size(), 4u);
    EXPECT_EQ(c.getA1in().front().key, 4);
    EXPECT_EQ(c.getA1in().front().page.value, 4);
    EXPECT_EQ(c.getIndexedCount(), 4u);
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());

    loadedPage.failCopy = false;
    EXPECT_EQ(c.lookupUpdate(5, slow).value, 5);
}

TEST(Cache2Q, FailedGhostPageCopyPreservesHotPageAndGhost) {
    cache::Cache2Q<CopyCheckedPage, int> c(2);
    CopyCheckedPage loadedPage;
    auto slow = [&](int key) -> CopyCheckedPage& {
        loadedPage.value = key;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 1}) {
        c.lookupUpdate(key, slow);
    }

    loadedPage.failCopy = true;
    EXPECT_THROW(c.lookupUpdate(2, slow), std::runtime_error);

    ASSERT_EQ(c.getAm().size(), 1u);
    EXPECT_EQ(c.getAm().front().key, 1);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front(), 2);
    EXPECT_EQ(c.getIndexedCount(), 3u);

    loadedPage.failCopy = false;
    EXPECT_EQ(c.lookupUpdate(2, slow).value, 2);
    EXPECT_EQ(c.getAm().front().key, 2);
}

TEST(Cache2Q, FailedEvictionLookupRollsBackNewIndexEntry) {
    cache::Cache2Q<int, HashCheckedKey> c(2);
    int loadedPage = 0;
    auto slow = [&](const HashCheckedKey& key) -> int& {
        loadedPage = key.value;
        if (key.value == 3)
            HashCheckedKey::failingValue = 1;
        return loadedPage;
    };
    c.lookupUpdate(HashCheckedKey{1}, slow);
    c.lookupUpdate(HashCheckedKey{2}, slow);
    EXPECT_THROW(c.lookupUpdate(HashCheckedKey{3}, slow), std::runtime_error);
    HashCheckedKey::failingValue = -1;

    EXPECT_EQ(c.getResidentCount(), 2u);
    EXPECT_EQ(c.getIndexedCount(), 2u);
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_EQ(c.getA1in().front().key.value, 2);
    auto successfulLoad = [&](const HashCheckedKey& key) -> int& {
        loadedPage = key.value;
        return loadedPage;
    };
    EXPECT_EQ(c.lookupUpdate(HashCheckedKey{3}, successfulLoad), 3);
    EXPECT_EQ(c.getA1out().front().value, 1);
}

TEST(Cache2Q, GhostHistoryStoresOnlyKeys) {
    cache::Cache2Q<std::vector<int>> c(2);
    using History = std::remove_cv_t<std::remove_reference_t<decltype(c.getA1out())>>;
    static_assert(std::is_same_v<typename History::value_type, int>);
    std::vector<int> loadedPage;
    auto slow = [&](int key) -> std::vector<int>& {
        loadedPage.assign(4096, key);
        return loadedPage;
    };
    for (int key : {1, 2, 3})
        c.lookupUpdate(key, slow);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1out().front(), 1);
}

namespace {
// Deliberately simple linear reference model, independent of index/iterator logic.
class Reference2Q {
    std::size_t capacity_;
    std::size_t a1inTargetSize_;
    std::size_t ghostLimit_;

public:
    std::vector<int> recent;
    std::vector<int> repeated;
    std::vector<int> history;

    explicit Reference2Q(std::size_t capacity)
        : capacity_(capacity), a1inTargetSize_(std::max<std::size_t>(1, capacity / 4)),
          ghostLimit_(std::max<std::size_t>(1, capacity / 2)) {}

    bool access(int key) {
        if (std::find(recent.begin(), recent.end(), key) != recent.end())
            return true;
        auto repeatedPage = std::find(repeated.begin(), repeated.end(), key);
        if (repeatedPage != repeated.end()) {
            repeated.erase(repeatedPage);
            repeated.insert(repeated.begin(), key);
            return true;
        }

        auto ghost = std::find(history.begin(), history.end(), key);
        const bool promote = ghost != history.end();
        if (promote)
            history.erase(ghost);
        if (recent.size() + repeated.size() == capacity_) {
            if (recent.size() > a1inTargetSize_ || repeated.empty()) {
                history.insert(history.begin(), recent.back());
                recent.pop_back();
                if (history.size() > ghostLimit_)
                    history.pop_back();
            } else {
                repeated.pop_back();
            }
        }
        auto& destination = promote ? repeated : recent;
        destination.insert(destination.begin(), key);
        return false;
    }
};

template <typename Queue>
std::vector<int> residentKeys(const Queue& queue) {
    std::vector<int> keys;
    for (const auto& record : queue)
        keys.push_back(record.key);
    return keys;
}
} // namespace

TEST(Cache2Q, RandomRequestsMatchReferenceModel) {
    for (std::size_t capacity : {2u, 3u, 4u, 8u, 17u}) {
        cache::Cache2Q<int> c(capacity);
        Reference2Q reference(capacity);
        std::mt19937 generator(12345);
        std::uniform_int_distribution<int> chooseKey(0, 31);
        int loadedPage = 0;
        for (int request = 0; request < 1000; ++request) {
            const int key = chooseKey(generator);
            const bool expectedHit = reference.access(key);
            bool loaderCalled = false;
            auto slow = [&](int requestedKey) -> int& {
                loaderCalled = true;
                loadedPage = requestedKey * 10;
                return loadedPage;
            };
            SCOPED_TRACE("capacity=" + std::to_string(capacity) +
                         " request=" + std::to_string(request));
            EXPECT_EQ(c.lookupUpdate(key, slow), key * 10);
            EXPECT_EQ(loaderCalled, !expectedHit);
            EXPECT_EQ(residentKeys(c.getA1in()), reference.recent);
            EXPECT_EQ(residentKeys(c.getAm()), reference.repeated);
            EXPECT_EQ((std::vector<int>(c.getA1out().begin(), c.getA1out().end())),
                      reference.history);
            EXPECT_LE(c.getResidentCount(), capacity);
            EXPECT_LE(c.getA1out().size(), c.getGhostLimit());
            EXPECT_EQ(c.getIndexedCount(), c.getResidentCount() + c.getA1out().size());
        }
    }
}

} // namespace tests
