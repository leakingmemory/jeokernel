//
// Loader-mode (-DLOADER) tests for simple_physpagemap_managed with a nonzero
// base_addr. The manager wraps a PhyspageMap bitmap ("the memory map") and
// shifts every page address down by base_addr before touching the bitmap, so
// these tests check that:
//   - max() reports base_addr + the bitmap's capacity,
//   - claim/release flip exactly the right bit (absolute page base_addr + r
//     lands on relative bit r, and nothing else moves),
//   - addresses outside [base_addr, max()) are rejected.
//
// physpagemap.cpp keeps simple_physpagemap_managed file-private, so - like the
// other unit tests do with the sources they exercise - we include the .cpp
// directly to reach it. -DLOADER selects the loader-only definitions.
//

#include "tests.h"
#include "../kernel/physpagemap.cpp"

// One PhyspageMap covers this many pages; the manager's range is this many
// pages starting at base_addr.
static constexpr uint32_t MAP_PAGES = PhyspageMap::mappageaddr_to_pageaddr(1);

// A page address well inside the bitmap, chosen so it is not page-32-aligned -
// that way claiming it proves both the word index and the bit offset are right.
static constexpr uint32_t BASE_ADDR = 0x4000;

int main() {
    // ---- max() --------------------------------------------------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);
        assert(mgr.max() == MAP_PAGES + BASE_ADDR);
        // Loader-mode set_max is a no-op; max() must not budge.
        mgr.set_max(123);
        assert(mgr.max() == MAP_PAGES + BASE_ADDR);
    }

    // ---- single claim lands on the base_addr-relative bit -------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        const uint32_t rel = 33;                 // word 1, bit 1
        const uint32_t page = BASE_ADDR + rel;
        mgr.claim(page);

        // The bit set is the relative one...
        assert(map.claimed(rel));
        assert(map.map[1] == (1u << 1));
        assert(mgr.claimed(page));
        // ...not the absolute page address (proves the base_addr offset).
        assert(!map.claimed(page));
        // Neighbours untouched.
        assert(!map.claimed(rel - 1));
        assert(!map.claimed(rel + 1));
    }

    // ---- release clears exactly that bit ------------------------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        const uint32_t rel = 70;                 // word 2, bit 6
        const uint32_t page = BASE_ADDR + rel;
        mgr.claim(page);
        assert(mgr.claimed(page));
        assert(map.claimed(rel));

        mgr.release(page);
        assert(!mgr.claimed(page));
        assert(!map.claimed(rel));
        assert(map.map[2] == 0);
    }

    // ---- claim of a range lands on the right run of bits --------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        const uint32_t rel = 100;
        mgr.claim(BASE_ADDR + rel, 5);           // relative 100..104
        assert(!map.claimed(rel - 1));
        for (uint32_t i = 0; i < 5; ++i) {
            assert(map.claimed(rel + i));
            assert(mgr.claimed(BASE_ADDR + rel + i));
        }
        assert(!map.claimed(rel + 5));
    }

    // ---- out-of-range single claim/release is ignored -----------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        mgr.claim(BASE_ADDR - 1);                // below the base
        mgr.claim(mgr.max());                    // at the top bound
        mgr.claim(mgr.max() + 50);               // above the top bound
        // Nothing should have been written to the bitmap.
        for (uint32_t i = 0; i < PhyspageMap::map_size; ++i) {
            assert(map.map[i] == 0);
        }
        // Anything outside [base_addr, max()) reads back as reserved/claimed.
        assert(mgr.claimed(BASE_ADDR - 1));
        assert(mgr.claimed(mgr.max()));
        // release of an out-of-range page must not touch the bitmap either.
        mgr.release(BASE_ADDR - 1);
        mgr.release(mgr.max());
        for (uint32_t i = 0; i < PhyspageMap::map_size; ++i) {
            assert(map.map[i] == 0);
        }
    }

    // ---- range that starts below base_addr is clipped to the base -----------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        // Start 2 pages below base, length 5: the 2 below-base pages are
        // dropped and relative pages 0..2 get claimed.
        mgr.claim(BASE_ADDR - 2, 5);
        assert(map.claimed(0));
        assert(map.claimed(1));
        assert(map.claimed(2));
        assert(!map.claimed(3));
    }

    // ---- range that runs past max() is clipped to the top -------------------
    {
        PhyspageMap map;
        simple_physpagemap_managed mgr(&map, BASE_ADDR);

        // Last two pages of the map, asked for ten: only the in-range two stick.
        const uint32_t rel = MAP_PAGES - 2;      // 32766, 32767
        mgr.claim(BASE_ADDR + rel, 10);
        assert(!map.claimed(rel - 1));
        assert(map.claimed(rel));
        assert(map.claimed(rel + 1));            // == MAP_PAGES - 1, the last bit
        assert(mgr.claimed(BASE_ADDR + rel));
        assert(mgr.claimed(BASE_ADDR + rel + 1));
    }

    return 0;
}
