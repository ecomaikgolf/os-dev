#include "shell/command.h"
#include "bootstrap/stivale_hdrs.h"
#include "kernel.h"
#include "lib/stdlib.h"
#include "shell/interpreter.h"

namespace shell {

namespace commands {

int
help(int argc, char **argv)
{
    for (uint32_t i = 0; kernel_commands[i].name != nullptr; i++) {
        kernel::tty.print(kernel_commands[i].name);
        if (kernel_commands[i + 1].name != nullptr)
            kernel::tty.print(", ");
        else
            kernel::tty.newline();
    }

    return 0;
}

int
echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        kernel::tty.print(argv[i]);
        if (i + 1 < argc)
            kernel::tty.print(" ");
    }
    kernel::tty.newline();

    return 0;
}

int
whoami(int argc, char **argv)
{
    kernel::tty.println("chad");
    return 0;
}

int
shell(int argc, char **argv)
{
    shell::interpreter inter(shell::kernel_commands);

    while (true) {
        kernel::tty.setColor(screen::color_e::GREEN);
        kernel::tty.print("$ ");
        kernel::tty.setColor(screen::color_e::WHITE);
        kernel::keyboard.scanf(inter.get_buffer(), inter.BUFFER_SIZE);
        auto ret = inter.process(inter.get_buffer());
        if (ret == 127) {
            kernel::tty.setColor(screen::color_e::RED);
            kernel::tty.println("Command not found");
            kernel::tty.setColor(screen::color_e::WHITE);
        }
        // kernel::tty.newline();
    }

    return 0;
}

int
clear(int argc, char **argv)
{
    kernel::tty.clear();
    return 0;
}

int
pci(int argc, char **argv)
{
    char buffer[256];
    for (pci::pci_device *i = kernel::devices; i != nullptr; i = i->next) {
        hstr(i->header.vendor, buffer);
        kernel::tty.print("* ");
        kernel::tty.print(buffer);
        kernel::tty.print(" - ");
        hstr(i->header.id, buffer);
        kernel::tty.print(buffer);
        kernel::tty.print(" (");
        str(i->device, buffer);
        kernel::tty.print(buffer);
        kernel::tty.print(", ");
        str(i->bus, buffer);
        kernel::tty.print(buffer);
        kernel::tty.print(", ");
        str(i->function, buffer);
        kernel::tty.print(buffer);
        kernel::tty.println(")");
    }

    return 0;
}

int
getpage(int argc, char **argv)
{
    if (argc >= 2) {
        uint64_t addr = strol(argv[1], 16);
        auto ret      = kernel::allocator.request_page((void *)addr);
        if (ret == nullptr) {
            kernel::tty.println("Not available");
            return 1;
        }
        kernel::tty.fmt("0x%p", (uint64_t)ret);
        return 0;
    }

    kernel::tty.fmt("0x%p", (uint64_t)kernel::allocator.request_page());
    return 0;
}

int
getmac(int argc, char **argv)
{

    char mac_addr[32];
    for (pci::pci_device *i = kernel::devices; i != nullptr; i = i->next) {
        if (i->header.id == 0x8139 && i->header.header_type == 0x0) {

            pci::header_t0 *ext_hdr = (pci::header_t0 *)i->header_ext;
            uint64_t mmio_addr      = (ext_hdr->BAR[1] & 0xfffffff0);

            char auxstr[16];
            for (int i = 0; i < 5; i++) {
                uint8_t aux = *((uint8_t *)mmio_addr + i);
                hstr(aux, auxstr);
                uint8_t endstr      = strlen(auxstr);
                mac_addr[i * 3]     = auxstr[endstr - 2];
                mac_addr[i * 3 + 1] = auxstr[endstr - 1];
                if (i != 4)
                    mac_addr[i * 3 + 2] = ':';
            }
            break;
        }
    }
    mac_addr[14] = '\0';
    kernel::tty.println(mac_addr);

    return 0;
}

int
getphys(int argc, char **argv)
{
    using namespace paging;
    using namespace paging::translator;

    if (argc <= 1) {
        // Y need print formatting :( ...
        kernel::tty.print("Usage: ");
        kernel::tty.print(argv[0]);
        kernel::tty.println(" virtaddr");
        return 1;
    }

    uint64_t addr                = strol(argv[1], 16);
    address_t *virtaddr          = (address_t *)&addr;
    page_global_dir_entry_t *PGD = &kernel::translator.get_PGDT()[virtaddr->global];

    if (!PGD->present)
        return 1;

    page_upper_dir_entry_t *PUDT = (page_upper_dir_entry_t *)((uint64_t)PGD->page_ppn << 12);
    page_upper_dir_entry_t *PUD  = &PUDT[virtaddr->upper];

    if (!PUD->present)
        return 1;

    page_mid_dir_entry_t *PMDT = (page_mid_dir_entry_t *)((uint64_t)PUD->page_ppn << 12);
    page_mid_dir_entry_t *PMD  = &PMDT[virtaddr->mid];

    if (!PMD->present)
        return 1;

    page_table_entry_t *PTDT = (page_table_entry_t *)((uint64_t)PMD->page_ppn << 12);
    page_table_entry_t *PTD  = &PTDT[virtaddr->table];

    uint64_t phys = (uint64_t)PTD->page_ppn << 12;

    char aux[256];
    hstr(phys, aux);
    kernel::tty.println(aux);

    return 0;
}

int
map(int argc, char **argv)
{
    if (argc <= 2) {
        // Y need print formatting :( ...
        kernel::tty.print("Usage: ");
        kernel::tty.print(argv[0]);
        kernel::tty.println(" virtaddr physaddr");
        return 1;
    }

    uint64_t virt = strol(argv[1], 16);
    uint64_t phys = strol(argv[2], 16);

    kernel::translator.map(virt, phys);

    kernel::tty.fmt("%s -> %s", argv[1], argv[2]);

    return 0;
}

int
unmap(int argc, char **argv)
{
    if (argc <= 1) {
        // Y need print formatting :( ...
        kernel::tty.fmt("Usage: %s virtaddr", argv[0]);
        return 1;
    }

    uint64_t virt = strol(argv[1], 16);

    kernel::translator.map(virt, virt);

    kernel::tty.fmt("%s -> %s", argv[1], argv[1]);

    return 0;
}

int
set(int argc, char **argv)
{
    if (argc <= 2) {
        // Y need print formatting :( ...
        kernel::tty.print("Usage: ");
        kernel::tty.print(argv[0]);
        kernel::tty.println(" addr true/false");
        return 1;
    }

    bool set = (strncmp(argv[2], "true", 4) == 0);

    uint64_t addr = strol(argv[1], 16);
    bool *data    = (bool *)addr;
    *data         = set;

    kernel::tty.print("*(bool *)0x");
    kernel::tty.print(argv[1]);
    if (*data)
        kernel::tty.println(" = true");
    else
        kernel::tty.println(" = false");

    return 0;
}

int
get(int argc, char **argv)
{
    if (argc <= 1) {
        // Y need print formatting :( ...
        kernel::tty.print("Usage: ");
        kernel::tty.print(argv[0]);
        kernel::tty.println(" addr");
        return 1;
    }

    uint64_t addr = strol(argv[1], 16);
    bool *data    = (bool *)addr;
    kernel::tty.print("*(bool *)0x");
    kernel::tty.print(argv[1]);
    if (*data)
        kernel::tty.println(" -> true");
    else
        kernel::tty.println(" -> false");
    return 0;
}

int
printmem(int argc, char **argv)
{
    /** Constants */
    static const uint32_t BYTES_PER_LINE = 16;
    static const uint32_t DEFAULT_LINES  = kernel::page_size / BYTES_PER_LINE;

    if (argc <= 1) {
        kernel::tty.fmt("Usage: %s addr [bytes]", argv[0]);
        return 1;
    }

    uint16_t nlines = DEFAULT_LINES;
    if (argc == 3)
        nlines = strol(argv[2], 10);

    uint64_t addr  = strol(argv[1], 16);
    uint8_t column = 0;
    uint8_t line   = 0;
    for (uint32_t byte = 0; byte < nlines * BYTES_PER_LINE; byte++) {

        if (column == 0) {
            char buff[32];
            hstr((uint64_t)(uint8_t *)addr + (line * BYTES_PER_LINE), buff);
            kernel::tty.print(buff);
            kernel::tty.print(": ");
        }

        char buff[16];
        uint8_t *ptr = ((uint8_t *)addr + byte);
        hstr(*ptr, buff);
        int wrongthing = strlen(buff); // TODO: fix this mess
        kernel::tty.put(buff[wrongthing - 2]);
        kernel::tty.put(buff[wrongthing - 1]);
        kernel::tty.put(' ');

        column++;

        if (column != 0 && column % BYTES_PER_LINE == 0) {
            line++;
            column = 0;
            kernel::tty.newline();
        }
    }

    return 0;
}

int
uefimmap(int argc, char **argv)
{
    auto *map = (stivale2_struct_tag_memmap *)stivale2_get_tag(&kernel::internal::stivalehdr,
                                                               STIVALE2_STRUCT_TAG_MEMMAP_ID);

    for (uint64_t i = 0; i < map->entries; i++) {
        auto entry          = map->memmap[i];
        uint64_t init_addr  = entry.base;
        uint64_t fini_addr  = entry.base + entry.length;
        uint64_t kbsize     = entry.length / 1024;
        const char *memtype = nullptr;
        switch (entry.type) {
            case 1:
                memtype = "Usable                ";
                break;
            case 2:
                memtype = "Reserved              ";
                break;
            case 3:
                memtype = "ACPI Reclaimable      ";
                break;
            case 4:
                memtype = "ACPI NVS              ";
                break;
            case 5:
                memtype = "Bad Memory            ";
                break;
            case 0x1000:
                memtype = "Bootloader Reclaimable";
                break;
            case 0x1001:
                memtype = "Kernel and Modules    ";
                break;
            case 0x1002:
                memtype = "Framebuffer           ";
                break;
            default:
                memtype = "ERROR                 ";
        }
        kernel::tty.fmt("%p - %p %s [%i KB]", init_addr, fini_addr, memtype, kbsize);
    }

    return 0;
}

int
printpfa(int argc, char **argv)
{
    auto it = kernel::allocator.get_first();
    while (it != nullptr) {
        auto limaddr = it->addr + (it->pages * kernel::page_size);
        kernel::tty.fmt("%p - %p [%i pages]", it->addr, limaddr, it->pages);
        it = it->next;
    }

    return 0;
}

} // namespace commands

} // namespace shell
