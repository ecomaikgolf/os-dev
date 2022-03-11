/**
 * Kernel file
 *
 * @author Ernesto Martínez García <me@ecomaikgolf.com>
 */

#include "kernel.h"
#include "acpi/acpi.h"
#include "bootstrap/startup.h"
#include "bootstrap/stivale_hdrs.h"
#include "float.h"
#include "interrupts/IDT.h"
#include "interrupts/interrupts.h"
#include "io/keyboard.h"
#include "lib/stdlib.h"
#include "lib/string.h"
#include "paging/BPFA.h"
#include "paging/PFA.h"
#include "paging/PTM.h"
#include "pci/pci.h"
#include "screen/fonts/psf1.h"
#include "screen/framebuffer.h"
#include "screen/renderer.h"
#include "segmentation/gdt.h"
#include "shell/command.h"
#include "stivale2.h"
#include <stddef.h>
#include <stdint.h>

static uint8_t stack[8192];

/**
 * Stivale configuration parameters
 */

static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    .tag   = { .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID, .next = 0 },
    .flags = 0
};

static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    .tag                = { .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
                            .next       = (uint64_t)&terminal_hdr_tag },
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0
};

__attribute__((section(".stivale2hdr"), used)) static struct stivale2_header stivale_hdr = {
    .entry_point = 0,
    .stack       = (uintptr_t)stack + sizeof(stack),
    .flags       = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
    .tags        = (uintptr_t)&framebuffer_hdr_tag
};

extern "C" void _init();
extern "C" void _fini();

/**
 * Kernel starting function
 * extern C to avoid C++ function mangling
 */
extern "C" [[noreturn]] void
_start(stivale2_struct *stivale2_struct)
{
    /* Call global constructors & functions */
    _init();

    bootstrap::allocator(stivale2_struct);
    bootstrap::translator(stivale2_struct);
    bootstrap::screen(stivale2_struct);
    bootstrap::gdt();
    bootstrap::interrupts();
    bootstrap::enable_virtualaddr();
    bootstrap::enable_interrupts();
    bootstrap::keyboard();
    bootstrap::acpi(stivale2_struct);
    bootstrap::heap((void *)0x100000, 0x10);
    bootstrap::pci();
    // bootstrap::rtl8139();

    for (pci::pci_device *i = kernel::devices; i != nullptr; i = i->next) {
        if (i->header->id == 0x8139 && i->header->header_type == 0x0) {
            net::rtl8139 aux(i);
            aux.start();
        }
    }

    kernel::internal::stivalehdr = *stivale2_struct;

    kernel::tty.println("welcome to the alma kernel");

    struct ethheader
    {
        unsigned char dsta[6];
        unsigned char srca[6];
        uint16_t type;
    };

    ethheader aux{

        { 0xca, 0xfe, 0xc0, 0xff, 0xee, 0x00 },
        { 0xee, 0xee, 0xee, 0xee, 0xee, 0xee },
        1,
    };

    auto test     = (ethheader *)kernel::allocator.request_page();
    test->dsta[0] = 0xca;
    test->dsta[1] = 0xfe;
    test->dsta[2] = 0xc0;
    test->dsta[3] = 0xff;
    test->dsta[4] = 0xee;
    test->dsta[5] = 0x00;

    test->srca[0] = 0xee;
    test->srca[1] = 0xee;
    test->srca[2] = 0xee;
    test->srca[3] = 0xee;
    test->srca[4] = 0xee;
    test->srca[5] = 0xee;

    test->type = 1;

    kernel::rtl8139.send_packet((uint64_t)test, sizeof(aux));

    shell::commands::shell(0, nullptr);

    /* Call global destructors & functions */
    _fini();
    /* Shoudln't return (poweroff) */
    __asm__("hlt");
    /* To supress diagnostics */
    while (1) {
    };
}
