#include "os.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289
#define MULTIBOOT2_TAG_TYPE_MMAP 6
#define LOW_IDENTITY_MAP_LIMIT 0x200000ULL

static int string_equals(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int string_starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static void kprint_char(char c) {
    char text[2] = {c, 0};
    kprint(text);
}

static void kprint_hex64(uint64_t value) {
    kprint("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        unsigned char nibble = (value >> i) & 0xF;
        if (nibble || started || i == 0) {
            started = 1;
            kprint_char("0123456789ABCDEF"[nibble]);
        }
    }
}

static void kprint_hex32(uint32_t value) {
    kprint_hex64(value);
}

static void kprint_dec(uint64_t value) {
    char buf[21];
    int pos = 20;

    buf[pos] = 0;
    if (value == 0) {
        kprint("0");
        return;
    }

    while (value && pos > 0) {
        buf[--pos] = '0' + (value % 10);
        value /= 10;
    }
    kprint(&buf[pos]);
}

static const char* skip_spaces(const char* s) {
    while (*s == ' ') s++;
    return s;
}

static const char* next_arg(const char* s) {
    while (*s && *s != ' ') s++;
    return skip_spaces(s);
}

static int parse_u64(const char* s, uint64_t* out, const char** end_out) {
    uint64_t value = 0;
    int base = 10;
    int digits = 0;

    s = skip_spaces(s);
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;
        value = value * base + digit;
        digits++;
        s++;
    }

    if (!digits) return 0;
    *out = value;
    if (end_out) *end_out = s;
    return 1;
}

static int parse_two_u64(const char* args, uint64_t* first, uint64_t* second) {
    const char* end;
    if (!parse_u64(args, first, &end)) return 0;
    if (!parse_u64(end, second, 0)) return 0;
    return 1;
}

static const char* mmap_type_name(uint32_t type) {
    if (type == 1) return "usable";
    if (type == 2) return "reserved";
    if (type == 3) return "acpi";
    if (type == 4) return "nvs";
    if (type == 5) return "badram";
    return "other";
}

static void cmd_memmap(void) {
    if (kernel_boot_magic != MULTIBOOT2_BOOTLOADER_MAGIC || kernel_boot_info_addr == 0) {
        kprint("No valid Multiboot2 memory map was captured.\n");
        return;
    }

    uint8_t* info = (uint8_t*)kernel_boot_info_addr;
    uint32_t total_size = *(uint32_t*)info;
    uint8_t* tag = info + 8;
    uint8_t* end = info + total_size;

    while (tag + 8 <= end) {
        uint32_t type = *(uint32_t*)tag;
        uint32_t size = *(uint32_t*)(tag + 4);
        if (type == 0 || size < 8) break;

        if (type == MULTIBOOT2_TAG_TYPE_MMAP) {
            uint32_t entry_size = *(uint32_t*)(tag + 8);
            uint8_t* entry = tag + 16;
            uint8_t* entries_end = tag + size;
            int index = 0;

            kprint("Memory map:\n");
            while (entry + 24 <= entries_end) {
                uint64_t base = *(uint64_t*)(entry + 0);
                uint64_t len = *(uint64_t*)(entry + 8);
                uint32_t mmap_type = *(uint32_t*)(entry + 16);

                kprint("  ");
                kprint_dec(index++);
                kprint(": ");
                kprint_hex64(base);
                kprint(" - ");
                kprint_hex64(base + len);
                kprint("  ");
                kprint_dec(len / 1024);
                kprint(" KiB  ");
                kprint(mmap_type_name(mmap_type));
                kprint("\n");
                entry += entry_size;
            }
            return;
        }

        tag += (size + 7) & ~7;
    }

    kprint("GRUB did not provide a Multiboot2 memory-map tag.\n");
}

static void cpuid_leaf(uint32_t leaf, uint32_t subleaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf));
}

static void print_cpuid_feature(uint32_t regs, uint32_t bit, const char* name) {
    if (regs & (1u << bit)) {
        kprint(name);
        kprint(" ");
    }
}

static void cmd_cpuid(const char* args) {
    uint64_t parsed_leaf = 0;
    uint32_t leaf = 0;
    uint32_t a, b, c, d;

    if (parse_u64(args, &parsed_leaf, 0)) leaf = (uint32_t)parsed_leaf;
    cpuid_leaf(leaf, 0, &a, &b, &c, &d);

    kprint("CPUID leaf ");
    kprint_hex32(leaf);
    kprint(":\n  EAX=");
    kprint_hex32(a);
    kprint(" EBX=");
    kprint_hex32(b);
    kprint(" ECX=");
    kprint_hex32(c);
    kprint(" EDX=");
    kprint_hex32(d);
    kprint("\n");

    if (leaf == 0) {
        char vendor[13];
        *(uint32_t*)&vendor[0] = b;
        *(uint32_t*)&vendor[4] = d;
        *(uint32_t*)&vendor[8] = c;
        vendor[12] = 0;
        kprint("  Vendor: ");
        kprint(vendor);
        kprint("\n");
        kprint("  Max basic leaf: ");
        kprint_hex32(a);
        kprint("\n");
    } else if (leaf == 1) {
        kprint("  Features: ");
        print_cpuid_feature(d, 0, "FPU");
        print_cpuid_feature(d, 4, "TSC");
        print_cpuid_feature(d, 5, "MSR");
        print_cpuid_feature(d, 8, "CX8");
        print_cpuid_feature(d, 9, "APIC");
        print_cpuid_feature(d, 15, "CMOV");
        print_cpuid_feature(d, 23, "MMX");
        print_cpuid_feature(d, 25, "SSE");
        print_cpuid_feature(d, 26, "SSE2");
        print_cpuid_feature(c, 0, "SSE3");
        print_cpuid_feature(c, 9, "SSSE3");
        print_cpuid_feature(c, 19, "SSE4.1");
        print_cpuid_feature(c, 20, "SSE4.2");
        print_cpuid_feature(c, 28, "AVX");
        kprint("\n");
    }
}

static int check_low_addr(uint64_t addr, unsigned int width) {
    if (addr + width > LOW_IDENTITY_MAP_LIMIT || addr + width < addr) {
        kprint("Address is outside the first 2 MiB identity map.\n");
        kprint("For higher physical addresses, add page-table mapping first.\n");
        return 0;
    }
    return 1;
}

static void cmd_peek(const char* args, unsigned int width) {
    uint64_t addr;
    uint64_t value;

    if (!parse_u64(args, &addr, 0)) {
        kprint("Usage: peek8|peek16|peek32 ADDR\n");
        return;
    }
    if (!check_low_addr(addr, width / 8)) return;

    if (width == 8) value = *(volatile uint8_t*)(uintptr_t)addr;
    else if (width == 16) value = *(volatile uint16_t*)(uintptr_t)addr;
    else value = *(volatile uint32_t*)(uintptr_t)addr;

    kprint("mem[");
    kprint_hex64(addr);
    kprint("] = ");
    kprint_hex64(value);
    kprint("\n");
}

static void cmd_poke(const char* args, unsigned int width) {
    uint64_t addr;
    uint64_t value;

    if (!parse_two_u64(args, &addr, &value)) {
        kprint("Usage: poke8|poke16|poke32 ADDR VAL\n");
        return;
    }
    if (!check_low_addr(addr, width / 8)) return;

    if (width == 8) *(volatile uint8_t*)(uintptr_t)addr = (uint8_t)value;
    else if (width == 16) *(volatile uint16_t*)(uintptr_t)addr = (uint16_t)value;
    else *(volatile uint32_t*)(uintptr_t)addr = (uint32_t)value;

    kprint("Wrote ");
    kprint_hex64(value);
    kprint(" to ");
    kprint_hex64(addr);
    kprint("\n");
}

static void cmd_hexdump(const char* args) {
    uint64_t addr;
    uint64_t len;

    if (!parse_two_u64(args, &addr, &len)) {
        kprint("Usage: dump ADDR LEN\n");
        return;
    }
    if (len > 256) len = 256;
    if (!check_low_addr(addr, (unsigned int)len)) return;

    for (uint64_t row = 0; row < len; row += 16) {
        kprint_hex64(addr + row);
        kprint(": ");
        for (uint64_t i = 0; i < 16; i++) {
            if (row + i < len) {
                uint8_t byte = *(volatile uint8_t*)(uintptr_t)(addr + row + i);
                kprint_char("0123456789ABCDEF"[byte >> 4]);
                kprint_char("0123456789ABCDEF"[byte & 0xF]);
            } else {
                kprint("  ");
            }
            kprint(" ");
        }
        kprint(" |");
        for (uint64_t i = 0; i < 16 && row + i < len; i++) {
            uint8_t byte = *(volatile uint8_t*)(uintptr_t)(addr + row + i);
            kprint_char((byte >= 32 && byte <= 126) ? byte : '.');
        }
        kprint("|\n");
    }
}

static int msr_is_allowed(uint32_t msr) {
    return msr == 0x10 || msr == 0x1B || msr == 0xE7 || msr == 0xE8 || msr == 0xC0000080;
}

static void cmd_rdmsr(const char* args) {
    uint64_t parsed_msr;
    uint32_t msr;
    uint32_t lo;
    uint32_t hi;

    if (!parse_u64(args, &parsed_msr, 0)) {
        kprint("Usage: rdmsr MSR\n");
        return;
    }
    msr = (uint32_t)parsed_msr;
    if (!msr_is_allowed(msr)) {
        kprint("MSR not on the safe allow-list. Add #GP handling before arbitrary RDMSR.\n");
        return;
    }

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    kprint("MSR ");
    kprint_hex32(msr);
    kprint(" = ");
    kprint_hex64(((uint64_t)hi << 32) | lo);
    kprint("\n");
}

static void cmd_lapic(void) {
    kprint("lapic needs the APIC MMIO page mapped first.\n");
    kprint("Current paging maps only 0x0-0x1FFFFF; LAPIC is usually at 0xFEE00000.\n");
}

static void cmd_monitor_stub(const char* name, const char* needs) {
    kprint(name);
    kprint(" is staged but not safe yet.\n");
    kprint("Needed first: ");
    kprint(needs);
    kprint("\n");
}


void monitor_print_help(void) {
    kprint("memmap - show boot memory map\n");
    kprint("cpuid [leaf] - show CPU registers/features\n");
    kprint("rdmsr MSR - read safe allow-listed MSRs\n");
    kprint("peek8/16/32 ADDR - read low identity-mapped memory\n");
    kprint("poke8/16/32 ADDR VAL - write low identity-mapped memory\n");
    kprint("dump ADDR LEN - hex-dump low memory\n");
    kprint("lapic/irqstat/timer/debug commands are staged for the next kernel layer\n");
}

int monitor_handle_command(const char* cmd) {
    if (string_equals(cmd, "memmap")) {
        cmd_memmap();
        return 1;
    } else if (string_equals(cmd, "cpuid") || string_starts_with(cmd, "cpuid ")) {
        cmd_cpuid(next_arg(cmd));
        return 1;
    } else if (string_starts_with(cmd, "rdmsr ")) {
        cmd_rdmsr(next_arg(cmd));
        return 1;
    } else if (string_starts_with(cmd, "wrmsr ")) {
        cmd_monitor_stub("wrmsr", "an IDT and #GP handler, then a strict MSR allow-list");
        return 1;
    } else if (string_starts_with(cmd, "peek8 ")) {
        cmd_peek(next_arg(cmd), 8);
        return 1;
    } else if (string_starts_with(cmd, "peek16 ")) {
        cmd_peek(next_arg(cmd), 16);
        return 1;
    } else if (string_starts_with(cmd, "peek32 ")) {
        cmd_peek(next_arg(cmd), 32);
        return 1;
    } else if (string_starts_with(cmd, "poke8 ")) {
        cmd_poke(next_arg(cmd), 8);
        return 1;
    } else if (string_starts_with(cmd, "poke16 ")) {
        cmd_poke(next_arg(cmd), 16);
        return 1;
    } else if (string_starts_with(cmd, "poke32 ")) {
        cmd_poke(next_arg(cmd), 32);
        return 1;
    } else if (string_starts_with(cmd, "dump ")) {
        cmd_hexdump(next_arg(cmd));
        return 1;
    } else if (string_starts_with(cmd, "setcr ")) {
        cmd_monitor_stub("setcr", "reserved-bit masks and fault recovery before writing CR0-CR4");
        return 1;
    } else if (string_equals(cmd, "lapic")) {
        cmd_lapic();
        return 1;
    } else if (string_equals(cmd, "irqstat")) {
        cmd_monitor_stub("irqstat", "IDT setup and common ISR counters");
        return 1;
    } else if (string_starts_with(cmd, "timer ")) {
        cmd_monitor_stub("timer", "PIT/LAPIC timer IRQ setup and an idle wait loop");
        return 1;
    } else if (string_equals(cmd, "loadx")) {
        cmd_monitor_stub("loadx", "serial receive buffering plus XMODEM CRC");
        return 1;
    } else if (string_starts_with(cmd, "exec ")) {
        cmd_monitor_stub("exec", "a scratch stack, executable load area, and a controlled jump path");
        return 1;
    } else if (string_starts_with(cmd, "break ")) {
        cmd_monitor_stub("break", "IDT entries for #BP/#DB and breakpoint bookkeeping");
        return 1;
    } else if (string_equals(cmd, "cont")) {
        cmd_monitor_stub("cont", "saved trap frames and iret-based resume");
        return 1;
    } else if (string_equals(cmd, "step")) {
        cmd_monitor_stub("step", "trap-flag handling and a #DB exception handler");
        return 1;
    } else if (string_starts_with(cmd, "trace ")) {
        cmd_monitor_stub("trace", "a syscall ABI and trace ring buffer");
        return 1;
    } else if (string_starts_with(cmd, "profile ")) {
        cmd_monitor_stub("profile", "LAPIC/TSC-deadline timer interrupts and RIP sampling");
        return 1;
    } else if (string_equals(cmd, "sensors")) {
        cmd_monitor_stub("sensors", "safe thermal MSR probing and optional SMBus drivers");
        return 1;
    } else if (string_equals(cmd, "freq")) {
        cmd_monitor_stub("freq", "APERF/MPERF sampling over a calibrated timer interval");
        return 1;
    }

    return 0;
}
