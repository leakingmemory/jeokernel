#include <core/nanotime.h>
#include <limits>

static uint64_t cnt_frequency = 0;
static uint64_t cnt_offset = 0;
static uint64_t stored_ref = 0;
static uint64_t stored_cnt = 0;

static inline uint64_t read_cntpct() {
    uint64_t val;
    asm volatile("isb\nmrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq() {
    uint64_t frq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));
    return frq;
}

bool is_nanotime_available() {
    if (cnt_frequency == 0) {
        cnt_frequency = read_cntfrq();
    }
    return cnt_frequency > 0;
}

bool is_nanotime_reliable() {
    return is_nanotime_available();
}

static uint128_t mul128(uint64_t a, uint64_t b) {
    uint64_t a_upper = a >> 32;
    uint64_t a_lower = a >> 32;
    uint64_t b_upper = a >> 32;
    uint64_t b_lower = a >> 32;
    uint64_t r_lower = a_lower * b_lower;
    uint64_t r_middle = a_lower * b_upper;
    r_middle += a_upper * b_lower;
    uint64_t r_upper = a_upper * b_upper;
    uint128_t r{r_upper};
    r = r << 32;
    r += r_middle;
    r = r << 32;
    r += r_lower;
    return r;
}

static uint128_t div128(uint128_t a, uint64_t b) {
    uint64_t a_upper = a >> 96;
    uint64_t a_umiddle = (a >> 64) & 0xFFFFFFFFULL;
    uint64_t a_lmiddle = (a >> 32) & 0xFFFFFFFFULL;
    uint64_t a_lower = a & 0xFFFFFFFFULL;
    if (a_upper > 0) {
        uint64_t r_rem = a_upper % b;
        uint64_t r_upper = a_upper / b;
        if (r_rem > 0xFFFFFFFFULL || r_upper > 0) {
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        a_umiddle += r_rem << 32;
    }
    if (a_umiddle > 0) {
        uint64_t r_rem = a_umiddle % b;
        uint64_t r_umiddle = a_umiddle / b;
        if (r_rem > 0xFFFFFFFFULL || r_umiddle > 0) {
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        a_lmiddle += r_rem << 32;
    }
    uint64_t r_lmiddle;
    if (a_lmiddle > 0) {
        uint64_t r_rem = a_lmiddle % b;
        r_lmiddle = a_lmiddle / b;
        if (r_rem > 0xFFFFFFFFULL || r_lmiddle > 0xFFFFFFFFULL) {
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        a_lower += r_rem << 32;
    } else {
        r_lmiddle = 0;
    }
    uint64_t r_lower = a_lower / b;
    r_lmiddle = r_lmiddle << 32;
    if (r_lower > (std::numeric_limits<uint64_t>::max() - r_lmiddle)) {
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    return r_lmiddle + r_lower;
}

uint64_t get_nanotime_ref() {
    if (cnt_frequency == 0) {
        cnt_frequency = read_cntfrq();
        if (cnt_frequency == 0) {
            return 0;
        }
    }

    uint64_t cnt = read_cntpct();
    if (cnt < cnt_offset) {
        return 0;
    }
    cnt -= cnt_offset;

    // Use 128-bit math to prevent 64-bit multiplication overflow:
    return div128(mul128(cnt, 1000000000ULL), cnt_frequency);
}

void set_nanotime_ref(uint64_t ref) {
    uint64_t cnt = read_cntpct();
    if (stored_cnt > 0 && stored_ref < ref && stored_cnt < cnt) {
        uint64_t cnt_diff = cnt - stored_cnt;
        uint64_t ref_diff = ref - stored_ref;
        cnt_diff *= 1000000000ULL;
        cnt_frequency = cnt_diff / ref_diff;
        cnt_offset = cnt - div128(mul128(ref, cnt_frequency), 1000000000ULL);
    } else if (cnt_frequency == 0) {
        cnt_frequency = read_cntfrq();
        cnt_offset = cnt;
    }
    stored_ref = ref;
    stored_cnt = cnt;
}

uint64_t get_tsc_frequency() {
    if (cnt_frequency == 0) {
        cnt_frequency = read_cntfrq();
    }
    return cnt_frequency;
}

uint64_t get_tsc_offset() {
    return cnt_offset;
}
