#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstddef>
#include <type_traits>

template <class T>
inline void hash_combine(std::size_t & seed, T v)
{
    seed ^= static_cast<std::size_t>(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class It>
inline std::size_t hash_range(It first, It last)
{
    std::size_t seed = 0;

    for(; first != last; ++first)
    {
        hash_combine(seed, *first);
    }

    return seed;
}

#endif // UTILITIES_H
