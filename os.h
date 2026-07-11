#ifndef OS_H
#define OS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} multiboot_info_t;

extern uint8_t current_bg;
extern uint8_t current_fg;
extern uint32_t kernel_boot_magic;
extern uintptr_t kernel_boot_info_addr;

extern uintptr_t frame_buffer_addr;
extern unsigned int frame_buffer_width;
extern unsigned int frame_buffer_height;
extern unsigned int frame_buffer_pitch;
extern uint8_t frame_buffer_bpp;

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void init_serial();
void serial_write(const char* str);
void init_graphics();
void clear_screen();
void set_cursor(int x, int y);
void set_framebuffer_console_enabled(int enabled);
void set_framebuffer_draw_target(uintptr_t addr, unsigned int width, unsigned int height, unsigned int pitch);
void set_kprint_mirror(void (*mirror)(const char* text));
void make_box(int w, int h, int x, int y, int color);
void fb_draw_text_at(int cell_x, int cell_y, const char* text, uint32_t fg, uint32_t bg);
void kprint(const char* str);
unsigned char read_key();
unsigned char scancode_to_ascii(unsigned char sc);
void cpu_info();
void reboot();
void beep();
void set_color(unsigned char color);
void time_info();
void dump_memory();
void calc(const char* expr);
void stack_info();
void shutdown();
void whoami();
void kernel_panic(const char* message);
int desktop_handle_command(const char* cmd);
int monitor_handle_command(const char* cmd);
void monitor_print_help(void);
void fs_init(void);
int fs_handle_command(const char* cmd);
void hardware_init(void);
int hardware_handle_command(const char* cmd);

typedef struct {
    char name[16];
    uint32_t size;
    uint8_t is_dir;
} FsDesktopEntry;

int fs_desktop_list(FsDesktopEntry* entries, int max_entries);
int fs_desktop_create_text(const char* name, const char* text);

void trim_spaces(char* out, const char* in);
void run_command(const char* cmd);

static inline void set_screen_color(uint8_t fg, uint8_t bg) {
    current_fg = fg;
    current_bg = bg;
}

#endif
