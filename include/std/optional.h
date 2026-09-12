//
// Created by sigsegv on 06.05.2021.
//

#ifndef JEOKERNEL_OPTIONAL_H
#define JEOKERNEL_OPTIONAL_H

#include <utility>
#include <variant>
#include <concepts>

namespace std {
    namespace detail {
        struct empty_optional {
            constexpr empty_optional() noexcept = default;
            constexpr empty_optional(const empty_optional &) noexcept : empty_optional() {}
            constexpr empty_optional(empty_optional &&) noexcept : empty_optional() {}
            constexpr ~empty_optional() noexcept = default;
            constexpr empty_optional & operator = (const empty_optional &) = default;
            constexpr empty_optional & operator = (empty_optional &&) = default;
        };
        template <> union variant_union<empty_optional> {
            static constexpr int count = 1;
            empty_optional value;
            constexpr variant_union() noexcept = default;
            constexpr variant_union(const empty_optional &) noexcept : variant_union() {}
            constexpr variant_union(empty_optional &&) noexcept : variant_union() {}
            constexpr ~variant_union() noexcept = default;
            constexpr variant_union<empty_optional> & operator = (const variant_union<empty_optional> &) = default;
            constexpr variant_union<empty_optional> & operator = (variant_union<empty_optional> &&) = default;
        };
        template <class T> class optional_variant_value_extraction_visitor {
        public:
            constexpr optional_variant_value_extraction_visitor() noexcept = default;
            typedef std::remove_reference_t<T> value_type;
            constexpr T operator () (T value) {
                return value;
            }
            [[noreturn]] T operator () (empty_optional &) {
#if defined(__x86_64__)
                asm("ud2");
#elif defined(__aarch64__)
                __builtin_trap();
#endif
                while (true) {
                }
            }
        };
        template <class T> constexpr T optional_variant_value_extraction(variant<typename optional_variant_value_extraction_visitor<T>::value_type,empty_optional> &var) {
            return std::visit(optional_variant_value_extraction_visitor<T>(), var);
        }
    }

    template<class T>
    class optional {
    private:
        std::variant<T,detail::empty_optional> _data;
    public:
        typedef T value_type;

        constexpr optional()
        noexcept : _data(detail::empty_optional()) {
        }

        constexpr optional(in_place_t) : _data(in_place_type<T>) {
        }
        template <typename... Arg> constexpr optional(in_place_t, Arg &&...arg) : _data(in_place_type<T>, forward<Arg...>(arg...)) {
        }

        constexpr optional(T &&value) : _data(std::move(value)) {
        }
        constexpr optional(const T &value) : _data(value) {
        }
        template<class U = value_type> requires convertible_to<U,T>
        constexpr optional(const U &value) : _data(value) {
        }

        constexpr optional( const optional& other ) : _data(other._data) {
        }

        constexpr optional( optional&& other ) noexcept : _data(std::move(other._data)) {
        }

        constexpr optional &operator =( const optional &other ) = default;

        constexpr optional &operator =(optional &&other) noexcept {
            _data = std::move(other._data);
            return *this;
        }

        constexpr ~optional() = default;

        constexpr std::optional<T> & operator = (const T &cp) {
            _data = cp;
            return *this;
        }
        constexpr void reset() {
            _data = detail::empty_optional();
        }

        constexpr const T &operator*() const {
            struct {
                constexpr const T & operator () (const T &ref) {
                    return ref;
                }
                [[noreturn]] const T & operator () (const detail::empty_optional &) {
#if defined(__x86_64__)
                    asm("ud2");
#elif defined(__aarch64__)
                    __builtin_trap();
#endif
                    while (true) {}
                }
            } visitor;
            return std::visit(visitor, _data);
        }

        constexpr T* operator->() {
            struct {
                constexpr T * operator () (T &ref) {
                    return &ref;
                }
                constexpr T * operator () (detail::empty_optional &) {
                    return nullptr;
                }
            } visitor;
            return std::visit(visitor, _data);
        }

        constexpr operator bool() const noexcept {
            struct {
                constexpr bool operator () (const T &) const {
                    return true;
                }
                constexpr bool operator () (const detail::empty_optional &) const {
                    return false;
                }
            } visitor;
            return std::visit(visitor, _data);
        }
    };

    static_assert(optional<uint32_t>(static_cast<uint32_t>(13U)).operator *() == 13);
    static_assert(optional<uint32_t>(static_cast<uint32_t>(13U)).operator bool());
    static_assert(!optional<uint32_t>().operator bool());

    template <typename T> constexpr optional<T> make_optional() {
        return optional<T>(in_place);
    }
    template <typename T, typename... Arg> constexpr optional<T> make_optional(Arg &&...arg) {
        return optional<T>(in_place, forward<Arg...>(arg...));
    }

    namespace test {
        struct optional_test_type {
            uint32_t v;

            constexpr optional_test_type() : v(42) {
            }
            constexpr optional_test_type(uint32_t v) : v(v) {
            }
            constexpr optional_test_type(const optional_test_type &cp) : v(cp.v) {
            }
            constexpr optional_test_type(optional_test_type &&mv) : v(mv.v) {
                mv.v = 0;
            }
            constexpr optional_test_type & operator = (const optional_test_type &cp) {
                v = cp.v;
                return *this;
            }
            constexpr operator uint32_t () const {
                return v;
            }
        };
        static_assert(std::make_optional<optional_test_type>(13U).operator *().operator uint32_t() == 13);
        static_assert(std::make_optional<optional_test_type>(13U).operator bool());
        static_assert(std::make_optional<optional_test_type>().operator *().operator uint32_t() == 42);
        static_assert(std::make_optional<optional_test_type>().operator bool());

        constexpr std::optional<optional_test_type> copy_of(uint32_t value) {
            auto orig = std::make_optional<optional_test_type>(value);
            std::optional<optional_test_type> cp{orig};
            orig = std::optional<optional_test_type>();
            return cp;
        }
        constexpr std::optional<optional_test_type> copy_of_empty() {
            std::optional<optional_test_type> orig{};
            std::optional<optional_test_type> cp{orig};
            orig = std::optional<optional_test_type>();
            return cp;
        }
        constexpr std::optional<optional_test_type> overwrite_copy(std::optional<optional_test_type> &&input, const std::optional<optional_test_type> &overwr) {
            std::optional<optional_test_type> obj{std::move(input)};
            obj = overwr;
            return obj;
        }
        constexpr std::optional<optional_test_type> overwrite_move(std::optional<optional_test_type> &&input, std::optional<optional_test_type> &&overwr) {
            std::optional<optional_test_type> obj{std::move(input)};
            obj = std::move(overwr);
            return obj;
        }
        static_assert(copy_of(14U).operator *() == 14U);
        static_assert(!copy_of_empty().operator bool());
        static_assert(overwrite_copy(std::move(copy_of(14U)), copy_of(14U)).operator *() == 14U);
        static_assert(overwrite_copy(std::move(copy_of_empty()), copy_of(14U)).operator *() == 14U);
        static_assert(!overwrite_copy(std::move(copy_of(14U)), copy_of_empty()).operator bool());
        static_assert(!overwrite_copy(std::move(copy_of_empty()), copy_of_empty()).operator bool());
        static_assert(overwrite_move(std::move(copy_of(14U)), copy_of(14U)).operator *() == 14U);
        static_assert(overwrite_move(std::move(copy_of_empty()), copy_of(14U)).operator *() == 14U);
        static_assert(!overwrite_move(std::move(copy_of(14U)), copy_of_empty()).operator bool());
        static_assert(!overwrite_move(std::move(copy_of_empty()), copy_of_empty()).operator bool());
    }
}

#endif //JEOKERNEL_OPTIONAL_H
