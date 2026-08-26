//
// Minimal aarch64 kernel entry.
//
// The real aarch64 kernel does not exist yet. For now this file's only job is
// to give the aarch64 build a kernel image to produce, which the arm64 shim
// embeds (see arm64shim/wrapbin.S) and will later hand control to. _start just
// parks the core.
//

extern "C" [[noreturn]] void _start() {
    volatile unsigned int *uart = reinterpret_cast<volatile unsigned int *>(0x09000000);
    const char *msg = "AArch64 kernel entrypoint reached with paging enabled!\n";
    while (*msg != '\0') {
        *uart = static_cast<unsigned int>(*msg++);
    }
    for (;;) {
        asm volatile("wfe");
    }
}
