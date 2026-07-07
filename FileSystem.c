#include "os.h"

#define FAT32_CLUSTER_SIZE 512u
#define FAT32_CLUSTER_COUNT 256u
#define FAT32_ROOT_CLUSTER 2u
#define FAT32_FREE 0x00000000u
#define FAT32_EOC 0x0FFFFFFFu
#define FAT32_ATTR_READ_ONLY 0x01u
#define FAT32_ATTR_DIRECTORY 0x10u
#define FAT32_ATTR_ARCHIVE 0x20u
#define FS_MAX_PATH 96
#define FS_MAX_NAME 12
#define VFS_MAX_MOUNTS 4

typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} Fat32DirEntry;

typedef struct {
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sector_count;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t total_sectors;
} Fat32Info;

typedef struct {
    Fat32DirEntry entry;
    uint32_t parent_cluster;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint32_t data_cluster;
    int is_root;
} FsNode;

typedef struct {
    const char* name;
    const char* type;
    const char* mount_path;
    void (*format)(void);
    int (*desktop_list)(FsDesktopEntry* entries, int max_entries);
    void (*cmd_ls)(const char* args);
    void (*cmd_cd)(const char* args);
    void (*cmd_mkdir)(const char* args);
    void (*cmd_touch)(const char* args);
    void (*cmd_write)(const char* args);
    void (*cmd_cat)(const char* args);
    void (*cmd_rm)(const char* args, int want_dir);
    void (*cmd_stat)(const char* args);
    void (*cmd_df)(void);
    const char* (*cwd)(void);
} VfsOps;

typedef struct {
    const char* path;
    const VfsOps* ops;
    int mounted;
} VfsMount;

static Fat32Info fs_info;
static uint32_t fs_fat[FAT32_CLUSTER_COUNT];
static uint8_t fs_data[FAT32_CLUSTER_COUNT][FAT32_CLUSTER_SIZE];
static uint32_t fs_cwd_cluster = FAT32_ROOT_CLUSTER;
static char fs_cwd_path[FS_MAX_PATH] = "/";
static int fs_ready = 0;
static VfsMount vfs_mounts[VFS_MAX_MOUNTS];
static int vfs_mount_count = 0;
static int vfs_ready = 0;

static void fs_format(void);
static int ramfs_desktop_list(FsDesktopEntry* entries, int max_entries);
static void fs_cmd_ls(const char* args);
static void fs_cmd_cd(const char* args);
static void fs_cmd_mkdir(const char* args);
static void fs_cmd_touch(const char* args);
static void fs_cmd_write(const char* args);
static void fs_cmd_cat(const char* args);
static void fs_cmd_rm(const char* args, int want_dir);
static void fs_cmd_stat(const char* args);
static void fs_cmd_df(void);
static const char* fs_get_cwd(void);

static int fs_streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int fs_starts(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static uint32_t fs_strlen(const char* s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static void fs_memzero(void* ptr, uint32_t len) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint32_t i = 0; i < len; i++) p[i] = 0;
}

static void fs_memcpy(void* dst, const void* src, uint32_t len) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < len; i++) d[i] = s[i];
}

static void fs_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
}

static const char* fs_skip_spaces(const char* s) {
    while (*s == ' ') s++;
    return s;
}

static int fs_valid_cluster(uint32_t cluster) {
    return cluster >= FAT32_ROOT_CLUSTER && cluster < FAT32_CLUSTER_COUNT;
}

static uint32_t fs_entry_cluster(const Fat32DirEntry* entry) {
    return ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
}

static void fs_set_entry_cluster(Fat32DirEntry* entry, uint32_t cluster) {
    entry->first_cluster_high = (uint16_t)(cluster >> 16);
    entry->first_cluster_low = (uint16_t)(cluster & 0xFFFF);
}

static Fat32DirEntry* fs_dir_entry(uint32_t cluster, uint32_t index) {
    return (Fat32DirEntry*)&fs_data[cluster][index * sizeof(Fat32DirEntry)];
}

static uint32_t fs_entries_per_cluster(void) {
    return FAT32_CLUSTER_SIZE / sizeof(Fat32DirEntry);
}

static uint32_t fs_alloc_cluster(void) {
    for (uint32_t i = FAT32_ROOT_CLUSTER; i < FAT32_CLUSTER_COUNT; i++) {
        if (fs_fat[i] == FAT32_FREE) {
            fs_fat[i] = FAT32_EOC;
            fs_memzero(fs_data[i], FAT32_CLUSTER_SIZE);
            return i;
        }
    }
    return 0;
}

static void fs_free_chain(uint32_t cluster) {
    while (fs_valid_cluster(cluster)) {
        uint32_t next = fs_fat[cluster];
        fs_fat[cluster] = FAT32_FREE;
        fs_memzero(fs_data[cluster], FAT32_CLUSTER_SIZE);
        if (next >= FAT32_EOC || next == FAT32_FREE) break;
        cluster = next;
    }
}

static uint32_t fs_last_cluster(uint32_t cluster) {
    while (fs_valid_cluster(cluster) && fs_valid_cluster(fs_fat[cluster])) {
        cluster = fs_fat[cluster];
    }
    return cluster;
}

static int fs_append_cluster(uint32_t first_cluster) {
    uint32_t next = fs_alloc_cluster();
    if (!next) return 0;
    fs_fat[fs_last_cluster(first_cluster)] = next;
    return 1;
}

static uint32_t fs_free_cluster_count(void) {
    uint32_t count = 0;
    for (uint32_t i = FAT32_ROOT_CLUSTER; i < FAT32_CLUSTER_COUNT; i++) {
        if (fs_fat[i] == FAT32_FREE) count++;
    }
    return count;
}

static uint32_t fs_chain_cluster_count(uint32_t cluster) {
    uint32_t count = 0;
    while (fs_valid_cluster(cluster)) {
        uint32_t next = fs_fat[cluster];
        count++;
        if (!fs_valid_cluster(next)) break;
        cluster = next;
    }
    return count;
}

static int fs_name_to_83(const char* input, char out[11]) {
    uint32_t i = 0;
    uint32_t base = 0;
    uint32_t ext = 8;
    int in_ext = 0;

    for (i = 0; i < 11; i++) out[i] = ' ';
    if (!input || !*input) return 0;

    for (i = 0; input[i]; i++) {
        char c = input[i];
        if (c == '/' || c == ' ') return 0;
        if (c == '.') {
            if (in_ext) return 0;
            in_ext = 1;
            continue;
        }
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;

        if (in_ext) {
            if (ext >= 11) return 0;
            out[ext++] = c;
        } else {
            if (base >= 8) return 0;
            out[base++] = c;
        }
    }

    return base > 0;
}

static void fs_name_from_83(const char in[11], char out[FS_MAX_NAME]) {
    uint32_t pos = 0;
    for (uint32_t i = 0; i < 8 && in[i] != ' '; i++) out[pos++] = in[i];
    if (in[8] != ' ') {
        out[pos++] = '.';
        for (uint32_t i = 8; i < 11 && in[i] != ' '; i++) out[pos++] = in[i];
    }
    out[pos] = 0;
}

static int fs_name_match(const Fat32DirEntry* entry, const char name83[11]) {
    for (uint32_t i = 0; i < 11; i++) {
        if (entry->name[i] != name83[i]) return 0;
    }
    return 1;
}

static void fs_make_dot_name(char out[11], int parent) {
    for (uint32_t i = 0; i < 11; i++) out[i] = ' ';
    out[0] = '.';
    if (parent) out[1] = '.';
}

static void fs_make_dir_entry(Fat32DirEntry* entry, const char name83[11], uint8_t attr, uint32_t cluster, uint32_t size) {
    fs_memzero(entry, sizeof(*entry));
    fs_memcpy(entry->name, name83, 11);
    entry->attr = attr;
    fs_set_entry_cluster(entry, cluster);
    entry->file_size = size;
}

static int fs_find_entry(uint32_t dir_cluster, const char* name, Fat32DirEntry* out, uint32_t* out_cluster, uint32_t* out_index) {
    char name83[11];
    uint32_t cluster = dir_cluster;

    if (!fs_name_to_83(name, name83)) return 0;
    while (fs_valid_cluster(cluster)) {
        for (uint32_t i = 0; i < fs_entries_per_cluster(); i++) {
            Fat32DirEntry* entry = fs_dir_entry(cluster, i);
            if ((uint8_t)entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->attr == 0x0F) continue;
            if (fs_name_match(entry, name83)) {
                if (out) *out = *entry;
                if (out_cluster) *out_cluster = cluster;
                if (out_index) *out_index = i;
                return 1;
            }
        }
        if (!fs_valid_cluster(fs_fat[cluster])) break;
        cluster = fs_fat[cluster];
    }
    return 0;
}

static int fs_find_free_entry(uint32_t dir_cluster, uint32_t* out_cluster, uint32_t* out_index) {
    uint32_t cluster = dir_cluster;
    while (fs_valid_cluster(cluster)) {
        for (uint32_t i = 0; i < fs_entries_per_cluster(); i++) {
            Fat32DirEntry* entry = fs_dir_entry(cluster, i);
            if ((uint8_t)entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5) {
                *out_cluster = cluster;
                *out_index = i;
                return 1;
            }
        }
        if (!fs_valid_cluster(fs_fat[cluster])) break;
        cluster = fs_fat[cluster];
    }

    if (!fs_append_cluster(dir_cluster)) return 0;
    *out_cluster = fs_last_cluster(dir_cluster);
    *out_index = 0;
    return 1;
}

static int fs_add_entry(uint32_t dir_cluster, const char* name, uint8_t attr, uint32_t data_cluster, uint32_t size) {
    char name83[11];
    uint32_t entry_cluster;
    uint32_t entry_index;

    if (!fs_name_to_83(name, name83)) return 0;
    if (fs_find_entry(dir_cluster, name, 0, 0, 0)) return 0;
    if (!fs_find_free_entry(dir_cluster, &entry_cluster, &entry_index)) return 0;
    fs_make_dir_entry(fs_dir_entry(entry_cluster, entry_index), name83, attr, data_cluster, size);
    return 1;
}

static int fs_dir_is_empty(uint32_t dir_cluster) {
    uint32_t cluster = dir_cluster;
    while (fs_valid_cluster(cluster)) {
        for (uint32_t i = 0; i < fs_entries_per_cluster(); i++) {
            Fat32DirEntry* entry = fs_dir_entry(cluster, i);
            if ((uint8_t)entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->name[0] == '.') continue;
            return 0;
        }
        if (!fs_valid_cluster(fs_fat[cluster])) break;
        cluster = fs_fat[cluster];
    }
    return 1;
}

static uint32_t fs_parent_of(uint32_t dir_cluster) {
    Fat32DirEntry entry;
    if (dir_cluster == FAT32_ROOT_CLUSTER) return FAT32_ROOT_CLUSTER;
    if (fs_find_entry(dir_cluster, "..", &entry, 0, 0)) {
        uint32_t parent = fs_entry_cluster(&entry);
        if (fs_valid_cluster(parent)) return parent;
    }
    return FAT32_ROOT_CLUSTER;
}

static int fs_next_component(const char** cursor, char out[FS_MAX_NAME]) {
    uint32_t pos = 0;
    const char* s = *cursor;

    while (*s == '/') s++;
    if (!*s) {
        *cursor = s;
        return 0;
    }
    while (*s && *s != '/') {
        if (pos >= FS_MAX_NAME - 1) return -1;
        out[pos++] = *s++;
    }
    out[pos] = 0;
    *cursor = s;
    return 1;
}

static int fs_resolve_path(const char* path, FsNode* out) {
    uint32_t current = (path[0] == '/') ? FAT32_ROOT_CLUSTER : fs_cwd_cluster;
    const char* cursor = path;
    char component[FS_MAX_NAME];
    Fat32DirEntry entry;
    uint32_t entry_cluster;
    uint32_t entry_index;
    int had_component = 0;

    if (!path || !*path) return 0;
    while (1) {
        int next = fs_next_component(&cursor, component);
        if (next < 0) return 0;
        if (next == 0) break;
        had_component = 1;

        if (fs_streq(component, ".")) {
            continue;
        }
        if (fs_streq(component, "..")) {
            current = fs_parent_of(current);
            continue;
        }
        if (!fs_find_entry(current, component, &entry, &entry_cluster, &entry_index)) return 0;

        while (*cursor == '/') cursor++;
        if (*cursor) {
            if (!(entry.attr & FAT32_ATTR_DIRECTORY)) return 0;
            current = fs_entry_cluster(&entry);
            continue;
        }

        out->entry = entry;
        out->parent_cluster = current;
        out->entry_cluster = entry_cluster;
        out->entry_index = entry_index;
        out->data_cluster = fs_entry_cluster(&entry);
        out->is_root = 0;
        return 1;
    }

    if (!had_component || fs_streq(path, ".") || fs_streq(path, "/")) {
        fs_memzero(out, sizeof(*out));
        out->entry.attr = FAT32_ATTR_DIRECTORY;
        out->data_cluster = current;
        out->is_root = current == FAT32_ROOT_CLUSTER;
        return 1;
    }

    fs_memzero(out, sizeof(*out));
    out->entry.attr = FAT32_ATTR_DIRECTORY;
    out->data_cluster = current;
    out->is_root = current == FAT32_ROOT_CLUSTER;
    return 1;
}

static int fs_split_parent(const char* path, char parent[FS_MAX_PATH], char name[FS_MAX_NAME]) {
    uint32_t len = fs_strlen(path);
    uint32_t end;
    uint32_t slash;
    uint32_t name_len;

    if (!path || !*path) return 0;
    while (len > 1 && path[len - 1] == '/') len--;
    if (len == 0) return 0;
    end = len;
    slash = end;
    while (slash > 0 && path[slash - 1] != '/') slash--;
    name_len = end - slash;
    if (name_len == 0 || name_len >= FS_MAX_NAME) return 0;

    for (uint32_t i = 0; i < name_len; i++) name[i] = path[slash + i];
    name[name_len] = 0;

    if (slash == 0) {
        fs_strcpy(parent, ".");
    } else if (slash == 1) {
        fs_strcpy(parent, "/");
    } else {
        uint32_t parent_len = slash - 1;
        if (parent_len >= FS_MAX_PATH) return 0;
        for (uint32_t i = 0; i < parent_len; i++) parent[i] = path[i];
        parent[parent_len] = 0;
    }

    return 1;
}

static void fs_path_pop(char path[FS_MAX_PATH]) {
    uint32_t len = fs_strlen(path);
    if (len <= 1) {
        fs_strcpy(path, "/");
        return;
    }
    while (len > 1 && path[len - 1] == '/') len--;
    while (len > 1 && path[len - 1] != '/') len--;
    if (len <= 1) {
        fs_strcpy(path, "/");
    } else {
        path[len - 1] = 0;
    }
}

static int fs_path_push(char path[FS_MAX_PATH], const char* component) {
    uint32_t len = fs_strlen(path);
    uint32_t clen = fs_strlen(component);
    if (fs_streq(component, ".") || clen == 0) return 1;
    if (fs_streq(component, "..")) {
        fs_path_pop(path);
        return 1;
    }
    if (len + clen + 2 >= FS_MAX_PATH) return 0;
    if (!fs_streq(path, "/")) path[len++] = '/';
    for (uint32_t i = 0; i < clen; i++) path[len++] = component[i];
    path[len] = 0;
    return 1;
}

static int fs_make_canonical_path(const char* input, char out[FS_MAX_PATH]) {
    const char* cursor = input;
    char component[FS_MAX_NAME];

    if (input[0] == '/') fs_strcpy(out, "/");
    else fs_strcpy(out, fs_cwd_path);

    while (1) {
        int next = fs_next_component(&cursor, component);
        if (next < 0) return 0;
        if (next == 0) return 1;
        if (!fs_path_push(out, component)) return 0;
    }
}

static void fs_print_dec(uint32_t value) {
    char buf[11];
    int pos = 10;
    buf[pos] = 0;
    if (value == 0) {
        kprint("0");
        return;
    }
    while (value && pos > 0) {
        buf[--pos] = (char)('0' + (value % 10));
        value /= 10;
    }
    kprint(&buf[pos]);
}

static void fs_format(void) {
    fs_memzero(fs_fat, sizeof(fs_fat));
    fs_memzero(fs_data, sizeof(fs_data));

    fs_info.bytes_per_sector = 512;
    fs_info.sectors_per_cluster = 1;
    fs_info.reserved_sector_count = 32;
    fs_info.fat_count = 1;
    fs_info.sectors_per_fat = 2;
    fs_info.root_cluster = FAT32_ROOT_CLUSTER;
    fs_info.total_sectors = FAT32_CLUSTER_COUNT;

    fs_fat[0] = FAT32_EOC;
    fs_fat[1] = FAT32_EOC;
    fs_fat[FAT32_ROOT_CLUSTER] = FAT32_EOC;
    fs_cwd_cluster = FAT32_ROOT_CLUSTER;
    fs_strcpy(fs_cwd_path, "/");
    fs_ready = 1;
}

void fs_init(void) {
    static const VfsOps ramfs_ops = {
        "ram0",
        "fat32-ram",
        "/",
        fs_format,
        ramfs_desktop_list,
        fs_cmd_ls,
        fs_cmd_cd,
        fs_cmd_mkdir,
        fs_cmd_touch,
        fs_cmd_write,
        fs_cmd_cat,
        fs_cmd_rm,
        fs_cmd_stat,
        fs_cmd_df,
        fs_get_cwd
    };

    if (!fs_ready) fs_format();
    if (!vfs_ready) {
        vfs_mounts[0].path = "/";
        vfs_mounts[0].ops = &ramfs_ops;
        vfs_mounts[0].mounted = 1;
        vfs_mount_count = 1;
        vfs_ready = 1;
    }
}

static int ramfs_desktop_list(FsDesktopEntry* entries, int max_entries) {
    uint32_t cluster;
    int count = 0;

    fs_init();
    if (!entries || max_entries <= 0) return 0;

    cluster = fs_cwd_cluster;
    while (fs_valid_cluster(cluster)) {
        for (uint32_t i = 0; i < fs_entries_per_cluster() && count < max_entries; i++) {
            Fat32DirEntry* entry = fs_dir_entry(cluster, i);
            char name[FS_MAX_NAME];
            int pos = 0;

            if ((uint8_t)entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->attr == 0x0F) continue;
            if (entry->name[0] == '.') continue;

            fs_name_from_83(entry->name, name);
            while (name[pos] && pos < 15) {
                entries[count].name[pos] = name[pos];
                pos++;
            }
            entries[count].name[pos] = 0;
            entries[count].size = entry->file_size;
            entries[count].is_dir = (entry->attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
            count++;
        }
        if (count >= max_entries || !fs_valid_cluster(fs_fat[cluster])) break;
        cluster = fs_fat[cluster];
    }

    return count;
}

static void fs_cmd_ls(const char* args) {
    FsNode node;
    uint32_t dir_cluster;
    uint32_t cluster;
    int printed = 0;

    args = fs_skip_spaces(args);
    if (!*args) args = ".";
    if (!fs_resolve_path(args, &node) || !(node.entry.attr & FAT32_ATTR_DIRECTORY)) {
        kprint("ls: directory not found\n");
        return;
    }

    dir_cluster = node.data_cluster;
    cluster = dir_cluster;
    while (fs_valid_cluster(cluster)) {
        for (uint32_t i = 0; i < fs_entries_per_cluster(); i++) {
            Fat32DirEntry* entry = fs_dir_entry(cluster, i);
            char name[FS_MAX_NAME];
            if ((uint8_t)entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->attr == 0x0F) continue;
            if (entry->name[0] == '.') continue;
            fs_name_from_83(entry->name, name);
            kprint((entry->attr & FAT32_ATTR_DIRECTORY) ? "<DIR> " : "      ");
            kprint(name);
            if (!(entry->attr & FAT32_ATTR_DIRECTORY)) {
                kprint("  ");
                fs_print_dec(entry->file_size);
                kprint(" bytes");
            }
            kprint("\n");
            printed = 1;
        }
        if (!fs_valid_cluster(fs_fat[cluster])) break;
        cluster = fs_fat[cluster];
    }
    if (!printed) kprint("(empty)\n");
}

static void fs_cmd_cd(const char* args) {
    FsNode node;
    char new_path[FS_MAX_PATH];

    args = fs_skip_spaces(args);
    if (!*args) args = "/";
    if (!fs_resolve_path(args, &node) || !(node.entry.attr & FAT32_ATTR_DIRECTORY)) {
        kprint("cd: directory not found\n");
        return;
    }
    if (!fs_make_canonical_path(args, new_path)) {
        kprint("cd: path is too long\n");
        return;
    }
    fs_cwd_cluster = node.data_cluster;
    fs_strcpy(fs_cwd_path, new_path);
}

static void fs_cmd_mkdir(const char* args) {
    char parent_path[FS_MAX_PATH];
    char name[FS_MAX_NAME];
    FsNode parent;
    uint32_t cluster;
    char dot[11];

    args = fs_skip_spaces(args);
    if (!fs_split_parent(args, parent_path, name)) {
        kprint("Usage: mkdir NAME\n");
        return;
    }
    if (!fs_resolve_path(parent_path, &parent) || !(parent.entry.attr & FAT32_ATTR_DIRECTORY)) {
        kprint("mkdir: parent directory not found\n");
        return;
    }
    if (fs_find_entry(parent.data_cluster, name, 0, 0, 0)) {
        kprint("mkdir: entry already exists\n");
        return;
    }
    cluster = fs_alloc_cluster();
    if (!cluster) {
        kprint("mkdir: disk full\n");
        return;
    }

    fs_make_dot_name(dot, 0);
    fs_make_dir_entry(fs_dir_entry(cluster, 0), dot, FAT32_ATTR_DIRECTORY, cluster, 0);
    fs_make_dot_name(dot, 1);
    fs_make_dir_entry(fs_dir_entry(cluster, 1), dot, FAT32_ATTR_DIRECTORY, parent.data_cluster, 0);

    if (!fs_add_entry(parent.data_cluster, name, FAT32_ATTR_DIRECTORY, cluster, 0)) {
        fs_free_chain(cluster);
        kprint("mkdir: invalid or duplicate 8.3 name\n");
    }
}

static int fs_write_file(const char* path, const char* text) {
    char parent_path[FS_MAX_PATH];
    char name[FS_MAX_NAME];
    FsNode parent;
    Fat32DirEntry existing;
    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;
    uint32_t needed;
    uint32_t first = 0;
    uint32_t current = 0;
    uint32_t len = fs_strlen(text);

    if (!fs_split_parent(path, parent_path, name)) {
        kprint("write: invalid path\n");
        return 0;
    }
    if (!fs_resolve_path(parent_path, &parent) || !(parent.entry.attr & FAT32_ATTR_DIRECTORY)) {
        kprint("write: parent directory not found\n");
        return 0;
    }
    if (fs_find_entry(parent.data_cluster, name, &existing, &entry_cluster, &entry_index)) {
        if (existing.attr & FAT32_ATTR_DIRECTORY) {
            kprint("write: cannot write a directory\n");
            return 0;
        }
    } else {
        if (!fs_add_entry(parent.data_cluster, name, FAT32_ATTR_ARCHIVE, 0, 0)) {
            kprint("write: invalid or duplicate 8.3 name\n");
            return 0;
        }
        fs_find_entry(parent.data_cluster, name, &existing, &entry_cluster, &entry_index);
    }

    needed = (len + FAT32_CLUSTER_SIZE - 1) / FAT32_CLUSTER_SIZE;
    if (needed > fs_free_cluster_count() + fs_chain_cluster_count(fs_entry_cluster(&existing))) {
        kprint("write: disk full\n");
        return 0;
    }

    if (fs_entry_cluster(&existing)) fs_free_chain(fs_entry_cluster(&existing));
    for (uint32_t i = 0; i < needed; i++) {
        uint32_t next = fs_alloc_cluster();
        if (!next) return 0;
        if (!first) first = next;
        if (current) fs_fat[current] = next;
        current = next;
    }

    current = first;
    for (uint32_t offset = 0; offset < len && fs_valid_cluster(current); offset += FAT32_CLUSTER_SIZE) {
        uint32_t chunk = len - offset;
        if (chunk > FAT32_CLUSTER_SIZE) chunk = FAT32_CLUSTER_SIZE;
        fs_memcpy(fs_data[current], text + offset, chunk);
        current = fs_fat[current];
    }

    existing = *fs_dir_entry(entry_cluster, entry_index);
    fs_set_entry_cluster(&existing, first);
    existing.file_size = len;
    *fs_dir_entry(entry_cluster, entry_index) = existing;
    return 1;
}

static void fs_cmd_touch(const char* args) {
    args = fs_skip_spaces(args);
    if (!*args) {
        kprint("Usage: touch FILE\n");
        return;
    }
    fs_write_file(args, "");
}

static void fs_cmd_write(const char* args) {
    const char* text;
    char path[FS_MAX_PATH];
    uint32_t pos = 0;

    args = fs_skip_spaces(args);
    if (!*args) {
        kprint("Usage: write FILE TEXT\n");
        return;
    }
    while (*args && *args != ' ') {
        if (pos >= FS_MAX_PATH - 1) {
            kprint("write: path too long\n");
            return;
        }
        path[pos++] = *args++;
    }
    path[pos] = 0;
    text = fs_skip_spaces(args);
    fs_write_file(path, text);
}

static void fs_cmd_cat(const char* args) {
    FsNode node;
    uint32_t cluster;
    uint32_t remaining;

    args = fs_skip_spaces(args);
    if (!*args) {
        kprint("Usage: cat FILE\n");
        return;
    }
    if (!fs_resolve_path(args, &node) || (node.entry.attr & FAT32_ATTR_DIRECTORY)) {
        kprint("cat: file not found\n");
        return;
    }

    cluster = node.data_cluster;
    remaining = node.entry.file_size;
    while (remaining > 0 && fs_valid_cluster(cluster)) {
        uint32_t chunk = remaining;
        if (chunk > FAT32_CLUSTER_SIZE) chunk = FAT32_CLUSTER_SIZE;
        for (uint32_t i = 0; i < chunk; i++) {
            char s[2] = {(char)fs_data[cluster][i], 0};
            kprint(s);
        }
        remaining -= chunk;
        cluster = fs_fat[cluster];
    }
    kprint("\n");
}

static void fs_cmd_rm(const char* args, int want_dir) {
    FsNode node;
    Fat32DirEntry* entry;

    args = fs_skip_spaces(args);
    if (!*args) {
        kprint(want_dir ? "Usage: rmdir DIR\n" : "Usage: rm FILE\n");
        return;
    }
    if (!fs_resolve_path(args, &node) || node.is_root) {
        kprint(want_dir ? "rmdir: directory not found\n" : "rm: file not found\n");
        return;
    }
    if (want_dir) {
        if (!(node.entry.attr & FAT32_ATTR_DIRECTORY)) {
            kprint("rmdir: not a directory\n");
            return;
        }
        if (!fs_dir_is_empty(node.data_cluster)) {
            kprint("rmdir: directory is not empty\n");
            return;
        }
    } else if (node.entry.attr & FAT32_ATTR_DIRECTORY) {
        kprint("rm: is a directory\n");
        return;
    }

    if (node.data_cluster) fs_free_chain(node.data_cluster);
    entry = fs_dir_entry(node.entry_cluster, node.entry_index);
    entry->name[0] = (char)0xE5;
}

static void fs_cmd_stat(const char* args) {
    FsNode node;
    char name[FS_MAX_NAME];

    args = fs_skip_spaces(args);
    if (!*args) args = ".";
    if (!fs_resolve_path(args, &node)) {
        kprint("stat: path not found\n");
        return;
    }
    if (node.is_root) fs_strcpy(name, "/");
    else fs_name_from_83(node.entry.name, name);
    kprint("Name: ");
    kprint(name);
    kprint("\nType: ");
    kprint((node.entry.attr & FAT32_ATTR_DIRECTORY) ? "directory" : "file");
    kprint("\nCluster: ");
    fs_print_dec(node.data_cluster);
    kprint("\nSize: ");
    fs_print_dec(node.entry.file_size);
    kprint(" bytes\n");
}

static void fs_cmd_df(void) {
    uint32_t free_clusters = fs_free_cluster_count();
    uint32_t used_clusters = FAT32_CLUSTER_COUNT - FAT32_ROOT_CLUSTER - free_clusters;
    kprint("FAT32 RAM disk\n");
    kprint("Cluster size: ");
    fs_print_dec(FAT32_CLUSTER_SIZE);
    kprint(" bytes\nUsed: ");
    fs_print_dec(used_clusters * FAT32_CLUSTER_SIZE);
    kprint(" bytes\nFree: ");
    fs_print_dec(free_clusters * FAT32_CLUSTER_SIZE);
    kprint(" bytes\nRoot cluster: ");
    fs_print_dec(fs_info.root_cluster);
    kprint("\n");
}

static const char* fs_get_cwd(void) {
    return fs_cwd_path;
}

static const VfsMount* vfs_root_mount(void) {
    fs_init();
    if (vfs_mount_count <= 0 || !vfs_mounts[0].mounted) return 0;
    return &vfs_mounts[0];
}

static const VfsOps* vfs_root_ops(void) {
    const VfsMount* mount = vfs_root_mount();
    if (!mount) return 0;
    return mount->ops;
}

static void vfs_print_mounts(void) {
    fs_init();
    kprint("VFS mounts:\n");
    for (int i = 0; i < vfs_mount_count; i++) {
        if (!vfs_mounts[i].mounted || !vfs_mounts[i].ops) continue;
        kprint("  ");
        kprint(vfs_mounts[i].path);
        kprint(" -> ");
        kprint(vfs_mounts[i].ops->name);
        kprint(" (");
        kprint(vfs_mounts[i].ops->type);
        kprint(")\n");
    }
}

int fs_desktop_list(FsDesktopEntry* entries, int max_entries) {
    const VfsOps* ops;
    fs_init();
    ops = vfs_root_ops();
    if (!ops || !ops->desktop_list) return 0;
    return ops->desktop_list(entries, max_entries);
}

int fs_desktop_create_text(const char* name, const char* text) {
    fs_init();
    if (!name || !*name) name = "NEW.TXT";
    if (!text) text = "";
    return fs_write_file(name, text);
}

int fs_handle_command(const char* cmd) {
    const VfsOps* ops;
    fs_init();
    ops = vfs_root_ops();
    if (!ops) {
        kprint("vfs: no root filesystem mounted\n");
        return 0;
    }

    if (fs_streq(cmd, "format")) {
        ops->format();
        kprint("Formatted root filesystem.\n");
        return 1;
    }
    if (fs_streq(cmd, "mount") || fs_streq(cmd, "mounts") || fs_streq(cmd, "vfs")) {
        vfs_print_mounts();
        return 1;
    }
    if (fs_streq(cmd, "pwd")) {
        kprint(ops->cwd());
        kprint("\n");
        return 1;
    }
    if (fs_streq(cmd, "df") || fs_streq(cmd, "checkfs")) {
        ops->cmd_df();
        return 1;
    }
    if (fs_streq(cmd, "ls") || fs_starts(cmd, "ls ")) {
        ops->cmd_ls(cmd + 2);
        return 1;
    }
    if (fs_streq(cmd, "cd") || fs_starts(cmd, "cd ")) {
        ops->cmd_cd(cmd + 2);
        return 1;
    }
    if (fs_starts(cmd, "mkdir ")) {
        ops->cmd_mkdir(cmd + 6);
        return 1;
    }
    if (fs_starts(cmd, "touch ")) {
        ops->cmd_touch(cmd + 6);
        return 1;
    }
    if (fs_starts(cmd, "write ")) {
        ops->cmd_write(cmd + 6);
        return 1;
    }
    if (fs_starts(cmd, "cat ")) {
        ops->cmd_cat(cmd + 4);
        return 1;
    }
    if (fs_starts(cmd, "rm ")) {
        ops->cmd_rm(cmd + 3, 0);
        return 1;
    }
    if (fs_starts(cmd, "rmdir ")) {
        ops->cmd_rm(cmd + 6, 1);
        return 1;
    }
    if (fs_streq(cmd, "stat") || fs_starts(cmd, "stat ")) {
        ops->cmd_stat(cmd + 4);
        return 1;
    }
    return 0;
}
