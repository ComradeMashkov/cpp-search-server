#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex bucket_mutex_;
        std::map<Key, Value> bucket_map_;
    };
    
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> lock_guard_mutex;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : lock_guard_mutex(bucket.bucket_mutex_)
            , ref_to_value(bucket.bucket_map_[key])
        {
        
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : bucket_(bucket_count)
    {
 
    }

    Access operator[](const Key& key) {
        auto& bucket = bucket_[uint64_t(key) % bucket_.size()];
        return {key, bucket};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [bucket_mutex_, bucket_map_] : bucket_) {
            std::lock_guard lock_guard_mutex(bucket_mutex_);
            result.insert(bucket_map_.begin(), bucket_map_.end());
        }
        return result;
    }
    
    void erase(const Key& key) {
        auto& bucket = bucket_[uint64_t(key) & bucket_.size()];
        std::lock_guard lock_guard_mutex(bucket.bucket_mutex_);
        bucket.bucket_map_.erase(key);
    }
    
private:
    std::vector<Bucket> bucket_;
};