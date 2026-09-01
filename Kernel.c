#include "os.h"
#include "ascii_art-bigstuff.h"

uint8_t current_bg = 7;
uint8_t current_fg = 15;
uint32_t kernel_boot_magic = 0;
uintptr_t kernel_boot_info_addr = 0;

static const char* all_commands[] = {
"help", "version", "cls", "uptime", "whoami", "uname", "date", "time", "hostname", "echo",
"history", "man", "credits", "license", "sysinfo", "motd", "wait", "exit", "clear", "beep",
"silence", "logo", "logo:", "desktop", "term", "terminalmode", "fullscreen-terminal", "start", "lock", "poweroff", "terminal", "files", "system", "about", "settings", "taskman", "taskmgr", "tasks",
"editor", "edit", "scroll", "bg", "bootmode", "open", "close", "focus", "move", "resize", "cursor", "mousespeed", "click", "vbe",
"env", "set", "unset", "alias", "unalias", "true", "false", "nop",
"memmap", "cpuid", "rdmsr", "wrmsr", "peek8", "peek16", "peek32", "poke8", "poke16", "poke32",
"setcr", "lapic", "irqstat", "timer", "loadx", "exec", "break", "cont", "step", "trace",
"profile", "sensors", "freq", "cpu-temp", "reboot", "shutdown", "msr-read", "msr-write", "inb", "inw", "ind",
"outb", "outw", "outd", "pci-list", "pci-info", "irq-info", "idt-dump", "gdt-dump",
"cr0-info", "cr3-info", "registers", "sse-test", "fpu-test", "rtc-read", "pit-set",
"pic-status", "ioapic-dump", "dma-list", "cpu-speed", "cpu-cores", "halt",
"memdump", "mmap", "malloc", "free", "memstat", "pages", "peek", "peekw", "peekd",
"poke", "pokew", "poked", "memset", "memcpy", "memfind", "stack-size", "heap-info",
"phys-alloc", "phys-free", "virt-to-phys", "phys-to-virt", "kheap-check", "page-fault",
"vmm-status", "pmm-status", "stack-walk", "mem-bench", "mprotect", "shm-list", "kmalloc-trace",
"ls", "cd", "pwd", "mkdir", "rmdir", "touch", "rm", "cat", "cp", "mv",
"stat", "find", "grep", "mount", "mounts", "vfs", "umount", "df", "format", "chmod", "chown",
"head", "tail", "wc", "dd", "sync", "hd-info", "hd-read", "hd-write", "lsblk",
"fdisk", "checkfs",
"ps", "kill", "top", "nice", "spawn", "waitpid", "sleep", "ipc-info", "sem-list",
"mutex-list", "yield", "sched-info", "killall", "suspend", "resume", "prio", "exec",
"fork-test", "thread-list", "affinity", "signal", "pipe", "mem-limit", "renice", "pgid",
"sid", "trace", "core-dump", "zombie-clean", "idle",
"panic", "divzero", "gp-fault", "stack-overflow", "klog", "vga-test", "kbd-test", "mouse-test",
"port-scan", "serial-send", "serial-recv", "speaker-freq", "color-test", "blink-test", "cursor-pos",
"cursor-set", "int-test", "heap-leak", "cpu-bench", "random", "checksum", "base64",
"hex", "dec", "bin", "crypt-test", "self-destruct", "debug-on", "debug-off", "godmode",
NULL
};
// new comment 1st of september 2026 the all commands thing is a old thing and NOT implemented at all

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

void trim_spaces(char* out, const char* in) {
    const char* start = in;
    while (*start == ' ') start++;
    const char* end = start;
    while (*end != 0) end++;
    while (end > start && *(end - 1) == ' ') end--;
    while (start < end) {
        *out++ = *start++;
    }
    *out = 0;
}

static void print_help(void) {
    kprint("Available commands:\n");
    kprint("help - show this message\n");
    kprint("clear/cls - clear the screen\n");
    kprint("desktop - redraw the desktop shell\n");
    kprint("term - hide desktop and use full terminal until 'desktop'\n");
    kprint("start - open the start menu\n");
    kprint("lock - return to the loading screen\n");
    kprint("poweroff - halt the CPU\n");
    kprint("terminal/files/system/about/taskman - open desktop apps\n");
    kprint("settings - open settings window\n");
    kprint("bg <color> - set wallpaper color\n");
    kprint("bootmode desktop|terminal - choose startup screen\n");
    kprint("open/focus/close <app> - manage app windows\n");
    kprint("move [app] left|right|up|down - move windows\n");
    kprint("resize [app] wider|narrower|taller|shorter - resize windows\n");
    kprint("cursor left|right|up|down|show - move pointer\n");
    kprint("mousespeed 1..6 - set pointer speed\n");
    kprint("click - activate item under pointer\n");
    kprint("vbe - show GRUB framebuffer info\n");
    kprint("echo <text> - print text\n");
    kprint("version - show OS version\n");
    kprint("protected/license - show open source build note\n");
    kprint("cpu - show CPU info\n");
    kprint("reboot - reboot the system\n");
    kprint("beep - make a beep sound\n");
    kprint("color <num> - set text color\n");
    kprint("time - show time (not implemented)\n");
    kprint("dump - dump memory\n");
    kprint("VFS: mount/mounts/vfs, ls/cd/pwd/mkdir/rmdir/touch/write/cat/rm/stat/df/format\n");
    kprint("Hardware: drives/lsblk/hd-info - detect read-only ATA disks\n");
    monitor_print_help();
    kprint("calc <expr> - simple calculator\n");
    kprint("stack - show stack pointer\n");
    kprint("shutdown - shutdown the system\n");
    kprint("whoami - show current user\n");
    kprint("uname - print system name\n");
    kprint("hostname - display system name\n");
    kprint("true - always success\n");
    kprint("false - always failure\n");
    kprint("nop - no operation\n");
    kprint("halt - halt the CPU\n");
    kprint("panic - trigger a kernel panic\n");
    kprint("screencolor - change screen color\n");
    kprint("runapp - run an application\n");
    kprint("Many more commands are recognized but not implemented.\n");
}

void run_command(const char* cmd) {
    if (string_equals(cmd, "help")) {
        print_help();
    } else if (desktop_handle_command(cmd)) {
        return;
    } else if (fs_handle_command(cmd)) {
        return;
    } else if (hardware_handle_command(cmd)) {
        return;
    } else if (monitor_handle_command(cmd)) {
        return;
    } else if (string_equals(cmd, "clear") || string_equals(cmd, "cls")) {
        clear_screen();
    } else if (string_starts_with(cmd, "echo ")) {
        kprint(cmd + 5);
        kprint("\n");
    } else if (string_equals(cmd, "version")) {
        kprint("ArchwayOS 0.2.0 © 2026 ArchwayOS\n");
    } else if (string_equals(cmd, "protected") || string_equals(cmd, "license")) {
        kprint("ArchwayOS 0.2.0 (c) 2026 ArchwayOS - open source preview build\n");
    } else if (string_equals(cmd, "cpu")) {
        cpu_info();
    } else if (string_equals(cmd, "reboot")) {
        reboot();
    } else if (string_equals(cmd, "beep")) {
        beep();
    } else if (string_starts_with(cmd, "color ")) {
        int color = 0;
        const char* arg = cmd + 6;
        while (*arg >= '0' && *arg <= '9') {
            color = color * 10 + (*arg - '0');
            arg++;
        }
        set_color((unsigned char)color);
    } else if (string_equals(cmd, "time")) {
        time_info();
    } else if (string_equals(cmd, "dump")) {
        dump_memory();
    } else if (string_starts_with(cmd, "calc ")) {
        calc(cmd + 5);
    } else if (string_equals(cmd, "stack")) {
        stack_info();
    } else if (string_equals(cmd, "shutdown")) {
        shutdown();
    } else if (string_equals(cmd, "whoami")) {
        whoami();
    } else if (string_equals(cmd, "uname")) {
        kprint("Archway\n");
    } else if (string_equals(cmd, "hostname")) {
        kprint("ArchwayOS\n");
    } else if (string_equals(cmd, "panic")) {
        kernel_panic("User requested manual kernel panic via CLI.");
    } else if (string_equals(cmd, "true")) {
    } else if (string_equals(cmd, "false")) {
        kprint("false: command failed\n");
    } else if (string_equals(cmd, "nop")) {
    } else if (string_equals(cmd, "halt")) {
        kprint("Halting CPU...\n");
        __asm__ volatile("hlt");
    } else if (string_starts_with(cmd, "screencolor")) {
        const char* arg_str = cmd + 12;
        while (*arg_str == ' ') arg_str++;

        if (*arg_str == '\0') {
            kprint("Usage: screencolor <1-16>\n");
        } else {
            int color_val = 0;
            while (*arg_str >= '0' && *arg_str <= '9') {
                color_val = color_val * 10 + (*arg_str - '0');
                arg_str++;
            }

            if (color_val >= 1 && color_val <= 16) {
                set_color((unsigned char)color_val);
                kprint("Screen color updated.\n");
            } else {
                kprint("Error: Color must be between 1 and 16.\n");
            }
        }
    } else if (string_equals(cmd, ";")) {
    } else {
        int found = 0;
        for (int i = 0; all_commands[i] != NULL; i++) {
            if (string_equals(cmd, all_commands[i])) {
                kprint("Command '");
                kprint(cmd);
                kprint("' not implemented in this version\n");
                found = 1;
                break;
            }
        }
        if (!found) {
            kprint("Unknown command: ");
            kprint(cmd);
            kprint("\n");
        }
    }
}

void kernel_panic(const char* message) {
    clear_screen();
    set_color(12);
    for (int i = 0; i < art_line_count; i++) {
        kprint(ascii_art[i]);
        kprint("\n");
    }
    kprint(message);
    kprint("\nSystem halted.\n");
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
}
