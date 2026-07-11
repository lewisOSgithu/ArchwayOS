#include "os.h"
#include "ascii_art-bigstuff.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER 0xB8000
#define COM1 0x3F8

#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN   14
#define VGA_COLOR_WHITE         15




uintptr_t frame_buffer_addr = 0;
unsigned int frame_buffer_width = 0;
unsigned int frame_buffer_height = 0;
unsigned int frame_buffer_pitch = 0;
uint8_t frame_buffer_bpp = 0;

static uintptr_t frame_draw_addr = 0;
static unsigned int frame_draw_width = 0;
static unsigned int frame_draw_height = 0;
static unsigned int frame_draw_pitch = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static unsigned char text_color = 0x0F;
static int framebuffer_console_enabled = 1;
static void (*kprint_mirror)(const char* text) = 0;

static uintptr_t active_framebuffer_addr(void) {
    return frame_draw_addr ? frame_draw_addr : frame_buffer_addr;
}

static unsigned int active_framebuffer_width(void) {
    return frame_draw_addr ? frame_draw_width : frame_buffer_width;
}

static unsigned int active_framebuffer_height(void) {
    return frame_draw_addr ? frame_draw_height : frame_buffer_height;
}

static unsigned int active_framebuffer_pitch(void) {
    return frame_draw_addr ? frame_draw_pitch : frame_buffer_pitch;
}

static uint32_t fb_text_color(void) {
    switch (text_color & 0x0F) {
        case VGA_COLOR_BLUE: return 0x002060D0;
        case VGA_COLOR_GREEN: return 0x0020C060;
        case VGA_COLOR_CYAN: return 0x0020C0C0;
        case VGA_COLOR_RED: return 0x00D04040;
        case VGA_COLOR_MAGENTA: return 0x00C040C0;
        case VGA_COLOR_BROWN: return 0x00A07030;
        case VGA_COLOR_DARK_GREY: return 0x00606060;
        case VGA_COLOR_LIGHT_BLUE: return 0x006EA8FF;
        case VGA_COLOR_LIGHT_GREEN: return 0x0078E08A;
        case VGA_COLOR_LIGHT_CYAN: return 0x0070E0E0;
        case VGA_COLOR_LIGHT_RED: return 0x00FF8080;
        case VGA_COLOR_LIGHT_MAGENTA: return 0x00FF80FF;
        case VGA_COLOR_LIGHT_BROWN: return 0x00E0C060;
        case VGA_COLOR_BLACK: return 0x00000000;
        case VGA_COLOR_LIGHT_GREY: return 0x00B8B8B8;
        case VGA_COLOR_WHITE:
        default: return 0x00FFFFFF;
    }
}

static void fb_fill_rect_api(int x, int y, int w, int h, uint32_t color) {
    uintptr_t target = active_framebuffer_addr();
    unsigned int target_width = active_framebuffer_width();
    unsigned int target_height = active_framebuffer_height();
    unsigned int target_pitch = active_framebuffer_pitch();

    if (frame_buffer_bpp != 32 || target == 0) return;
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((unsigned int)x >= target_width || (unsigned int)y >= target_height) return;
    if ((unsigned int)(x + w) > target_width) w = (int)target_width - x;
    if ((unsigned int)(y + h) > target_height) h = (int)target_height - y;

    for (int row = 0; row < h; row++) {
        int py = y + row;
        uint32_t* fb_row = (uint32_t*)((uint8_t*)target + (uint32_t)py * target_pitch) + x;
        for (int col = 0; col < w; col++) {
            fb_row[col] = color;
        }
    }
}

static const uint8_t* fb_glyph_rows(char c) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t unknown[7] = {14, 17, 1, 6, 4, 0, 4};
    static const uint8_t glyphs[][7] = {
        {14, 17, 19, 21, 25, 17, 14},
        {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2},
        {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14},
        {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14},
        {14, 17, 17, 31, 17, 17, 17},
        {30, 17, 17, 30, 17, 17, 30},
        {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30},
        {31, 16, 16, 30, 16, 16, 31},
        {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 14},
        {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14},
        {7, 2, 2, 2, 18, 18, 12},
        {17, 18, 20, 24, 20, 18, 17},
        {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17},
        {17, 25, 21, 19, 17, 17, 17},
        {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13},
        {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},
        {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},
        {17, 17, 17, 21, 21, 21, 10},
        {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},
        {31, 1, 2, 4, 8, 16, 31}
    };
    static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const uint8_t colon[7] = {0, 12, 12, 0, 12, 12, 0};
    static const uint8_t semi[7] = {0, 12, 12, 0, 12, 4, 8};
    static const uint8_t lt[7] = {2, 4, 8, 16, 8, 4, 2};
    static const uint8_t gt[7] = {8, 4, 2, 1, 2, 4, 8};
    static const uint8_t eq[7] = {0, 0, 31, 0, 31, 0, 0};
    static const uint8_t plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t underscore[7] = {0, 0, 0, 0, 0, 0, 31};
    static const uint8_t pipe[7] = {4, 4, 4, 4, 4, 4, 4};
    static const uint8_t quote[7] = {10, 10, 0, 0, 0, 0, 0};
    static const uint8_t bang[7] = {4, 4, 4, 4, 4, 0, 4};
    static const uint8_t comma[7] = {0, 0, 0, 0, 12, 4, 8};

    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == ' ') return blank;
    if (c >= '0' && c <= '9') return glyphs[c - '0'];
    if (c >= 'A' && c <= 'Z') return glyphs[10 + c - 'A'];
    if (c == '-') return dash;
    if (c == '.') return dot;
    if (c == '/') return slash;
    if (c == ':') return colon;
    if (c == ';') return semi;
    if (c == '<') return lt;
    if (c == '>') return gt;
    if (c == '=') return eq;
    if (c == '+') return plus;
    if (c == '_') return underscore;
    if (c == '|') return pipe;
    if (c == '"' || c == '\'') return quote;
    if (c == '!') return bang;
    if (c == ',') return comma;
    return unknown;
}

static void fb_draw_char_api(int cell_x, int cell_y, char c) {
    const uint8_t* rows;
    int cell_w = (int)(active_framebuffer_width() / VGA_WIDTH);
    int cell_h = (int)(active_framebuffer_height() / VGA_HEIGHT);
    int x = cell_x * cell_w;
    int y = cell_y * cell_h;
    int scale_x = cell_w / 7;
    int scale_y = cell_h / 10;
    uint32_t fg = fb_text_color();
    uint32_t bg = 0x00101010;

    if (frame_buffer_bpp != 32 || active_framebuffer_addr() == 0) return;
    if (cell_w < 8) cell_w = 8;
    if (cell_h < 14) cell_h = 14;
    if (scale_x < 1) scale_x = 1;
    if (scale_y < 1) scale_y = 1;
    if (bg != 0xFFFFFFFF) fb_fill_rect_api(x, y, cell_w, cell_h, bg);
    rows = fb_glyph_rows(c);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (rows[row] & (1 << (4 - col))) {
                fb_fill_rect_api(
                    x + scale_x + col * scale_x,
                    y + scale_y * 2 + row * scale_y,
                    scale_x,
                    scale_y,
                    fg
                );
            }
        }
    }
}

static void fb_draw_char_color_api(int cell_x, int cell_y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t* rows;
    int cell_w = (int)(active_framebuffer_width() / VGA_WIDTH);
    int cell_h = (int)(active_framebuffer_height() / VGA_HEIGHT);
    int x = cell_x * cell_w;
    int y = cell_y * cell_h;
    int scale_x = cell_w / 7;
    int scale_y = cell_h / 10;

    if (frame_buffer_bpp != 32 || active_framebuffer_addr() == 0) return;
    if (cell_x < 0 || cell_x >= VGA_WIDTH || cell_y < 0 || cell_y >= VGA_HEIGHT) return;
    if (cell_w < 8) cell_w = 8;
    if (cell_h < 14) cell_h = 14;
    if (scale_x < 1) scale_x = 1;
    if (scale_y < 1) scale_y = 1;

    if (bg != 0xFFFFFFFF) fb_fill_rect_api(x, y, cell_w, cell_h, bg);
    rows = fb_glyph_rows(c);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (rows[row] & (1 << (4 - col))) {
                fb_fill_rect_api(
                    x + scale_x + col * scale_x,
                    y + scale_y * 2 + row * scale_y,
                    scale_x,
                    scale_y,
                    fg
                );
            }
        }
    }
}

void fb_draw_text_at(int cell_x, int cell_y, const char* text, uint32_t fg, uint32_t bg) {
    while (*text && cell_x < VGA_WIDTH) {
        fb_draw_char_color_api(cell_x, cell_y, *text, fg, bg);
        cell_x++;
        text++;
    }
}

static void fb_scroll_console_api(void) {
    if (frame_buffer_bpp != 32 || frame_buffer_addr == 0) return;
    fb_fill_rect_api(0, 0, (int)frame_buffer_width, (int)frame_buffer_height, 0x00101010);
}

void init_serial() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write(const char* str) {
    while (*str) {
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, *str);
        str++;
    }
}

void init_graphics() {
    
    make_box(100, 100, 10, 10, 0xFF0000); 
}

void clear_screen() {
    volatile char* video = (volatile char*)0xB8000;
    for (int i = 0; i < 80 * 25 * 2; i++) {
        video[i] = 0;
    }
    if (frame_buffer_bpp == 32 && frame_buffer_addr != 0) {
        fb_fill_rect_api(0, 0, (int)frame_buffer_width, (int)frame_buffer_height, 0x00101010);
    }
    cursor_x = 0;
    cursor_y = 0;
}

void set_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= 80) x = 79;
    if (y < 0) y = 0;
    if (y >= 25) y = 24;

    cursor_x = x;
    cursor_y = y;
}

void set_framebuffer_console_enabled(int enabled) {
    framebuffer_console_enabled = enabled;
}

void set_framebuffer_draw_target(uintptr_t addr, unsigned int width, unsigned int height, unsigned int pitch) {
    frame_draw_addr = addr;
    frame_draw_width = width;
    frame_draw_height = height;
    frame_draw_pitch = pitch;
}

void set_kprint_mirror(void (*mirror)(const char* text)) {
    kprint_mirror = mirror;
}

void make_box(int w, int h, int x, int y, int color) {
    if (frame_buffer_bpp == 32) {
        uint32_t* fb = (uint32_t*)frame_buffer_addr;
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                fb[(y + i) * (frame_buffer_pitch / 4) + (x + j)] = color;
            }
        }
    }
}

void kprint(const char* str) {
    if (kprint_mirror) kprint_mirror(str);
    serial_write(str);
    volatile char* video = (volatile char*)0xB8000;
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= 25) {
                for (int i = 0; i < 24 * 80 * 2; i++) {
                    video[i] = video[i + 80 * 2];
                }
                for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i++) {
                    video[i] = 0;
                }
                if (framebuffer_console_enabled) fb_scroll_console_api();
                cursor_y = 24;
            }
        } else if (*str == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = 79;
            }
        } else {
            int pos = cursor_y * 80 + cursor_x;
            video[pos * 2] = *str;
            video[pos * 2 + 1] = text_color;
            if (framebuffer_console_enabled) fb_draw_char_api(cursor_x, cursor_y, *str);
            cursor_x++;
            if (cursor_x >= 80) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= 25) {
                    for (int i = 0; i < 24 * 80 * 2; i++) {
                        video[i] = video[i + 80 * 2];
                    }
                    for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i++) {
                        video[i] = 0;
                    }
                    if (framebuffer_console_enabled) fb_scroll_console_api();
                    cursor_y = 24;
                }
            }
        }
        str++;
    }
}

unsigned char read_key() {
    while ((inb(0x64) & 1) == 0);
    return inb(0x60);
}

unsigned char scancode_to_ascii(unsigned char sc) {
    static unsigned char table[128] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };
    if (sc < 128) return table[sc];
    return 0;
}

void cpu_info() {
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    char vendor[13];
    vendor[0] = (char)(ebx & 0xFF);
    vendor[1] = (char)((ebx >> 8) & 0xFF);
    vendor[2] = (char)((ebx >> 16) & 0xFF);
    vendor[3] = (char)((ebx >> 24) & 0xFF);
    vendor[4] = (char)(edx & 0xFF);
    vendor[5] = (char)((edx >> 8) & 0xFF);
    vendor[6] = (char)((edx >> 16) & 0xFF);
    vendor[7] = (char)((edx >> 24) & 0xFF);
    vendor[8] = (char)(ecx & 0xFF);
    vendor[9] = (char)((ecx >> 8) & 0xFF);
    vendor[10] = (char)((ecx >> 16) & 0xFF);
    vendor[11] = (char)((ecx >> 24) & 0xFF);
    vendor[12] = 0;
    kprint("CPU Vendor: ");
    kprint(vendor);
    kprint("\n");
}

void reboot() {
    kprint("Rebooting...\n");
    outb(0x64, 0xFE);
}

void beep() {
    outb(0x43, 0xB6);
    outb(0x42, 0xFF);
    outb(0x42, 0xFF);
    unsigned char tmp = inb(0x61);
    outb(0x61, tmp | 3);
    for (volatile int i = 0; i < 100000; i++);
    outb(0x61, tmp);
    kprint("Beep!\n");
}

void set_color(unsigned char color) {
    current_fg = color;
    text_color = color;
}

void time_info() {
    kprint("Time: Not implemented\n");
}

void dump_memory() {
    kprint("Memory dump (first 256 bytes):\n");
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            unsigned char byte = *( (unsigned char*)( (uintptr_t)(0x00000000 + i * 16 + j) ) );    
            char hex[3];
            hex[0] = "0123456789ABCDEF"[byte >> 4];
            hex[1] = "0123456789ABCDEF"[byte & 0xF];
            hex[2] = 0;
            kprint(hex);
            kprint(" ");
        }
        kprint("\n");
    }
}

void calc(const char* expr) {
    int a = 0, b = 0;
    char op = 0;
    int i = 0;
    while (expr[i] >= '0' && expr[i] <= '9') {
        a = a * 10 + (expr[i] - '0');
        i++;
    }
    op = expr[i++];
    while (expr[i] >= '0' && expr[i] <= '9') {
        b = b * 10 + (expr[i] - '0');
        i++;
    }
    int result = 0;
    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/' && b != 0) result = a / b;
    else {
        kprint("Invalid expression\n");
        return;
    }
    char buf[20];
    int pos = 0;
    if (result < 0) {
        buf[pos++] = '-';
        result = -result;
    }
    if (result == 0) buf[pos++] = '0';
    else {
        int temp = result;
        int digits = 0;
        while (temp) {
            digits++;
            temp /= 10;
        }
        pos += digits;
        for (int j = digits - 1; j >= 0; j--) {
            buf[j] = '0' + (result % 10);
            result /= 10;
        }
    }
    buf[pos] = 0;
    kprint("Result: ");
    kprint(buf);
    kprint("\n");
}

void stack_info() {
    unsigned int esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    kprint("ESP: 0x");
    for (int i = 7; i >= 0; i--) {
        unsigned char nib = (esp >> (i*4)) & 0xF;
        char s[2];
        s[0] = "0123456789ABCDEF"[nib];
        s[1] = 0;
        kprint(s);
    }
    kprint("\n");
}

void shutdown() {
    kprint("Shutting down...\n");
    while (1) __asm__ volatile("hlt");
}

void whoami() {
    kprint("root\n");
}

    
