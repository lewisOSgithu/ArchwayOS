#include "os.h"


#define VGA_TEXT_BUFFER ((volatile char*)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ATTR_WHITE 15
#define VGA_ATTR_LIGHT_BLUE 9
#define VGA_ATTR_DESKTOP 0x17
#define VGA_ATTR_PANEL 0x1F
#define VGA_ATTR_TASKBAR 0x70
#define VGA_ATTR_START 0x2F
#define VGA_ATTR_ICON 0x1E
#define VGA_ATTR_ICON_SHADOW 0x10
#define VGA_ATTR_MENU 0x0F
#define VGA_ATTR_MENU_TITLE 0x1F
#define VGA_ATTR_WINDOW 0x0F
#define VGA_ATTR_WINDOW_TITLE 0x70
#define VGA_ATTR_BORDER 0x1F
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289
#define MULTIBOOT2_TAG_TYPE_CMDLINE 1
#define MULTIBOOT2_TAG_TYPE_MMAP 6
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER 8
#define LOW_IDENTITY_MAP_LIMIT 0x200000ULL
#define DESKTOP_WINDOW_TERMINAL 1
#define DESKTOP_WINDOW_SETTINGS 2
#define DESKTOP_WINDOW_FILES 3
#define DESKTOP_WINDOW_SYSTEM 4
#define DESKTOP_WINDOW_ABOUT 5
#define DESKTOP_WINDOW_EDITOR 6
#define DESKTOP_WINDOW_TASKMANAGER 7
#define FB_BACKBUFFER_MAX_WIDTH 1920
#define FB_BACKBUFFER_MAX_HEIGHT 1080
#define DESKTOP_DRAG_MOVE 1
#define DESKTOP_DRAG_RESIZE 2
#define TERMINAL_LOG_LINES 32
#define TERMINAL_LOG_COLS 74

static unsigned char desktop_background_attr = VGA_ATTR_DESKTOP;
static int prefer_desktop_boot = 1;
static int desktop_active_window = 0;
static int terminal_row = 4;
static int terminal_col = 18;
static int settings_row = 3;
static int settings_col = 15;
static int files_row = 5;
static int files_col = 20;
static int system_row = 6;
static int system_col = 22;
static int about_row = 7;
static int about_col = 24;
static int editor_row = 5;
static int editor_col = 26;
static int taskman_row = 4;
static int taskman_col = 28;
static int terminal_px_x = 430;
static int terminal_px_y = 230;
static int terminal_px_w = 960;
static int terminal_px_h = 560;
static int settings_px_x = 520;
static int settings_px_y = 180;
static int settings_px_w = 740;
static int settings_px_h = 430;
static int files_px_x = 500;
static int files_px_y = 220;
static int files_px_w = 820;
static int files_px_h = 480;
static int system_px_x = 560;
static int system_px_y = 260;
static int system_px_w = 720;
static int system_px_h = 380;
static int about_px_x = 650;
static int about_px_y = 300;
static int about_px_w = 580;
static int about_px_h = 320;
static int editor_px_x = 540;
static int editor_px_y = 210;
static int editor_px_w = 800;
static int editor_px_h = 500;
static int taskman_px_x = 460;
static int taskman_px_y = 190;
static int taskman_px_w = 920;
static int taskman_px_h = 520;
static int terminal_open = 0;
static int settings_open = 0;
static int files_open = 0;
static int system_open = 0;
static int about_open = 0;
static int editor_open = 0;
static int taskman_open = 0;
static int files_scroll = 0;
static int editor_scroll = 0;
static int desktop_cursor_row = 12;
static int desktop_cursor_col = 40;
static int fb_cursor_x = 960;
static int fb_cursor_y = 540;
static int framebuffer_available = 0;
static int boot_text_mode = 0;
static int desktop_hidden_terminal = 0;
static int start_menu_open = 0;
static int context_menu_open = 0;
static int context_menu_x = 0;
static int context_menu_y = 0;
static int dragging_window = 0;
static int drag_mode = 0;
static int drag_redraw_pending_px = 0;
static int mouse_accum_x = 0;
static int mouse_accum_y = 0;
static int mouse_speed = 3;
static int desktop_serial_banner_sent = 0;
static int fb_backbuffer_enabled = 0;
static int fb_frame_drawing = 0;
static const char* terminal_input_text = "";
static char terminal_log[TERMINAL_LOG_LINES][TERMINAL_LOG_COLS + 1];
static int terminal_log_line = 0;
static int terminal_log_col = 0;
static int terminal_log_scroll = 0;
static int fb_cursor_saved_valid = 0;
static int fb_cursor_saved_x = 0;
static int fb_cursor_saved_y = 0;
static uint32_t fb_cursor_saved_pixels[32 * 36];
static uint32_t fb_backbuffer[FB_BACKBUFFER_MAX_WIDTH * FB_BACKBUFFER_MAX_HEIGHT];

static void draw_desktop_windows(void);
static void draw_desktop_screen(void);
static void desktop_redraw_with_prompt(void);
static void desktop_kprint_dec(uint64_t value);

static const char* const archway_logo[] = {
    "        @@@@@        ",
    "     @@@@@@@@@@@     ",
    "   @@@@@@@@@@@@@@@   ",
    "  @@@@@@@@@@@@@@@@@  ",
    " @@@@@@       @@@@@@ ",
    " @@@@@  @@@@@  @@@@@ ",
    "@@@@@ @@     @@ @@@@@",
    "@@@@@ @@     @@ @@@@@",
    " @@@@ @@     @@ @@@@ ",
    " @@@@ @@     @@ @@@@ ",
    "  @@@@@@@@@@@@@@@@@  ",
    "    @@@@@@@@@@@@@    ",
    "       @@@@@@@       "
};

static const int archway_logo_line_count = 13;

static void vga_put_at(int row, int col, unsigned char color, char ch) {
    volatile char* video = VGA_TEXT_BUFFER;
    int offset = (row * VGA_WIDTH + col) * 2;

    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) return;
    video[offset] = ch;
    video[offset + 1] = color;
}

static void vga_write_at(int row, int col, unsigned char color, const char* text) {
    while (*text && col < VGA_WIDTH) {
        vga_put_at(row, col, color, *text);
        text++;
        col++;
    }
}

static int boot_word_matches(const char* word, const char* wanted) {
    while (*wanted) {
        if (*word != *wanted) return 0;
        word++;
        wanted++;
    }
    return *word == 0 || *word == ' ' || *word == '\t';
}

static int boot_cmdline_has_word(const char* cmdline, const char* wanted) {
    while (*cmdline) {
        while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
        if (boot_word_matches(cmdline, wanted)) return 1;
        while (*cmdline && *cmdline != ' ' && *cmdline != '\t') cmdline++;
    }
    return 0;
}

static void capture_multiboot_cmdline(void) {
    if (kernel_boot_magic != MULTIBOOT2_BOOTLOADER_MAGIC || kernel_boot_info_addr == 0) return;

    uint8_t* info = (uint8_t*)kernel_boot_info_addr;
    uint32_t total_size = *(uint32_t*)info;
    uint8_t* tag = info + 8;
    uint8_t* end = info + total_size;

    while (tag + 8 <= end) {
        uint32_t type = *(uint32_t*)tag;
        uint32_t size = *(uint32_t*)(tag + 4);
        if (type == 0 || size < 8) break;

        if (type == MULTIBOOT2_TAG_TYPE_CMDLINE) {
            const char* cmdline = (const char*)(tag + 8);
            if (boot_cmdline_has_word(cmdline, "text")) boot_text_mode = 1;
            if (boot_cmdline_has_word(cmdline, "desktop")) boot_text_mode = 0;
            return;
        }

        tag += (size + 7) & ~7;
    }
}

static void capture_multiboot_framebuffer(void) {
    if (kernel_boot_magic != MULTIBOOT2_BOOTLOADER_MAGIC || kernel_boot_info_addr == 0) return;

    uint8_t* info = (uint8_t*)kernel_boot_info_addr;
    uint32_t total_size = *(uint32_t*)info;
    uint8_t* tag = info + 8;
    uint8_t* end = info + total_size;

    while (tag + 8 <= end) {
        uint32_t type = *(uint32_t*)tag;
        uint32_t size = *(uint32_t*)(tag + 4);
        if (type == 0 || size < 8) break;

        if (type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER && size >= 32) {
            frame_buffer_addr = (uintptr_t)(*(uint64_t*)(tag + 8));
            frame_buffer_pitch = *(uint32_t*)(tag + 16);
            frame_buffer_width = *(uint32_t*)(tag + 20);
            frame_buffer_height = *(uint32_t*)(tag + 24);
            frame_buffer_bpp = *(uint8_t*)(tag + 28);
            framebuffer_available = frame_buffer_addr != 0 &&
                frame_buffer_addr <= 0xFFFFFFFFULL &&
                frame_buffer_bpp == 32;
            fb_backbuffer_enabled = framebuffer_available &&
                frame_buffer_width <= FB_BACKBUFFER_MAX_WIDTH &&
                frame_buffer_height <= FB_BACKBUFFER_MAX_HEIGHT;
            return;
        }

        tag += (size + 7) & ~7;
    }
}

static uintptr_t fb_draw_addr(void) {
    if (fb_backbuffer_enabled) return (uintptr_t)fb_backbuffer;
    return frame_buffer_addr;
}

static unsigned int fb_draw_pitch(void) {
    if (fb_backbuffer_enabled) return frame_buffer_width * 4;
    return frame_buffer_pitch;
}

static void fb_put_screen_pixel(int x, int y, uint32_t color) {
    if (!framebuffer_available) return;
    if (x < 0 || y < 0) return;
    if ((unsigned int)x >= frame_buffer_width || (unsigned int)y >= frame_buffer_height) return;

    uint8_t* row = (uint8_t*)frame_buffer_addr + (uint32_t)y * frame_buffer_pitch;
    *(uint32_t*)(row + (uint32_t)x * 4) = color;
}

static uint32_t fb_get_screen_pixel(int x, int y) {
    if (!framebuffer_available) return 0;
    if (x < 0 || y < 0) return 0;
    if ((unsigned int)x >= frame_buffer_width || (unsigned int)y >= frame_buffer_height) return 0;

    uint8_t* row = (uint8_t*)frame_buffer_addr + (uint32_t)y * frame_buffer_pitch;
    return *(uint32_t*)(row + (uint32_t)x * 4);
}

static void fb_fill_screen_rect(int x, int y, int width, int height, uint32_t color) {
    if (!framebuffer_available || width <= 0 || height <= 0) return;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if ((unsigned int)x >= frame_buffer_width || (unsigned int)y >= frame_buffer_height) return;
    if ((unsigned int)(x + width) > frame_buffer_width) width = (int)frame_buffer_width - x;
    if ((unsigned int)(y + height) > frame_buffer_height) height = (int)frame_buffer_height - y;

    for (int row = 0; row < height; row++) {
        uint32_t* dst = (uint32_t*)((uint8_t*)frame_buffer_addr + (uint32_t)(y + row) * frame_buffer_pitch) + x;
        for (int col = 0; col < width; col++) dst[col] = color;
    }
}

static void fb_draw_screen_rect(int x, int y, int width, int height, uint32_t color) {
    fb_fill_screen_rect(x, y, width, 2, color);
    fb_fill_screen_rect(x, y + height - 2, width, 2, color);
    fb_fill_screen_rect(x, y, 2, height, color);
    fb_fill_screen_rect(x + width - 2, y, 2, height, color);
}

static void fb_begin_frame(void) {
    if (!framebuffer_available || !fb_backbuffer_enabled) return;
    fb_frame_drawing = 1;
    set_framebuffer_draw_target((uintptr_t)fb_backbuffer, frame_buffer_width, frame_buffer_height, frame_buffer_width * 4);
}

static void fb_present_frame(void) {
    if (!framebuffer_available || !fb_backbuffer_enabled) return;

    for (unsigned int y = 0; y < frame_buffer_height; y++) {
        uint32_t* dst = (uint32_t*)((uint8_t*)frame_buffer_addr + y * frame_buffer_pitch);
        uint32_t* src = fb_backbuffer + y * frame_buffer_width;
        for (unsigned int x = 0; x < frame_buffer_width; x++) dst[x] = src[x];
    }

    set_framebuffer_draw_target(0, 0, 0, 0);
    fb_frame_drawing = 0;
}

static void fb_put_pixel(int x, int y, uint32_t color) {
    if (!framebuffer_available) return;
    if (x < 0 || y < 0) return;
    if ((unsigned int)x >= frame_buffer_width || (unsigned int)y >= frame_buffer_height) return;

    uint8_t* row = (uint8_t*)fb_draw_addr() + (uint32_t)y * fb_draw_pitch();
    *(uint32_t*)(row + (uint32_t)x * 4) = color;
}

static void fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    uintptr_t base;
    unsigned int pitch;

    if (!framebuffer_available || width <= 0 || height <= 0) return;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if ((unsigned int)x >= frame_buffer_width || (unsigned int)y >= frame_buffer_height) return;
    if ((unsigned int)(x + width) > frame_buffer_width) width = (int)frame_buffer_width - x;
    if ((unsigned int)(y + height) > frame_buffer_height) height = (int)frame_buffer_height - y;

    base = fb_draw_addr();
    pitch = fb_draw_pitch();
    for (int row = 0; row < height; row++) {
        uint32_t* dst = (uint32_t*)((uint8_t*)base + (uint32_t)(y + row) * pitch) + x;
        for (int col = 0; col < width; col++) dst[col] = color;
    }
}

static void fb_draw_rect(int x, int y, int width, int height, uint32_t color) {
    fb_fill_rect(x, y, width, 2, color);
    fb_fill_rect(x, y + height - 2, width, 2, color);
    fb_fill_rect(x, y, 2, height, color);
    fb_fill_rect(x + width - 2, y, 2, height, color);
}

static void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void fb_fill_circle(int cx, int cy, int radius, uint32_t color) {
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= r2) fb_put_pixel(cx + x, cy + y, color);
        }
    }
}

static void fb_fill_round_rect(int x, int y, int width, int height, int radius, uint32_t color) {
    if (radius < 1) {
        fb_fill_rect(x, y, width, height, color);
        return;
    }
    if (radius * 2 > height) radius = height / 2;
    if (radius * 2 > width) radius = width / 2;
    fb_fill_rect(x + radius, y, width - radius * 2, height, color);
    fb_fill_rect(x, y + radius, width, height - radius * 2, color);
    fb_fill_circle(x + radius, y + radius, radius, color);
    fb_fill_circle(x + width - radius - 1, y + radius, radius, color);
    fb_fill_circle(x + radius, y + height - radius - 1, radius, color);
    fb_fill_circle(x + width - radius - 1, y + height - radius - 1, radius, color);
}

static void fb_draw_soft_rect(int x, int y, int width, int height, int radius, uint32_t fill, uint32_t shadow) {
    fb_fill_round_rect(x + 5, y + 5, width, height, radius, shadow);
    fb_fill_round_rect(x, y, width, height, radius, fill);
}

static uint32_t fb_text_color_from_vga(unsigned char color) {
    switch (color & 0x0F) {
        case 0x09: return 0x006EA8FF;
        case 0x0F: return 0x00FFFFFF;
        case 0x07: return 0x00D7D7D7;
        case 0x02: return 0x0078E08A;
        default: return 0x00FFFFFF;
    }
}

static void fb_console_print_at(int cell_x, int cell_y, unsigned char color, const char* text) {
    fb_draw_text_at(cell_x, cell_y, text, fb_text_color_from_vga(color), 0xFFFFFFFF);
}

static void fb_console_print_clipped_at(int cell_x, int cell_y, int max_cells, unsigned char color, const char* text) {
    char clipped[81];
    int pos = 0;

    if (max_cells <= 0) return;
    if (max_cells > 80) max_cells = 80;
    while (*text && pos < max_cells) {
        clipped[pos++] = *text++;
    }
    clipped[pos] = 0;
    fb_draw_text_at(cell_x, cell_y, clipped, fb_text_color_from_vga(color), 0xFFFFFFFF);
}

static void fb_window_text_at(int cell_x, int cell_y, int max_cells, const char* text) {
    char clipped[81];
    int pos = 0;

    if (max_cells <= 0) return;
    if (max_cells > 80) max_cells = 80;
    while (*text && pos < max_cells) {
        clipped[pos++] = *text++;
    }
    clipped[pos] = 0;
    fb_draw_text_at(cell_x, cell_y, clipped, 0x00102030, 0xFFFFFFFF);
}

static void fb_window_heading_at(int cell_x, int cell_y, int max_cells, const char* text) {
    char clipped[81];
    int pos = 0;

    if (max_cells <= 0) return;
    if (max_cells > 80) max_cells = 80;
    while (*text && pos < max_cells) {
        clipped[pos++] = *text++;
    }
    clipped[pos] = 0;
    fb_draw_text_at(cell_x, cell_y, clipped, 0x000C8DCE, 0xFFFFFFFF);
}

static void fb_terminal_text_at(int cell_x, int cell_y, int max_cells, const char* text) {
    char clipped[81];
    int pos = 0;

    if (max_cells <= 0) return;
    if (max_cells > 80) max_cells = 80;
    while (*text && pos < max_cells) {
        clipped[pos++] = *text++;
    }
    clipped[pos] = 0;
    fb_draw_text_at(cell_x, cell_y, clipped, 0x00FFFFFF, 0x00101820);
}

static int terminal_cell_row(void) {
    return (terminal_px_y * VGA_HEIGHT) / (int)frame_buffer_height;
}

static int terminal_cell_col(void) {
    return (terminal_px_x * VGA_WIDTH) / (int)frame_buffer_width;
}

static int terminal_cell_width(void) {
    int width = (terminal_px_w * VGA_WIDTH) / (int)frame_buffer_width;
    return width < 20 ? 20 : width;
}

static int terminal_cell_height(void) {
    int height = (terminal_px_h * VGA_HEIGHT) / (int)frame_buffer_height;
    return height < 8 ? 8 : height;
}

static int terminal_prompt_row(void) {
    return terminal_cell_row() + terminal_cell_height() - 3;
}

static int terminal_prompt_col(void) {
    return terminal_cell_col() + 12;
}

static void terminal_log_newline(void) {
    terminal_log_line = (terminal_log_line + 1) % TERMINAL_LOG_LINES;
    terminal_log_col = 0;
    terminal_log[terminal_log_line][0] = 0;
}

static void terminal_log_put_char(char c) {
    terminal_log_scroll = 0;
    if (c == '\r') return;
    if (c == '\n') {
        terminal_log_newline();
        return;
    }
    if (c == '\b') {
        if (terminal_log_col > 0) {
            terminal_log_col--;
            terminal_log[terminal_log_line][terminal_log_col] = 0;
        }
        return;
    }
    if (terminal_log_col >= TERMINAL_LOG_COLS) terminal_log_newline();
    terminal_log[terminal_log_line][terminal_log_col++] = c;
    terminal_log[terminal_log_line][terminal_log_col] = 0;
}

static void terminal_log_append(const char* text) {
    while (*text) {
        terminal_log_put_char(*text);
        text++;
    }
}

static void terminal_log_clear(void) {
    for (int i = 0; i < TERMINAL_LOG_LINES; i++) {
        terminal_log[i][0] = 0;
    }
    terminal_log_line = 0;
    terminal_log_col = 0;
    terminal_log_scroll = 0;
}

static void u32_to_dec_text(uint32_t value, char* out) {
    char tmp[11];
    int pos = 0;
    int out_pos = 0;
    if (value == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (value && pos < 10) {
        tmp[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (pos > 0) out[out_pos++] = tmp[--pos];
    out[out_pos] = 0;
}

static void fb_draw_shadowed_rect(int x, int y, int width, int height, uint32_t fill, uint32_t border) {
    fb_fill_rect(x + 8, y + 8, width, height, 0x00404040);
    fb_fill_rect(x, y, width, height, fill);
    fb_draw_rect(x, y, width, height, border);
}

static void fb_draw_close_button(int x, int y, int size) {
    if (size < 18) size = 18;
    fb_fill_rect(x, y, size, size, 0x00B53A3A);
    fb_draw_rect(x, y, size, size, 0x00FFFFFF);
    for (int i = 5; i < size - 5; i++) {
        fb_fill_rect(x + i, y + i, 2, 2, 0x00FFFFFF);
        fb_fill_rect(x + size - i - 2, y + i, 2, 2, 0x00FFFFFF);
    }
}

static int desktop_px_x(int col) {
    return ((int)frame_buffer_width * col) / VGA_WIDTH;
}

static int desktop_px_y(int row) {
    return ((int)frame_buffer_height * row) / VGA_HEIGHT;
}

static void clamp_fb_cursor(void) {
    if (!framebuffer_available) return;
    if (fb_cursor_x < 0) fb_cursor_x = 0;
    if (fb_cursor_y < 0) fb_cursor_y = 0;
    if ((unsigned int)fb_cursor_x >= frame_buffer_width) fb_cursor_x = (int)frame_buffer_width - 1;
    if ((unsigned int)fb_cursor_y >= frame_buffer_height) fb_cursor_y = (int)frame_buffer_height - 1;
}

static void sync_fb_cursor_to_desktop_cursor(void) {
    if (!framebuffer_available) return;
    clamp_fb_cursor();
    desktop_cursor_col = (fb_cursor_x * VGA_WIDTH) / (int)frame_buffer_width;
    desktop_cursor_row = (fb_cursor_y * VGA_HEIGHT) / (int)frame_buffer_height;
}

static void fb_restore_pixel_cursor(void) {
    if (!framebuffer_available || !fb_cursor_saved_valid) return;
    for (int y = 0; y < 36; y++) {
        for (int x = 0; x < 32; x++) {
            fb_put_screen_pixel(fb_cursor_saved_x + x, fb_cursor_saved_y + y, fb_cursor_saved_pixels[y * 32 + x]);
        }
    }
    fb_cursor_saved_valid = 0;
}

static void fb_save_pixel_cursor_area(int x, int y) {
    if (!framebuffer_available) return;
    fb_cursor_saved_x = x;
    fb_cursor_saved_y = y;
    for (int row = 0; row < 36; row++) {
        for (int col = 0; col < 32; col++) {
            fb_cursor_saved_pixels[row * 32 + col] = fb_get_screen_pixel(x + col, y + row);
        }
    }
    fb_cursor_saved_valid = 1;
}

static void fb_draw_pixel_cursor(void) {
    clamp_fb_cursor();
    int x = fb_cursor_x;
    int y = fb_cursor_y;
    if (!fb_frame_drawing) fb_save_pixel_cursor_area(x, y);
    fb_fill_screen_rect(x, y, 4, 24, 0x00FFFFFF);
    fb_fill_screen_rect(x, y, 18, 4, 0x00FFFFFF);
    fb_fill_screen_rect(x + 4, y + 4, 12, 4, 0x00FFFFFF);
    fb_fill_screen_rect(x + 8, y + 8, 8, 4, 0x00FFFFFF);
    fb_fill_screen_rect(x + 12, y + 12, 8, 4, 0x00FFFFFF);
    fb_fill_screen_rect(x + 16, y + 16, 8, 4, 0x00FFFFFF);
    fb_draw_screen_rect(x, y, 24, 28, 0x00000000);
}

static void draw_vbe_icon(int row, int col, uint32_t color, const char* label) {
    int x = desktop_px_x(col);
    int y = desktop_px_y(row);
    int w = desktop_px_x(col + 5) - x;
    int h = desktop_px_y(row + 3) - y;
    int center_x = x + w / 2;
    int icon_top = y;
    uint32_t text_color = 0x00102030;

    if (color == 0x00A9D6FF) {
        int body_y = icon_top + h / 4;
        fb_fill_round_rect(x + w / 8, body_y, w * 3 / 4, h / 2, 8, 0x007FB7E7);
        fb_fill_round_rect(x + w / 5, body_y - h / 8, w / 3, h / 5, 6, 0x0095C7EE);
        fb_fill_rect(x + w / 8, body_y + h / 5, w * 3 / 4, h / 3, 0x006CA9DE);
    } else if (color == 0x00C7C7C7 || color == 0x00A5E36E) {
        int r = h / 4;
        fb_fill_circle(center_x, icon_top + h / 2, r + 12, 0x000C8DCE);
        for (int i = 0; i < 8; i++) {
            int tx = center_x + ((i & 1) ? r : -r);
            int ty = icon_top + h / 2 + ((i & 2) ? r : -r);
            fb_fill_rect(tx - 5, ty - 5, 10, 10, 0x000C8DCE);
        }
        fb_fill_circle(center_x, icon_top + h / 2, r / 2, 0x00C7F5F8);
    } else if (color == 0x00FFFFFF) {
        fb_draw_soft_rect(x + w / 7, icon_top + h / 5, w * 5 / 7, h / 2, 8, 0x00FFFFFF, 0x006AAAD8);
        fb_fill_rect(x + w / 5, icon_top + h / 3, w * 3 / 5, 4, 0x006EA8FF);
        fb_fill_rect(x + w / 5, icon_top + h / 2, w / 2, 4, 0x006EA8FF);
    } else {
        fb_fill_circle(center_x, icon_top + h / 2, h / 3, color);
        fb_fill_rect(center_x - 8, icon_top + h / 3, 16, h / 3, 0x00E6FAFF);
    }
    fb_draw_text_at(col, row + 3, label, text_color, 0xFFFFFFFF);
}

static void draw_vbe_start_menu(void) {
    int x = desktop_px_x(3);
    int y = desktop_px_y(9);
    int w = desktop_px_x(22) - x;
    int h = desktop_px_y(23) - y;
    uint32_t ink = 0x00102030;
    fb_draw_soft_rect(x, y, w, h, 18, 0x00EAF7FA, 0x006E99A8);
    fb_fill_round_rect(x + 12, y + 12, w - 24, desktop_px_y(2), 12, 0x002EB4DD);
    fb_draw_text_at(5, 10, "Archway Start", 0x00FFFFFF, 0xFFFFFFFF);
    fb_draw_text_at(5, 12, "Terminal", ink, 0xFFFFFFFF);
    fb_draw_text_at(5, 13, "Settings", ink, 0xFFFFFFFF);
    fb_draw_text_at(5, 14, "System", ink, 0xFFFFFFFF);
    fb_draw_text_at(5, 15, "About PC", ink, 0xFFFFFFFF);
    fb_draw_text_at(5, 16, "Documents", ink, 0xFFFFFFFF);
    fb_draw_text_at(5, 17, "Task Manager", ink, 0xFFFFFFFF);
    fb_fill_rect(x + 20, desktop_px_y(18), w - 40, 2, 0x00A8C8D4);
    fb_draw_text_at(5, 20, "Lock", 0x002B7190, 0xFFFFFFFF);
    fb_draw_text_at(5, 21, "Poweroff", 0x002B7190, 0xFFFFFFFF);
}

static void draw_vbe_context_menu(void) {
    int x = context_menu_x;
    int y = context_menu_y;
    int w = desktop_px_x(18);
    int h = desktop_px_y(9);
    uint32_t ink = 0x00102030;

    if (x + w > (int)frame_buffer_width - 12) x = (int)frame_buffer_width - w - 12;
    if (y + h > desktop_px_y(24) - 8) y = desktop_px_y(24) - h - 8;
    if (x < 8) x = 8;
    if (y < desktop_px_y(2)) y = desktop_px_y(2);

    context_menu_x = x;
    context_menu_y = y;
    fb_draw_soft_rect(x, y, w, h, 12, 0x00F7FAFC, 0x006E99A8);
    fb_fill_rect(x + 12, y + desktop_px_y(1), w - 24, 2, 0x00A8C8D4);
    fb_draw_text_at((x * VGA_WIDTH) / (int)frame_buffer_width + 2, (y * VGA_HEIGHT) / (int)frame_buffer_height + 1, "Open Terminal", ink, 0xFFFFFFFF);
    fb_draw_text_at((x * VGA_WIDTH) / (int)frame_buffer_width + 2, (y * VGA_HEIGHT) / (int)frame_buffer_height + 2, "Open Files", ink, 0xFFFFFFFF);
    fb_draw_text_at((x * VGA_WIDTH) / (int)frame_buffer_width + 2, (y * VGA_HEIGHT) / (int)frame_buffer_height + 3, "Task Manager", ink, 0xFFFFFFFF);
    fb_draw_text_at((x * VGA_WIDTH) / (int)frame_buffer_width + 2, (y * VGA_HEIGHT) / (int)frame_buffer_height + 4, "Refresh", ink, 0xFFFFFFFF);
    fb_draw_text_at((x * VGA_WIDTH) / (int)frame_buffer_width + 2, (y * VGA_HEIGHT) / (int)frame_buffer_height + 5, "Lock", ink, 0xFFFFFFFF);
}

static void draw_vbe_wallpaper_logo(void) {
    int cx = (int)frame_buffer_width / 2;
    int cy = (int)frame_buffer_height / 2;
    int r = (int)frame_buffer_height / 6;
    int arch_w = r / 5;
    int top = cy - r / 2;
    uint32_t blue = 0x000EA8E8;
    uint32_t dark = 0x000D8CC2;
    uint32_t light = 0x00C9F6FB;

    if (r < 80) r = 80;
    fb_fill_circle(cx, cy, r, blue);
    fb_fill_rect(cx + r / 5, cy - r / 4, r * 3 / 5, r, dark);
    fb_fill_rect(cx - r / 2, cy + r / 5, r, r / 2, dark);
    fb_fill_rect(cx - r / 2, top, arch_w, r, light);
    fb_fill_rect(cx + r / 2 - arch_w, top, arch_w, r, light);
    for (int i = 0; i < 13; i++) {
        int bx = cx - r / 2 + i * (r / 13);
        int by = top - (i < 7 ? i : 12 - i) * (r / 30);
        fb_fill_rect(bx, by, r / 20, r / 13, light);
    }
}

static void draw_vbe_start_screen(void) {
    int w = (int)frame_buffer_width;
    int h = (int)frame_buffer_height;
    int panel_w = w > 760 ? 560 : w - 80;
    int panel_h = 260;
    int panel_x = (w - panel_w) / 2;
    int panel_y = h > 500 ? 70 : 40;

    fb_begin_frame();
    fb_fill_rect(0, 0, w, h, 0x001C3559);
    for (int y = 0; y < h; y += 18) {
        uint32_t shade = 0x001C3559 + (uint32_t)(((y / 18) & 7) * 0x00030303);
        fb_fill_rect(0, y, w, 9, shade);
    }

    fb_draw_shadowed_rect(panel_x, panel_y, panel_w, panel_h, 0x00101824, 0x006EA8FF);
    fb_fill_rect(panel_x, panel_y, panel_w, 42, 0x00264B78);

    fb_console_print_at(panel_x / 8 + 4, panel_y / 14 + 3, VGA_ATTR_LIGHT_BLUE, "ARCHWAYOS");
    fb_console_print_at(panel_x / 8 + 4, panel_y / 14 + 6, VGA_ATTR_WHITE, "Version 0.2.0 Desktop VBE");
    fb_console_print_at(panel_x / 8 + 4, panel_y / 14 + 8, VGA_ATTR_LIGHT_BLUE, "Open source preview build");
    fb_console_print_at(panel_x / 8 + 4, panel_y / 14 + 12, VGA_ATTR_WHITE, "Press any key to start");
    fb_console_print_at(panel_x / 8 + 4, panel_y / 14 + 15, VGA_ATTR_LIGHT_BLUE, "Text mode: make run text");
    fb_present_frame();
    set_cursor(0, 24);
}

static void draw_vbe_desktop_screen(void) {
    if (!framebuffer_available) return;

    uint32_t wallpaper = 0x00BFEFF2;
    int top_w = desktop_px_x(34);
    int top_h = desktop_px_y(2) - desktop_px_y(0);
    int top_x = ((int)frame_buffer_width - top_w) / 2;
    int top_y = desktop_px_y(1);
    int task_x = desktop_px_x(8);
    int task_y = desktop_px_y(22);
    int task_w = desktop_px_x(68) - task_x;
    int task_h = desktop_px_y(24) - task_y;
    int dock_y = desktop_px_y(24);
    fb_begin_frame();
    fb_cursor_saved_valid = 0;
    fb_fill_rect(0, 0, (int)frame_buffer_width, (int)frame_buffer_height, wallpaper);
    for (unsigned int y = 0; y < frame_buffer_height; y += 24) {
        uint32_t shade = (y / 24) & 1 ? 0x00B7E8EC : 0x00C7F5F8;
        fb_fill_rect(0, (int)y, (int)frame_buffer_width, 10, shade);
    }
    for (int x = desktop_px_x(1); x < (int)frame_buffer_width; x += desktop_px_x(8)) {
        fb_draw_line(x, desktop_px_y(2), x + desktop_px_x(3), desktop_px_y(21), 0x00A9DDE3);
    }
    draw_vbe_wallpaper_logo();

    fb_fill_round_rect(top_x + 4, top_y + 5, top_w, top_h, top_h / 2, 0x007FA7B0);
    fb_fill_round_rect(top_x, top_y, top_w, top_h, top_h / 2, 0x0045545D);
    fb_fill_round_rect(top_x + 12, top_y + 9, top_w - 24, top_h - 18, (top_h - 18) / 2, 0x005B6972);
    fb_draw_text_at(37, 2, "4:00", 0x00F8FEFF, 0xFFFFFFFF);
    fb_draw_text_at(51, 2, "Lock", 0x008BD4FF, 0xFFFFFFFF);
    fb_fill_circle(top_x + top_w - top_h / 2, top_y + top_h / 2, top_h / 3, 0x00FF4B4B);
    fb_fill_circle(top_x + top_w - top_h / 2, top_y + top_h / 2, top_h / 5, 0x00C92634);
    fb_fill_rect(top_x + top_w - top_h / 2 - 3, top_y + top_h / 2 - 12, 6, 16, 0x00FF4B4B);
    fb_draw_text_at(2, 0, "ArchwayOS", 0x00102030, 0xFFFFFFFF);

    fb_fill_round_rect(task_x + 4, task_y + 5, task_w, task_h, task_h / 2, 0x0082AFC0);
    fb_fill_round_rect(task_x, task_y, task_w, task_h, task_h / 2, 0x002FB3E8);
    fb_fill_round_rect(desktop_px_x(10), task_y + task_h / 7, desktop_px_x(24) - desktop_px_x(10), task_h * 5 / 7, task_h / 3, 0x004A5258);
    fb_fill_round_rect(desktop_px_x(25), task_y + task_h / 7, desktop_px_x(39) - desktop_px_x(25), task_h * 5 / 7, task_h / 3, 0x004A5258);
    fb_fill_round_rect(desktop_px_x(40), task_y + task_h / 7, desktop_px_x(54) - desktop_px_x(40), task_h * 5 / 7, task_h / 3, 0x004A5258);
    fb_draw_text_at(14, 23, "Settings", 0x00FFFFFF, 0xFFFFFFFF);
    fb_draw_text_at(30, 23, "Files", 0x00FFFFFF, 0xFFFFFFFF);
    fb_draw_text_at(44, 23, "Tasks", 0x00FFFFFF, 0xFFFFFFFF);
    fb_fill_round_rect(desktop_px_x(2) + 4, dock_y + 4, desktop_px_x(74) - desktop_px_x(2), desktop_px_y(25) - dock_y - 8, 18, 0x0082AFC0);
    fb_fill_round_rect(desktop_px_x(2), dock_y, desktop_px_x(74) - desktop_px_x(2), desktop_px_y(25) - dock_y - 8, 18, 0x002FB3E8);
    fb_fill_circle(desktop_px_x(5), dock_y + (desktop_px_y(25) - dock_y) / 2 - 4, desktop_px_y(1) / 2, 0x001F8DBD);
    fb_draw_text_at(4, 24, "A", 0x00C9F6FB, 0xFFFFFFFF);

    draw_vbe_icon(3, 4, 0x00A9D6FF, "Files");
    draw_vbe_icon(8, 4, 0x00C7C7C7, "Settings");
    draw_vbe_icon(13, 4, 0x00A5E36E, "System");
    draw_vbe_icon(18, 4, 0x00F0B0D0, "About");
    draw_vbe_icon(3, 14, 0x00F3C04F, "Terminal");
    draw_vbe_icon(8, 14, 0x00FFFFFF, "Editor");
    draw_vbe_icon(13, 14, 0x002FB3E8, "Tasks");

    draw_desktop_windows();
    if (start_menu_open) draw_vbe_start_menu();
    if (context_menu_open) draw_vbe_context_menu();

    fb_present_frame();
    fb_draw_pixel_cursor();
}

static void vga_draw_box(int row, int col, int width, int height, unsigned char border, unsigned char fill) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int top = y == 0;
            int bottom = y == height - 1;
            int left = x == 0;
            int right = x == width - 1;
            char ch = ' ';
            unsigned char color = fill;

            if ((top || bottom) && (left || right)) {
                ch = '+';
                color = border;
            } else if (top || bottom) {
                ch = '-';
                color = border;
            } else if (left || right) {
                ch = ' ';
                color = fill;
            }

            vga_put_at(row + y, col + x, color, ch);
        }
    }
}

static void draw_wallpaper(void) {
    for (int row = 1; row < VGA_HEIGHT - 1; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            char ch = ' ';
            unsigned char color = desktop_background_attr;

            if (((row * 3 + col) % 17) == 0) ch = '.';
            if (((row + col * 2) % 29) == 0) ch = '\'';
            if (row > 14 && ((row + col) % 11) == 0) color = (unsigned char)((desktop_background_attr & 0xF0) | 0x03);
            vga_put_at(row, col, color, ch);
        }
    }

    vga_write_at(2, 33, 0x1F, "ARCHWAY");
    vga_write_at(3, 31, 0x17, "desktop shell");
}

static void draw_topbar(void) {
    for (int col = 0; col < VGA_WIDTH; col++) vga_put_at(0, col, 0x1F, ' ');
    vga_write_at(0, 2, 0x1F, "ArchwayOS");
    vga_write_at(0, 58, 0x1F, "[ Lock ]");
    vga_write_at(0, 69, 0x4F, "[ Power ]");
}

static void draw_desktop_icon(int row, int col, const char* art, const char* label) {
    vga_put_at(row + 1, col + 1, VGA_ATTR_ICON_SHADOW, ' ');
    vga_put_at(row, col, VGA_ATTR_ICON, '[');
    vga_write_at(row, col + 1, VGA_ATTR_ICON, art);
    vga_put_at(row, col + 4, VGA_ATTR_ICON, ']');
    vga_write_at(row + 1, col, desktop_background_attr, label);
}

static void draw_taskbar(void) {
    for (int col = 0; col < VGA_WIDTH; col++) vga_put_at(24, col, VGA_ATTR_TASKBAR, ' ');
    vga_write_at(24, 1, VGA_ATTR_START, " Start ");
    vga_write_at(24, 10, VGA_ATTR_TASKBAR, "[ Terminal ]");
    vga_write_at(24, 23, VGA_ATTR_TASKBAR, "[ Files ]");
    vga_write_at(24, 34, VGA_ATTR_TASKBAR, "[ System ]");
    vga_write_at(24, 47, VGA_ATTR_TASKBAR, "[ Settings ]");
    vga_write_at(24, 60, VGA_ATTR_TASKBAR, "[ Tasks ]");
    vga_write_at(24, 70, VGA_ATTR_TASKBAR, "Archway");
}

static void clamp_desktop_cursor(void) {
    if (desktop_cursor_row < 0) desktop_cursor_row = 0;
    if (desktop_cursor_row >= VGA_HEIGHT) desktop_cursor_row = VGA_HEIGHT - 1;
    if (desktop_cursor_col < 0) desktop_cursor_col = 0;
    if (desktop_cursor_col >= VGA_WIDTH) desktop_cursor_col = VGA_WIDTH - 1;
}

static void draw_desktop_cursor(void) {
    clamp_desktop_cursor();
    vga_put_at(desktop_cursor_row, desktop_cursor_col, 0xF0, '^');
}

static void draw_start_menu(void) {
    vga_draw_box(11, 1, 25, 13, VGA_ATTR_BORDER, VGA_ATTR_MENU);
    for (int col = 2; col < 25; col++) vga_put_at(12, col, VGA_ATTR_MENU_TITLE, ' ');
    vga_write_at(12, 3, VGA_ATTR_MENU_TITLE, "Archway Start");
    vga_write_at(13, 3, VGA_ATTR_MENU, "terminal  - open shell");
    vga_write_at(14, 3, VGA_ATTR_MENU, "files     - open files");
    vga_write_at(15, 3, VGA_ATTR_MENU, "system    - open system");
    vga_write_at(16, 3, VGA_ATTR_MENU, "about     - open about");
    vga_write_at(17, 3, VGA_ATTR_MENU, "settings  - preferences");
    vga_write_at(18, 3, VGA_ATTR_MENU, "taskman   - app status");
    vga_write_at(20, 3, VGA_ATTR_MENU, "lock      - loading");
    vga_write_at(21, 3, VGA_ATTR_MENU, "poweroff  - halt");
    vga_write_at(22, 3, VGA_ATTR_MENU, "Type a menu command.");
}

static void clamp_window(int* row, int* col, int width, int height) {
    if (*row < 1) *row = 1;
    if (*col < 0) *col = 0;
    if (*row + height > 24) *row = 24 - height;
    if (*col + width > VGA_WIDTH) *col = VGA_WIDTH - width;
}

static int* desktop_window_row_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_row;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_row;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_row;
    if (window == DESKTOP_WINDOW_FILES) return &files_row;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_row;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_row;
    return &terminal_row;
}

static int* desktop_window_col_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_col;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_col;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_col;
    if (window == DESKTOP_WINDOW_FILES) return &files_col;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_col;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_col;
    return &terminal_col;
}

static int* desktop_window_px_x_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_px_x;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_px_x;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_px_x;
    if (window == DESKTOP_WINDOW_FILES) return &files_px_x;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_px_x;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_px_x;
    return &terminal_px_x;
}

static int* desktop_window_px_y_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_px_y;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_px_y;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_px_y;
    if (window == DESKTOP_WINDOW_FILES) return &files_px_y;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_px_y;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_px_y;
    return &terminal_px_y;
}

static int* desktop_window_px_w_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_px_w;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_px_w;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_px_w;
    if (window == DESKTOP_WINDOW_FILES) return &files_px_w;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_px_w;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_px_w;
    return &terminal_px_w;
}

static int* desktop_window_px_h_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_px_h;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_px_h;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_px_h;
    if (window == DESKTOP_WINDOW_FILES) return &files_px_h;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_px_h;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_px_h;
    return &terminal_px_h;
}

static int* desktop_window_open_ptr(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return &taskman_open;
    if (window == DESKTOP_WINDOW_EDITOR) return &editor_open;
    if (window == DESKTOP_WINDOW_SETTINGS) return &settings_open;
    if (window == DESKTOP_WINDOW_FILES) return &files_open;
    if (window == DESKTOP_WINDOW_SYSTEM) return &system_open;
    if (window == DESKTOP_WINDOW_ABOUT) return &about_open;
    return &terminal_open;
}

static int desktop_window_width(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return 52;
    if (window == DESKTOP_WINDOW_EDITOR) return 48;
    if (window == DESKTOP_WINDOW_SETTINGS) return 46;
    if (window == DESKTOP_WINDOW_FILES) return 42;
    if (window == DESKTOP_WINDOW_SYSTEM) return 42;
    if (window == DESKTOP_WINDOW_ABOUT) return 38;
    return 58;
}

static int desktop_window_height(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return 15;
    if (window == DESKTOP_WINDOW_EDITOR) return 14;
    if (window == DESKTOP_WINDOW_SETTINGS) return 14;
    if (window == DESKTOP_WINDOW_FILES) return 12;
    if (window == DESKTOP_WINDOW_SYSTEM) return 11;
    if (window == DESKTOP_WINDOW_ABOUT) return 9;
    return 18;
}

static const char* desktop_window_title(int window) {
    if (window == DESKTOP_WINDOW_TASKMANAGER) return "Task Manager";
    if (window == DESKTOP_WINDOW_EDITOR) return "Text Editor";
    if (window == DESKTOP_WINDOW_SETTINGS) return "Settings";
    if (window == DESKTOP_WINDOW_FILES) return "Files";
    if (window == DESKTOP_WINDOW_SYSTEM) return "System";
    if (window == DESKTOP_WINDOW_ABOUT) return "About";
    return "Terminal";
}

static int desktop_window_is_open(int window) {
    return *desktop_window_open_ptr(window);
}

static void desktop_open_window(int window) {
    *desktop_window_open_ptr(window) = 1;
    desktop_active_window = window;
}

static void desktop_close_window(int window) {
    *desktop_window_open_ptr(window) = 0;
    if (desktop_active_window == window) {
        if (terminal_open) desktop_active_window = DESKTOP_WINDOW_TERMINAL;
        else if (files_open) desktop_active_window = DESKTOP_WINDOW_FILES;
        else if (system_open) desktop_active_window = DESKTOP_WINDOW_SYSTEM;
        else if (taskman_open) desktop_active_window = DESKTOP_WINDOW_TASKMANAGER;
        else if (about_open) desktop_active_window = DESKTOP_WINDOW_ABOUT;
        else if (editor_open) desktop_active_window = DESKTOP_WINDOW_EDITOR;
        else if (settings_open) desktop_active_window = DESKTOP_WINDOW_SETTINGS;
        else desktop_active_window = 0;
    }
}

static void desktop_clamp_window_id(int window) {
    if (framebuffer_available) {
        int* x = desktop_window_px_x_ptr(window);
        int* y = desktop_window_px_y_ptr(window);
        int* w = desktop_window_px_w_ptr(window);
        int* h = desktop_window_px_h_ptr(window);
        int min_w = window == DESKTOP_WINDOW_ABOUT ? 360 : 460;
        int min_h = window == DESKTOP_WINDOW_ABOUT ? 230 : 300;
        int top_limit = desktop_px_y(4);
        int bottom_limit = desktop_px_y(24) - 12;

        if (window == DESKTOP_WINDOW_TASKMANAGER) {
            min_w = 620;
            min_h = 360;
        }
        if (*w < min_w) *w = min_w;
        if (*h < min_h) *h = min_h;
        if (*w > (int)frame_buffer_width - 80) *w = (int)frame_buffer_width - 80;
        if (*h > bottom_limit - top_limit) *h = bottom_limit - top_limit;
        if (*x < 24) *x = 24;
        if (*y < top_limit) *y = top_limit;
        if (*x + *w > (int)frame_buffer_width - 24) *x = (int)frame_buffer_width - 24 - *w;
        if (*y + *h > bottom_limit) *y = bottom_limit - *h;
        return;
    }

    clamp_window(
        desktop_window_row_ptr(window),
        desktop_window_col_ptr(window),
        desktop_window_width(window),
        desktop_window_height(window)
    );
}

static void draw_window_contents(int window, int row, int col, unsigned char fill) {
    if (window == DESKTOP_WINDOW_TERMINAL) {
        vga_write_at(row + 2, col + 2, fill, "ArchwayOS command terminal");
        vga_write_at(row + 3, col + 2, fill, "Try: start, open files, move terminal right");
        vga_write_at(row + 5, col + 2, fill, "Root User:");
    } else if (window == DESKTOP_WINDOW_SETTINGS) {
        vga_write_at(row + 3, col + 3, fill, "Wallpaper:");
        vga_write_at(row + 4, col + 5, fill, "bg blue | green | cyan | red | gray | black");
        vga_write_at(row + 6, col + 3, fill, "Startup:");
        vga_write_at(row + 7, col + 5, fill, "bootmode desktop");
        vga_write_at(row + 8, col + 5, fill, "bootmode terminal");
        vga_write_at(row + 10, col + 3, fill, "Windows:");
        vga_write_at(row + 11, col + 5, fill, "open, focus, move, close any app");
    } else if (window == DESKTOP_WINDOW_FILES) {
        vga_write_at(row + 3, col + 3, fill, "FAT32 RAM disk");
        vga_write_at(row + 5, col + 3, fill, "Commands: ls, cat, touch, write");
        vga_write_at(row + 7, col + 3, fill, "Open terminal to run file commands.");
    } else if (window == DESKTOP_WINDOW_SYSTEM) {
        vga_write_at(row + 3, col + 3, fill, "System information");
        vga_write_at(row + 5, col + 3, fill, "Command: cpu");
        vga_write_at(row + 7, col + 3, fill, "Command: memmap");
    } else if (window == DESKTOP_WINDOW_TASKMANAGER) {
        int out_row = row + 7;
        vga_write_at(row + 3, col + 3, fill, "Open apps");
        vga_write_at(row + 5, col + 3, fill, "PID  APP           STATE");
        vga_write_at(row + 6, col + 3, fill, "1    Desktop       running");
        if (terminal_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "2    Terminal      open");
        if (files_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "3    Files         open");
        if (settings_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "4    Settings      open");
        if (system_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "5    System        open");
        if (about_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "6    About         open");
        if (editor_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "7    Editor        open");
        if (taskman_open && out_row < row + 14) vga_write_at(out_row++, col + 3, fill, "8    Task Manager  active");
    } else if (window == DESKTOP_WINDOW_ABOUT) {
        vga_write_at(row + 3, col + 3, fill, "ArchwayOS 0.2.0");
        vga_write_at(row + 5, col + 3, fill, "(c) 2026 ArchwayOS");
    }
}

static void draw_vbe_window_contents(int window, int row, int col, int width) {
    if (window == DESKTOP_WINDOW_TERMINAL) {
        int prompt_row = row + terminal_cell_height() - 3;
        int output_rows = prompt_row - (row + 3);
        int max_cells = (col + width - 3) - (col + 2);
        if (output_rows < 1) output_rows = 1;
        if (output_rows > TERMINAL_LOG_LINES) output_rows = TERMINAL_LOG_LINES;
        for (int i = 0; i < output_rows; i++) {
            int log_index = terminal_log_line - terminal_log_scroll - output_rows + 1 + i;
            while (log_index < 0) log_index += TERMINAL_LOG_LINES;
            log_index %= TERMINAL_LOG_LINES;
            fb_draw_text_at(col + 2, row + 3 + i, "                                                                          ", 0x00FFFFFF, 0x00101820);
            fb_terminal_text_at(col + 2, row + 3 + i, max_cells, terminal_log[log_index]);
        }
        fb_console_print_clipped_at(col + 2, prompt_row, max_cells, VGA_ATTR_WHITE, "Root User:");
        fb_console_print_clipped_at(col + 12, prompt_row, (col + width - 3) - (col + 12), VGA_ATTR_WHITE, terminal_input_text);
    } else if (window == DESKTOP_WINDOW_SETTINGS) {
        fb_window_heading_at(col + 3, row + 3, (col + width - 3) - (col + 3), "Settings");
        fb_window_text_at(col + 3, row + 5, 16, "Appearance");
        fb_window_text_at(col + 22, row + 5, 20, "Wallpaper color");
        fb_window_heading_at(col + 22, row + 6, 20, "bg blue/green/cyan/red");
        fb_window_text_at(col + 3, row + 8, 16, "Startup");
        fb_window_text_at(col + 22, row + 8, 20, prefer_desktop_boot ? "Desktop at boot" : "Terminal at boot");
        fb_window_text_at(col + 3, row + 10, 16, "Input");
        fb_window_text_at(col + 22, row + 10, 20, "Mouse speed");
        fb_window_heading_at(col + 35, row + 10, 4, "1..6");
        fb_window_text_at(col + 3, row + 12, 16, "Windows");
        fb_window_text_at(col + 22, row + 12, 20, "drag, resize, close");
    } else if (window == DESKTOP_WINDOW_FILES) {
        fb_window_text_at(col + 3, row + 3, (col + width - 3) - (col + 3), "FAT32 RAM disk");
        fb_window_text_at(col + 3, row + 5, (col + width - 3) - (col + 3), "Commands: ls, cat, touch, write");
        fb_window_heading_at(col + 3, row + 7, (col + width - 3) - (col + 3), "Open terminal to run file commands.");
    } else if (window == DESKTOP_WINDOW_SYSTEM) {
        char number[16];
        int logo_x = desktop_px_x(col + 3);
        int logo_y = desktop_px_y(row + 4);
        fb_fill_circle(logo_x + 52, logo_y + 54, 48, 0x000EA8E8);
        fb_fill_rect(logo_x + 27, logo_y + 42, 10, 48, 0x00C9F6FB);
        fb_fill_rect(logo_x + 68, logo_y + 42, 10, 48, 0x00C9F6FB);
        fb_fill_rect(logo_x + 35, logo_y + 78, 36, 10, 0x00C9F6FB);
        fb_window_heading_at(col + 10, row + 3, (col + width - 3) - (col + 10), "archwayOS_0.2.0/0.3.0");
        fb_window_text_at(col + 10, row + 5, (col + width - 3) - (col + 10), "Kernel: long mode x86_64");
        fb_window_text_at(col + 10, row + 6, (col + width - 3) - (col + 10), "Boot: GRUB Multiboot2");
        fb_window_text_at(col + 10, row + 7, (col + width - 3) - (col + 10), "Graphics: VBE framebuffer");
        u32_to_dec_text(frame_buffer_width, number);
        fb_window_text_at(col + 10, row + 8, 10, "Video:");
        fb_window_heading_at(col + 18, row + 8, 8, number);
        u32_to_dec_text(frame_buffer_height, number);
        fb_window_text_at(col + 26, row + 8, 1, "x");
        fb_window_heading_at(col + 27, row + 8, 8, number);
        u32_to_dec_text(frame_buffer_pitch, number);
        fb_window_text_at(col + 10, row + 9, 10, "Pitch:");
        fb_window_heading_at(col + 18, row + 9, 10, number);
        fb_window_text_at(col + 10, row + 10, (col + width - 3) - (col + 10), "Storage: FAT32 RAM VFS");
        fb_window_text_at(col + 10, row + 11, (col + width - 3) - (col + 10), "Apps: Terminal Files Editor Settings Tasks");
    } else if (window == DESKTOP_WINDOW_TASKMANAGER) {
        fb_window_heading_at(col + 3, row + 3, (col + width - 3) - (col + 3), "PID  APP           STATE");
        fb_window_text_at(col + 3, row + 5, (col + width - 3) - (col + 3), "1    Kernel        running");
        fb_window_text_at(col + 3, row + 6, (col + width - 3) - (col + 3), "2    Desktop       running");
        fb_window_text_at(col + 3, row + 7, (col + width - 3) - (col + 3), "3    Terminal      window");
        fb_window_text_at(col + 3, row + 8, (col + width - 3) - (col + 3), "4    Files         window");
        fb_window_text_at(col + 3, row + 9, (col + width - 3) - (col + 3), "5    Settings      window");
        fb_window_text_at(col + 3, row + 10, (col + width - 3) - (col + 3), "6    System        window");
        fb_window_text_at(col + 3, row + 11, (col + width - 3) - (col + 3), "7    Editor        window");
    } else if (window == DESKTOP_WINDOW_ABOUT) {
        fb_window_text_at(col + 3, row + 3, (col + width - 3) - (col + 3), "ArchwayOS 0.2.0");
        fb_window_heading_at(col + 3, row + 5, (col + width - 3) - (col + 3), "(c) 2026 ArchwayOS");
    } else if (window == DESKTOP_WINDOW_EDITOR) {
        fb_window_heading_at(col + 3, row + 3, (col + width - 3) - (col + 3), "File: NOTES.TXT");
        if (editor_scroll == 0) {
            fb_window_text_at(col + 3, row + 5, (col + width - 3) - (col + 3), "Archway text editor");
            fb_window_text_at(col + 3, row + 6, (col + width - 3) - (col + 3), "This is a GUI app window.");
            fb_window_text_at(col + 3, row + 7, (col + width - 3) - (col + 3), "Use write NOTES.TXT text");
            fb_window_text_at(col + 3, row + 8, (col + width - 3) - (col + 3), "from Terminal to save files.");
        } else if (editor_scroll == 1) {
            fb_window_text_at(col + 3, row + 5, (col + width - 3) - (col + 3), "Editor tools:");
            fb_window_text_at(col + 3, row + 6, (col + width - 3) - (col + 3), "- open editor");
            fb_window_text_at(col + 3, row + 7, (col + width - 3) - (col + 3), "- scroll up/down");
            fb_window_text_at(col + 3, row + 8, (col + width - 3) - (col + 3), "- close editor");
        } else {
            fb_window_text_at(col + 3, row + 5, (col + width - 3) - (col + 3), "Next step:");
            fb_window_text_at(col + 3, row + 6, (col + width - 3) - (col + 3), "wire keyboard text");
            fb_window_text_at(col + 3, row + 7, (col + width - 3) - (col + 3), "directly into editor");
            fb_window_text_at(col + 3, row + 8, (col + width - 3) - (col + 3), "instead of commands.");
        }
        fb_window_heading_at(col + 3, row + 11, (col + width - 3) - (col + 3), "scroll up/down changes this view");
    }
}


static const char* desktop_task_state(int window) {
    if (desktop_active_window == window && desktop_window_is_open(window)) return "active";
    if (desktop_window_is_open(window)) return "open";
    return "closed";
}

static void draw_vbe_task_row(int row, int col, int width, int index, const char* pid, const char* app, const char* state) {
    uint32_t bg = (index & 1) ? 0x00F2F7FA : 0x00FFFFFF;
    uint32_t state_color = state[0] == 'a' ? 0x000C8DCE : 0x00102030;
    int x = desktop_px_x(col + 2);
    int y = desktop_px_y(row);
    int w = desktop_px_x(col + width - 2) - x;
    int h = desktop_px_y(row + 1) - y;

    fb_fill_rect(x, y, w, h, bg);
    fb_draw_text_at(col + 3, row, pid, 0x00102030, 0xFFFFFFFF);
    fb_draw_text_at(col + 9, row, app, 0x00102030, 0xFFFFFFFF);
    fb_draw_text_at(col + 25, row, state, state_color, 0xFFFFFFFF);
}

static int draw_vbe_task_row_if_open(int window, int row, int col, int width, int index, const char* pid, const char* app) {
    if (!desktop_window_is_open(window)) return index;
    draw_vbe_task_row(row + index, col, width, index, pid, app, desktop_task_state(window));
    return index + 1;
}

static void draw_vbe_task_manager(int row, int col, int width, int height) {
    char size_text[16];
    int max_row = row + height - 2;
    int list_row = row + 8;
    int shown = 0;
    int x = desktop_px_x(col + 1);
    int y = desktop_px_y(row + 3);
    int w = desktop_px_x(col + width - 1) - x;
    int h = desktop_px_y(row + height - 1) - y;
    int stat_x = desktop_px_x(col + 2);
    int stat_y = desktop_px_y(row + 3);
    int stat_w = desktop_px_x(col + width - 2) - stat_x;
    int stat_h = desktop_px_y(3) - desktop_px_y(0);

    fb_fill_rect(x, y, w, h, 0x00FFFFFF);
    fb_draw_rect(x, y, w, h, 0x0090A4B8);
    fb_fill_round_rect(stat_x, stat_y, stat_w, stat_h, 8, 0x00E7F5FA);
    fb_draw_text_at(col + 3, row + 3, "System overview", 0x000C8DCE, 0xFFFFFFFF);
    fb_draw_text_at(col + 3, row + 4, "Video:", 0x00102030, 0xFFFFFFFF);
    u32_to_dec_text(frame_buffer_width, size_text);
    fb_draw_text_at(col + 12, row + 4, size_text, 0x00102030, 0xFFFFFFFF);
    fb_draw_text_at(col + 18, row + 4, "x", 0x00102030, 0xFFFFFFFF);
    u32_to_dec_text(frame_buffer_height, size_text);
    fb_draw_text_at(col + 19, row + 4, size_text, 0x00102030, 0xFFFFFFFF);
    fb_draw_text_at(col + 28, row + 4, fb_backbuffer_enabled ? "double buffer" : "direct draw", 0x00102030, 0xFFFFFFFF);
    fb_draw_text_at(col + 3, row + 6, "PID", 0x000C8DCE, 0xFFFFFFFF);
    fb_draw_text_at(col + 9, row + 6, "APP", 0x000C8DCE, 0xFFFFFFFF);
    fb_draw_text_at(col + 25, row + 6, "STATE", 0x000C8DCE, 0xFFFFFFFF);
    if (list_row + shown <= max_row) {
        draw_vbe_task_row(list_row + shown, col, width, shown, "1", "Desktop", "running");
        shown++;
    }
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_TERMINAL, list_row, col, width, shown, "2", "Terminal");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_FILES, list_row, col, width, shown, "3", "Files");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_SETTINGS, list_row, col, width, shown, "4", "Settings");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_SYSTEM, list_row, col, width, shown, "5", "System");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_ABOUT, list_row, col, width, shown, "6", "About");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_EDITOR, list_row, col, width, shown, "7", "Editor");
    if (list_row + shown <= max_row) shown = draw_vbe_task_row_if_open(DESKTOP_WINDOW_TASKMANAGER, list_row, col, width, shown, "8", "Task Manager");
}

static void draw_vbe_files_manager(int row, int col, int width, int height) {
    FsDesktopEntry entries[24];
    int count = fs_desktop_list(entries, 24);
    int list_rows = height - 8;
    int x = desktop_px_x(col + 1);
    int y = desktop_px_y(row + 3);
    int w = desktop_px_x(col + width - 1) - x;
    int h = desktop_px_y(row + height - 1) - y;

    if (files_scroll < 0) files_scroll = 0;
    if (files_scroll > count - list_rows) files_scroll = count > list_rows ? count - list_rows : 0;

    fb_fill_rect(x, y, w, h, 0x00FFFFFF);
    fb_fill_rect(x, y, desktop_px_x(col + 12) - x, h, 0x00E5EDF6);
    fb_draw_rect(x, y, w, h, 0x0090A4B8);
    fb_fill_round_rect(desktop_px_x(col + 13), desktop_px_y(row + 4), desktop_px_x(col + 25) - desktop_px_x(col + 13), desktop_px_y(row + 5) - desktop_px_y(row + 4), 6, 0x002FB3E8);
    fb_window_heading_at(col + 2, row + 3, 9, "Places");
    fb_window_text_at(col + 2, row + 5, 9, "Home");
    fb_window_text_at(col + 2, row + 6, 9, "Files");
    fb_console_print_clipped_at(col + 14, row + 4, 10, VGA_ATTR_WHITE, "+ Text");
    fb_window_heading_at(col + 14, row + 6, 15, "Name");
    fb_window_heading_at(col + 31, row + 6, 8, "Size");

    if (count == 0) {
        fb_window_text_at(col + 14, row + 8, width - 17, "(empty folder)");
    }
    for (int i = 0; i < list_rows && i + files_scroll < count; i++) {
        int entry = i + files_scroll;
        char size_text[16];
        int draw_row = row + 8 + i;
        uint32_t row_color = (i & 1) ? 0x00F7F7F7 : 0x00FFFFFF;
        fb_fill_rect(desktop_px_x(col + 13), desktop_px_y(draw_row), desktop_px_x(col + width - 3) - desktop_px_x(col + 13), desktop_px_y(draw_row + 1) - desktop_px_y(draw_row), row_color);
        if (entries[entry].is_dir) fb_window_heading_at(col + 14, draw_row, 5, "[DIR]");
        else fb_window_text_at(col + 14, draw_row, 5, "[TXT]");
        fb_window_text_at(col + 20, draw_row, 10, entries[entry].name);
        u32_to_dec_text(entries[entry].size, size_text);
        fb_window_text_at(col + 31, draw_row, 8, entries[entry].is_dir ? "-" : size_text);
    }
    fb_window_heading_at(col + width - 11, row + height - 2, 8, "scroll");
}

static void draw_vbe_app_window(int window) {
    int x;
    int y;
    int pixel_width;
    int pixel_height;
    int title_height = 48;
    int row;
    int col;
    int width;
    int height;
    uint32_t title = desktop_active_window == window ? 0x001B6F98 : 0x006C7782;
    uint32_t fill = window == DESKTOP_WINDOW_TERMINAL ? 0x00101820 : 0x00F7FAFC;
    uint32_t border = desktop_active_window == window ? 0x002DB8F0 : 0x008AA7B8;

    if (!desktop_window_is_open(window)) return;
    desktop_clamp_window_id(window);
    x = *desktop_window_px_x_ptr(window);
    y = *desktop_window_px_y_ptr(window);
    pixel_width = *desktop_window_px_w_ptr(window);
    pixel_height = *desktop_window_px_h_ptr(window);
    row = (y * VGA_HEIGHT) / (int)frame_buffer_height;
    col = (x * VGA_WIDTH) / (int)frame_buffer_width;
    width = (pixel_width * VGA_WIDTH) / (int)frame_buffer_width;
    height = (pixel_height * VGA_HEIGHT) / (int)frame_buffer_height;
    if (width < 20) width = 20;
    if (height < 8) height = 8;

    fb_fill_round_rect(x + 10, y + 12, pixel_width, pixel_height, 10, 0x00365F73);
    fb_fill_round_rect(x, y, pixel_width, pixel_height, 10, fill);
    fb_draw_rect(x, y, pixel_width, pixel_height, border);
    fb_fill_round_rect(x + 2, y + 2, pixel_width - 4, title_height, 8, title);
    fb_fill_rect(x + 2, y + title_height - 8, pixel_width - 4, 10, title);
    fb_draw_text_at(col + 2, row + 1, desktop_window_title(window), 0x00FFFFFF, 0xFFFFFFFF);
    fb_draw_close_button(x + pixel_width - 42, y + 10, 28);

    if (window != DESKTOP_WINDOW_ABOUT) {
        fb_draw_rect(x + pixel_width - 34, y + pixel_height - 34, 24, 24, 0x0098ADB9);
        fb_fill_rect(x + pixel_width - 26, y + pixel_height - 14, 14, 3, 0x0098ADB9);
        fb_fill_rect(x + pixel_width - 18, y + pixel_height - 22, 6, 3, 0x0098ADB9);
    }

    if (window == DESKTOP_WINDOW_TERMINAL) {
        fb_fill_round_rect(x + 18, y + title_height + 16, pixel_width - 36, pixel_height - title_height - 34, 6, 0x00101820);
    } else {
        fb_fill_round_rect(x + 18, y + title_height + 16, pixel_width - 36, pixel_height - title_height - 34, 6, 0x00FFFFFF);
    }

    if (window == DESKTOP_WINDOW_FILES) draw_vbe_files_manager(row, col, width, height);
    else if (window == DESKTOP_WINDOW_TASKMANAGER) draw_vbe_task_manager(row, col, width, height);
    else draw_vbe_window_contents(window, row, col, width);
}

static void draw_app_window(int window) {
    int row;
    int col;
    int width;
    int height;
    unsigned char fill = window == DESKTOP_WINDOW_SETTINGS ? VGA_ATTR_MENU : VGA_ATTR_WINDOW;
    unsigned char title = desktop_active_window == window ? VGA_ATTR_START : VGA_ATTR_WINDOW_TITLE;

    if (framebuffer_available) {
        draw_vbe_app_window(window);
        return;
    }

    if (!desktop_window_is_open(window)) return;
    desktop_clamp_window_id(window);
    row = *desktop_window_row_ptr(window);
    col = *desktop_window_col_ptr(window);
    width = desktop_window_width(window);
    height = desktop_window_height(window);

    vga_draw_box(row, col, width, height, VGA_ATTR_BORDER, fill);
    for (int x = col + 1; x < col + width - 1; x++) vga_put_at(row + 1, x, title, ' ');
    vga_write_at(row + 1, col + 2, title, desktop_window_title(window));
    vga_write_at(row + 1, col + width - 7, title, "[_][X]");
    draw_window_contents(window, row, col, fill);
}

static void draw_desktop_windows(void) {
    int windows[] = {
        DESKTOP_WINDOW_TERMINAL,
        DESKTOP_WINDOW_FILES,
        DESKTOP_WINDOW_SYSTEM,
        DESKTOP_WINDOW_ABOUT,
        DESKTOP_WINDOW_EDITOR,
        DESKTOP_WINDOW_TASKMANAGER,
        DESKTOP_WINDOW_SETTINGS
    };

    for (int i = 0; i < 7; i++) {
        if (windows[i] != desktop_active_window) draw_app_window(windows[i]);
    }
    if (desktop_active_window) draw_app_window(desktop_active_window);
}

static void draw_terminal_only_screen(void) {
    set_framebuffer_console_enabled(1);
    clear_screen();
    serial_write("ArchwayOS terminal mode\n");
    if (framebuffer_available) {
        set_color(VGA_ATTR_LIGHT_BLUE);
        kprint("ArchwayOS full terminal\n");
        set_color(VGA_ATTR_WHITE);
        kprint("Desktop is hidden. Type 'desktop' to return.\n");
        kprint("Root User:");
        return;
    }
    vga_draw_box(1, 2, 76, 22, VGA_ATTR_BORDER, VGA_ATTR_WINDOW);
    for (int col = 3; col < 77; col++) vga_put_at(2, col, VGA_ATTR_WINDOW_TITLE, ' ');
    vga_write_at(2, 4, VGA_ATTR_WINDOW_TITLE, "ArchwayOS Terminal");
    vga_write_at(4, 4, VGA_ATTR_WINDOW, "Plain terminal mode. Type 'desktop' for the desktop shell.");
    vga_write_at(6, 4, VGA_ATTR_WINDOW, "Root User:");
    set_color(VGA_ATTR_WHITE);
    set_cursor(14, 6);
}

static void print_archway_logo(void) {
    unsigned char old_color = current_fg;

    set_color(VGA_ATTR_LIGHT_BLUE);
    for (int i = 0; i < archway_logo_line_count; i++) {
        kprint(archway_logo[i]);
        kprint("\n");
    }
    set_color(old_color);
}

static void draw_text_boot_screen(void) {
    set_framebuffer_console_enabled(1);
    clear_screen();
    print_archway_logo();
    set_color(VGA_ATTR_WHITE);
    kprint("ArchwayOS 0.2.0 (c) 2026 ArchwayOS\n");
    kprint("Open source preview build\n");
    kprint("Type 'help' for a list of commands.\n");
    kprint("Root User:");
}

static void draw_desktop_start_screen(void) {
    if (framebuffer_available) {
        draw_vbe_start_screen();
        return;
    }

    clear_screen();
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            unsigned char color = (row < 12) ? 0x17 : 0x10;
            char ch = ' ';
            if (((row * 5 + col * 3) % 37) == 0) ch = '.';
            vga_put_at(row, col, color, ch);
        }
    }

    vga_draw_box(4, 18, 44, 15, VGA_ATTR_BORDER, 0x10);
    vga_write_at(6, 31, VGA_ATTR_LIGHT_BLUE, "ARCHWAYOS");
    vga_write_at(8, 26, VGA_ATTR_WHITE, "Version 0.2.0 Desktop");
    vga_write_at(10, 23, VGA_ATTR_LIGHT_BLUE, "Open source preview build");
    vga_write_at(13, 25, VGA_ATTR_WHITE, "Press any key to start");
    vga_write_at(16, 24, VGA_ATTR_LIGHT_BLUE, "Text mode: make run text");
    set_color(VGA_ATTR_WHITE);
    set_cursor(0, 24);
}

static void wait_for_keyboard_press(void) {
    while (1) {
        unsigned char status = inb(0x64);
        if ((status & 0x01) == 0) continue;

        unsigned char data = inb(0x60);
        if (status & 0x20) {
            if (data & 0x01) return;
            continue;
        }
        if (data == 0xE0) continue;
        if (data < 128) return;
    }
}

static void desktop_lock_screen(void) {
    start_menu_open = 0;
    fb_restore_pixel_cursor();
    draw_desktop_start_screen();
    wait_for_keyboard_press();
    draw_desktop_screen();
}

static void desktop_power_off(void) {
    kprint("Powering off...\n");
    __asm__ volatile("cli");
    while (1) __asm__ volatile("hlt");
}

static int desktop_activate_start_menu_row(int row) {
    if (row == 12 || row == 13 || row == 14 || row == 15 || row == 16 || row == 17 || row == 18 || row == 20 || row == 21) {
        start_menu_open = 0;
    }

    if (row == 12) {
        desktop_open_window(DESKTOP_WINDOW_TERMINAL);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 13) {
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 14) {
        desktop_open_window(DESKTOP_WINDOW_SYSTEM);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 15) {
        desktop_open_window(DESKTOP_WINDOW_ABOUT);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 16) {
        desktop_open_window(DESKTOP_WINDOW_FILES);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 17 && !framebuffer_available) {
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 17 || row == 18) {
        desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
        desktop_redraw_with_prompt();
        return 1;
    }
    if (row == 20) {
        desktop_lock_screen();
        return 1;
    }
    if (row == 21) {
        desktop_power_off();
        return 1;
    }
    return 0;
}

static void draw_desktop_screen(void) {
    if (framebuffer_available) {
        set_framebuffer_console_enabled(0);
        draw_vbe_desktop_screen();
        if (!desktop_serial_banner_sent) {
            serial_write("ArchwayOS 0.2.0 (c) 2026 ArchwayOS\n");
            serial_write("Open source preview build\n");
            serial_write("Type 'help' for a list of commands.\n");
            desktop_serial_banner_sent = 1;
        }
        return;
    }

    clear_screen();
    set_framebuffer_console_enabled(1);
    draw_wallpaper();
    draw_topbar();
    if (!desktop_serial_banner_sent) {
        serial_write("ArchwayOS 0.2.0 (c) 2026 ArchwayOS\n");
        serial_write("Open source preview build\n");
        serial_write("Type 'help' for a list of commands.\n");
        desktop_serial_banner_sent = 1;
    }

    draw_desktop_icon(2, 3, "T>", "Terminal");
    draw_desktop_icon(7, 3, "FS", "Files");
    draw_desktop_icon(12, 3, "i ", "System");
    draw_desktop_icon(17, 3, "? ", "About");
    draw_desktop_icon(2, 14, "S ", "Settings");
    draw_desktop_icon(7, 14, "ED", "Editor");
    draw_desktop_icon(12, 14, "TM", "Tasks");
    draw_desktop_windows();
    draw_taskbar();
    draw_desktop_cursor();
    set_color(VGA_ATTR_WHITE);
    if (terminal_open) set_cursor(terminal_col + 12, terminal_row + 5);
    else set_cursor(0, 24);
}




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

static int parse_small_int(const char* s, int* out) {
    int value = 0;
    if (!s || !*s) return 0;
    while (*s == ' ') s++;
    if (!*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        value = value * 10 + (*s - '0');
        s++;
    }
    *out = value;
    return 1;
}

static int desktop_color_from_name(const char* name, unsigned char* out) {
    if (string_equals(name, "blue")) {
        *out = 0x17;
        return 1;
    }
    if (string_equals(name, "green")) {
        *out = 0x27;
        return 1;
    }
    if (string_equals(name, "cyan")) {
        *out = 0x37;
        return 1;
    }
    if (string_equals(name, "red")) {
        *out = 0x47;
        return 1;
    }
    if (string_equals(name, "gray") || string_equals(name, "grey")) {
        *out = 0x87;
        return 1;
    }
    if (string_equals(name, "black")) {
        *out = 0x07;
        return 1;
    }
    return 0;
}

static void desktop_redraw_with_prompt(void) {
    draw_desktop_screen();
}

static void draw_vbe_command_input(const char* input) {
    if (!framebuffer_available || !terminal_open) return;
    int row = terminal_prompt_row();
    int col = terminal_prompt_col();
    int max_cells = terminal_cell_width() - 15;

    terminal_input_text = input;
    if (max_cells < 1) max_cells = 1;
    if (max_cells > 58) max_cells = 58;
    fb_restore_pixel_cursor();
    fb_draw_text_at(col, row, "                                                          ", 0x00FFFFFF, 0x00101820);
    fb_console_print_clipped_at(col, row, max_cells, VGA_ATTR_WHITE, input);
    fb_draw_pixel_cursor();
}

static void desktop_move_window(int window, const char* direction) {
    int* row = desktop_window_row_ptr(window);
    int* col = desktop_window_col_ptr(window);

    if (!window) {
        kprint("Usage: move [app] left|right|up|down\n");
        return;
    }

    if (string_equals(direction, "left")) (*col) -= 2;
    else if (string_equals(direction, "right")) (*col) += 2;
    else if (string_equals(direction, "up")) (*row)--;
    else if (string_equals(direction, "down")) (*row)++;
    else {
        kprint("Usage: move [app] left|right|up|down\n");
        return;
    }

    desktop_clamp_window_id(window);
    desktop_redraw_with_prompt();
}

static void desktop_resize_window(int window, const char* direction) {
    if (!window) {
        kprint("Usage: resize [app] wider|narrower|taller|shorter\n");
        return;
    }

    if (framebuffer_available) {
        int* w = desktop_window_px_w_ptr(window);
        int* h = desktop_window_px_h_ptr(window);
        if (string_equals(direction, "wider")) (*w) += 80;
        else if (string_equals(direction, "narrower")) (*w) -= 80;
        else if (string_equals(direction, "taller")) (*h) += 60;
        else if (string_equals(direction, "shorter")) (*h) -= 60;
        else {
            kprint("Usage: resize [app] wider|narrower|taller|shorter\n");
            return;
        }
        desktop_clamp_window_id(window);
        desktop_redraw_with_prompt();
        return;
    }

    kprint("resize: graphics mode only\n");
}

static void desktop_scroll_active(const char* direction) {
    int delta = 0;
    if (string_equals(direction, "up")) delta = -1;
    else if (string_equals(direction, "down")) delta = 1;
    else {
        kprint("Usage: scroll up|down\n");
        return;
    }

    if (desktop_active_window == DESKTOP_WINDOW_TERMINAL) {
        terminal_log_scroll -= delta;
        if (terminal_log_scroll < 0) terminal_log_scroll = 0;
        if (terminal_log_scroll > TERMINAL_LOG_LINES - 1) terminal_log_scroll = TERMINAL_LOG_LINES - 1;
    } else if (desktop_active_window == DESKTOP_WINDOW_FILES) {
        files_scroll += delta;
        if (files_scroll < 0) files_scroll = 0;
    } else if (desktop_active_window == DESKTOP_WINDOW_EDITOR) {
        editor_scroll += delta;
        if (editor_scroll < 0) editor_scroll = 0;
        if (editor_scroll > 3) editor_scroll = 3;
    } else {
        kprint("scroll works in terminal/files/editor windows\n");
        return;
    }
    desktop_redraw_with_prompt();
}

static int desktop_window_from_name(const char* name, int* window) {
    if (string_equals(name, "terminal")) {
        *window = DESKTOP_WINDOW_TERMINAL;
        return 1;
    }
    if (string_equals(name, "settings")) {
        *window = DESKTOP_WINDOW_SETTINGS;
        return 1;
    }
    if (string_equals(name, "files")) {
        *window = DESKTOP_WINDOW_FILES;
        return 1;
    }
    if (string_equals(name, "system")) {
        *window = DESKTOP_WINDOW_SYSTEM;
        return 1;
    }
    if (string_equals(name, "about")) {
        *window = DESKTOP_WINDOW_ABOUT;
        return 1;
    }
    if (string_equals(name, "editor") || string_equals(name, "edit")) {
        *window = DESKTOP_WINDOW_EDITOR;
        return 1;
    }
    if (string_equals(name, "taskman") || string_equals(name, "taskmgr") ||
        string_equals(name, "tasks") || string_equals(name, "taskmanager")) {
        *window = DESKTOP_WINDOW_TASKMANAGER;
        return 1;
    }
    return 0;
}

static int point_in_rect(int row, int col, int top, int left, int width, int height) {
    return row >= top && row < top + height && col >= left && col < left + width;
}

static int point_in_px_rect(int x, int y, int top, int left, int width, int height) {
    return y >= top && y < top + height && x >= left && x < left + width;
}

static int desktop_window_at_cursor(void) {
    int windows[] = {
        DESKTOP_WINDOW_SETTINGS,
        DESKTOP_WINDOW_TASKMANAGER,
        DESKTOP_WINDOW_EDITOR,
        DESKTOP_WINDOW_ABOUT,
        DESKTOP_WINDOW_SYSTEM,
        DESKTOP_WINDOW_FILES,
        DESKTOP_WINDOW_TERMINAL
    };

    if (desktop_active_window && desktop_window_is_open(desktop_active_window)) {
        int row = *desktop_window_row_ptr(desktop_active_window);
        int col = *desktop_window_col_ptr(desktop_active_window);
        if (point_in_rect(desktop_cursor_row, desktop_cursor_col, row, col,
                desktop_window_width(desktop_active_window),
                desktop_window_height(desktop_active_window))) {
            return desktop_active_window;
        }
    }

    for (int i = 0; i < 7; i++) {
        int window = windows[i];
        int row;
        int col;
        if (!desktop_window_is_open(window)) continue;
        row = *desktop_window_row_ptr(window);
        col = *desktop_window_col_ptr(window);
        if (point_in_rect(desktop_cursor_row, desktop_cursor_col, row, col,
                desktop_window_width(window), desktop_window_height(window))) {
            return window;
        }
    }
    return 0;
}

static int desktop_close_button_window_at_cursor(void) {
    int window = desktop_window_at_cursor();
    int row;
    int col;
    int width;
    if (!window) return 0;
    row = *desktop_window_row_ptr(window);
    col = *desktop_window_col_ptr(window);
    width = desktop_window_width(window);
    if (point_in_rect(desktop_cursor_row, desktop_cursor_col, row + 1, col + width - 4, 3, 1)) return window;
    return 0;
}

static int desktop_vbe_window_at_pixel(void) {
    int windows[] = {
        DESKTOP_WINDOW_SETTINGS,
        DESKTOP_WINDOW_TASKMANAGER,
        DESKTOP_WINDOW_EDITOR,
        DESKTOP_WINDOW_ABOUT,
        DESKTOP_WINDOW_SYSTEM,
        DESKTOP_WINDOW_FILES,
        DESKTOP_WINDOW_TERMINAL
    };

    if (desktop_active_window && desktop_window_is_open(desktop_active_window)) {
        int x = *desktop_window_px_x_ptr(desktop_active_window);
        int y = *desktop_window_px_y_ptr(desktop_active_window);
        int w = *desktop_window_px_w_ptr(desktop_active_window);
        int h = *desktop_window_px_h_ptr(desktop_active_window);
        if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y, x, w, h)) return desktop_active_window;
    }

    for (int i = 0; i < 7; i++) {
        int window = windows[i];
        int x;
        int y;
        int w;
        int h;
        if (!desktop_window_is_open(window)) continue;
        x = *desktop_window_px_x_ptr(window);
        y = *desktop_window_px_y_ptr(window);
        w = *desktop_window_px_w_ptr(window);
        h = *desktop_window_px_h_ptr(window);
        if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y, x, w, h)) return window;
    }
    return 0;
}

static int desktop_vbe_close_button_at_pixel(void) {
    int windows[] = {
        DESKTOP_WINDOW_SETTINGS,
        DESKTOP_WINDOW_TASKMANAGER,
        DESKTOP_WINDOW_EDITOR,
        DESKTOP_WINDOW_ABOUT,
        DESKTOP_WINDOW_SYSTEM,
        DESKTOP_WINDOW_FILES,
        DESKTOP_WINDOW_TERMINAL
    };

    if (desktop_active_window && desktop_window_is_open(desktop_active_window)) {
        int x = *desktop_window_px_x_ptr(desktop_active_window);
        int y = *desktop_window_px_y_ptr(desktop_active_window);
        int w = *desktop_window_px_w_ptr(desktop_active_window);
        if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y + 4, x + w - 58, 56, 50)) return desktop_active_window;
    }

    for (int i = 0; i < 7; i++) {
        int window = windows[i];
        int x;
        int y;
        int w;
        if (!desktop_window_is_open(window) || window == desktop_active_window) continue;
        x = *desktop_window_px_x_ptr(window);
        y = *desktop_window_px_y_ptr(window);
        w = *desktop_window_px_w_ptr(window);
        if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y + 4, x + w - 58, 56, 50)) return window;
    }
    return 0;
}

static int desktop_vbe_resize_window_at_pixel(void) {
    int window = desktop_vbe_window_at_pixel();
    int x;
    int y;
    int w;
    int h;
    if (!window) return 0;
    x = *desktop_window_px_x_ptr(window);
    y = *desktop_window_px_y_ptr(window);
    w = *desktop_window_px_w_ptr(window);
    h = *desktop_window_px_h_ptr(window);
    if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y + h - 42, x + w - 42, 42, 42)) return window;
    return 0;
}

static int desktop_vbe_titlebar_window_at_pixel(void) {
    int window = desktop_vbe_window_at_pixel();
    int x;
    int y;
    int w;
    if (!window) return 0;
    x = *desktop_window_px_x_ptr(window);
    y = *desktop_window_px_y_ptr(window);
    w = *desktop_window_px_w_ptr(window);
    if (point_in_px_rect(fb_cursor_x, fb_cursor_y, y, x, w, 52)) return window;
    return 0;
}

static int desktop_vbe_files_new_text_hit(void) {
    int x;
    int y;
    int row;
    int col;
    if (!files_open) return 0;
    row = (files_px_y * VGA_HEIGHT) / (int)frame_buffer_height;
    col = (files_px_x * VGA_WIDTH) / (int)frame_buffer_width;
    x = desktop_px_x(col + 13);
    y = desktop_px_y(row + 4);
    return point_in_px_rect(fb_cursor_x, fb_cursor_y, y, x, desktop_px_x(col + 25) - x, desktop_px_y(row + 5) - y);
}

static void desktop_move_cursor(const char* direction) {
    if (string_equals(direction, "left")) desktop_cursor_col -= 2;
    else if (string_equals(direction, "right")) desktop_cursor_col += 2;
    else if (string_equals(direction, "up")) desktop_cursor_row--;
    else if (string_equals(direction, "down")) desktop_cursor_row++;
    else {
        kprint("Usage: cursor left|right|up|down|show\n");
        return;
    }

    clamp_desktop_cursor();
    if (framebuffer_available) {
        fb_cursor_x = desktop_px_x(desktop_cursor_col);
        fb_cursor_y = desktop_px_y(desktop_cursor_row);
    }
    desktop_redraw_with_prompt();
}

static int vbe_icon_hit(int row, int col) {
    int x = desktop_px_x(col - 1);
    int y = desktop_px_y(row - 1);
    int w = desktop_px_x(col + 10) - x;
    int h = desktop_px_y(row + 5) - y;
    return point_in_px_rect(fb_cursor_x, fb_cursor_y, y, x, w, h);
}

static void desktop_vbe_click_cursor(void) {
    int window = desktop_vbe_close_button_at_pixel();
    int top_w = desktop_px_x(34);
    int top_h = desktop_px_y(2) - desktop_px_y(0);
    int top_x = ((int)frame_buffer_width - top_w) / 2;
    int top_y = desktop_px_y(1);

    if (context_menu_open) {
        if (point_in_px_rect(fb_cursor_x, fb_cursor_y, context_menu_y, context_menu_x, desktop_px_x(18), desktop_px_y(9))) {
            int row = ((fb_cursor_y - context_menu_y) * VGA_HEIGHT) / (int)frame_buffer_height;
            context_menu_open = 0;
            if (row <= 1) {
                desktop_open_window(DESKTOP_WINDOW_TERMINAL);
            } else if (row == 2) {
                desktop_open_window(DESKTOP_WINDOW_FILES);
            } else if (row == 3) {
                desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
            } else if (row == 4) {
                desktop_redraw_with_prompt();
                return;
            } else if (row == 5) {
                desktop_lock_screen();
                return;
            }
            desktop_redraw_with_prompt();
            return;
        }
        context_menu_open = 0;
        desktop_redraw_with_prompt();
        return;
    }

    if (window) {
        desktop_close_window(window);
        desktop_redraw_with_prompt();
        return;
    }

    if (point_in_px_rect(fb_cursor_x, fb_cursor_y, top_y, top_x + top_w - desktop_px_x(6), desktop_px_x(3), top_h)) {
        desktop_lock_screen();
        return;
    }

    if (point_in_px_rect(fb_cursor_x, fb_cursor_y, top_y, top_x + top_w - desktop_px_x(3), desktop_px_x(3), top_h)) {
        desktop_power_off();
        return;
    }

    if (point_in_px_rect(fb_cursor_x, fb_cursor_y, desktop_px_y(24), desktop_px_x(2), desktop_px_x(8) - desktop_px_x(2), desktop_px_y(25) - desktop_px_y(24))) {
        start_menu_open = !start_menu_open;
        desktop_redraw_with_prompt();
        return;
    }

    if (start_menu_open &&
        point_in_px_rect(fb_cursor_x, fb_cursor_y, desktop_px_y(9), desktop_px_x(3), desktop_px_x(22) - desktop_px_x(3), desktop_px_y(23) - desktop_px_y(9))) {
        int row = (fb_cursor_y * VGA_HEIGHT) / (int)frame_buffer_height;
        if (desktop_activate_start_menu_row(row)) return;
    }

    if (vbe_icon_hit(3, 14)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_TERMINAL);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(3, 4) || point_in_px_rect(fb_cursor_x, fb_cursor_y, desktop_px_y(22), desktop_px_x(25), desktop_px_x(39) - desktop_px_x(25), desktop_px_y(24) - desktop_px_y(22))) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_FILES);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(13, 4)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_SYSTEM);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(18, 4)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_ABOUT);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(8, 4) || point_in_px_rect(fb_cursor_x, fb_cursor_y, desktop_px_y(22), desktop_px_x(10), desktop_px_x(24) - desktop_px_x(10), desktop_px_y(24) - desktop_px_y(22))) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(8, 14)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_EDITOR);
        desktop_redraw_with_prompt();
        return;
    }
    if (vbe_icon_hit(13, 14) || point_in_px_rect(fb_cursor_x, fb_cursor_y, desktop_px_y(22), desktop_px_x(40), desktop_px_x(54) - desktop_px_x(40), desktop_px_y(24) - desktop_px_y(22))) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
        desktop_redraw_with_prompt();
        return;
    }

    window = desktop_vbe_window_at_pixel();
    if (window) {
        if (window == DESKTOP_WINDOW_FILES && desktop_vbe_files_new_text_hit()) {
            fs_desktop_create_text("NEW.TXT", "Created from Archway Files.");
            desktop_open_window(DESKTOP_WINDOW_EDITOR);
            desktop_redraw_with_prompt();
            return;
        }
        start_menu_open = 0;
        desktop_active_window = window;
        desktop_redraw_with_prompt();
        return;
    }

    start_menu_open = 0;
    desktop_redraw_with_prompt();
}

static void desktop_vbe_right_click_cursor(void) {
    fb_restore_pixel_cursor();
    context_menu_open = 1;
    start_menu_open = 0;
    context_menu_x = fb_cursor_x;
    context_menu_y = fb_cursor_y;
    desktop_redraw_with_prompt();
}

static void desktop_click_cursor(void) {
    if (framebuffer_available) {
        desktop_vbe_click_cursor();
        return;
    }

    if (framebuffer_available) sync_fb_cursor_to_desktop_cursor();
    int row = desktop_cursor_row;
    int col = desktop_cursor_col;
    int window;

    window = desktop_close_button_window_at_cursor();
    if (window) {
        desktop_close_window(window);
        desktop_redraw_with_prompt();
        return;
    }

    if (point_in_rect(row, col, 0, 58, 8, 1)) {
        desktop_lock_screen();
        return;
    }

    if (point_in_rect(row, col, 0, 69, 9, 1)) {
        desktop_power_off();
        return;
    }

    if (point_in_rect(row, col, 24, 1, 7, 1)) {
        start_menu_open = !start_menu_open;
        if (framebuffer_available) desktop_redraw_with_prompt();
        else {
            draw_start_menu();
            draw_desktop_cursor();
        }
        return;
    }

    if (start_menu_open && point_in_rect(row, col, 11, 1, 25, 13)) {
        if (desktop_activate_start_menu_row(row)) return;
    }
    if (point_in_rect(row, col, 24, 10, 12, 1) || point_in_rect(row, col, 2, 3, 10, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_TERMINAL);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 24, 23, 9, 1) || point_in_rect(row, col, 7, 3, 8, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_FILES);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 24, 34, 10, 1) || point_in_rect(row, col, 12, 3, 9, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_SYSTEM);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 24, 47, 12, 1)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 24, 60, 9, 1)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 17, 3, 8, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_ABOUT);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 2, 14, 10, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 7, 14, 8, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_EDITOR);
        desktop_redraw_with_prompt();
        return;
    }
    if (point_in_rect(row, col, 12, 14, 8, 2)) {
        start_menu_open = 0;
        desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
        desktop_redraw_with_prompt();
        return;
    }

    window = desktop_window_at_cursor();
    if (window) {
        start_menu_open = 0;
        desktop_active_window = window;
        desktop_redraw_with_prompt();
        return;
    }

    desktop_redraw_with_prompt();
}

static int desktop_titlebar_window_at_cursor(void) {
    if (framebuffer_available) return desktop_vbe_titlebar_window_at_pixel();
    int window = desktop_window_at_cursor();
    int row;
    int col;
    int width;
    if (!window) return 0;
    row = *desktop_window_row_ptr(window);
    col = *desktop_window_col_ptr(window);
    width = desktop_window_width(window);
    if (point_in_rect(desktop_cursor_row, desktop_cursor_col, row + 1, col + 1, width - 2, 1)) return window;
    return 0;
}

static void desktop_drag_window_by_mouse(int window, int dx, int dy) {
    int* row = desktop_window_row_ptr(window);
    int* col = desktop_window_col_ptr(window);
    int moved = 0;

    if (framebuffer_available) {
        int* x = desktop_window_px_x_ptr(window);
        int* y = desktop_window_px_y_ptr(window);
        int* w = desktop_window_px_w_ptr(window);
        int* h = desktop_window_px_h_ptr(window);
        int delta = dx;
        if (delta < 0) delta = -delta;
        if (dy < 0) drag_redraw_pending_px -= dy;
        else drag_redraw_pending_px += dy;
        drag_redraw_pending_px += delta;

        fb_restore_pixel_cursor();
        fb_cursor_x += dx;
        fb_cursor_y -= dy;
        clamp_fb_cursor();
        if (drag_mode == DESKTOP_DRAG_RESIZE) {
            *w += dx;
            *h -= dy;
        } else {
            *x += dx;
            *y -= dy;
        }
        desktop_clamp_window_id(window);
        if (drag_redraw_pending_px >= 10) {
            drag_redraw_pending_px = 0;
            desktop_redraw_with_prompt();
        } else {
            fb_draw_pixel_cursor();
        }
        return;
    }

    mouse_accum_x += dx;
    mouse_accum_y += dy;

    while (mouse_accum_x >= 36) {
        (*col)++;
        mouse_accum_x -= 36;
        moved = 1;
    }
    while (mouse_accum_x <= -36) {
        (*col)--;
        mouse_accum_x += 36;
        moved = 1;
    }
    while (mouse_accum_y >= 52) {
        (*row)--;
        mouse_accum_y -= 52;
        moved = 1;
    }
    while (mouse_accum_y <= -52) {
        (*row)++;
        mouse_accum_y += 52;
        moved = 1;
    }

    desktop_clamp_window_id(window);
    if (moved) desktop_redraw_with_prompt();
    else if (framebuffer_available) fb_draw_pixel_cursor();
}

static int desktop_handle_arrow_scancode(unsigned char sc) {
    if (sc == 0x48) {
        desktop_move_cursor("up");
        return 1;
    }
    if (sc == 0x50) {
        desktop_move_cursor("down");
        return 1;
    }
    if (sc == 0x4B) {
        desktop_move_cursor("left");
        return 1;
    }
    if (sc == 0x4D) {
        desktop_move_cursor("right");
        return 1;
    }
    return 0;
}

static int ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02) == 0) return 1;
    }
    return 0;
}

static int ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 0x01) return 1;
    }
    return 0;
}

static int ps2_read_data(unsigned char* out) {
    if (!ps2_wait_read()) return 0;
    *out = inb(0x60);
    return 1;
}

static int ps2_read_mouse_data(unsigned char* out) {
    for (int i = 0; i < 100000; i++) {
        unsigned char status = inb(0x64);
        if ((status & 0x21) == 0x21) {
            *out = inb(0x60);
            return 1;
        }
    }
    return 0;
}

static int ps2_write_controller(unsigned char value) {
    if (!ps2_wait_write()) return 0;
    outb(0x64, value);
    return 1;
}

static int ps2_write_data(unsigned char value) {
    if (!ps2_wait_write()) return 0;
    outb(0x60, value);
    return 1;
}

static int ps2_mouse_command(unsigned char command) {
    unsigned char ack;
    if (!ps2_write_controller(0xD4)) return 0;
    if (!ps2_write_data(command)) return 0;
    if (!ps2_read_mouse_data(&ack)) return 0;
    return ack == 0xFA;
}

static void mouse_init(void) {
    unsigned char config = 0;

    ps2_write_controller(0xA8);
    if (ps2_write_controller(0x20) && ps2_read_data(&config)) {
        config &= (unsigned char)~0x20;
        config |= 0x02;
        ps2_write_controller(0x60);
        ps2_write_data(config);
    }

    ps2_mouse_command(0xF6);
    ps2_mouse_command(0xE8);
    ps2_mouse_command(0x03);
    ps2_mouse_command(0xF4);
}

static void mouse_handle_packet(unsigned char b0, unsigned char b1, unsigned char b2) {
    static int previous_left = 0;
    static int previous_right = 0;
    int left = b0 & 0x01;
    int right = b0 & 0x02;
    int dx = (int8_t)b1;
    int dy = (int8_t)b2;
    int col_delta = 0;
    int row_delta = 0;

    if (mouse_speed < 1) mouse_speed = 1;
    if (mouse_speed > 6) mouse_speed = 6;
    dx *= mouse_speed;
    dy *= mouse_speed;

    if (desktop_hidden_terminal) {
        previous_left = left;
        previous_right = right;
        return;
    }

    if (framebuffer_available) {
        if (!left) {
            if (dragging_window && drag_redraw_pending_px) {
                drag_redraw_pending_px = 0;
                desktop_redraw_with_prompt();
            }
            dragging_window = 0;
            drag_mode = 0;
        }

        if (left && !previous_left) {
            if (desktop_vbe_close_button_at_pixel()) {
                desktop_click_cursor();
                previous_left = left;
                previous_right = right;
                return;
            }

            dragging_window = desktop_vbe_resize_window_at_pixel();
            drag_mode = dragging_window ? DESKTOP_DRAG_RESIZE : 0;
            drag_redraw_pending_px = 0;
            if (!dragging_window) {
                dragging_window = desktop_titlebar_window_at_cursor();
                drag_mode = dragging_window ? DESKTOP_DRAG_MOVE : 0;
            }
            if (dragging_window) desktop_active_window = dragging_window;
        }

        if (left && dragging_window) {
            desktop_drag_window_by_mouse(dragging_window, dx, dy);
            previous_left = left;
            return;
        }

        fb_cursor_x += dx;
        fb_cursor_y -= dy;
        clamp_fb_cursor();

        if (right && !previous_right) {
            desktop_vbe_right_click_cursor();
        } else if (left && !previous_left) {
            desktop_click_cursor();
        } else if (dx || dy) {
            fb_restore_pixel_cursor();
            fb_draw_pixel_cursor();
        }

        previous_left = left;
        previous_right = right;
        return;
    }

    if (!left) {
        dragging_window = 0;
        drag_mode = 0;
    }

    if (left && !previous_left) {
        dragging_window = desktop_titlebar_window_at_cursor();
        drag_mode = dragging_window ? DESKTOP_DRAG_MOVE : 0;
        if (dragging_window) desktop_active_window = dragging_window;
    }

    if (left && dragging_window) {
        desktop_drag_window_by_mouse(dragging_window, dx, dy);
        previous_left = left;
        return;
    }

    mouse_accum_x += dx;
    mouse_accum_y += dy;
    while (mouse_accum_x >= 44) {
        col_delta++;
        mouse_accum_x -= 44;
    }
    while (mouse_accum_x <= -44) {
        col_delta--;
        mouse_accum_x += 44;
    }
    while (mouse_accum_y >= 64) {
        row_delta++;
        mouse_accum_y -= 64;
    }
    while (mouse_accum_y <= -64) {
        row_delta--;
        mouse_accum_y += 64;
    }

    desktop_cursor_col += col_delta;
    desktop_cursor_row -= row_delta;
    clamp_desktop_cursor();

    if (left && !previous_left) {
        desktop_click_cursor();
    } else if (col_delta || row_delta) {
        if (framebuffer_available) {
            fb_restore_pixel_cursor();
            fb_draw_pixel_cursor();
        } else {
            desktop_redraw_with_prompt();
        }
    }

    previous_left = left;
    previous_right = right;
}

static void mouse_handle_byte(unsigned char data) {
    static unsigned char packet[3];
    static int index = 0;

    if (index == 0 && (data & 0x08) == 0) return;
    packet[index++] = data;
    if (index == 3) {
        mouse_handle_packet(packet[0], packet[1], packet[2]);
        index = 0;
    }
}

static const char* skip_spaces(const char* s) {
    while (*s == ' ') s++;
    return s;
}

static const char* next_arg(const char* s) {
    while (*s && *s != ' ') s++;
    return skip_spaces(s);
}

static void desktop_kprint_char(char c) {
    char text[2] = {c, 0};
    kprint(text);
}

static void desktop_kprint_hex64(uint64_t value) {
    kprint("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        unsigned char nibble = (value >> i) & 0xF;
        if (nibble || started || i == 0) {
            started = 1;
            desktop_kprint_char("0123456789ABCDEF"[nibble]);
        }
    }
}

static void desktop_kprint_dec(uint64_t value) {
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

static void cmd_vbe_info(void) {
    if (!framebuffer_available) {
        kprint("VBE framebuffer not available; using VGA text mode fallback.\n");
        return;
    }

    kprint("VBE framebuffer: ");
    desktop_kprint_dec(frame_buffer_width);
    kprint("x");
    desktop_kprint_dec(frame_buffer_height);
    kprint("x");
    desktop_kprint_dec(frame_buffer_bpp);
    kprint(" pitch=");
    desktop_kprint_dec(frame_buffer_pitch);
    kprint(" addr=");
    desktop_kprint_hex64(frame_buffer_addr);
    kprint("\n");
}


int desktop_handle_command(const char* cmd) {
    if (string_equals(cmd, "desktop")) {
        desktop_hidden_terminal = 0;
        if (framebuffer_available) set_kprint_mirror(terminal_log_append);
        draw_desktop_screen();
        return 1;
    } else if (string_equals(cmd, "term") || string_equals(cmd, "terminalmode") ||
               string_equals(cmd, "fullscreen-terminal")) {
        desktop_hidden_terminal = 1;
        start_menu_open = 0;
        context_menu_open = 0;
        if (framebuffer_available) set_kprint_mirror(0);
        draw_terminal_only_screen();
        return 1;
    } else if (string_equals(cmd, "start")) {
        start_menu_open = 1;
        if (framebuffer_available) desktop_redraw_with_prompt();
        else {
            draw_start_menu();
            set_cursor(32, 10);
        }
        return 1;
    } else if (string_equals(cmd, "lock")) {
        desktop_lock_screen();
        return 1;
    } else if (string_equals(cmd, "poweroff")) {
        desktop_power_off();
        return 1;
    } else if (string_equals(cmd, "terminal")) {
        desktop_open_window(DESKTOP_WINDOW_TERMINAL);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_equals(cmd, "files")) {
        desktop_open_window(DESKTOP_WINDOW_FILES);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_equals(cmd, "system")) {
        desktop_open_window(DESKTOP_WINDOW_SYSTEM);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_equals(cmd, "about")) {
        desktop_open_window(DESKTOP_WINDOW_ABOUT);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_equals(cmd, "settings")) {
        desktop_open_window(DESKTOP_WINDOW_SETTINGS);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_equals(cmd, "taskman") || string_equals(cmd, "taskmgr") || string_equals(cmd, "tasks")) {
        desktop_open_window(DESKTOP_WINDOW_TASKMANAGER);
        desktop_redraw_with_prompt();
        return 1;
    } else if (string_starts_with(cmd, "open ")) {
        const char* app = skip_spaces(cmd + 5);
        int window;
        if (desktop_window_from_name(app, &window)) {
            desktop_open_window(window);
            desktop_redraw_with_prompt();
        } else {
            kprint("Usage: open terminal|files|system|about|settings|taskman\n");
        }
        return 1;
    } else if (string_starts_with(cmd, "close ")) {
        const char* app = skip_spaces(cmd + 6);
        int window;
        if (desktop_window_from_name(app, &window)) {
            desktop_close_window(window);
            desktop_redraw_with_prompt();
        } else {
            kprint("Usage: close terminal|files|system|about|settings|taskman\n");
        }
        return 1;
    } else if (string_starts_with(cmd, "focus ")) {
        int window;
        const char* app = skip_spaces(cmd + 6);
        if (desktop_window_from_name(app, &window)) {
            desktop_open_window(window);
            desktop_redraw_with_prompt();
        } else {
            kprint("Usage: focus terminal|files|system|about|settings|taskman\n");
        }
        return 1;
    } else if (string_starts_with(cmd, "move ")) {
        const char* arg = skip_spaces(cmd + 5);
        const char* second = next_arg(arg);
        int window = desktop_active_window;

        if (string_starts_with(arg, "terminal ") || string_starts_with(arg, "settings ") ||
            string_starts_with(arg, "files ") || string_starts_with(arg, "system ") ||
            string_starts_with(arg, "about ") || string_starts_with(arg, "editor ") ||
            string_starts_with(arg, "taskman ") || string_starts_with(arg, "tasks ")) {
            char name[16];
            int pos = 0;
            while (*arg && *arg != ' ' && pos < 15) name[pos++] = *arg++;
            name[pos] = 0;
            if (!desktop_window_from_name(name, &window)) {
                kprint("Usage: move [app] left|right|up|down\n");
                return 1;
            }
            arg = second;
        }
        desktop_open_window(window);
        desktop_move_window(window, arg);
        return 1;
    } else if (string_starts_with(cmd, "resize ")) {
        const char* arg = skip_spaces(cmd + 7);
        const char* second = next_arg(arg);
        int window = desktop_active_window;

        if (string_starts_with(arg, "terminal ") || string_starts_with(arg, "settings ") ||
            string_starts_with(arg, "files ") || string_starts_with(arg, "system ") ||
            string_starts_with(arg, "about ") || string_starts_with(arg, "editor ") ||
            string_starts_with(arg, "taskman ") || string_starts_with(arg, "tasks ")) {
            char name[16];
            int pos = 0;
            while (*arg && *arg != ' ' && pos < 15) name[pos++] = *arg++;
            name[pos] = 0;
            if (!desktop_window_from_name(name, &window)) {
                kprint("Usage: resize [app] wider|narrower|taller|shorter\n");
                return 1;
            }
            arg = second;
        }
        desktop_open_window(window);
        desktop_resize_window(window, arg);
        return 1;
    } else if (string_starts_with(cmd, "cursor ")) {
        const char* arg = skip_spaces(cmd + 7);
        if (string_equals(arg, "show")) {
            desktop_redraw_with_prompt();
        } else {
            desktop_move_cursor(arg);
        }
        return 1;
    } else if (string_equals(cmd, "mousespeed")) {
        kprint("Mouse speed: ");
        desktop_kprint_dec((uint32_t)mouse_speed);
        kprint("\n");
        return 1;
    } else if (string_starts_with(cmd, "mousespeed ")) {
        int speed = 0;
        if (!parse_small_int(skip_spaces(cmd + 11), &speed) || speed < 1 || speed > 6) {
            kprint("Usage: mousespeed 1..6\n");
            return 1;
        }
        mouse_speed = speed;
        kprint("Mouse speed set to ");
        desktop_kprint_dec((uint32_t)mouse_speed);
        kprint("\n");
        return 1;
    } else if (string_starts_with(cmd, "scroll ")) {
        desktop_scroll_active(skip_spaces(cmd + 7));
        return 1;
    } else if (string_equals(cmd, "click")) {
        desktop_click_cursor();
        return 1;
    } else if (string_equals(cmd, "vbe")) {
        cmd_vbe_info();
        return 1;
    } else if (string_starts_with(cmd, "bg ")) {
        unsigned char color;
        const char* name = skip_spaces(cmd + 3);
        if (desktop_color_from_name(name, &color)) {
            desktop_background_attr = color;
            draw_desktop_screen();
        } else {
            kprint("Usage: bg blue|green|cyan|red|gray|black\n");
        }
        return 1;
    } else if (string_starts_with(cmd, "bootmode ")) {
        const char* mode = skip_spaces(cmd + 9);
        if (string_equals(mode, "desktop")) {
            prefer_desktop_boot = 1;
            kprint("Startup preference: desktop\n");
        } else if (string_equals(mode, "terminal")) {
            prefer_desktop_boot = 0;
            kprint("Startup preference: terminal\n");
        } else {
            kprint("Usage: bootmode desktop|terminal\n");
        }
        return 1;
    } else if (string_equals(cmd, "logo") || string_equals(cmd, "logo:")) {
        print_archway_logo();
        return 1;
    }

    return 0;
}

void Kernel_main(uint32_t boot_magic, uintptr_t boot_info_addr) {
    kernel_boot_magic = boot_magic;
    kernel_boot_info_addr = boot_info_addr;

    init_serial();
    serial_write("Kernel_main entered\n");
    capture_multiboot_cmdline();
    capture_multiboot_framebuffer();
    fs_init();
    hardware_init();
    mouse_init();
   
 

    if (boot_text_mode) {
        draw_text_boot_screen();
    } else if (prefer_desktop_boot) {
        draw_desktop_start_screen();
        wait_for_keyboard_press();
        draw_desktop_screen();
    } else {
        draw_terminal_only_screen();
    }
    if (framebuffer_available) {
        set_kprint_mirror(terminal_log_append);
        terminal_log_append("ArchwayOS command terminal\nType help for commands\n");
    } else {
        set_kprint_mirror(0);
    }
    char buffer[100];
    int buf_pos = 0;
    int extended_scancode = 0;

    while (1) {
        unsigned char status = inb(0x64);
        unsigned char sc;

        if ((status & 0x01) == 0) continue;

        sc = inb(0x60);
        if (status & 0x20) {
            mouse_handle_byte(sc);
            continue;
        }

        if (sc == 0xE0) {
            extended_scancode = 1;
            continue;
        }
        if (extended_scancode) {
            extended_scancode = 0;
            if (!desktop_hidden_terminal && sc < 128 && desktop_handle_arrow_scancode(sc)) continue;
        }
        if (sc < 128) {
            int terminal_keyboard = !framebuffer_available ||
                desktop_hidden_terminal ||
                (terminal_open && desktop_active_window == DESKTOP_WINDOW_TERMINAL);
            if (!desktop_hidden_terminal && desktop_handle_arrow_scancode(sc)) continue;
            char c = scancode_to_ascii(sc);
            if (framebuffer_available && !terminal_keyboard && c >= 32 && c <= 126) {
                continue;
            }
            if (c == '\n') {
                if (framebuffer_available && !terminal_keyboard) {
                    buffer[0] = 0;
                    buf_pos = 0;
                    continue;
                }
                buffer[buf_pos] = 0;
                char trimmed[100];
                trim_spaces(trimmed, buffer);
                if (framebuffer_available &&
                    (string_equals(trimmed, "clear") || string_equals(trimmed, "cls"))) {
                    terminal_log_clear();
                }
                if (framebuffer_available && !desktop_hidden_terminal) set_framebuffer_console_enabled(0);
                run_command(trimmed);
                if (framebuffer_available && !desktop_hidden_terminal) set_framebuffer_console_enabled(1);
                if (framebuffer_available && desktop_hidden_terminal) {
                    if (trimmed[0] && !string_equals(trimmed, "term") &&
                        !string_equals(trimmed, "terminalmode") &&
                        !string_equals(trimmed, "fullscreen-terminal")) {
                        kprint("Root User:");
                    }
                    buffer[0] = 0;
                } else if (framebuffer_available) {
                    buffer[0] = 0;
                    draw_desktop_screen();
                    if (terminal_open) draw_vbe_command_input(buffer);
                } else {
                    kprint("Root User:");
                }
                buf_pos = 0;
            } else if (c == '\b') {
                if (buf_pos > 0) {
                    buf_pos--;
                    buffer[buf_pos] = 0;
                    if (framebuffer_available && desktop_hidden_terminal) kprint("\b \b");
                    else if (framebuffer_available && terminal_keyboard) draw_vbe_command_input(buffer);
                    else if (!framebuffer_available) kprint("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                if (buf_pos < (int)sizeof(buffer) - 1) {
                    buffer[buf_pos++] = c;
                    buffer[buf_pos] = 0;
                    if (framebuffer_available && desktop_hidden_terminal) {
                        char s[2] = {c, 0};
                        kprint(s);
                    } else if (framebuffer_available && terminal_keyboard) {
                        draw_vbe_command_input(buffer);
                    } else if (!framebuffer_available) {
                        char s[2] = {c, 0};
                        kprint(s);
                    }
                }
            }
        }
    }
}
