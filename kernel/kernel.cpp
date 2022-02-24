/**
 * Kernel file
 *
 * @author Ernesto Martínez García <me@ecomaikgolf.com>
 */

#include "kernel.h"
#include "acpi/acpi.h"
#include "bootstrap/startup.h"
#include "float.h"
#include "interrupts/IDT.h"
#include "interrupts/interrupts.h"
#include "io/keyboard.h"
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
#include <stddef.h>
#include <stdint.h>

/**
 * Kernel starting function
 * extern C to avoid C++ function mangling
 */
extern "C" [[noreturn]] void
_start(bootstrap::boot_args *args)
{
    /* Bootstrap the kernel (function order is mandatory) */
    bootstrap::allocator(args->map);
    bootstrap::translator(args->map);
    bootstrap::screen(args->fb, args->font);
    bootstrap::gdt();
    bootstrap::interrupts();
    bootstrap::enable_virtualaddr();
    bootstrap::enable_interrupts();
    bootstrap::keyboard();
    bootstrap::acpi(args->rsdp);
    // bootstrap::heap((void *)0x0000100000000000, 0x1000);

    bootstrap::pci();

    // kernel::tty.println("Hola desde el kernel!");
    // asm("int $0x09");
    // kernel::tty.println("Hola otra vez desde el kernel!");

    shell::commands::shell(0, nullptr);

    /* Shoudln't return */
    while (1) {
    }
}
