//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_MEMORY_H
#define JEOKERNEL_MEMORY_H

namespace std {
    template<class T> class allocator {
    public:
        constexpr allocator() noexcept {}
        constexpr allocator( const allocator& other ) noexcept {}
        template<class U> constexpr allocator( const allocator<U>& other) noexcept {}
        constexpr ~allocator() {}

        [[nodiscard]] constexpr T* allocate( std::size_t n ) {
            return (T *) malloc(sizeof(T) * n);
        }
        constexpr void deallocate( T* p, std::size_t n ) {
            free(p);
        }
    };
}

#endif //JEOKERNEL_MEMORY_H
