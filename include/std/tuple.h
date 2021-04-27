//
// Created by sigsegv on 27.04.2021.
//

#ifndef JEOKERNEL_TUPLE_H
#define JEOKERNEL_TUPLE_H


namespace std {
    template<class... Types> class tuple;

    template <std::size_t i, class Tuple> struct tuple_element {
    };
    template <std::size_t i, class T, class... Types> struct tuple_element_deref : tuple_element<i, tuple<T, Types...>> {
        static_assert(i > 0);
        typedef typename std::conditional<i == 0,T,typename tuple_element_deref<i - 1, Types...>::type>::type type;
    };
    template <class T, class... Types> struct tuple_element_deref<0, T, Types...> : tuple_element<0, tuple<T, Types...>> {
        typedef T type;
    };

    template <class T, class... Types> struct tuple_onion {
        typename std::conditional<sizeof...(Types) != 0, tuple_onion<Types...>,void>::type layer;
        T obj;

        tuple_onion() : obj(), layer() {}
        tuple_onion(T &&obj, Types&&... additional) : obj(std::move(obj)), layer(std::move(additional...)) {}
        tuple_onion(const T &obj, const Types&... additional) : obj(obj), layer(additional...) {}

        tuple_onion(const tuple_onion &) = default;
        tuple_onion(tuple_onion &&) = default;
        tuple_onion &operator =(const tuple_onion &) = default;
        tuple_onion &operator =(tuple_onion &&) = default;

        template <std::size_t i> constexpr const typename std::conditional<i == 0,T,typename tuple_element_deref<i,T,Types...>::type>::type &const_at() const noexcept {
            if (i == 0) {
                return obj;
            } else {
                return layer.template const_at<i - 1>();
            }
        }
        template <std::size_t i> constexpr typename std::conditional<i == 0, T, typename tuple_element_deref<i,T,Types...>::type>::type &at() const noexcept {
            if (i == 0) {
                return obj;
            } else {
                return layer.template at<i - 1>();
            }
        }
    };

    template <class T> struct tuple_onion<T> {
        T obj;

        tuple_onion() : obj() {}
        tuple_onion(T &&obj) : obj(std::move(obj)) {}
        tuple_onion(const T &obj) : obj(obj) {}

        tuple_onion(const tuple_onion &) = default;
        tuple_onion(tuple_onion &&) = default;
        tuple_onion &operator =(const tuple_onion &) = default;
        tuple_onion &operator =(tuple_onion &&) = default;

        template <std::size_t i> constexpr const T &const_at() const noexcept {
            return obj;
        }
        template <std::size_t i> constexpr T &at() const noexcept {
            return obj;
        }
    };

    template<class... Types> class tuple {
    private:
        tuple_onion<Types...> onion;
    public:
        constexpr tuple() : onion() {}
        tuple(Types&&... args) : onion(args...) {}
        tuple(const Types&... args) : onion(args...) {}
        tuple(tuple<Types...> &&) = default;
        tuple(const tuple<Types...> &) = default;
        tuple &operator =(const tuple &) = default;
        tuple &operator =(tuple &&) = default;

        constexpr const tuple_onion<Types...> &const_onion() const noexcept {
            return onion;
        }
        constexpr tuple_onion<Types...> &get_onion() const noexcept {
            return onion;
        }
    };

    template<class... Types> std::tuple<Types...> make_tuple(const Types&... args) {
        return tuple<Types...>(args...);
    }

    template<class... Types> std::tuple<Types...> make_tuple(Types&&... args) {
        return tuple<Types...>(args...);
    }

    template<std::size_t I, class T, class... Types> constexpr typename std::conditional<I == 0,T,typename std::tuple_element_deref<I, T, Types...>::type>::type &
    get(tuple<Types...>& t) noexcept {
        return t.get_onion().template at<I>();
    }

    template<std::size_t I, class T, class... Types> constexpr const typename std::conditional<I == 0,T,typename std::tuple_element_deref<I, T, Types...>::type>::type &
    get(const tuple<T, Types...>& t) noexcept {
        return t.const_onion().template const_at<I>();
    }
}



#endif //JEOKERNEL_TUPLE_H
