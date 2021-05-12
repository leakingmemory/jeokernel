//
// Created by sigsegv on 5/12/21.
//

#ifndef JEOKERNEL_CHRONO_H
#define JEOKERNEL_CHRONO_H

#include <std/ratio.h>
#include <std/utility.h>

namespace std {
    namespace chrono {
        template <class Rep, class Period = std::ratio<1>> class duration {
        private:
            Rep r;
        public:
            typedef Rep rep;
            typedef Period period;

            constexpr duration() = default;
            duration( const duration& ) = default;
            template <class Rep2>
            constexpr explicit duration(const Rep2 &r) : r(r) {
            }

            constexpr rep count() const {
                return r;
            }
        };

        template <class Rep1, class Period, class Rep2> constexpr duration<typename std::common_type<Rep1, Rep2>::type, Period>
                operator *(const duration<Rep1, Period> &dur, const Rep2 &r2) {
                    return duration<typename std::common_type<Rep1, Rep2>::type, Period>(dur.count() * r2);
                }

        template <class ToDuration, class Rep, class Period>
        constexpr ToDuration duration_cast(const duration<Rep,Period>& d) {
            typename ToDuration::rep rep{d.count() * ToDuration::period::den * Period::num / (Period::den * ToDuration::period::num)};
            return ToDuration(rep);
        }

        static_assert(duration<int64_t,std::ratio<1,100>>(10).count() == 10);
        static_assert(duration<int64_t,std::ratio<1,1000>>(100).count() == 100);
        static_assert(duration_cast<duration<int64_t,std::ratio<1,100>>>(duration<int64_t,std::ratio<1,1000>>(100)).count() == 10);

        typedef duration<int64_t, std::nano> nanoseconds;
        typedef duration<int64_t, std::micro> microseconds;
        typedef duration<int64_t, std::milli> milliseconds;
        typedef duration<int64_t> seconds;
        typedef duration<int64_t, std::ratio<60>> minutes;
        typedef duration<int64_t, std::ratio<3600>> hours;
        typedef duration<int64_t, std::ratio<24*3600>> days;
        typedef duration<int64_t, std::ratio<7*24*3600>> weeks;
        typedef duration<int64_t, std::ratio<30*24*3600>> months;
        typedef duration<int64_t, std::ratio<365*24*3600>> years;

    }
}

namespace std::literals::chrono_literals {
    constexpr std::chrono::hours operator "" h(uint64_t hrs) {
        return std::chrono::hours(hrs);
    }
    constexpr std::chrono::minutes operator "" min(uint64_t mins) {
        return std::chrono::minutes(mins);
    }
    constexpr std::chrono::seconds operator "" s(uint64_t s) {
        return std::chrono::seconds(s);
    }
    constexpr std::chrono::milliseconds operator "" ms(uint64_t ms) {
        return std::chrono::milliseconds(ms);
    }
    constexpr std::chrono::microseconds operator "" us(uint64_t micros) {
        return std::chrono::microseconds (micros);
    }
    constexpr std::chrono::nanoseconds operator "" ns(uint64_t ns) {
        return std::chrono::nanoseconds(ns);
    }
}

namespace std::chrono {
    using namespace std::literals::chrono_literals;

    static_assert(duration_cast<seconds>(1s).count() == 1);
    static_assert(duration_cast<milliseconds>(1s).count() == 1000);
}

#endif //JEOKERNEL_CHRONO_H
