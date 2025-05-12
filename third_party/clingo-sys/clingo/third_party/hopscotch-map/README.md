[![CI](https://github.com/Tessil/hopscotch-map/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Tessil/hopscotch-map/actions/workflows/ci.yml)

## A C++ implementation of a fast hash map and hash set using hopscotch hashing

The hopscotch-map library is a C++ implementation of a fast hash map and hash set using open-addressing and hopscotch hashing to resolve collisions. It is a cache-friendly data structure offering better performances than `std::unordered_map` in most cases and is closely similar to `google::dense_hash_map` while using less memory and providing more functionalities.

The library provides the following main classes: `tsl::hopscotch_map`, `tsl::hopscotch_set`, `tsl::hopscotch_pg_map` and `tsl::hopscotch_pg_set`. The first two are faster and use a power of two growth policy, the last two use a prime growth policy instead and are able to cope better with a poor hash function. Use the prime version if there is a chance of repeating patterns in the lower bits of your hash (e.g. you are storing pointers with an identity hash function). See [GrowthPolicy](#growth-policy) for details.


In addition to these classes the library also provides `tsl::bhopscotch_map`, `tsl::bhopscotch_set`, `tsl::bhopscotch_pg_map` and `tsl::bhopscotch_pg_set`. These classes have an additional requirement for the key, it must be `LessThanComparable`, but they provide a better asymptotic upper bound, see [details](#deny-of-service-dos-attack) in example. Nonetheless if you don't have specific requirements (risk of hash DoS attacks), `tsl::hopscotch_map` and `tsl::hopscotch_set` should be sufficient in most cases and should be your default pick as they perform better in general.


An overview of hopscotch hashing and some implementation details can be found [here](https://tessil.github.io/2016/08/29/hopscotch-hashing.html).

A **benchmark** of `tsl::hopscotch_map` against other hash maps may be found [here](https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html). This page also gives some advices on which hash table structure you should try for your use case (useful if you are a bit lost with the multiple hash tables implementations in the `tsl` namespace).

### Key features
- Header-only library, just add the [include](include/) directory to your include path and you are ready to go. If you use CMake, you can also use the `tsl::hopscotch_map` exported target from the [CMakeLists.txt](CMakeLists.txt).
- Fast hash table, see [benchmark](https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html) for some numbers.
- Support for move-only and non-default constructible key/value.
- Support for heterogeneous lookups allowing the usage of `find` with a type different than `Key` (e.g. if you have a map that uses `std::unique_ptr<foo>` as key, you can use a `foo*` or a `std::uintptr_t` as key parameter to `find` without constructing a `std::unique_ptr<foo>`, see [example](#heterogeneous-lookups)).
- No need to reserve any sentinel value from the keys.
- Possibility to store the hash value on insert for faster rehash and lookup if the hash or the key equal functions are expensive to compute (see the [StoreHash](https://tessil.github.io/hopscotch-map/classtsl_1_1hopscotch__map.html#details) template parameter).
- If the hash is known before a lookup, it is possible to pass it as parameter to speed-up the lookup (see `precalculated_hash` parameter in [API](https://tessil.github.io/hopscotch-map/classtsl_1_1hopscotch__map.html#a74d83c67c50bc8385bb11f78142eaa86)).
- The `tsl::bhopscotch_map` and `tsl::bhopscotch_set` provide a worst-case of O(log n) on lookups and deletions making these classes resistant to hash table Deny of Service (DoS) attacks (see [details](#deny-of-service-dos-attack) in example).
- The library can be used with exceptions disabled (through `-fno-exceptions` option on Clang and GCC, without an `/EH` option on MSVC or simply by defining `TSL_NO_EXCEPTIONS`). `std::terminate` is used in replacement of the `throw` instruction when exceptions are disabled.
- API closely similar to `std::unordered_map` and `std::unordered_set`.

### Differences compared to `std::unordered_map`
`tsl::hopscotch_map` tries to have an interface similar to `std::unordered_map`, but some differences exist.
- Iterator invalidation on insert doesn't behave in the same way. In general any operation modifying the hash table, except `erase`, invalidate all the iterators (see [API](https://tessil.github.io/hopscotch-map/classtsl_1_1hopscotch__map.html#details) for details).
- References and pointers to keys or values in the map are invalidated in the same way as iterators to these keys-values on insert.
- For iterators, `operator*()` and `operator->()` return a reference and a pointer to `const std::pair<Key, T>` instead of `std::pair<const Key, T>`, making the value `T` not modifiable. To modify the value you have to call the `value()` method of the iterator to get a mutable reference. Example:
```c++
tsl::hopscotch_map<int, int> map = {{1, 1}, {2, 1}, {3, 1}};
for(auto it = map.begin(); it != map.end(); ++it) {
    //it->second = 2; // Illegal
    it.value() = 2; // Ok
}
```
- Move-only types must have a nothrow move constructor (with open addressing, it is not possible to keep the strong exception guarantee on rehash if the move constructor may throw).
- No support for some buckets related methods (like `bucket_size`, `bucket`, ...).

These differences also apply between `std::unordered_set` and `tsl::hopscotch_set`.

Thread-safety and exceptions guarantees are the same as `std::unordered_map/set` (i.e. possible to have multiple readers with no writer).

### Growth policy

The library supports multiple growth policies through the `GrowthPolicy` template parameter. Three policies are provided by the library but you can easily implement your own if needed.

* **[tsl::hh::power_of_two_growth_policy.](https://tessil.github.io/hopscotch-map/classtsl_1_1hh_1_1power__of__two__growth__policy.html)** Default policy used by `tsl::(b)hopscotch_map/set`. This policy keeps the size of the bucket array of the hash table to a power of two. This constraint allows the policy to avoid the usage of the slow modulo operation to map a hash to a bucket, instead of <code>hash % 2<sup>n</sup></code>, it uses <code>hash & (2<sup>n</sup> - 1)</code> (see [fast modulo](https://en.wikipedia.org/wiki/Modulo_operation#Performance_issues)). Fast but this may cause a lot of collisions with a poor hash function as the modulo with a power of two only masks the most significant bits in the end.
* **[tsl::hh::prime_growth_policy.](https://tessil.github.io/hopscotch-map/classtsl_1_1hh_1_1prime__growth__policy.html)** Default policy used by `tsl::(b)hopscotch_pg_map/set`. The policy keeps the size of the bucket array of the hash table to a prime number. When mapping a hash to a bucket, using a prime number as modulo will result in a better distribution of the hashes across the buckets even with a poor hash function. To allow the compiler to optimize the modulo operation, the policy use a lookup table with constant primes modulos (see [API](https://tessil.github.io/hopscotch-map/classtsl_1_1hh_1_1prime__growth__policy.html#details) for details). Slower than `tsl::hh::power_of_two_growth_policy` but more secure.
* **[tsl::hh::mod_growth_policy.](https://tessil.github.io/hopscotch-map/classtsl_1_1hh_1_1mod__growth__policy.html)** The policy grows the map by a customizable growth factor passed in parameter. It then just use the modulo operator to map a hash to a bucket. Slower but more flexible.

If you encounter poor performances check the `overflow_size()`, if it is not zero you may have a lot of hash collisions. Either change the hash function for something more uniform or try another growth policy (mainly `tsl::hh::prime_growth_policy`). Unfortunately it is sometimes difficult to guard yourself against collisions (e.g. DoS attack on the hash map). If needed, check also `tsl::bhopscotch_map/set` which offer a worst-case scenario of O(log n) on lookups instead of O(n), see [details](#deny-of-service-dos-attack) in example.

To implement your own policy, you have to implement the following interface.

```c++
struct custom_policy {
    // Called on hash table construction and rehash, min_bucket_count_in_out is the minimum buckets
    // that the hash table needs. The policy can change it to a higher number of buckets if needed 
    // and the hash table will use this value as bucket count. If 0 bucket is asked, then the value
    // must stay at 0.
    explicit custom_policy(std::size_t& min_bucket_count_in_out);
    
    // Return the bucket [0, bucket_count()) to which the hash belongs. 
    // If bucket_count() is 0, it must always return 0.
    std::size_t bucket_for_hash(std::size_t hash) const noexcept;
    
    // Return the number of buckets that should be used on next growth
    std::size_t next_bucket_count() const;
    
    // Maximum number of buckets supported by the policy
    std::size_t max_bucket_count() const;
    
    // Reset the growth policy as if the policy was created with a bucket count of 0.
    // After a clear, the policy must always return 0 when bucket_for_hash() is called.
    void clear() noexcept;
}
```
### Installation
To use hopscotch-map, just add the [include](include/) directory to your include path. It is a **header-only** library.

If you use CMake, you can also use the `tsl::hopscotch_map` exported target from the [CMakeLists.txt](CMakeLists.txt) with `target_link_libraries`. 
```cmake
# Example where the hopscotch-map project is stored in a third-party directory
add_subdirectory(third-party/hopscotch-map)
target_link_libraries(your_target PRIVATE tsl::hopscotch_map)  
```

If the project has been installed through `make install`, you can also use `find_package(tsl-hopscotch-map REQUIRED)` instead of `add_subdirectory`.

The code should work with any C++11 standard-compliant compiler and has been tested with GCC 4.8.4, Clang 3.5.0 and Visual Studio 2015.

To run the tests you will need the Boost Test library and CMake. 

```bash
git clone https://github.com/Tessil/hopscotch-map.git
cd hopscotch-map/tests
mkdir build
cd build
cmake ..
cmake --build .
./tsl_hopscotch_map_tests 
```


### Usage
The API can be found [here](https://tessil.github.io/hopscotch-map/). 

All methods are not documented yet, but they replicate the behaviour of the ones in `std::unordered_map` and `std::unordered_set`, except if specified otherwise.

### Example

```c++
#include <cstdint>
#include <iostream>
#include <string>
#include <tsl/hopscotch_map.h>
#include <tsl/hopscotch_set.h>

int main() {
    tsl::hopscotch_map<std::string, int> map = {{"a", 1}, {"b", 2}};
    map["c"] = 3;
    map["d"] = 4;
    
    map.insert({"e", 5});
    map.erase("b");
    
    for(auto it = map.begin(); it != map.end(); ++it) {
        //it->second += 2; // Not valid.
        it.value() += 2;
    }
    
    // {d, 6} {a, 3} {e, 7} {c, 5}
    for(const auto& key_value : map) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
        
    if(map.find("a") != map.end()) {
        std::cout << "Found \"a\"." << std::endl;
    }
    
    const std::size_t precalculated_hash = std::hash<std::string>()("a");
    // If we already know the hash beforehand, we can pass it in parameter to speed-up lookups.
    if(map.find("a", precalculated_hash) != map.end()) {
        std::cout << "Found \"a\" with hash " << precalculated_hash << "." << std::endl;
    }
    
    
    /*
     * Calculating the hash and comparing two std::string may be slow. 
     * We can store the hash of each std::string in the hash map to make 
     * the inserts and lookups faster by setting StoreHash to true.
     */ 
    tsl::hopscotch_map<std::string, int, std::hash<std::string>, 
                       std::equal_to<std::string>,
                       std::allocator<std::pair<std::string, int>>,
                       30, true> map2;
                       
    map2["a"] = 1;
    map2["b"] = 2;
    
    // {a, 1} {b, 2}
    for(const auto& key_value : map2) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
    
    
    
    tsl::hopscotch_set<int> set;
    set.insert({1, 9, 0});
    set.insert({2, -1, 9});
    
    // {0} {1} {2} {9} {-1}
    for(const auto& key : set) {
        std::cout << "{" << key << "}" << std::endl;
    }
} 
```

#### Heterogeneous lookups

Heterogeneous overloads allow the usage of other types than `Key` for lookup and erase operations as long as the used types are hashable and comparable to `Key`.

To activate the heterogeneous overloads in `tsl::hopscotch_map/set`, the qualified-id `KeyEqual::is_transparent` must be valid. It works the same way as for [`std::map::find`](http://en.cppreference.com/w/cpp/container/map/find). You can either use [`std::equal_to<>`](http://en.cppreference.com/w/cpp/utility/functional/equal_to_void) or define your own function object.

Both `KeyEqual` and `Hash` will need to be able to deal with the different types.

```c++
#include <functional>
#include <iostream>
#include <string>
#include <tsl/hopscotch_map.h>


struct employee {
    employee(int id, std::string name) : m_id(id), m_name(std::move(name)) {
    }
    
    // Either we include the comparators in the class and we use `std::equal_to<>`...
    friend bool operator==(const employee& empl, int empl_id) {
        return empl.m_id == empl_id;
    }
    
    friend bool operator==(int empl_id, const employee& empl) {
        return empl_id == empl.m_id;
    }
    
    friend bool operator==(const employee& empl1, const employee& empl2) {
        return empl1.m_id == empl2.m_id;
    }
    
    
    int m_id;
    std::string m_name;
};

// ... or we implement a separate class to compare employees.
struct equal_employee {
    using is_transparent = void;
    
    bool operator()(const employee& empl, int empl_id) const {
        return empl.m_id == empl_id;
    }
    
    bool operator()(int empl_id, const employee& empl) const {
        return empl_id == empl.m_id;
    }
    
    bool operator()(const employee& empl1, const employee& empl2) const {
        return empl1.m_id == empl2.m_id;
    }
};

struct hash_employee {
    std::size_t operator()(const employee& empl) const {
        return std::hash<int>()(empl.m_id);
    }
    
    std::size_t operator()(int id) const {
        return std::hash<int>()(id);
    }
};


int main() {
    // Use std::equal_to<> which will automatically deduce and forward the parameters
    tsl::hopscotch_map<employee, int, hash_employee, std::equal_to<>> map; 
    map.insert({employee(1, "John Doe"), 2001});
    map.insert({employee(2, "Jane Doe"), 2002});
    map.insert({employee(3, "John Smith"), 2003});

    // John Smith 2003
    auto it = map.find(3);
    if(it != map.end()) {
        std::cout << it->first.m_name << " " << it->second << std::endl;
    }

    map.erase(1);



    // Use a custom KeyEqual which has an is_transparent member type
    tsl::hopscotch_map<employee, int, hash_employee, equal_employee> map2;
    map2.insert({employee(4, "Johnny Doe"), 2004});

    // 2004
    std::cout << map2.at(4) << std::endl;
} 
```

#### Deny of Service (DoS) attack
In addition to `tsl::hopscotch_map` and `tsl::hopscotch_set`, the library provides two more "secure" options: `tsl::bhopscotch_map` and `tsl::bhopscotch_set` (all with their `pg` counterparts). 

These two additions have a worst-case asymptotic complexity of O(log n) for lookups and deletions and an amortized worst case of O(log n) for insertions (amortized due to the possibility of rehash which would be in O(n)). Even if the hash function maps all the elements to the same bucket, the O(log n) would still hold.

This provides a security against hash table Deny of Service (DoS) attacks. 

To achieve this, the *secure* versions use a binary search tree for the overflown elements (see [implementation details](https://tessil.github.io/2016/08/29/hopscotch-hashing.html)) and thus need the elements to be `LessThanComparable`. An additional `Compare` template parameter is needed.

```c++
#include <chrono>
#include <cstdint>
#include <iostream>
#include <tsl/hopscotch_map.h>
#include <tsl/bhopscotch_map.h>

/*
 * Poor hash function which always returns 1 to simulate
 * a Deny of Service attack.
 */
struct dos_attack_simulation_hash {
    std::size_t operator()(int id) const {
        return 1;
    }
};

int main() {
    /*
     * Slow due to the hash function, insertions are done in O(n).
     */
    tsl::hopscotch_map<int, int, dos_attack_simulation_hash> map;
    
    auto start = std::chrono::high_resolution_clock::now();
    for(int i=0; i < 10000; i++) {
        map.insert({i, 0});
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    // 110 ms
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    std::cout << duration.count() << " ms" << std::endl;
    
    
    
    
    /*
     * Faster. Even with the poor hash function, insertions end-up to
     * be O(log n) in average (and O(n) when a rehash occurs).
     */
    tsl::bhopscotch_map<int, int, dos_attack_simulation_hash> map_secure;
    
    start = std::chrono::high_resolution_clock::now();
    for(int i=0; i < 10000; i++) {
        map_secure.insert({i, 0});
    }
    end = std::chrono::high_resolution_clock::now();
    
    // 2 ms
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    std::cout << duration.count() << " ms" << std::endl;
} 
```

### License

The code is licensed under the MIT license, see the [LICENSE file](LICENSE) for details.
