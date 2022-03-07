#include "bootstrap/startup.h"
#include "bootstrap/stivale_hdrs.h"
#include "io/bus.h"
#include "kernel.h"
#include "lib/stdlib.h"
#include "pci/pci.h"

namespace bootstrap {

const uint16_t KEYBOARD_BUFF_SIZE = kernel::page_size;

void
screen(stivale2_struct *st)
{
    auto *fb =
      (stivale2_struct_tag_framebuffer *)stivale2_get_tag(st, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);

    auto *mod = (stivale2_struct_tag_modules *)stivale2_get_tag(st, STIVALE2_STRUCT_TAG_MODULES_ID);

    screen::framebuffer frame;
    frame.base        = (unsigned int *)fb->framebuffer_addr;
    frame.buffer_size = fb->framebuffer_width * fb->framebuffer_pitch;
    frame.ppscl       = (fb->framebuffer_pitch / sizeof(uint32_t));
    frame.width       = fb->framebuffer_width;
    frame.height      = fb->framebuffer_height;

    screen::fonts::psf1 *font_ptr = (screen::fonts::psf1 *)stivale2_get_mod(mod, "font");
    screen::fonts::psf1 font;
    font.header = *(screen::fonts::psf1_header *)font_ptr;
    font.buffer = (uint8_t *)font_ptr + sizeof(screen::fonts::psf1_header);

    /* Lock the tty itself  */
    // kernel::allocator.lock_pages(&kernel::tty, sizeof(kernel::tty) / kernel::page_size + 1);
    /* Create the tty */
    kernel::tty = screen::psf1_renderer(frame, font, 0, 0, screen::color_e::WHITE);
    /* Clean the screen */
    kernel::tty.clear();

    // uint64_t fbbase = (uint64_t)fb->base;
    // uint64_t fbsize = (uint64_t)fb->buffer_size + kernel::page_size;
    ///* Map screen's memory */
    // for (uint64_t i = fbbase; i < fbbase + fbsize; i += kernel::page_size)
    //     kernel::translator.map(i, i);
}

void
allocator(stivale2_struct *st)
{
    auto *map = (stivale2_struct_tag_memmap *)stivale2_get_tag(st, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    /* Construct the allocator with the UEFI mem map */
    kernel::allocator = paging::allocator::PFA(map);
    /* Lock kernel memory */
    // kernel::allocator.lock_pages(kernel::_start_addr, kernel::_size_npages);
    /* Lock the allocator itself  */
    // kernel::allocator.lock_pages(&kernel::allocator,
    //                              sizeof(kernel::allocator) / kernel::page_size + 1);
}

void
gdt()
{
    using namespace segmentation;
    /* Lock the GDT */
    kernel::allocator.lock_pages(&kernel::gdt, sizeof(kernel::gdt) / kernel::page_size + 1);
    /* Create an empty GDT */
    kernel::gdt.size   = sizeof(table) - 1;
    kernel::gdt.offset = (uint64_t)&table;
    /* Load it (assembly) */
    load_gdt(&kernel::gdt);
}

void
translator(stivale2_struct *st)
{

    uint64_t mapaddr;
    asm volatile("mov %%cr3, %0" : [Var] "=r"(mapaddr));
    paging::translator::PGDT_wrapper *newpgdt = (paging::translator::PGDT_wrapper *)mapaddr;
    kernel::translator.set_PGDT(newpgdt);

    // auto *map = (stivale2_struct_tag_memmap *)stivale2_get_tag(st,
    // STIVALE2_STRUCT_TAG_MEMMAP_ID);

    ///* Locks the PTM */
    // kernel::allocator.lock_pages(&kernel::translator,
    //                              sizeof(kernel::translator) / kernel::page_size + 1);

    ///* Map virtual memory to physical memory (same address for the kernel) */
    // size_t usable_mem_size = uefi::memory::get_memsize(map);
    // for (uint64_t i = 0; i < usable_mem_size; i += kernel::page_size)
    //     kernel::translator.map(i, i);

    // asm("mov %0, %%cr3" : : "r"(kernel::translator.get_PGDT()));
}

void
interrupts()
{
    /* Locks the IDT */
    kernel::allocator.lock_pages(&kernel::idtr, sizeof(kernel::idtr) / kernel::page_size + 1);
    kernel::idtr.set_ptr((uint64_t)kernel::allocator.request_page());

    /* Load the interrupt handlers */
    kernel::idtr.add_handle(interrupts::vector_e::reserved, interrupts::reserved);

    /*
     * From OSDev:
     * In protected mode, the IRQs 0 to 7 conflict with the CPU exception which are reserved
     * by Intel up until 0x1F. It is thus recommended to change the PIC's offsets (also known
     * as remapping the PIC) so that IRQs use non-reserved vectors. A common choice is to move
     * them to the beginning of the available range (IRQs 0..0xF -> INT 0x20..0x2F).  For that,
     * we need to set the master PIC's offset to 0x20 and the slave's to 0x28.
     */
    kernel::idtr.remap_pic(0x20, 0x28);

    io::outb(io::PIC1_DATA, 0b11111101); // enable IRQ1 from PIC1 (keyboard)
    io::outb(io::PIC2_DATA, 0b11111111); // mask all lines

    /* Enable interrupts */
    asm("sti");
}

void
enable_virtualaddr()
{
    /* Enable virtual addresses */
    asm("mov %0, %%cr3" : : "r"(kernel::translator.get_PGDT()));
}

void
enable_interrupts()
{
    /* Enable interrupts */
    asm("lidt %0" : : "m"(kernel::idtr));
}

void
keyboard()
{
    auto buffer = kernel::allocator.request_page();
    auto size   = KEYBOARD_BUFF_SIZE;

    kernel::keyboard.set_buffer(static_cast<char *>(buffer));
    kernel::keyboard.set_maxsize(size);
    io::PS2::enable_keyboard();
};

void
acpi(stivale2_struct *st)
{

    auto *rsdp   = (stivale2_struct_tag_rsdp *)stivale2_get_tag(st, STIVALE2_STRUCT_TAG_RSDP_ID);
    kernel::rsdp = *(acpi::rsdp_v2 *)rsdp->rsdp;
    kernel::rsdp.memmap_acpi_tables();
    // kernel::rsdp.print_acpi_tables();
};

void
pci()
{
    acpi::sdt *mcfg_ptr = (acpi::sdt *)kernel::rsdp.find_table("MCFG");
    pci::enum_pci(mcfg_ptr);
}

void
heap(void *start_addr, size_t size)
{
    heap::simple_allocator aux(start_addr, size);
    kernel::heap = aux;
}

} // namespace bootstrap
