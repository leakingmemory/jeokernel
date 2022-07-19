//
// Created by sigsegv on 29.04.2021.
//

#ifndef JEOKERNEL_FUNCTIONAL_H
#define JEOKERNEL_FUNCTIONAL_H

namespace std {

    template <class R, class... Args> struct supercaller {
        virtual ~supercaller() {}
        virtual R invoke(Args... args) = 0;
        virtual supercaller<R,Args...> *copy() = 0;
    };

    template<class F, class R, class... Args>
    struct instructive_caller : public supercaller<R, Args...> {
        F f;

        instructive_caller(F f) : f(f) {
        }
        ~instructive_caller() {
        }

        R invoke(Args... args) override {
            return f(args...);
        }

        instructive_caller<F, R, Args...> *copy() override {
            return new instructive_caller<F, R, Args...>(f);
        }
    };

    template<class R, class... Args>
    class function;

    template<class R, class... Args>
    class function<R (Args...)> {
    private:
        supercaller<R, Args...> *_invoke;

    public:
        function() : _invoke(nullptr) {
        }
        template<class F>
        function(F f) {
            _invoke = new instructive_caller<F, R, Args...>(f);
        }
        function(const function &cp) {
            _invoke = cp._invoke->copy();
        }
        function(function &&mv) : _invoke(mv._invoke) {
            mv._invoke = nullptr;
        }

        function &operator = (const function &cp) {
            if (_invoke != nullptr) {
                delete _invoke;
            }
            _invoke = cp._invoke->copy();
            return *this;
        }

        function &operator = (function &&mv) {
            if (_invoke != nullptr) {
                delete _invoke;
            }
            _invoke = mv._invoke;
            mv._invoke = nullptr;
            return *this;
        }

        ~function() {
            if (_invoke != nullptr) {
                delete _invoke;
            }
        }

        R operator()(Args... args) {
            return _invoke->invoke(args...);
        }

        explicit operator bool () const noexcept {
            return _invoke != nullptr;
        }
    };

}

#endif //JEOKERNEL_FUNCTIONAL_H
