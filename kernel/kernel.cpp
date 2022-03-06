/**
 * Kernel file
 *
 * @author Ernesto Martínez García <me@ecomaikgolf.com>
 */
#include "stivale2.h"

// We need to tell the stivale bootloader where we want our stack to be.
// We are going to allocate our stack as an array in .bss.
static uint8_t stack[8192];

// stivale2 uses a linked list of tags for both communicating TO the
// bootloader, or receiving info FROM it. More information about these tags
// is found in the stivale2 specification.

// stivale2 offers a runtime terminal service which can be ditched at any
// time, but it provides an easy way to print out to graphical terminal,
// especially during early boot.
// Read the notes about the requirements for using this feature below this
// code block.
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    // All tags need to begin with an identifier and a pointer to the next tag.
    .tag = {
        // Identification constant defined in stivale2.h and the specification.
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        // If next is 0, it marks the end of the linked list of header tags.
        .next = 0
    },
    // The terminal header tag possesses a flags field, leave it as 0 for now
    // as it is unused.
    .flags = 0
};

// We are now going to define a framebuffer header tag.
// This tag tells the bootloader that we want a graphical framebuffer instead
// of a CGA-compatible text mode. Omitting this tag will make the bootloader
// default to text mode, if available.
static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    // Same as above.
    .tag = { .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
             // Instead of 0, we now point to the previous header tag. The order in
             // which header tags are linked does not matter.
             .next = (uint64_t)&terminal_hdr_tag },
    // We set all the framebuffer specifics to 0 as we want the bootloader
    // to pick the best it can.
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0
};

// The stivale2 specification says we need to define a "header structure".
// This structure needs to reside in the .stivale2hdr ELF section in order
// for the bootloader to find it. We use this __attribute__ directive to
// tell the compiler to put the following structure in said section.
__attribute__((section(".stivale2hdr"), used)) static struct stivale2_header stivale_hdr = {
    // The entry_point member is used to specify an alternative entry
    // point that the bootloader should jump to instead of the executable's
    // ELF entry point. We do not care about that so we leave it zeroed.
    .entry_point = 0,
    // Let's tell the bootloader where our stack is.
    // We need to add the sizeof(stack) since in x86(_64) the stack grows
    // downwards.
    .stack = (uintptr_t)stack + sizeof(stack),
    // Bit 1, if set, causes the bootloader to return to us pointers in the
    // higher half, which we likely want since this is a higher half kernel.
    // Bit 2, if set, tells the bootloader to enable protected memory ranges,
    // that is, to respect the ELF PHDR mandated permissions for the executable's
    // segments.
    // Bit 3, if set, enables fully virtual kernel mappings, which we want as
    // they allow the bootloader to pick whichever *physical* memory address is
    // available to load the kernel, rather than relying on us telling it where
    // to load it.
    // Bit 4 disables a deprecated feature and should always be set.
    .flags = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
    // This header structure is the root of the linked list of header tags and
    // points to the first one in the linked list.
    .tags = (uintptr_t)&framebuffer_hdr_tag
};

//

#include "acpi/acpi.h"
#include "bootstrap/startup.h"
#include "float.h"
#include "interrupts/IDT.h"
#include "interrupts/interrupts.h"
#include "io/keyboard.h"
#include "kernel.h"
#include "libc/stdlib.h"
#include "libc/string.h"
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

    bootstrap::screen(stivale2_struct);

    kernel::tty.println("Hola!");

    asm("hlt");

    //    stivale2_struct_tag_memmap *map =
    //      (stivale2_struct_tag_memmap *)stivale2_get_tag(stivale2_struct, 0x2187f79e8612de07);
    //    stivale2_struct_tag_framebuffer *fb =
    //      (stivale2_struct_tag_framebuffer *)stivale2_get_tag(stivale2_struct,
    //      0x506461d2950408fa);
    //    stivale2_struct_tag_rsdp *rsdp =
    //      (stivale2_struct_tag_rsdp *)stivale2_get_tag(stivale2_struct, 0x9e1786930a375e78);
    //    stivale2_struct_tag_terminal *cmd =
    //      (stivale2_struct_tag_terminal *)stivale2_get_tag(stivale2_struct, 0xc2b3f4c3233b0974);
    //    stivale2_struct_tag_modules *mod =
    //      (stivale2_struct_tag_modules *)stivale2_get_tag(stivale2_struct, 0x4b6fe466aade04ce);
    //    stivale2_struct_tag_rsdp *rs =
    //      (stivale2_struct_tag_rsdp *)stivale2_get_tag(stivale2_struct, 0x9e1786930a375e78);
    //
    //    screen::fonts::psf1 *font = nullptr;
    //    for (int i = 0; i < mod->module_count; i++) {
    //        if (strcmp(mod->modules[i].string, "font") == 0) {
    //            font = (screen::fonts::psf1 *)mod->modules[i].begin;
    //        }
    //    }
    //
    //    screen::framebuffer frame;
    //    frame.base        = (unsigned int *)fb->framebuffer_addr;
    //    frame.buffer_size = fb->framebuffer_width * fb->framebuffer_pitch;
    //    frame.ppscl       = (fb->framebuffer_pitch / sizeof(uint32_t));
    //    frame.width       = fb->framebuffer_width;
    //    frame.height      = fb->framebuffer_height;
    //
    //    // screen::fonts::psf1 f;
    //    // screen::fonts::psf1_header *f_hdr = (screen::fonts::psf1_header *)font;
    //    // f.header                          = f_hdr;
    //    // f.buffer                          = (uint8_t *)font +
    //    sizeof(screen::fonts::psf1_header);
    //
    //    void (*stivale2_term_write)(uint64_t ptr, uint64_t length);
    //
    //    stivale2_term_write = (void (*)(unsigned long, unsigned long))cmd->term_write;
    //
    //    char aux[] = "hola";
    //    stivale2_term_write((uint64_t)&aux[0], 4);
    //
    //    bootstrap::boot_args *args = nullptr;
    //    /* Bootstrap the kernel (function order is mandatory) */
    //
    //    kernel::tty.println("hola");
    //
    //    char mem[256];
    //    for (int i = 0; i < map->entries; i++) {
    //        if (map->memmap[i].type == 1) {
    //            hstr((uint64_t)map->memmap[i].base, mem);
    //            kernel::tty.print(mem);
    //            str((int)map->memmap[i].length / kernel::page_size, mem);
    //            kernel::tty.print("(");
    //            kernel::tty.print(mem);
    //            kernel::tty.print("), ");
    //            // str((int)map->memmap[i].length, mem);
    //            // kernel::tty.print(" - Lenght ");
    //            // kernel::tty.print(mem);
    //            // str((int)map->memmap[i].type, mem);
    //            // kernel::tty.print(" - Type ");
    //            // kernel::tty.print(mem);
    //            // kernel::tty.newline();
    //        }
    //    }
    //
    //    bootstrap::allocator(map);
    //
    //    kernel::tty.newline();
    //
    //    bootstrap::translator(map);
    //
    //    hstr((uint64_t)kernel::allocator.request_page(), mem);
    //    kernel::tty.println(mem);
    //    hstr((uint64_t)kernel::allocator.request_page(), mem);
    //    kernel::tty.println(mem);
    //    hstr((uint64_t)kernel::allocator.request_page(), mem);
    //    kernel::tty.println(mem);
    //    hstr((uint64_t)kernel::allocator.request_page(), mem);
    //    kernel::tty.println(mem);
    //    hstr((uint64_t)kernel::allocator.request_page(), mem);
    //    kernel::tty.println(mem);
    //
    //    // bootstrap::screen(&frame, font);
    //    bootstrap::gdt();
    //    bootstrap::interrupts();
    //    // bootstrap::enable_virtualaddr();
    //    bootstrap::enable_interrupts();
    //    bootstrap::keyboard();
    //    bootstrap::acpi((acpi::rsdp_v2 *)rs->rsdp);
    //    // bootstrap::heap((void *)0x0000100000000000, 0x1000);
    //
    //    bootstrap::pci();
    //
    //    // kernel::tty.println("Hola desde el kernel!");
    //    // asm("int $0x09");
    //    // kernel::tty.println("Hola otra vez desde el kernel!");
    //
    //    shell::commands::shell(0, nullptr);

    /* Call global destructors & functions */
    _fini();
    /* Shoudln't return (poweroff) */
    __asm__("hlt");
    /* To supress diagnostics */
    while (1) {
    };
}
