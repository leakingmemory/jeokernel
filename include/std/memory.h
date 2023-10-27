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
#ifndef WEAK_PTR_CRITICAL_SECTION

#include <concurrency/critical_section.h>

#define WEAK_PTR_CRITICAL_SECTION critical_section

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
            if (ptr != nullptr && ptr != mv.ptr) {
                Deleter deleter{};
                deleter(ptr);
            }
            ptr = mv.ptr;
            if (this != &mv) {
                mv.ptr = nullptr;
            }
            return *this;
        }

        unique_ptr &operator = (T *setptr) {
            if (ptr != nullptr && ptr != setptr) {
                Deleter deleter{};
                deleter(ptr);
            }
            ptr = setptr;
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

    template <class T, class Deleter> class weak_ptr;

    namespace impl {
        class weak_ptr_dir {
        private:
            uint32_t ref;
            uint32_t xref;
        public:
            weak_ptr_dir() : ref(1), xref(1) {}
            weak_ptr_dir(const weak_ptr_dir &) = delete;
            weak_ptr_dir(weak_ptr_dir &&) = delete;
            weak_ptr_dir &operator =(const weak_ptr_dir &) = delete;
            weak_ptr_dir &operator =(weak_ptr_dir &&) = delete;

            void acquire() {
                asm("lock incl %0;" : "+m"(ref));
            }

            uint32_t release() {
                uint32_t xref;
                asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(ref), "=rm"(xref)::"%rax");
                --xref;
                return xref;
            }

            bool xacquire() {
                while (true) {
                    uint8_t ret;
                    {
                        uint32_t xref = this->xref;
                        if (xref == 0) {
                            return false;
                        }
                        uint32_t cmp = xref;
                        ++xref;
                        asm("lock cmpxchgl %[newv], %[atom]; sete %[retv]" : [retv] "=q"(ret), [atom] "+m"(
                                this->xref), [cmpv] "+a"(cmp) : [newv] "r"(xref));
                    }
                    if ((ret & 1) == 1) {
                        return true;
                    }
                }
            }

            bool xrelease_or_take_ownership() {
                while (true) {
                    uint8_t ret;
                    {
                        uint32_t xref = this->xref;
                        if (xref <= 1) {
                            return false;
                        }
                        uint32_t cmp = xref;
                        --xref;
                        asm("lock cmpxchgl %[newv], %[atom]; sete %[retv]" : [retv] "=q"(ret), [atom] "+m"(
                                this->xref), [cmpv] "+a"(cmp) : [newv] "r"(xref));
                    }
                    if ((ret & 1) == 1) {
                        return true;
                    }
                }
            }

            uint32_t xrelease() {
                uint32_t xref;
                asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(this->xref), "=rm"(xref)::"%rax");
                --xref;
                return xref;
            }

            uint32_t xrefcount() {
                uint32_t xref;
                asm("xor %%rax, %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(this->xref), "=rm"(xref)::"%rax");
                return xref;
            }
        };

        class shared_ptr_container_typeerasure {
        private:
            void *ptr;
            uint32_t ref;
            uint32_t ticketgen;
            uint32_t currentticket;
            weak_ptr_dir *weak;
        public:
            constexpr shared_ptr_container_typeerasure() noexcept: ptr(nullptr), ref(1), ticketgen(0), currentticket(0), weak(nullptr) {
            }

            explicit shared_ptr_container_typeerasure(void *ptr) noexcept: ptr(ptr), ref(1), ticketgen(0), currentticket(0), weak(nullptr) {
            }

            shared_ptr_container_typeerasure(shared_ptr_container_typeerasure &&mv) = delete;

            shared_ptr_container_typeerasure(const shared_ptr_container_typeerasure &cp) = delete;

            shared_ptr_container_typeerasure &operator=(shared_ptr_container_typeerasure &&mv) = delete;

            shared_ptr_container_typeerasure &operator=(const shared_ptr_container_typeerasure &cp) = delete;

            virtual ~shared_ptr_container_typeerasure() {}

            void Weak(weak_ptr_dir *weak) {
                this->weak = weak;
            }
            weak_ptr_dir *Weak() {
                return weak;
            }

            void acquire() {
                asm("lock incl %0;" : "+m"(ref));
            }

            uint32_t release() {
                uint32_t xref;
                asm("xor %%rax, %%rax; dec %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(ref), "=rm"(xref)::"%rax");
                --xref;
                if (xref == 0 && weak != nullptr) {
                    if (weak->xrelease() != 0) {
                        return 1;
                    }
                    if (weak->release() == 0) {
                        delete weak;
                    }
                    weak = nullptr;
                }
                return xref;
            }

            uint32_t create_ticket() noexcept {
                uint32_t ticket;
                asm("xor %%rax, %%rax; inc %%rax; lock xaddl %%eax, %0; movl %%eax, %1" : "+m"(ticketgen), "=rm"(ticket) :: "%rax");
                return ticket;
            }

            void release_ticket() noexcept {
                asm("lock incl %0" : "+m"(currentticket));
            }

            void lock() noexcept {
                uint32_t ticket = create_ticket();
                while (ticket != currentticket) {
                    asm("pause");
                }
            }

            void unlock() noexcept {
                release_ticket();
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

    namespace impl {
        template<class T> class dynamic_pointer_cast;
    }

    template <class T, class Deleter = std::default_delete<T>> class shared_ptr {
        friend shared_ptr;
        template <class W, class X> friend class shared_ptr;
        template <class W> friend class impl::dynamic_pointer_cast;
        friend class weak_ptr<T,Deleter>;
    public:
        typedef T *pointer;
    private:
        impl::shared_ptr_container_typeerasure *container;
        pointer ptr;
        shared_ptr(impl::shared_ptr_container_typeerasure *container, pointer ptr) : container(container), ptr(ptr) {}
    public:
        shared_ptr() : container(nullptr), ptr(nullptr) {}
        explicit shared_ptr(pointer p) : container(new impl::shared_ptr_container<T,Deleter>(p)), ptr(p) {}
        ~shared_ptr() {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
                container = nullptr;
                ptr = nullptr;
            }
        }

        template <class P,class PDeleter> shared_ptr(shared_ptr<P,Deleter> &&mv) : container(mv.container), ptr(mv.ptr) {
            static_assert(std::is_assignable<T*,P*>::value);
            if (ptr != nullptr) {
                mv.container = nullptr;
                mv.ptr = nullptr;
            } else {
                container = nullptr;
            }
        }
        shared_ptr(shared_ptr<T,Deleter> &&mv) noexcept : container(mv.container), ptr(mv.ptr) {
            mv.container = nullptr;
            mv.ptr = nullptr;
        }
        shared_ptr(const shared_ptr<T,Deleter> &cp) : container(cp.container), ptr(cp.ptr) {
            if (container != nullptr) {
                container->acquire();
            }
        }
        template <class P,class PDeleter> shared_ptr(const shared_ptr<P,PDeleter> &cp) : container(cp.Container()), ptr(cp.ptr) {
            if (container != nullptr && ptr != nullptr) {
                container->acquire();
            } else {
                container = nullptr;
                ptr = nullptr;
            }
        };

        impl::shared_ptr_container_typeerasure *Container() const {
            return container;
        }

        template <class P,class PDeleter> shared_ptr &operator = (shared_ptr<P,PDeleter> &&mv) {
            static_assert(is_assignable<T*,P*>::value);
            if (((void *) this) == ((void *) &mv)) {
                return *this;
            }
            ptr = mv.ptr;
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            container = mv.container;
            mv.container = nullptr;
            return *this;
        }

        constexpr bool is_self(const std::shared_ptr<T> &other) {
            return this == &other;
        }
        template<class P> constexpr bool is_self(const std::shared_ptr<P> &other) {
            return false;
        }

        template <class P,class PDeleter> shared_ptr &operator = (const shared_ptr<P,PDeleter> &cp) {
            if (is_self(cp)) {
                return *this;
            }
            auto *prevContainer = container;
            container = cp.container;
            if constexpr(is_polymorphic<P>::value) {
                ptr = dynamic_cast<T *>(cp.ptr);
            } else {
                ptr = cp.ptr;
            }
            if (ptr == nullptr) {
                container = nullptr;
            }
            if (container != nullptr) {
                container->acquire();
            }
            if (prevContainer != nullptr) {
                if (prevContainer->release() == 0) {
                    delete prevContainer;
                }
            }
            return *this;
        }

        shared_ptr &operator = (shared_ptr &&mv) {
            if (is_self(mv)) {
                return *this;
            }
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            ptr = mv.ptr;
            container = mv.container;
            mv.container = nullptr;
            return *this;
        }

        shared_ptr &operator = (const shared_ptr &cp) {
            if (is_self(cp)) {
                return *this;
            }
            auto *prevContainer = container;
            ptr = cp.ptr;
            container = cp.container;
            if (container != nullptr) {
                container->acquire();
            }
            if (prevContainer != nullptr) {
                if (prevContainer->release() == 0) {
                    delete prevContainer;
                }
            }
            return *this;
        }

        shared_ptr &operator = (pointer ptr) {
            if (container != nullptr) {
                if (container->release() == 0) {
                    delete container;
                }
            }
            this->ptr = ptr;
            if (ptr != nullptr) {
                container = new impl::shared_ptr_container<T, Deleter>(ptr);
            } else {
                container = nullptr;
            }
            return *this;
        }

        T &operator * () const {
            return *ptr;
        }

        pointer operator->() const noexcept {
            return (pointer) ptr;
        }

        explicit operator bool () const {
            return container != nullptr && container->isset();
        }

        bool operator == (const shared_ptr<T> &other) const {
            if (other) {
                T *other_ptr = &(*other);
                return *this == other_ptr;
            }
            return false;
        }
        bool operator == (T *ptr) const {
            return this->ptr == ptr;
        }
    };

    namespace impl {
        template<class T>
        class dynamic_pointer_cast {
        private:
            std::shared_ptr<T> shptr{};
        public:
            template<class P, class PDeleter>
            dynamic_pointer_cast(shared_ptr<P, PDeleter> &&mv) {
                T *ptr = dynamic_cast<T *>(mv.ptr);
                if (ptr != nullptr) {
                    shptr.container = mv.container;
                    shptr.ptr = ptr;
                    mv.container = nullptr;
                    mv.ptr = nullptr;
                }
            }

            template<class P, class PDeleter>
            dynamic_pointer_cast(const shared_ptr<P, PDeleter> &cp) {
                T *ptr;
                if constexpr (is_polymorphic<P>::value) {
                    ptr = dynamic_cast<T *>(cp.ptr);
                } else {
                    ptr = cp.ptr;
                }
                auto *container = cp.container;
                if (container != nullptr && ptr != nullptr) {
                    container->acquire();
                    shptr.ptr = ptr;
                    shptr.container = container;
                }
            }

            std::shared_ptr<T> Get() {
                return shptr;
            }
        };
    }

    template<class T, class... Args> shared_ptr<T> make_shared(Args&&... args) {
        return shared_ptr<T>(new T(args...));
    }

    template<class T, class S> shared_ptr<T> dynamic_pointer_cast(const std::shared_ptr<S> &src) {
        impl::dynamic_pointer_cast<T> cst{src};
        return cst.Get();
    }

    template<class T, class S> shared_ptr<T> dynamic_pointer_cast(std::shared_ptr<S> &&src) {
        impl::dynamic_pointer_cast<T> cst{std::move(src)};
        return cst.Get();
    }

    template <class T, class Deleter = std::default_delete<T>> class weak_ptr {
    public:
        typedef T *pointer;
    private:
        impl::weak_ptr_dir *weak;
        impl::shared_ptr_container_typeerasure *container;
        pointer ptr;
    public:
        weak_ptr() : weak(nullptr), container(nullptr), ptr(nullptr) {}
        weak_ptr(shared_ptr<T,Deleter> shared) : weak(nullptr), container(nullptr), ptr(nullptr) {
            if (shared) {
                container = shared.container;
                ptr = shared.ptr;
                WEAK_PTR_CRITICAL_SECTION cli{};
                container->lock();
                if (container->Weak() != nullptr) {
                    container->unlock();
                    weak = container->Weak();
                } else {
                    container->Weak(new impl::weak_ptr_dir());
                    container->unlock();
                    weak = container->Weak();
                }
                weak->acquire();
            }
        }
        ~weak_ptr() {
            if (weak != nullptr && weak->release() == 0) {
                delete weak;
            }
            weak = nullptr;
        }
        weak_ptr(const weak_ptr &cp) : weak(cp.weak), container(cp.container), ptr(cp.ptr) {
            if (this == &cp) {
                return;
            }
            if (weak != nullptr) {
                weak->acquire();
            }
        }
        weak_ptr(weak_ptr &&mv) : weak(mv.weak), container(mv.container), ptr(mv.ptr) {
            if (this == &mv) {
                return;
            }
            mv.weak = nullptr;
            mv.container = nullptr;
            mv.ptr = nullptr;
        }
        weak_ptr &operator =(const weak_ptr &cp) {
            if (this == &cp) {
                return *this;
            }
            if (weak != nullptr) {
                if (weak->release() == 0) {
                    delete weak;
                }
            }
            weak = cp.weak;
            container = cp.container;
            ptr = cp.ptr;
            if (weak != nullptr) {
                weak->acquire();
            }
            return *this;
        }
        weak_ptr &operator =(weak_ptr &&mv) {
            if (this == &mv) {
                return *this;
            }
            if (weak != nullptr) {
                if (weak->release() == 0) {
                    delete weak;
                }
            }
            weak = mv.weak;
            container = mv.container;
            ptr = mv.container;
            return *this;
        }
        shared_ptr<T,Deleter> lock() {
            if (weak == nullptr) {
                return {};
            }
            if (!weak->xacquire()) {
                return {};
            }
            container->acquire();
            shared_ptr<T,Deleter> shared{container, ptr};
            weak->xrelease_or_take_ownership();
            return shared;
        }
    };
}

#endif //JEOKERNEL_MEMORY_H
