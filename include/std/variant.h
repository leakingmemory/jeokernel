//
// Created by sigsegv on 4/22/26.
//

#ifndef JEOKERNEL_STD_VARIANT_H
#define JEOKERNEL_STD_VARIANT_H

#include <cstdint>
#include <utility>
#include <type_traits>

#include "variant.h"

namespace std {
    namespace detail {
        template <const int idx, typename Tdef, typename... T> struct type_from_index {
            static constexpr int max = -1;
            typedef Tdef type;
        };
        template <const int idx, typename Tdef, typename T> struct type_from_index<idx,Tdef,T> {
            static constexpr int max = 0;
            typedef std::conditional<idx == max, T, Tdef>::type type;
        };
        template <const int idx, typename Tdef, typename T, typename... Tres> struct type_from_index<idx,Tdef,T,Tres...> {
            static constexpr int max = type_from_index<idx, Tdef, Tres...>::max + 1;
            typedef std::conditional<idx == max, T, typename type_from_index<idx, Tdef, Tres...>::type>::type type;
        };
        template <typename... T> union variant_union {
            static constexpr int count = -1;
        };
        template <typename T> union variant_union<T> {
            static constexpr int count = 1;
            T value;
            constexpr variant_union() {}
            constexpr variant_union(T &&mv) : value(mv) {}
            constexpr variant_union(const T &cp) : value(cp) {}
        };
        template <> union variant_union<char> {
            static constexpr int count = 1;
            char value;
            constexpr variant_union() {}
            constexpr variant_union(char ch) : value(ch) {}
        };
        template <typename T, typename... Tres> union variant_union<T, Tres...> {
            static constexpr int count = variant_union<Tres...>::count + 1;
            T value;
            variant_union<Tres...> rest;
            constexpr variant_union(T &&mv) : value(mv) {}
            constexpr variant_union(const T &cp) : value(cp) {}
            template <typename Targ> constexpr variant_union(Targ bv) requires (std::is_same<variant_union<Tres...>,variant_union<char>>::value) : rest(bv) {}
            template <typename Targ> constexpr variant_union(Targ &&mv) requires (!std::is_same<variant_union<Tres...>,variant_union<char>>::value) : rest(std::move(mv)) {}
            template <typename Targ> constexpr variant_union(const Targ &cp) requires (!std::is_same<variant_union<Tres...>,variant_union<char>>::value) : rest(std::move(cp)) {}
        };
        template <typename... T> struct first_type {
            typedef int type;
        };
        template <typename T> struct first_type<T> {
            typedef T type;
        };
        template <typename T, typename... Tres> struct first_type<T,Tres...> {
            typedef T type;
        };
        template <const size_t idx, typename... T> constexpr type_from_index<idx, typename first_type<T...>::type, T...>::type &variant_union_get(variant_union<T...> &u) {
            constexpr int max = variant_union<T...>::count - 1;
            if constexpr(max == 0) {
                return u.value;
            } else {
                constexpr int raw_idx = max - idx;
                if constexpr(raw_idx <= 0) {
                    return u.value;
                } else {
                    return variant_union_get<idx>(u.rest);
                }
            }
        }
        template <const size_t idx, typename... T> constexpr const type_from_index<idx, typename first_type<T...>::type, T...>::type &variant_union_get(const variant_union<T...> &u) {
            constexpr int max = variant_union<T...>::count - 1;
            if constexpr(max == 0) {
                return u.value;
            } else {
                constexpr int raw_idx = max - idx;
                if constexpr(raw_idx <= 0) {
                    return u.value;
                } else {
                    return variant_union_get<idx>(u.rest);
                }
            }
        }
        template <typename... T> struct max_size {
            static constexpr size_t value = 0;
        };
        template <typename T> struct max_size<T> {
            static constexpr size_t value = sizeof(T);
        };
        template <typename T, typename... Trest> struct max_size<T, Trest...> {
            static constexpr size_t value = max_size<Trest...>::value > sizeof(T) ? max_size<Trest...>::value : sizeof(T);
        };
        static_assert(max_size<bool>::value == sizeof(bool));
        static_assert(max_size<int>::value == sizeof(int));
        static_assert(max_size<uint64_t>::value == sizeof(uint64_t));
        static_assert(max_size<bool,int>::value == sizeof(int));
        static_assert(max_size<int,bool>::value == sizeof(int));
        static_assert(max_size<bool,int,uint64_t>::value == sizeof(uint64_t));
        static_assert(max_size<bool,uint64_t,int>::value == sizeof(uint64_t));
        static_assert(max_size<int,bool,uint64_t>::value == sizeof(uint64_t));
        static_assert(max_size<int,uint64_t,bool>::value == sizeof(uint64_t));
        static_assert(max_size<uint64_t,bool,int>::value == sizeof(uint64_t));
        static_assert(max_size<uint64_t,int,bool>::value == sizeof(uint64_t));
        template <typename Tval, typename... T> struct type_index {
            static constexpr int max = -1;
            static constexpr int value = -1;
        };
        template <typename Tval, typename T> struct type_index<Tval,T> {
            static constexpr int max = 0;
            static constexpr int value = std::is_same<Tval,T>::value ? 0 : -1;

        };
        template <typename Tval, typename T, typename... T2> struct type_index<Tval,T,T2...> {
            static constexpr int max = type_index<Tval, T2...>::max + 1;
            static constexpr int value = std::is_same<Tval,T>::value ? max : type_index<Tval,T2...>::value;
        };
        static_assert(type_index<bool,int>::value == -1);
        template <typename... T> struct variant_storage {
            variant_union<T...> data;
            int type;
            template <typename Targ> constexpr variant_storage(Targ &&mv) : data(std::forward<Targ>(mv)), type(type_index<Targ,T...>::value) {}
            template <typename Tres> constexpr Tres *as_ptr() {
                return &variant_union_get<type_index<Tres,T...>::value>(data);
            }
            template <typename Tres> constexpr const Tres *as_ptr() const {
                return &variant_union_get<type_index<Tres,T...>::value>(data);
            }
            template <const int idx> constexpr bool is() const {
                return idx == type;
            }
            template <const int max_idx, typename Visitor> constexpr decltype(auto) call_visitor(Visitor &&v) {
                if (max_idx == type) {
                    typename type_from_index<max_idx,T...>::type &ref = *(as_ptr<typename type_from_index<max_idx,T...>::type>());
                    return v(ref);
                }
                if constexpr(max_idx > 0) {
                    return call_visitor<max_idx - 1,Visitor>(std::forward<Visitor>(v));
                }
                typename type_from_index<0,T...>::type &ref = *(as_ptr<typename type_from_index<0,T...>::type>());
                return v(ref);
            }
            template <const int max_idx, typename Visitor> constexpr decltype(auto) call_visitor(Visitor &&v) const {
                if (max_idx == type) {
                    const typename type_from_index<max_idx,T...>::type &ref = *(as_ptr<typename type_from_index<max_idx,T...>::type>());
                    return v(ref);
                }
                if constexpr(max_idx > 0) {
                    return call_visitor<max_idx - 1,Visitor>(std::forward<Visitor>(v));
                }
                const typename type_from_index<0,T...>::type &ref = *(as_ptr<typename type_from_index<0,T...>::type>());
                return v(ref);
            }
            template <typename Visitor> constexpr decltype(auto) visit(Visitor &&v) {
                return call_visitor<type_index<int, T...>::max,Visitor>(std::forward<Visitor>(v));
            }
            template <typename Visitor> constexpr decltype(auto) visit(Visitor &&v) const {
                return call_visitor<type_index<int, T...>::max,Visitor>(std::forward<Visitor>(v));
            }
            constexpr ~variant_storage() noexcept {
                visit([](auto &v) {
                    using type = std::remove_reference<decltype(v)>::type;
                    if constexpr(!std::is_pointer<type>::value &&
                                !std::is_same<char,type>::value) {
                        v.~decltype(v)();
                    }
                });
            }
        };
        static_assert(type_index<bool,bool>::value == 0);
        static_assert(type_index<int,bool>::value == -1);
        static_assert(type_index<bool,bool,int>::value == 1);
        static_assert(type_index<int,bool,int>::value == 0);
        static_assert(type_index<char *,bool,int>::value == -1);
        static_assert(type_index<bool,bool,int,char *>::value == 2);
        static_assert(type_index<int,bool,int,char *>::value == 1);
        static_assert(type_index<char *,bool,int,char *>::value == 0);

        class variant_visitor;
    }
    template <typename... T> class variant {
        friend detail::variant_visitor;
    private:
        detail::variant_storage<T...> container;
    public:
        variant() = delete;
        constexpr variant(const variant<T...> &cp) : container() {
            this->container.type = cp.container.type;
            cp.container.visit([this] (const auto &cpval) {
                const typename std::remove_cv<typeof(cpval)>::type *ptr = this->container->as_ptr();
                ptr = new (reinterpret_cast<void *>(ptr)) std::remove_cv<typeof(cpval)>::type(*ptr);
            });
        }
        constexpr variant(variant<T...> &&mv) : container() {
            this->container.type = mv.container.type;
            mv.container.visit([this] (auto &&mvval) {
                typename std::remove_cv<typeof(mvval)>::type *ptr = this->container->as_ptr();
                ptr = new (reinterpret_cast<void *>(ptr)) std::remove_cv<typeof(mvval)>::type(std::move(*ptr));
            });
        }
        template <typename Targ> constexpr variant(const Targ &cp) : container(cp) {
        }
        template <typename Targ> constexpr variant(Targ &&mv) : container(std::forward<Targ>(mv)) {
        }
        constexpr ~variant() noexcept = default;
    };

    namespace detail {
        class variant_visitor {
        public:
            template <typename Visitor, typename... T> static constexpr decltype(auto) visit(Visitor &&v, variant<T...> &&cp) {
                return cp.container.template visit<Visitor>(std::forward<Visitor>(v));
            }
            template <typename Visitor, typename... T> static constexpr decltype(auto) visit(Visitor &&v, const variant<T...> &cp) {
                return cp.container.template visit<Visitor>(std::forward<Visitor>(v));
            }
            /*template <typename Visitor, typename... T, typename... Args, typename R> static constexpr R visit(Visitor &v, const variant<T...> &cp, Args... args) {
                return cp.container.visit(v, cp.index, std::forward(args)...);
            }*/
        };
    }

    template <typename Visitor, typename... T> constexpr decltype(auto) visit(Visitor &&v, variant<T...> &&cp) {
        return detail::variant_visitor::visit<Visitor,T...>(std::forward<Visitor>(v), std::forward<variant<T...>>(cp));
    }
    template <typename Visitor, typename... T> constexpr decltype(auto) visit(Visitor &&v, const variant<T...> &cp) {
        return detail::variant_visitor::visit<Visitor,T...>(std::forward<Visitor>(v), cp);
    }
    /*template <typename Visitor, typename... Args, typename R> constexpr R visit(Visitor &&v, const variant<Args...> &&cp, Args&&... args) {
        return detail::variant_visitor::visit(std::forward(v), std::forward(cp), std::forward(args)...);
    }*/

    namespace test {
        struct VariantTest1 {
            constexpr int operator ()(const char *v) const {
                return 1;
            }
            constexpr int operator ()(const char v) const {
                return v;
            }
        };

        static_assert(std::visit(VariantTest1(), std::variant<const char *, char>(static_cast<const char *>("AB"))) == 1);
        static_assert(std::visit(VariantTest1(), std::variant<char *, char>(static_cast<char>('a'))) == 'a');
    }
}


#endif //JEOKERNEL_VARIANT_H
