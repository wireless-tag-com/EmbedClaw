/**
 * @file ec_skill_loader.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "ec_skill_loader.h"
#include "ec_config_internal.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static const char *extract_title(const char *line, size_t len, char *out, size_t out_size);
static void extract_description(FILE *f, char *out, size_t out_size);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "skills";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_skill_loader_init(void)
{
    ESP_LOGI(TAG, "Skills system ready (skills loaded from SPIFFS)");
    return ESP_OK;
}

size_t ec_skill_loader_build_summary(char *buf, size_t size)
{
    DIR *dir = opendir(EC_FS_BASE);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open SPIFFS for skill enumeration");
        buf[0] = '\0';
        return 0;
    }

    size_t off = 0;
    struct dirent *ent;
    /* SPIFFS readdir returns filenames relative to the mount point (e.g. "skills/weather.md").
       We match entries that start with "skills/" and end with ".md". */
    const char *skills_subdir = "skills/";
    const size_t subdir_len = strlen(skills_subdir);

    while ((ent = readdir(dir)) != NULL && off < size - 1) {
        const char *name = ent->d_name;

        /* Match files under skills/ with .md extension */
        if (strncmp(name, skills_subdir, subdir_len) != 0) continue;

        size_t name_len = strlen(name);
        if (name_len < subdir_len + 4) continue;  /* at least "skills/x.md" */
        if (strcmp(name + name_len - 3, ".md") != 0) continue;

        /* Build full path */
        char full_path[296];
        snprintf(full_path, sizeof(full_path), "%s/%s", EC_FS_BASE, name);

        FILE *f = fopen(full_path, "r");
        if (!f) continue;

        /* Read first line for title */
        char first_line[128];
        if (!fgets(first_line, sizeof(first_line), f)) {
            fclose(f);
            continue;
        }

        char title[64];
        extract_title(first_line, strlen(first_line), title, sizeof(title));

        /* Read description (until blank line) */
        char desc[256];
        extract_description(f, desc, sizeof(desc));
        fclose(f);

        /* Append to summary */
        off += snprintf(buf + off, size - off,
            "- **%s**: %s (read with: read_file %s)\n",
            title, desc, full_path);
    }

    closedir(dir);

    buf[off] = '\0';
    ESP_LOGI(TAG, "Skills summary: %d bytes", (int)off);
    return off;
}


/* ==================== [Static Functions] ================================== */

/**
 * Parse first line as title: expects "# Title"
 * Returns pointer past "# " or the line itself if no prefix.
 */
static const char *extract_title(const char *line, size_t len, char *out, size_t out_size)
{
    const char *start = line;
    if (len >= 2 && line[0] == '#' && line[1] == ' ') {
        start = line + 2;
        len -= 2;
    }

    /* Trim trailing whitespace/newline */
    while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r' || start[len - 1] == ' ')) {
        len--;
    }

    size_t copy = len < out_size - 1 ? len : out_size - 1;
    memcpy(out, start, copy);
    out[copy] = '\0';
    return out;
}

/**
 * Extract description: text between the first line and the first blank line.
 */
static void extract_description(FILE *f, char *out, size_t out_size)
{
    size_t off = 0;
    char line[256];

    while (fgets(line, sizeof(line), f) && off < out_size - 1) {
        size_t len = strlen(line);

        if (len == 0) {
            continue;
        }

        /* Skip blank lines between the title and the description body. */
        if (off == 0 && line[0] == '\n') {
            continue;
        }

        /* Stop at blank line or section header */
        if (len == 0 || (len == 1 && line[0] == '\n') ||
            (len >= 2 && line[0] == '#' && line[1] == '#')) {
            break;
        }

        /* Trim trailing newline for concatenation */
        if (line[len - 1] == '\n') {
            line[len - 1] = ' ';
        }

        size_t copy = len < out_size - off - 1 ? len : out_size - off - 1;
        memcpy(out + off, line, copy);
        off += copy;
    }

    /* Trim trailing space */
    while (off > 0 && out[off - 1] == ' ') off--;
    out[off] = '\0';
}
