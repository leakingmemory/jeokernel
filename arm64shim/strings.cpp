/*
 * Freestanding C string/memory primitives for the arm64 boot shim.
 *
 * Even with -nostdlib the compiler is free to emit calls to memset/memcpy/...
 * (e.g. to zero a stack struct or for placement new), so the shim has to
 * provide them itself. This file starts with memset; the others can join it as
 * they are needed.
 */

// Freestanding build (-nostdinc): pull the standard typedefs from the
// compiler's built-ins rather than <cstddef>/<cstdint>, matching shim.cpp.
using u8 = __UINT8_TYPE__;
using usize = __SIZE_TYPE__;

namespace {
	// The actual byte fill, kept naive on purpose: one byte at a time, no
	// alignment or word-at-a-time tricks. constexpr lets the compiler fold the
	// whole loop away at any call site where the size (and value) are known,
	// which is the common case for small fixed-size clears.
	constexpr void *fill_bytes(void *dst, int value, usize n) {
		u8 *p = static_cast<u8 *>(dst);
		const u8 byte = static_cast<u8>(value);
		for (usize i = 0; i < n; ++i) {
			p[i] = byte;
		}
		return dst;
	}
}

extern "C" void *memset(void *dst, int value, usize n) {
	return fill_bytes(dst, value, n);
}

// The compiler points every pure-virtual vtable slot at this symbol; it is the
// thing that would run if a pure virtual were ever called through a base under
// construction. Hosted C++ gets it from libc++abi, but the freestanding shim
// must supply its own. It should never actually run, so just wedge the core.
extern "C" [[noreturn]] void __cxa_pure_virtual() {
	for (;;) {
		asm volatile("wfe");
	}
}
