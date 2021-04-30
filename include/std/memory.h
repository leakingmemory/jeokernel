//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_MEMORY_H
#define JEOKERNEL_MEMORY_H

#include <core/malloc.h>

namespace std {
    template<class T> class allocator {
    public:
        typedef std::size_t size_type;

        constexpr allocator() noexcept {}
        constexpr allocator( const allocator& other ) noexcept {}
        template<class U> constexpr allocator( const allocator<U>& other) noexcept {}
        ~allocator() {}

        [[nodiscard]] constexpr T* allocate( size_type n ) {
            return (T *) malloc(sizeof(T) * n);
        }
        constexpr void deallocate( T* p, size_type n ) {
            free(p);
        }
    };
}

#endif //JEOKERNEL_MEMORY_H
