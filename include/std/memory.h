//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_MEMORY_H
#define JEOKERNEL_MEMORY_H

#ifndef UNIT_TESTING

#include <core/malloc.h>
#include <std/cstdint.h>
#include <utility>

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

    namespace impl {
        class shared_ptr_container_typeerasure {
        private:
            void *ptr;
            uint32_t ref;
        public:
            constexpr shared_ptr_container_typeerasure() noexcept: ptr(nullptr), ref(1) {
            }

            explicit shared_ptr_container_typeerasure(void *ptr) noexcept: ptr(ptr), ref(1) {
            }

            shared_ptr_container_typeerasure(shared_ptr_container_typeerasure &&mv) = delete;

            shared_ptr_container_typeerasure(const shared_ptr_container_typeerasure &cp) = delete;

            shared_ptr_container_typeerasure &operator=(shared_ptr_container_typeerasure &&mv) = delete;

            shared_ptr_container_typeerasure &operator=(const shared_ptr_container_typeerasure &cp) = delete;

            virtual ~shared_ptr_container_typeerasure() {}

            void acquire() {
                asm("lock incl %0;" : "+m"(ref));
            }

            uint32_t release() {
                uint32_t xref;
                asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(ref), "=rm"(xref)::"%rax");
                --xref;
                return xref;
            }

            void *Ptr() {
                return ptr;
            }

            void Ptr(void *newp) {
                ptr = newp;
            }

            bool isset() const {
                return ptr != nullptr;
            }
        };

        template<class T, class Deleter = std::default_delete<T>>
        class shared_ptr_container : public shared_ptr_container_typeerasure {
        public:
            typedef T *pointer;

            constexpr shared_ptr_container() noexcept: shared_ptr_container_typeerasure() {
            }

            explicit shared_ptr_container(pointer ptr) noexcept: shared_ptr_container_typeerasure(ptr) {
            }

            virtual ~shared_ptr_container() {
                T *ptr = (T *) Ptr();
                if (ptr != nullptr) {
                    Deleter deleter{};
                    deleter(ptr);
                    Ptr(nullptr);
                }
            }
        };
    }

    template <class T, class Deleter = std::default_delete<T>> class shared_ptr {
        friend shared_ptr;
    public:
        typedef T *pointer;
    private:
        impl::shared_ptr_container_typeerasure *container;
    public:
        shared_ptr() : container(nullptr) {}
        shared_ptr(pointer p) : container(new impl::shared_ptr_container<T,Deleter>(p)) {}
        ~shared_ptr() {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
        }

        shared_ptr(shared_ptr<T,Deleter> &&mv) : container(mv.container) {
            mv.container = nullptr;
        }
        shared_ptr(const shared_ptr<T,Deleter> &cp) : container(cp.container) {
            container->acquire();
        }
        template <class P,class PDeleter> shared_ptr(const shared_ptr<P,PDeleter> &cp) : container(cp.Container()) {
            static_assert(is_assignable<T*,P*>::value);
            container->acquire();
        };

        impl::shared_ptr_container_typeerasure *Container() const {
            return container;
        }

        shared_ptr &operator = (shared_ptr &&mv) {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            container = mv.container;
            mv.container = nullptr;
            return *this;
        }

        shared_ptr &operator = (const shared_ptr &cp) {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            container = cp.container;
            if (container != nullptr) {
                container->acquire();
            }
            return *this;
        }

        shared_ptr &operator = (pointer ptr) {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            if (ptr != nullptr) {
                container = new impl::shared_ptr_container<T, Deleter>(ptr);
            } else {
                container = nullptr;
            }
            return *this;
        }

        T &operator * () const {
            return *((T *) container->Ptr());
        }

        pointer operator->() const noexcept {
            return (pointer) container->Ptr();
        }

        explicit operator bool () const {
            return container != nullptr && container->isset();
        }

        bool operator == (T *ptr) {
            if (container != nullptr) {
                T *obj = (T *) container->Ptr();
                return obj == ptr;
            } else {
                return ptr == nullptr;
            }
        }
    };

    template<class T, class... Args> shared_ptr<T> make_shared(Args&&... args) {
        return shared_ptr<T>(new T(args...));
    }
}

#endif //JEOKERNEL_MEMORY_H
