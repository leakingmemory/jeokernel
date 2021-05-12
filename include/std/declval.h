//
// Created by sigsegv on 5/12/21.
//

#ifndef JEOKERNEL_DECLVAL_H
#define JEOKERNEL_DECLVAL_H

namespace std {
    namespace detail {
        template<class T>
        auto try_add_rvalue_reference(int) -> type_identity<T &&>;

        template<class T>
        auto try_add_rvalue_reference(...) -> type_identity<T>;
    }
    template<class T>
    struct add_rvalue_reference : decltype(detail::try_add_rvalue_reference<T>(0)) {
    };

    template<class T>
    constexpr typename add_rvalue_reference<T>::type declval() noexcept;

}
#endif //JEOKERNEL_DECLVAL_H
