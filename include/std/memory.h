//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_MEMORY_H
#define JEOKERNEL_MEMORY_H

#ifndef UNIT_TESTING

#include <core/malloc.h>
#include <std/cstdint.h>

#endif

namespace std {
#ifndef UNIT_TESTING
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

#endif

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

        explicit constexpr operator bool () const {
            return ptr != nullptr;
        }
    };

    template<class T, class... Args> unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(args...));
    }

    template <class T, class Deleter = std::default_delete<T>> class shared_ptr_container {
    public:
        typedef T *pointer;
    private:
        pointer ptr;
        uint32_t ref;
    public:
        constexpr shared_ptr_container() noexcept : ptr(nullptr), ref(1) {
        }
        explicit shared_ptr_container(pointer ptr) noexcept : ptr(ptr), ref(1) {
        }

        shared_ptr_container(shared_ptr_container &&mv) = delete;
        shared_ptr_container(const shared_ptr_container &cp) = delete;

        shared_ptr_container &operator = (shared_ptr_container &&mv) = delete;
        shared_ptr_container &operator = (const shared_ptr_container &cp) = delete;

        shared_ptr_container &operator = (pointer ptr) {
            this->ptr = ptr;
            return *this;
        }

        ~shared_ptr_container() {
            if (ptr != nullptr) {
                Deleter deleter{};
                deleter(ptr);
                ptr = nullptr;
            }
        }

        void acquire() {
            asm("lock incl %0;" : "+m"(ref));
        }

        uint32_t release() {
            uint32_t xref;
            asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(ref), "=rm"(xref) :: "%rax");
            --xref;
            return xref;
        }

        T &operator * () const {
            return *ptr;
        }

        pointer operator->() const noexcept {
            return ptr;
        }

        bool isset() const {
            return ptr != nullptr;
        }
    };

    template <class T, class Deleter = std::default_delete<T>> class shared_ptr {
    public:
        typedef T *pointer;
    private:
        shared_ptr_container<T,Deleter> *container;
    public:
        shared_ptr() : container(new shared_ptr_container<T,Deleter>()) {}
        shared_ptr(pointer p) : container(new shared_ptr_container<T,Deleter>(p)) {}
        ~shared_ptr() {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
        }

        shared_ptr(shared_ptr &&mv) : container(mv.container) {
            mv.container = nullptr;
        }
        shared_ptr(const shared_ptr &cp) : container(cp.container) {
            container->acquire();
        }

        shared_ptr &operator = (shared_ptr &&mv) {
            container = mv.container;
            mv.container = nullptr;
            return *this;
        }

        shared_ptr &operator = (const shared_ptr &cp) {
            container = cp.container;
            container->acquire();
            return *this;
        }

        shared_ptr &operator = (pointer ptr) {
            *container = ptr;
            return *this;
        }

        T &operator * () const {
            return *container;
        }

        pointer operator->() const noexcept {
            return &(*(*container));
        }

        explicit operator bool () const {
            return container->isset();
        }
    };

    template<class T, class... Args> shared_ptr<T> make_shared(Args&&... args) {
        return shared_ptr<T>(new T(args...));
    }
}

#endif //JEOKERNEL_MEMORY_H
