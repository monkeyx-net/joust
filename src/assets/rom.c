#include "assets/rom.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "assets/zip.h"

/* MAME `joust` (0.289): name, CRC32. Indexes 0-11 are program banks 1-12. */
#define ROM_FILES 13
#define SOUND_INDEX 12
static const struct { const char *mame; uint32_t crc; } files[ROM_FILES] = {
    { "joust_rom_1b_3006-13.e4", 0xfe41b2af },
    { "joust_rom_2b_3006-14.c4", 0x501c143c },
    { "joust_rom_3b_3006-15.a4", 0x43f7161d },
    { "joust_rom_4b_3006-16.e5", 0xdb5571b6 },
    { "joust_rom_5b_3006-17.c5", 0xc686bb6b },
    { "joust_rom_6b_3006-18.a5", 0xfac5f2cf },
    { "joust_rom_7b_3006-19.e6", 0x81418240 },
    { "joust_rom_8b_3006-20.c6", 0xba5359ba },
    { "joust_rom_9b_3006-21.a6", 0x39643147 },
    { "joust_rom_10b_3006-22.a7", 0x3f1c4f89 },
    { "joust_rom_11b_3006-23.c7", 0xea48b359 },
    { "joust_rom_12b_3006-24.e7", 0xc710717b },
    { "video_sound_rom_4_std_780.ic12", 0xf1835bdd },
};

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return c ^ 0xFFFFFFFFu;
}

static uint8_t *slot(RomSet *rom, int i)
{
    return i == SOUND_INDEX ? rom->sound : rom->prog[i];
}

/* Every 4 KB file in a folder, kept where its CRC matches a set file. */
static void scan_folder(RomSet *rom, const char *dir, int *found)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    uint8_t buf[ROM_SIZE];
    while ((e = readdir(d))) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size != ROM_SIZE) continue;
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        size_t got = fread(buf, 1, ROM_SIZE, f);
        fclose(f);
        if (got != ROM_SIZE) continue;
        uint32_t crc = crc32(buf, ROM_SIZE);
        for (int i = 0; i < ROM_FILES; i++)
            if (!found[i] && crc == files[i].crc) {
                memcpy(slot(rom, i), buf, ROM_SIZE);
                found[i] = 1;
            }
    }
    closedir(d);
}

int rom_load(RomSet *rom, const char *src, char *err, int errlen)
{
    memset(rom, 0, sizeof *rom);
    struct stat st;
    if (stat(src, &st) != 0) {
        snprintf(err, (size_t)errlen, "%s: not found", src);
        return -1;
    }
    int found[ROM_FILES] = { 0 };
    if (S_ISREG(st.st_mode)) {
        for (int i = 0; i < ROM_FILES; i++) {
            uint8_t *data;
            size_t len;
            int rc = zip_read_crc(src, files[i].crc, &data, &len, err, errlen);
            if (rc == -1) return -1;
            if (rc == 0) {
                if (len == ROM_SIZE) {
                    memcpy(slot(rom, i), data, ROM_SIZE);
                    found[i] = 1;
                }
                free(data);
            }
        }
    } else {
        scan_folder(rom, src, found);
    }
    for (int i = 0; i < ROM_FILES; i++)
        if (!found[i]) {
            snprintf(err, (size_t)errlen, "%s: missing %s (CRC %08x); the MAME 'joust' set is required", src,
                     files[i].mame, (unsigned)files[i].crc);
            return -1;
        }
    rom->loaded = 1;
    return 0;
}

int rom_byte(const RomSet *rom, unsigned addr)
{
    if (!rom->loaded) return -1;
    if (addr < 0x9000) return rom->prog[addr >> 12][addr & 0xFFF];
    if (addr >= 0xD000 && addr <= 0xFFFF) return rom->prog[9 + ((addr - 0xD000) >> 12)][addr & 0xFFF];
    return -1;
}

/* want_dir: a folder; else a regular file */
static int exists(const char *path, int want_dir)
{
    struct stat st;
    return stat(path, &st) == 0 && (want_dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
}

int rom_locate(const char *argv0, char *out, size_t outlen)
{
    const char *env = getenv("JOUST_ROM");
    if (env && *env && (exists(env, 0) || exists(env, 1))) {
        snprintf(out, outlen, "%s", env);
        return 0;
    }
    char exe_dir[900] = ".";
    if (argv0) {
        snprintf(exe_dir, sizeof exe_dir, "%s", argv0);
        char *slash = strrchr(exe_dir, '/');
#ifdef _WIN32
        char *bslash = strrchr(exe_dir, '\\');
        if (bslash > slash) slash = bslash;
#endif
        if (slash) *slash = '\0';
        else snprintf(exe_dir, sizeof exe_dir, ".");
    }
    const char *const dirs[2] = { exe_dir, "." };
    /* the folder must be a folder: the program itself is called joust too */
    const char *const names[2] = { "joust.zip", "joust" };
    for (int d = 0; d < 2; d++)
        for (int n = 0; n < 2; n++) {
            snprintf(out, outlen, "%s/%s", dirs[d], names[n]);
            if (exists(out, n == 1)) return 0;
        }
#ifdef JOUST_SOURCE_DIR
    for (int n = 0; n < 2; n++) { /* development builds: the source tree's roms/ */
        snprintf(out, outlen, "%s/roms/%s", JOUST_SOURCE_DIR, names[n]);
        if (exists(out, n == 1)) return 0;
    }
#endif
    out[0] = '\0';
    return -1;
}
