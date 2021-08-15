//
// Created by sigsegv on 6/5/21.
//

#include "tests.h"
#include <cstdint>
#include <optional>
#include <thread>
#include "../include/core/vmem.h"
#include "../include/devices/devices.h"
#include "../include/devices/drivers.h"
#include "../include/concurrency/hw_spinlock.h"
#include "../include/acpi/pci_irq_rt.h"
#include "../include/interrupt_frame.h"
#include "../include/core/cpu_mpfp.h"
#include "../boot64/IOApic.h"
#include "../include/core/LocalApic.h"
#include "../boot64/pci/pci.h"
#include "../boot64/usb/xhci.h"

int main() {
    uint64_t legsup_value = 0x1234567800011201;
    uint16_t *low = (uint16_t *) (void *) &legsup_value;
    uint8_t *bios_sema = (uint8_t *) (void *) &legsup_value;
    bios_sema += 2;
    uint8_t *os_sema = bios_sema + 1;
    uint32_t *high = (uint32_t *) (void *) &legsup_value;
    high++;
    assert(*low == 0x1201);
    assert(*bios_sema == 1);
    assert(*os_sema == 0);
    assert(*high = 0x12345678);

    xhci_ext_cap cap{(uint32_t *) (void *) &legsup_value};
    std::thread init_test{[&cap] () {
        auto *legsup = cap.legsup();
        assert(!legsup->is_owned());
        legsup->assert_os_own();
        while (!legsup->is_owned()) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(20ms);
        }
        std::cout << "Aqcuired" << std::endl;
    }};
    while (*os_sema == 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
    }
    assert(*os_sema == 1);
    *bios_sema = 0;
    init_test.join();
    assert(*low == 0x1201);
    assert(*bios_sema == 0);
    assert(*os_sema == 1);
    assert(*high = 0x12345678);
    std::cout << "Legsup disabled" << std::endl;
    return 0;
}

