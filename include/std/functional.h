//
// Created by sigsegv on 29.04.2021.
//

#ifndef JEOKERNEL_FUNCTIONAL_H
#define JEOKERNEL_FUNCTIONAL_H

namespace std {

    template <class R, class... Args> struct supercaller {
        virtual R invoke(Args... args) = 0;
    };

    template<class F, class R, class... Args>
    struct instructive_caller : public supercaller<R, Args...> {
        F f;

        instructive_caller(F f) : f(f) {
        }

        R invoke(Args... args) override {
            return f(args...);
        }
    };

    template<class R, class... Args>
    class function;

    template<class R, class... Args>
    class function<R (Args...)> {
    private:
        supercaller<R, Args...> *_invoke;

    public:
        template<class F>
        function(F f) {
            _invoke = new instructive_caller<F, R, Args...>(f);
        }

        ~function() {
            delete _invoke;
        }

        R operator()(Args... args) {
            return _invoke->invoke(args...);
        }
    };

}

#endif //JEOKERNEL_FUNCTIONAL_H
