//
// Created by sigsegv on 27.04.2021.
//

#ifndef JEOKERNEL_STD_STRING_H
#define JEOKERNEL_STD_STRING_H

#include <cstdint>
#include <string.h>
#include <memory>
#include <limits>

namespace std {

    template<class CharT>
    class char_traits {
    public:
        typedef CharT char_type;
        typedef std::size_t off_type;
        typedef std::size_t pos_type;

        static constexpr char_type *move(char_type *dest, const char_type *src, std::size_t count) {
            return (char_type *) memmove((void *) dest, (void *) src, count * sizeof(char_type));
        }

        static constexpr char_type *copy(char_type *dest, const char_type *src, std::size_t count) {
            return (char_type *) memcpy((void *) dest, (void *) src, count * sizeof(char_type));
        }

        static constexpr std::size_t length(const char_type *s) {
            std::size_t i = 0;
            while (s[i] != 0) {
                i++;
            }
            return i;
        }

        static constexpr int compare(const char_type *s1, const char_type *s2, std::size_t length) {
            for (std::size_t i = 0; i < length; i++) {
                char_type diff = s1[i] - s2[i];
                if (diff != 0) {
                    return diff;
                }
            }
            return 0;
        }
    };

    template<class CharT, class string_pointer>
    struct short_string_s {
        /* May be too restrictive. But to make sure algorithms are not applied outside of good ranges
         * Tests 8 byte alignment and size between 16 and 32 bytes.
         * */
        static_assert((sizeof(string_pointer) & ((8 * sizeof(CharT)) - 1)) == 0 && sizeof(string_pointer) >= 16 &&
                      sizeof(string_pointer) <= 32);

        union {
            CharT str[sizeof(string_pointer) / sizeof(CharT)];
            struct {
                uint8_t short_string_space[sizeof(string_pointer) - 1];
                uint8_t unused_capacity;
            } __attribute__((__packed__));
        } __attribute__((__packed__));

        constexpr CharT *get_data() {
            return &(str[0]);
        }

        constexpr const CharT *get_data() const {
            return &(str[0]);
        }

        void set_empty() {
            str[0] = 0;
            unused_capacity = get_capacity();
        }

        void set_size(uint8_t size) {
            if (size != 0) {
                unused_capacity = get_capacity() - size;
                str[size] = 0;
            } else {
                set_empty();
            }
        }

        constexpr bool is_short() const {
            return unused_capacity <= get_capacity();
        }

        constexpr uint8_t get_size() const {
            return (sizeof(string_pointer) / sizeof(CharT)) - unused_capacity - 1;
        }

        constexpr uint8_t get_capacity() const {
            return (sizeof(string_pointer) / sizeof(CharT)) - 1;
        }
    };

    template<class CharT, class size_type, size_type memory_alloc_unit_multiplier = 16>
    struct string_pointer_s {
        /*
         * Pad for allocation unit size limitations
         */
        static_assert(sizeof(CharT) <= 2 || sizeof(CharT) == 4 || sizeof(CharT) == 8);

        typedef CharT char_type;
        CharT *pointer;
        size_type size;
        size_type capacity; // allocated - 1

        constexpr CharT *get_data() const {
            return pointer;
        }

        constexpr size_type get_size() const {
            return size;
        }

        void set_size(size_type size) {
            pointer[size] = 0;
            this->size = size;
        }

        constexpr void set_capacity(size_type cap) {
            capacity = (cap >> get_shift_bits()) | get_end_mark();
        }
        constexpr size_type get_capacity() const {
            return ((capacity & ~get_end_mark()) << get_shift_bits()) | get_alloc_unit_minus_1();
        }

        constexpr size_type get_alloc_unit() const {
            const auto ptr_size = sizeof(*this);

            const auto multiplier_val = get_bits(memory_alloc_unit_multiplier - 1);
            const auto capacity_val = get_bits((ptr_size / sizeof(CharT)) - 1);

            const auto combined = multiplier_val > capacity_val ? multiplier_val : capacity_val;

            return 1 << combined;
        }
        constexpr size_type get_alloc_unit_minus_1() const {
            return get_alloc_unit() - 1;
        }
        constexpr uint8_t get_bits(size_type num) const {
            uint8_t i = 0;
            while (num != 0) {
                num = num >> 1;
                i++;
            }
            return i;
        }
        constexpr uint8_t get_shift_bits() const {
            return get_bits(get_alloc_unit_minus_1());
        }
        constexpr size_type get_end_mark() const {
            size_type mark = ~((size_type) 0);
            mark = mark << ((sizeof(mark) * 8) - get_shift_bits());
            return mark;
        }

        constexpr size_type pad_capacity(size_type suggestion) const {
            suggestion *= sizeof(CharT);
            size_type overshoot = suggestion & get_alloc_unit_minus_1();
            if (overshoot == 0) {
                return suggestion / sizeof(CharT);
            } else {
                return (suggestion + get_alloc_unit() - overshoot) / sizeof(CharT);
            }
        }
    } __attribute__((__packed__));

    static_assert(sizeof(void *) == 8);
    static_assert(sizeof(string_pointer_s<uint8_t, uint32_t>) == 16);
    static_assert(string_pointer_s<uint8_t, uint32_t>().get_alloc_unit_minus_1() == 15);
    static_assert(string_pointer_s<uint8_t, uint32_t>().pad_capacity(8) == 16);
    static_assert(string_pointer_s<uint8_t, uint32_t>().pad_capacity(15) == 16);
    static_assert(string_pointer_s<uint8_t, uint32_t>().pad_capacity(16) == 16);
    static_assert(string_pointer_s<uint8_t, uint32_t>().pad_capacity(17) == 32);
    static_assert(sizeof(string_pointer_s<uint16_t, uint32_t>) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t>().get_alloc_unit_minus_1() == 15);
    static_assert(string_pointer_s<uint16_t, uint32_t>().pad_capacity(8) == 8);
    static_assert(string_pointer_s<uint16_t, uint32_t>().pad_capacity(15) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t>().pad_capacity(16) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t>().pad_capacity(17) == 24);
    static_assert(sizeof(string_pointer_s<uint16_t, uint32_t, 8>) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t, 8>().get_alloc_unit_minus_1() == 7);
    static_assert(string_pointer_s<uint16_t, uint32_t, 8>().pad_capacity(15) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t, 8>().pad_capacity(15) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t, 8>().pad_capacity(16) == 16);
    static_assert(string_pointer_s<uint16_t, uint32_t, 8>().pad_capacity(17) == 20);
    static_assert(sizeof(string_pointer_s<uint32_t, uint32_t, 8>) == 16);
    static_assert(string_pointer_s<uint32_t, uint32_t, 8>().get_alloc_unit_minus_1() == 7);
    static_assert(string_pointer_s<uint32_t, uint32_t, 8>().pad_capacity(7) == 8);
    static_assert(string_pointer_s<uint32_t, uint32_t, 8>().pad_capacity(15) == 16);
    static_assert(string_pointer_s<uint32_t, uint32_t, 8>().pad_capacity(16) == 16);
    static_assert(string_pointer_s<uint32_t, uint32_t, 8>().pad_capacity(17) == 18);

    template<class CharT> class basic_string_iterator;

    template<class CharT> class basic_string_iterator_base {
        friend basic_string_iterator<CharT>;
    private:
        CharT *ptr;
        basic_string_iterator_base(CharT *ptr) : ptr(ptr) {}
    public:
        constexpr CharT & operator *() {
            return *ptr;
        }

        constexpr bool operator ==(const basic_string_iterator_base &other) const noexcept {
            return ptr == other.ptr;
        }
        constexpr bool operator !=(const basic_string_iterator_base &other) const noexcept {
            return ptr != other.ptr;
        }
        constexpr bool operator <=(const basic_string_iterator_base &other) const noexcept {
            return ptr <= other.ptr;
        }
        constexpr bool operator >=(const basic_string_iterator_base &other) const noexcept{
            return ptr >= other.ptr;
        }
        constexpr bool operator <(const basic_string_iterator_base &other) const noexcept {
            return ptr < other.ptr;
        }
        constexpr bool operator >(const basic_string_iterator_base &other) const noexcept {
            return ptr > other.ptr;
        }
    };
    template<class CharT> class basic_string_iterator : public basic_string_iterator_base<CharT> {
    public:
        explicit basic_string_iterator(CharT *ptr) : basic_string_iterator_base<CharT>(ptr) {}

        constexpr basic_string_iterator operator ++() noexcept {
            return basic_string_iterator<CharT>(this->ptr++);
        }
        constexpr basic_string_iterator &operator ++(int) noexcept {
            ++this->ptr;
            return *this;
        }
        constexpr basic_string_iterator operator --() noexcept {
            return basic_string_iterator<CharT>(this->ptr--);
        }
        constexpr basic_string_iterator &operator --(int) noexcept {
            ++this->ptr;
            return *this;
        }
        constexpr basic_string_iterator &operator +=(int diff) noexcept {
            this->ptr += diff;
            return *this;
        }
        constexpr basic_string_iterator &operator -=(int diff) noexcept {
            this->ptr -= diff;
            return *this;
        }
    };
    template<class CharT, class Traits = char_traits<CharT>, class allocator = std::allocator<CharT>>
    class basic_string {
    public:
        typedef typename allocator::size_type size_type;
        typedef basic_string_iterator<CharT> iterator;
        typedef basic_string_iterator<const CharT> const_iterator;
        static constexpr size_type npos = std::numeric_limits<size_type>::max();
    private:
        struct _data_container : allocator {
            union {
                short_string_s<CharT, string_pointer_s<CharT, size_type>> shrt;
                string_pointer_s<CharT, size_type> ptr;
            }__attribute__((__packed__));

            _data_container() : allocator(), shrt() {
            }
        }__attribute__((__packed__));

        _data_container c;

        allocator &get_allocator() {
            return c;
        }

    public:
        basic_string() {
            c.shrt.set_empty();
        }

        constexpr CharT *data() {
            if (c.shrt.is_short()) {
                return c.shrt.get_data();
            } else {
                return c.ptr.get_data();
            }
        }

        constexpr const CharT *data() const {
            if (c.shrt.is_short()) {
                return c.shrt.get_data();
            } else {
                return c.ptr.get_data();
            }
        }

        constexpr iterator begin() noexcept {
            return iterator(data());
        }
        constexpr iterator end() noexcept {
            return iterator(data() + size());
        }

        constexpr const_iterator cbegin() const noexcept {
            return const_iterator(data());
        }
        constexpr const_iterator cend() const noexcept {
            return const_iterator(data() + size());
        }

        constexpr CharT *c_str() {
            return data();
        }

        constexpr const CharT *c_str() const {
            return data();
        }

        constexpr size_type size() const {
            if (c.shrt.is_short()) {
                return c.shrt.get_size();
            } else {
                return c.ptr.get_size();
            }
        }

        constexpr size_type capacity() const {
            if (c.shrt.is_short()) {
                return c.shrt.get_capacity();
            } else {
                return c.ptr.get_capacity();
            }
        }

        constexpr CharT &at(size_type pos) {
            CharT *ptr;
            if (pos >= 0 && pos < size()) {
                ptr = &(data()[pos]);
            } else {
                ptr = nullptr;
            }
            return *ptr;
        }

        constexpr const CharT &at(size_type pos) const {
            CharT *ptr;
            if (pos >= 0 && pos < size()) {
                ptr = &(data()[pos]);
            } else {
                ptr = nullptr;
            }
            return *ptr;
        }

        CharT &operator[](size_type pos) {
            return at(pos);
        }

        constexpr CharT &front() {
            return at(0);
        }

        constexpr CharT &back() {
            return at(size() - 1);
        }

        constexpr const CharT &front() const {
            return at(0);
        }

        constexpr const CharT &back() const {
            return at(size() - 1);
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        }

        constexpr void reserve(size_type new_cap) {
            size_type prev_cap = capacity();
            if (new_cap > prev_cap) {
                bool is_short = c.shrt.is_short();
                size_type padded_alloc_len = c.ptr.pad_capacity(new_cap + 1);
                size_type size_v = size();
                CharT *old_data = data();
                CharT *new_data = get_allocator().allocate(padded_alloc_len);
                Traits::move(new_data, old_data, size_v + 1);
                c.ptr.pointer = new_data;
                c.ptr.set_capacity(padded_alloc_len - 1);
                if (is_short) {
                    c.ptr.size = size_v;
                } else {
                    get_allocator().deallocate(old_data, prev_cap + 1);
                }
            }
        }

        constexpr void shrink_to_fit() {
            bool is_short = c.shrt.is_short();
            if (!is_short) {
                size_type prev_cap = capacity();
                size_type size_v = size();
                size_type padded_alloc_len = c.ptr.pad_capacity(size_v + 1);
                if (padded_alloc_len < prev_cap) {
                    CharT *old_data = data();
                    CharT *new_data = get_allocator().allocate(padded_alloc_len);
                    Traits::move(new_data, old_data, size_v + 1);
                    c.ptr.pointer = new_data;
                    c.ptr.set_capacity(padded_alloc_len - 1);
                    get_allocator().deallocate(old_data, prev_cap + 1);
                }
            }
        }

        basic_string(basic_string &&) = default;

    private:
        void copy_from(const basic_string &cp) {
            if (cp.c.shrt.is_short() && c.shrt.is_short()) {
                this->c.shrt = cp.c.shrt;
            } else {
                size_type s = cp.size();
                reserve(s);
                Traits::copy(data(), cp.data(), s + 1);
                if (c.shrt.is_short()) {
                    c.shrt.set_size(s);
                } else {
                    c.ptr.size = s;
                }
            }
        }

    public:

        basic_string(const basic_string &cp) {
            copy_from(cp);
        }

        basic_string &operator=(basic_string &&mv) {
            if (this == &mv) {
                return *this;
            }
            if (!c.shrt.is_short()) {
                get_allocator().deallocate(c.ptr.pointer, c.ptr.capacity + 1);
                c.ptr = mv.c.ptr;
                mv.c.shrt.set_empty();
            } else {
                c.shrt = mv.c.shrt;
            }
            return *this;
        }

        basic_string &operator=(const basic_string &cp) {
            copy_from(cp);
            return *this;
        }

        basic_string(const CharT *cstr, size_type length) : basic_string() {
            if (length >= capacity()) {
                reserve(length);
            }
            Traits::copy(data(), cstr, length + 1);
            if (c.shrt.is_short()) {
                c.shrt.set_size(length);
            } else {
                c.ptr.size = length;
            }
        }

        basic_string(const CharT *cstr) : basic_string(cstr, Traits::length(cstr)) {
        }

        basic_string &operator=(const char *cstr) {
            size_type length = Traits::length(cstr);
            if (length >= capacity()) {
                reserve(length);
            }
            Traits::copy(data(), cstr, length + 1);
            if (c.shrt.is_short()) {
                c.shrt.set_size(length);
            } else {
                c.ptr.size = length;
            }
            return *this;
        }

        constexpr void clear() noexcept {
            if (c.shrt.is_short()) {
                c.shrt.set_empty();
            } else {
                c.ptr.size = 0;
                c.ptr.pointer[0] = 0;
            }
        }

        constexpr basic_string &append(const CharT *str, size_type count) {
            size_type s = size();
            if ((s + count) > capacity()) {
                reserve(s + count);
            }
            Traits::copy(data() + s, str, count);
            if (c.shrt.is_short()) {
                c.shrt.set_size(s + count);
            } else {
                data()[s + count] = 0;
                c.ptr.size = s + count;
            }
            return *this;
        }

        constexpr basic_string &append(const CharT *str) {
            size_type count = Traits::length(str);
            return append(str, count);
        }

        constexpr basic_string &append(const basic_string &str) {
            return append(str.data(), str.size());
        }

        basic_string &erase(size_type index = 0, size_type count = npos) {
            {
                size_type overflow_cap = npos - index;
                if (count > overflow_cap) {
                    count = overflow_cap;
                }
            }
            size_type end = index + count;
            {
                size_type sz = size();
                if (end > sz) {
                    end = sz;
                }
                if (end < sz) {
                    size_t preserve = sz - end;
                    memcpy(data() + index, data() + end, preserve);
                }
                if (this->c.shrt.is_short()) {
                    this->c.shrt.set_size(sz - count);
                } else {
                    this->c.ptr.size = sz - count;
                }
            }
            return *this;
        }

        constexpr void resize( size_type count) {
            resize(count, ' ');
        }
        constexpr void resize( size_type count, CharT ch ) {
            size_type size{0};
            if (c.shrt.is_short()) {
                size = c.shrt.get_size();
                if (size == count) {
                    return;
                }
                if (count < size) {
                    c.shrt.set_size(count);
                    return;
                }
                if (count <= c.shrt.get_capacity()) {
                    for (auto pos = size; pos < count; pos++) {
                        c.shrt.str[pos] = ch;
                    }
                    c.shrt.set_size(count);
                    return;
                }
                reserve(count);
            } else {
                size = c.ptr.get_size();
                if (count == size) {
                    return;
                }
                if (count < size) {
                    c.ptr.set_size(count);
                    return;
                }
                if (count > capacity()) {
                    reserve(count);
                }
            }
            for (auto pos = size; pos < count; pos++) {
                c.ptr.pointer[pos] = ch;
            }
            c.ptr.set_size(count);
        }

        ~basic_string() {
            if (!c.shrt.is_short()) {
                get_allocator().deallocate(c.ptr.pointer, c.ptr.capacity + 1);
                c.shrt.set_empty();
            }
        }

        constexpr bool starts_with( const CharT* s ) const {
            auto iterator = cbegin();
            while (iterator != cend()) {
                if (*s == '\0') {
                    return true;
                }
                if (*s != *iterator) {
                    return false;
                }
                ++iterator;
                ++s;
            }
            return *s == '\0';
        }

        constexpr int compare( const basic_string& str ) const noexcept {
            bool equal_s{false};
            bool larger{false};
            std::size_t comp = size();
            if (comp == str.size()) {
                equal_s = true;
            } else if (comp > str.size()) {
                larger = true;
                comp = str.size();
            }
            int diff = Traits::compare(data(), str.data(), comp);
            if (diff == 0 && !equal_s) {
                return larger ? 1 : -1;
            }
            return diff;
        }
        int compare(const CharT *str, std::size_t len) const {
            bool equal_s{false};
            bool larger{false};
            if (len == size()) {
                equal_s = true;
            } else if (len > size()) {
                larger = true;
                len = size();
            }
            int diff = Traits::compare(data(), str, len);
            if (diff == 0 && !equal_s) {
                return larger ? 1 : -1;
            }
            return diff;
        }

        bool operator==(const CharT *rhs ) {
            return compare(rhs, Traits::length(rhs)) == 0;
        }
    };

    typedef basic_string<char> string;
}

template< class CharT, class Traits, class Alloc >
constexpr bool operator==( const std::basic_string<CharT,Traits,Alloc>& lhs,
                           const std::basic_string<CharT,Traits,Alloc>& rhs ) noexcept {
    return lhs.compare(rhs) == 0;
}

#endif //JEOKERNEL_STD_STRING_H
