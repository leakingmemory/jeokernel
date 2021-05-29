//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_MEMORY_H
#define JEOKERNEL_MEMORY_H

#include <core/malloc.h>
#include <std/cstdint.h>

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

    template <class T> struct default_delete {
        constexpr default_delete() noexcept = default;
        default_delete &operator () (T *ptr) {
            delete ptr;
            return *this;
        }
    };

    template <class T, class Deleter = std::default_delete<T>> class unique_ptr {
    public:
        typedef T *pointer;
    private:
        pointer ptr;
    public:
        constexpr unique_ptr() noexcept : ptr(nullptr) {
        }
        explicit unique_ptr(pointer ptr) noexcept : ptr(ptr) {
        }

        ~unique_ptr() {
            if (ptr != nullptr) {
                Deleter deleter{};
                deleter(ptr);
                ptr = nullptr;
            }
        }
        unique_ptr &operator = (unique_ptr &&mv) noexcept {
            if (ptr != nullptr) {
                Deleter deleter{};
                deleter(ptr);
            }
            ptr = mv.ptr;
            mv.ptr = nullptr;
            return *this;
        }

        T &operator * () const {
            return *ptr;
        }

        pointer operator->() const noexcept {
            return ptr;
        }
    };

    template<class T, class... Args> unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(args...));
    }
}

#endif //JEOKERNEL_MEMORY_H
