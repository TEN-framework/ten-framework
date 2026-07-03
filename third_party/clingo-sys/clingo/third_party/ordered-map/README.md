[![CI](https://github.com/Tessil/ordered-map/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Tessil/ordered-map/actions/workflows/ci.yml)

## C++ hash map and hash set which preserves the order of insertion

The ordered-map library provides a hash map and a hash set which preserve the order of insertion in a way similar to Python's [OrderedDict](https://docs.python.org/3/library/collections.html#collections.OrderedDict). When iterating over the map, the values will be returned in the same order as they were inserted.

The values are stored contiguously in an underlying structure, no holes in-between values even after an erase operation. By default a `std::deque` is used for this structure, but it's also possible to  use a `std::vector`. This structure is directly accessible through the `values_container()` method and if the structure is a `std::vector`, a `data()` method is also provided to easily interact with C APIs. This provides fast iteration but with the drawback of an O(bucket_count) erase operation. An O(1) `pop_back()` and an O(1) `unordered_erase()` functions are available. **If ordered erase is often used, another data structure is recommended.**

To resolve collisions on hashes, the library uses linear robin hood probing with backward shift deletion.

The library provides a behaviour similar to a `std::deque/std::vector` with unique values but with an average time complexity of O(1) for lookups and an amortised time complexity of O(1) for insertions. This comes at the price of a little higher memory footprint (8 bytes per bucket by default).

Two classes are provided: `tsl::ordered_map` and `tsl::ordered_set`.

**Note**: The library uses a power of two for the size of its buckets array to take advantage of the [fast modulo](https://en.wikipedia.org/wiki/Modulo_operation#Performance_issues). For good performances, it requires the hash table to have a well-distributed hash function. If you encounter performance issues check your hash function.

### Key features

- Header-only library, just add the [include](include/) directory to your include path and you are ready to go. If you use CMake, you can also use the `tsl::ordered_map` exported target from the [CMakeLists.txt](CMakeLists.txt).
- Values are stored in the same order as the insertion order. The library provides a direct access to the underlying structure which stores the values.
- O(1) average time complexity for lookups with performances similar to `std::unordered_map` but with faster insertions and reduced memory usage (see [benchmark](https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html) for details).
- Provide random access iterators and also reverse iterators.
- Support for heterogeneous lookups allowing the usage of `find` with a type different than `Key` (e.g. if you have a map that uses `std::unique_ptr<foo>` as key, you can use a `foo*` or a `std::uintptr_t` as key parameter to `find` without constructing a `std::unique_ptr<foo>`, see [example](#heterogeneous-lookups)).
- If the hash is known before a lookup, it is possible to pass it as parameter to speed-up the lookup (see `precalculated_hash` parameter in [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html#a7fcde27edc6697a0b127f4b1aefa8a7d)).
- Support for efficient serialization and deserialization (see [example](#serialization) and the `serialize/deserialize` methods in the [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html) for details).
- The library can be used with exceptions disabled (through `-fno-exceptions` option on Clang and GCC, without an `/EH` option on MSVC or simply by defining `TSL_NO_EXCEPTIONS`). `std::terminate` is used in replacement of the `throw` instruction when exceptions are disabled.
- API closely similar to `std::unordered_map` and `std::unordered_set`.

### Differences compared to `std::unordered_map`
`tsl::ordered_map` tries to have an interface similar to `std::unordered_map`, but some differences exist.
- The iterators are `RandomAccessIterator`.
- Iterator invalidation behaves in a way closer to `std::vector` and `std::deque` (see [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html#details) for details). If you use `std::vector` as `ValueTypeContainer`, you can use `reserve()` to preallocate some space and avoid the invalidation of the iterators on insert.
- Slow `erase()` operation, it has a complexity of O(bucket_count). A faster O(1) version `unordered_erase()` exists, but it breaks the insertion order (see [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html#a9f94a7889fa7fa92eea41ca63b3f98a4) for details). An O(1) `pop_back()` is also available.
- The equality operators `operator==` and `operator!=` are order dependent. Two `tsl::ordered_map` with the same values but inserted in a different order don't compare equal.
- For iterators, `operator*()` and `operator->()` return a reference and a pointer to `const std::pair<Key, T>` instead of `std::pair<const Key, T>` making the value `T` not modifiable. To modify the value you have to call the `value()` method of the iterator to get a mutable reference. Example:
```c++
tsl::ordered_map<int, int> map = {{1, 1}, {2, 1}, {3, 1}};
for(auto it = map.begin(); it != map.end(); ++it) {
    //it->second = 2; // Illegal
    it.value() = 2; // Ok
}
```
- By default the map can only hold up to 2<sup>32</sup> - 1 values, that is 4 294 967 295 values. This can be raised through the `IndexType` class template parameter, check the [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html#details) for details. 
- No support for some bucket related methods (like `bucket_size`, `bucket`, ...).

Thread-safety guarantee is the same as `std::unordered_map`  (i.e. possible to have multiple concurrent readers with no writer).

These differences also apply between `std::unordered_set` and `tsl::ordered_set`.

### Exception Guarantees

If not mentioned otherwise, functions have the strong exception guarantee, see [details](https://en.cppreference.com/w/cpp/language/exceptions). We below list cases in which this guarantee is not provided.

The guarantee is only provided if `ValueContainer::emplace_back` has the strong exception guarantee (which is true for `std::vector` and `std::deque` as long as the type `T` is not a move-only type with a move constructor that may throw an exception, see [details](http://en.cppreference.com/w/cpp/container/vector/emplace_back#Exceptions)).

The `tsl::ordered_map::erase_if` and `tsl::ordered_set::erase_if` functions only have the guarantee under the preconditions listed in their documentation.

### Installation

To use ordered-map, just add the [include](include/) directory to your include path. It is a **header-only** library.

If you use CMake, you can also use the `tsl::ordered_map` exported target from the [CMakeLists.txt](CMakeLists.txt) with `target_link_libraries`. 
```cmake
# Example where the ordered-map project is stored in a third-party directory
add_subdirectory(third-party/ordered-map)
target_link_libraries(your_target PRIVATE tsl::ordered_map)  
```

If the project has been installed through `make install`, you can also use `find_package(tsl-ordered-map REQUIRED)` instead of `add_subdirectory`.

The code should work with any C++11 standard-compliant compiler and has been tested with GCC 4.8.4, Clang 3.5.0 and Visual Studio 2015.

To run the tests you will need the Boost Test library and CMake.

```bash
git clone https://github.com/Tessil/ordered-map.git
cd ordered-map/tests
mkdir build
cd build
cmake ..
cmake --build .
./tsl_ordered_map_tests 
```

### Usage

The API can be found [here](https://tessil.github.io/ordered-map/).

### Example

```c++
#include <iostream>
#include <string>
#include <cstdlib>
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>

int main() {
    tsl::ordered_map<char, int> map = {{'d', 1}, {'a', 2}, {'g', 3}};
    map.insert({'b', 4});
    map['h'] = 5;
    map['e'] = 6;
    
    map.erase('a');
    
    
    // {d, 1} {g, 3} {b, 4} {h, 5} {e, 6}
    for(const auto& key_value : map) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
    
    map.unordered_erase('b');
    
    // Break order: {d, 1} {g, 3} {e, 6} {h, 5}
    for(const auto& key_value : map) {
        std::cout << "{" << key_value.first << ", " << key_value.second << "}" << std::endl;
    }
    
    
    for(auto it = map.begin(); it != map.end(); ++it) {
        //it->second += 2; // Not valid.
        it.value() += 2;
    }
    
    
    if(map.find('d') != map.end()) {
        std::cout << "Found 'd'." << std::endl;
    }
    
    const std::size_t precalculated_hash = std::hash<char>()('d');
    // If we already know the hash beforehand, we can pass it as argument to speed-up the lookup.
    if(map.find('d', precalculated_hash) != map.end()) {
        std::cout << "Found 'd' with hash " << precalculated_hash << "." << std::endl;
    }
    
    
    tsl::ordered_set<char, std::hash<char>, std::equal_to<char>,
                     std::allocator<char>, std::vector<char>> set;
    set.reserve(6);
    
    set = {'3', '4', '9', '2'};
    set.erase('2');
    set.insert('1');
    set.insert('\0');
    
    set.pop_back();
    set.insert({'0', '\0'});
    
    // Get raw buffer for C API: 34910
    std::cout << atoi(set.data()) << std::endl;
}
```

#### Heterogeneous lookup

Heterogeneous overloads allow the usage of other types than `Key` for lookup and erase operations as long as the used types are hashable and comparable to `Key`.

To activate the heterogeneous overloads in `tsl::ordered_map/set`, the qualified-id `KeyEqual::is_transparent` must be valid. It works the same way as for [`std::map::find`](http://en.cppreference.com/w/cpp/container/map/find). You can either use [`std::equal_to<>`](http://en.cppreference.com/w/cpp/utility/functional/equal_to_void) or define your own function object.

Both `KeyEqual` and `Hash` will need to be able to deal with the different types.

```c++
#include <functional>
#include <iostream>
#include <string>
#include <tsl/ordered_map.h>



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
    tsl::ordered_map<employee, int, hash_employee, std::equal_to<>> map; 
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
    tsl::ordered_map<employee, int, hash_employee, equal_employee> map2;
    map2.insert({employee(4, "Johnny Doe"), 2004});

    // 2004
    std::cout << map2.at(4) << std::endl;
} 
```

#### Serialization

The library provides an efficient way to serialize and deserialize a map or a set so that it can be saved to a file or send through the network.
To do so, it requires the user to provide a function object for both serialization and deserialization.

```c++
struct serializer {
    // Must support the following types for U: std::uint64_t, float 
    // and std::pair<Key, T> if a map is used or Key for a set.
    template<typename U>
    void operator()(const U& value);
};
```

```c++
struct deserializer {
    // Must support the following types for U: std::uint64_t, float 
    // and std::pair<Key, T> if a map is used or Key for a set.
    template<typename U>
    U operator()();
};
```

Note that the implementation leaves binary compatibility (endianness, float binary representation, size of int, ...) of the types it serializes/deserializes in the hands of the provided function objects if compatibility is required.

More details regarding the `serialize` and `deserialize` methods can be found in the [API](https://tessil.github.io/ordered-map/classtsl_1_1ordered__map.html).

```c++
#include <cassert>
#include <cstdint>
#include <fstream>
#include <type_traits>
#include <tsl/ordered_map.h>


class serializer {
public:
    explicit serializer(const char* file_name) {
        m_ostream.exceptions(m_ostream.badbit | m_ostream.failbit);
        m_ostream.open(file_name, std::ios::binary);
    }
    
    template<class T,
             typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
    void operator()(const T& value) {
        m_ostream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }
    
    void operator()(const std::pair<std::int64_t, std::int64_t>& value) {
        (*this)(value.first);
        (*this)(value.second);
    }

private:
    std::ofstream m_ostream;
};

class deserializer {
public:
    explicit deserializer(const char* file_name) {
        m_istream.exceptions(m_istream.badbit | m_istream.failbit | m_istream.eofbit);
        m_istream.open(file_name, std::ios::binary);
    }
    
    template<class T>
    T operator()() {
        T value;
        deserialize(value);
        
        return value;
    }
    
private:
    template<class T,
             typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
    void deserialize(T& value) {
        m_istream.read(reinterpret_cast<char*>(&value), sizeof(T));
    }
    
    void deserialize(std::pair<std::int64_t, std::int64_t>& value) {
        deserialize(value.first);
        deserialize(value.second);
    }

private:
    std::ifstream m_istream;
};


int main() {
    const tsl::ordered_map<std::int64_t, std::int64_t> map = {{1, -1}, {2, -2}, {3, -3}, {4, -4}};
    
    
    const char* file_name = "ordered_map.data";
    {
        serializer serial(file_name);
        map.serialize(serial);
    }
    
    {
        deserializer dserial(file_name);
        auto map_deserialized = tsl::ordered_map<std::int64_t, std::int64_t>::deserialize(dserial);
        
        assert(map == map_deserialized);
    }
    
    {
        deserializer dserial(file_name);
        
        /**
         * If the serialized and deserialized map are hash compatibles (see conditions in API), 
         * setting the argument to true speed-up the deserialization process as we don't have 
         * to recalculate the hash of each key. We also know how much space each bucket needs.
         */
        const bool hash_compatible = true;
        auto map_deserialized = 
            tsl::ordered_map<std::int64_t, std::int64_t>::deserialize(dserial, hash_compatible);
        
        assert(map == map_deserialized);
    }
} 
```

##### Serialization with Boost Serialization and compression with zlib

It's possible to use a serialization library to avoid the boilerplate. 

The following example uses Boost Serialization with the Boost zlib compression stream to reduce the size of the resulting serialized file. The example requires C++20 due to the usage of the template parameter list syntax in lambdas, but it can be adapted to less recent versions.

```c++
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/utility.hpp>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <tsl/ordered_map.h>


namespace boost { namespace serialization {
    template<class Archive, class Key, class T>
    void serialize(Archive & ar, tsl::ordered_map<Key, T>& map, const unsigned int version) {
        split_free(ar, map, version); 
    }

    template<class Archive, class Key, class T>
    void save(Archive & ar, const tsl::ordered_map<Key, T>& map, const unsigned int /*version*/) {
        auto serializer = [&ar](const auto& v) { ar & v; };
        map.serialize(serializer);
    }

    template<class Archive, class Key, class T>
    void load(Archive & ar, tsl::ordered_map<Key, T>& map, const unsigned int /*version*/) {
        auto deserializer = [&ar]<typename U>() { U u; ar & u; return u; };
        map = tsl::ordered_map<Key, T>::deserialize(deserializer);
    }
}}


int main() {
    tsl::ordered_map<std::int64_t, std::int64_t> map = {{1, -1}, {2, -2}, {3, -3}, {4, -4}};
    
    
    const char* file_name = "ordered_map.data";
    {
        std::ofstream ofs;
        ofs.exceptions(ofs.badbit | ofs.failbit);
        ofs.open(file_name, std::ios::binary);
        
        boost::iostreams::filtering_ostream fo;
        fo.push(boost::iostreams::zlib_compressor());
        fo.push(ofs);
        
        boost::archive::binary_oarchive oa(fo);
        
        oa << map;
    }
    
    {
        std::ifstream ifs;
        ifs.exceptions(ifs.badbit | ifs.failbit | ifs.eofbit);
        ifs.open(file_name, std::ios::binary);
        
        boost::iostreams::filtering_istream fi;
        fi.push(boost::iostreams::zlib_decompressor());
        fi.push(ifs);
        
        boost::archive::binary_iarchive ia(fi);
     
        tsl::ordered_map<std::int64_t, std::int64_t> map_deserialized;   
        ia >> map_deserialized;
        
        assert(map == map_deserialized);
    }
}
```

### License

The code is licensed under the MIT license, see the [LICENSE file](LICENSE) for details.
