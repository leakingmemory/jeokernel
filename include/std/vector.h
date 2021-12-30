//
// Created by sigsegv on 26.04.2021.
//

#ifndef JEOKERNEL_VECTOR_H
#define JEOKERNEL_VECTOR_H

#include <cstdint>
#include <utility>
#include <memory>
#include <new>

namespace std {

    template <class Class> struct vector_container_element {
        Class element;

        vector_container_element(const Class &cp) : element(cp) {
        }
        vector_container_element(Class &&mv) : element(move(mv)) {
        }
    };
    template <class Class> struct vector_container_element<Class *> {
        Class *element;

        vector_container_element(Class *ptr) : element(ptr) {}
    } __attribute__((__packed__));

    template <class T> class vector_iterator;

    template <class T> class vector_iterator_base {
        friend vector_iterator<T>;
    private:
        T *ptr;
    public:
        constexpr explicit vector_iterator_base(T *ptr) noexcept : ptr(ptr) {}

        constexpr T & operator *() {
            return *ptr;
        }

        constexpr bool operator ==(const vector_iterator_base &other) const noexcept {
            return ptr == other.ptr;
        }
        constexpr bool operator !=(const vector_iterator_base &other) const noexcept {
            return ptr != other.ptr;
        }
        constexpr bool operator <=(const vector_iterator_base &other) const noexcept {
            return ptr <= other.ptr;
        }
        constexpr bool operator >=(const vector_iterator_base &other) const noexcept{
            return ptr >= other.ptr;
        }
        constexpr bool operator <(const vector_iterator_base &other) const noexcept {
            return ptr < other.ptr;
        }
        constexpr bool operator >(const vector_iterator_base &other) const noexcept {
            return ptr > other.ptr;
        }
    };
    template <class T> class vector_iterator : public vector_iterator_base<T> {
    public:
        vector_iterator(T *ptr) : vector_iterator_base<T>(ptr) { }
        vector_iterator(const vector_iterator &cp) : vector_iterator_base<T>(cp.ptr) { }

        operator vector_iterator<const T> () const {
            vector_iterator<const T> cvec(this->ptr);
            return cvec;
        }

        constexpr vector_iterator operator ++() noexcept {
            return vector_iterator<T>(this->ptr++);
        }
        constexpr vector_iterator &operator ++(int) noexcept {
            ++this->ptr;
            return *this;
        }
        constexpr vector_iterator operator --() noexcept {
            return vector_iterator<T>(this->ptr--);
        }
        constexpr vector_iterator &operator --(int) noexcept {
            ++this->ptr;
            return *this;
        }
        constexpr vector_iterator &operator +=(int diff) noexcept {
            this->ptr += diff;
            return *this;
        }
        constexpr vector_iterator &operator -=(int diff) noexcept {
            this->ptr -= diff;
            return *this;
        }
    };

    template<class T, class Allocator = std::allocator<T>>
    class vector {
    public:
        typedef std::size_t size_type;
        typedef T &reference;
        typedef const T &const_reference;
        typedef vector_iterator<T> iterator;
        typedef vector_iterator<const T> const_iterator;
    private:
        struct vector_container : Allocator {
            vector_container_element<T> *_data;
            size_type _size;
            size_type _capacity;

            vector_container() : Allocator(), _data(nullptr), _size(0), _capacity(0) {
            }
        };
        static_assert(sizeof(vector_container_element<T>) == sizeof(T));

        vector_container c;

        Allocator &get_allocator() {
            return c;
        }
    public:

        constexpr const T *data() const noexcept {
            return (T *) (void *) c._data;
        }

        constexpr size_type size() const noexcept {
            return c._size;
        }

        constexpr size_type capacity() const noexcept {
            return c._capacity;
        }

        constexpr bool empty() const noexcept {
            return c._size == 0;
        }

        constexpr iterator begin() noexcept {
            return iterator((T *) (void *) c._data);
        }
        constexpr iterator end() noexcept {
            return iterator(((T *) (void *) c._data) + c._size);
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator((T *) (void *) c._data);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(((T *) (void *) c._data) + c._size);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(((T *) (void *) c._data));
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(((T *) (void *) c._data) + c._size);
        }

        vector() : c() {
        }
        vector(const vector &cp) : c() {
            reserve(cp.size());
            for (const auto &cp_elem : cp) {
                push_back(cp_elem);
            }
        }
        vector(vector &&mv) : c(mv.c) {
            mv.c._data = nullptr;
            mv.c._size = 0;
            mv.c._capacity = 0;
        }

        ~vector() {
            if (c._data != nullptr) {
                for (size_type i = 0; i < c._size; i++) {
                    c._data[i].~vector_container_element<T>();
                }
                get_allocator().deallocate((T *) (void *) c._data, c._capacity);
                c._data = nullptr;
                c._capacity = 0;
                c._size = 0;
            }
        }

        vector &operator =(const vector &cp) {
            if (c._data != nullptr) {
                for (size_type i = 0; i < c._size; i++) {
                    c._data[i].~vector_container_element<T>();
                }
            }
            c._size = 0;
            if (c._capacity < cp.size()) {
                reserve(cp.size());
            }
            for (const auto &cp_elem : cp) {
                push_back(cp_elem);
            }
            return *this;
        }
        vector &operator =(vector &&mv) {
            if (c._data != nullptr) {
                for (size_type i = 0; i < c._size; i++) {
                    c._data[i].~vector_container_element<T>();
                }
                get_allocator().deallocate((T *) (void *) c._data, c._capacity);
            }
            c = mv.c;
            mv.c._data = nullptr;
            mv.c._capacity = 0;
            mv.c._size = 0;
            return *this;
        }

        constexpr void clear() noexcept {
            if (c._data != nullptr) {
                for (size_type i = 0; i < c._size; i++) {
                    c._data[i].~vector_container_element<T>();
                }
                c._size = 0;
            }
        }

        constexpr size_type minimum_size() {
            if (sizeof(T) > 16) {
                return 1;
            } else {
                return 16 / sizeof(T);
            }
        }

        constexpr void reserve(size_type new_cap) {
            if (new_cap < minimum_size()) {
                new_cap = minimum_size();
            }
            if (c._data != nullptr) {
                if (c._capacity < new_cap) {
                    vector_container_element<T> *new_ptr = (vector_container_element<T> *) (void *) get_allocator().allocate(new_cap);
                    for (size_type i = 0; i < c._size; i++) {
                        new ((void *) &new_ptr[i]) vector_container_element<T>(move(c._data[i]));
                        c._data[i].~vector_container_element<T>();
                    }
                    get_allocator().deallocate((T *) (void *) c._data, c._capacity);
                    c._data = new_ptr;
                    c._capacity = new_cap;
                }
            } else {
                c._data = (vector_container_element<T> *) (void *) get_allocator().allocate(new_cap);
                c._capacity = new_cap;
            }
        }

        constexpr void shrink_to_fit() {
            if (c._data != nullptr && c._size < c._capacity) {
                T *new_ptr = get_allocator().allocate(c._size);
                for (size_type i = 0; i < c._size; i++) {
                    new ((void *) &new_ptr[i]) T(move(c._data[i]));
                    c._data[i].~T();
                }
                get_allocator().deallocate(c._data, c._capacity);
                c._data = new_ptr;
                c._capacity = c._size;
            }
        }

        constexpr reference front() {
            return *c._data;
        }

        constexpr const_reference front() const {
            return *c._data;
        }

        constexpr reference back() {
            T *ptr = c._size > 0 ? c._data[c._size - 1] : nullptr;
            return *ptr;
        }

        constexpr const_reference back() const {
            T *ptr = c._size > 0 ? c._data[c._size - 1] : nullptr;
            return *ptr;
        }

        reference at(size_type i) {
            vector_container_element<T> *ptr = i >= 0 && i < c._size ? &(c._data[i]) : nullptr;
            return ptr->element;
        }

        const_reference at(size_type i) const {
            const vector_container_element<T> *ptr = i >= 0 && i < c._size ? &(c._data[i]) : nullptr;
            return ptr->element;
        }

        reference operator[](size_type i) {
            return at(i);
        }

        const_reference operator[](size_type i) const {
            return at(i);
        }

        constexpr void push_back(const T &cp) {
            size_type exp = c._size + 1;
            if (exp >= c._capacity) {
                reserve(exp);
            }
            new ((void *) &(c._data[c._size])) vector_container_element<T>(cp);
            c._size = exp;
        }

        constexpr void push_back(T &&mv) {
            size_type exp = c._size + 1;
            if (exp >= c._capacity) {
                reserve(exp);
            }
            new ((void *) &(c._data[c._size])) vector_container_element<T>(std::move(mv));
            c._size = exp;
        }

        template<class... Args>
        constexpr reference emplace_back(Args &&... args) {
            size_type exp = c._size + 1;
            if (exp >= c._capacity) {
                reserve(exp);
            }
            T *ptr = new((void *) &(c._data[c._size])) T(args...);
            c._size = exp;
            return *ptr;
        }

        iterator erase(const_iterator position) {
            uint64_t index = &(*position) - &(*begin());
            c._data[index].~vector_container_element<T>();
            c._size--;
            for (uint64_t del = index; del < c._size; del++) {
                c._data[del] = c._data[del + 1];
            }
            auto iterator = begin();
            iterator += index;
            return iterator;
        }
    };
}

#endif //JEOKERNEL_VECTOR_H
