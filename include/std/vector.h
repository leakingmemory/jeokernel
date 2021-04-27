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
        T *_data;
        size_type _size;
        size_type _capacity;
        Allocator _allocator;
    public:

        constexpr const T *data() const noexcept {
            return _data;
        }

        constexpr size_type size() const noexcept {
            return _size;
        }

        constexpr size_type capacity() const noexcept {
            return _capacity;
        }

        constexpr bool empty() const noexcept {
            return _size == 0;
        }

        constexpr iterator begin() noexcept {
            return iterator(_data);
        }
        constexpr iterator end() noexcept {
            return iterator(_data + _size);
        }
        constexpr const_iterator begin() const noexcept {
            return const_iterator(_data);
        }
        constexpr const_iterator end() const noexcept {
            return const_iterator(_data + _size);
        }
        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(_data);
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(_data + _size);
        }

        vector() : _data(nullptr), _size(0), _capacity(0), _allocator() {}

        ~vector() {
            if (_data != nullptr) {
                for (size_type i = 0; i < _size; i++) {
                    _data[i].~T();
                }
                _allocator.deallocate(_data, _capacity);
                _data = nullptr;
                _capacity = 0;
                _size = 0;
            }
        }

        constexpr void reserve(size_type new_cap) {
            if (_data != nullptr) {
                if (_capacity < new_cap) {
                    T *new_ptr = _allocator.allocate(new_cap);
                    for (size_type i = 0; i < _size; i++) {
                        new ((void *) &new_ptr[i]) T(move(_data[i]));
                        _data[i].~T();
                    }
                    _allocator.deallocate(_data, _capacity);
                    _data = new_ptr;
                    _capacity = new_cap;
                }
            } else {
                _data = _allocator.allocate(new_cap);
                _capacity = new_cap;
            }
        }

        constexpr void shrink_to_fit() {
            if (_data != nullptr && _size < _capacity) {
                T *new_ptr = _allocator.allocate(_size);
                for (size_type i = 0; i < _size; i++) {
                    new ((void *) &new_ptr[i]) T(move(_data[i]));
                    _data[i].~T();
                }
                _allocator.deallocate(_data, _capacity);
                _data = new_ptr;
                _capacity = _size;
            }
        }

        constexpr reference front() {
            return *_data;
        }

        constexpr const_reference front() const {
            return *_data;
        }

        constexpr reference back() {
            T *ptr = _size > 0 ? _data[_size - 1] : nullptr;
            return *ptr;
        }

        constexpr const_reference back() const {
            T *ptr = _size > 0 ? _data[_size - 1] : nullptr;
            return *ptr;
        }

        reference at(size_type i) {
            T *ptr = i >= 0 && i < _size ? &(_data[i]) : nullptr;
            return *ptr;
        }

        const_reference at(size_type i) const {
            T *ptr = i >= 0 && i < _size ? &(_data[i]) : nullptr;
            return *ptr;
        }

        reference operator[](size_type i) {
            return at(i);
        }

        const_reference operator[](size_type i) const {
            return at(i);
        }

        constexpr void push_back(const T &cp) {
            size_type exp = _size + 1;
            if (exp >= _capacity) {
                reserve(exp);
            }
            new ((void *) &(_data[_size])) T(cp);
            _size = exp;
        }

        constexpr void push_back(T &&mv) {
            size_type exp = _size + 1;
            if (exp >= _capacity) {
                reserve(exp);
            }
            new ((void *) &(_data[_size])) T(std::move(mv));
            _size = exp;
        }

        template<class... Args>
        constexpr reference emplace_back(Args &&... args) {
            size_type exp = _size + 1;
            if (exp >= _capacity) {
                reserve(exp);
            }
            T *ptr = new((void *) &(_data[_size])) T(args...);
            _size = exp;
            return *ptr;
        }
    };
}

#endif //JEOKERNEL_VECTOR_H
