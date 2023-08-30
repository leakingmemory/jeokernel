//
// Created by sigsegv on 06.05.2021.
//

#ifndef JEOKERNEL_OPTIONAL_H
#define JEOKERNEL_OPTIONAL_H

#include <utility>

namespace std {
    template<class T>
    class optional {
    private:
        uint8_t _data[sizeof(T)];
        T *ptr;
    public:
        typedef T value_type;

        constexpr optional()
        noexcept : _data(), ptr(nullptr) {
        }

        template<class U = value_type>
        constexpr optional(U &&value) {
            ptr = new((void *) &(_data[0])) T(value);
        }

        constexpr optional( const optional& other ) : _data(), ptr(nullptr) {
            static_assert(sizeof(*ptr) <= sizeof(_data));
            static_assert(sizeof(*ptr) == sizeof(*other));
            if (other) {
                ptr = new((void *) &(_data[0])) T(*other);
            }
        }

        constexpr optional( optional&& other ) noexcept {
            static_assert(sizeof(*ptr) <= sizeof(_data));
            static_assert(sizeof(*ptr) == sizeof(*other));
            if (other) {
                ptr = new((void *) &(_data[0])) T(std::move(*other));
            } else {
                ptr = nullptr;
            }
        }

        optional &operator =( const optional &other ) {
            static_assert(sizeof(*ptr) <= sizeof(_data));
            static_assert(sizeof(*ptr) == sizeof(*other));
            if (!other) {
                if (ptr != nullptr) {
                    ptr->~T();
                    ptr = nullptr;
                }
                return *this;
            }
            if (ptr != nullptr) {
                *ptr = *other;
            } else {
                ptr = new((void *) &(_data[0])) T(*other);
            }
            return *this;
        }

        optional &operator =(optional &&other) noexcept {
            static_assert(sizeof(*ptr) <= sizeof(_data));
            static_assert(sizeof(*ptr) == sizeof(*other));
            if (!other) {
                if (ptr != nullptr) {
                    ptr->~T();
                    ptr = nullptr;
                }
                return *this;
            }
            if (ptr != nullptr) {
                *ptr = std::move(*other);
            } else {
                ptr = new((void *) &(_data[0])) T(std::move(*other));
            }
            return *this;
        }

        ~optional() {
            if (ptr != nullptr) {
                ptr->~T();
                ptr = nullptr;
            }
        }

        std::optional<T> & operator = (const T &cp) {
            if (ptr != nullptr) {
                ptr->~T();
            }
            ptr = new((void *) &(_data[0])) T(cp);
            return *this;
        }

        T &operator*() const {
            return *ptr;
        }

        constexpr T* operator->() {
            return ptr;
        }

        operator bool() const noexcept {
            return ptr != nullptr;
        }
    };
}

#endif //JEOKERNEL_OPTIONAL_H
