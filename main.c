#include "proto/wlr-layer-shell-unstable-v1-client-protocol.h"
#include <locus.h>
#include <cairo.h>
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

Locus app;

#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define NUM_APPS 20
#define APPS_PER_ROW 5
#define APP_ICON_SIZE 100
#define APP_PADDING 50
#define APP_TEXT_HEIGHT 20

// Struct to hold app details
typedef struct {
    char name[1024];
    char icon[1024];
} App;

// Array to hold the processed apps
App apps[NUM_APPS];
int app_count = 0;

// Function to check if a line starts with a specific key
int starts_with(const char *line, const char *key) {
    return strncmp(line, key, strlen(key)) == 0;
}

// Function to trim leading and trailing whitespace
char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (*str == ' ') str++;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\n' || *end == '\r')) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

// Function to process a .desktop file and extract Name and Icon
void process_desktop_file(const char *filepath) {
    if (app_count >= NUM_APPS) return;  // Stop processing if we have enough apps

    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char line[1024];
    char app_name[1024] = "";
    char icon_name[1024] = "";
    int no_display = 0;
    int name_found = 0;

    // Read file line by line
    while (fgets(line, sizeof(line), file)) {
        if (starts_with(line, "NoDisplay=") && strstr(line, "true")) {
            no_display = 1;
            break;
        }
        if (!name_found && starts_with(line, "Name=")) {
            strncpy(app_name, trim_whitespace(line + 5), sizeof(app_name));
            name_found = 1;
        }
        if (starts_with(line, "Icon=")) {
            strncpy(icon_name, trim_whitespace(line + 5), sizeof(icon_name));
        }
    }

    fclose(file);

    if (!no_display && strlen(app_name) > 0 && strlen(icon_name) > 0) {
        strncpy(apps[app_count].name, app_name, sizeof(apps[app_count].name));
        strncpy(apps[app_count].icon, icon_name, sizeof(apps[app_count].icon));
        app_count++;
    }
}

// Function to read all .desktop files from a directory and process them
void process_desktop_directory(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    char filepath[1024];

    while ((entry = readdir(dir)) != NULL && app_count < NUM_APPS) {
        if (strstr(entry->d_name, ".desktop")) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            process_desktop_file(filepath);
        }
    }

    closedir(dir);
}

// Function to draw an app icon and label using Cairo
void draw_icon_with_label(cairo_t *cr, int x, int y, const char *name, const char *icon_path) {
    cairo_surface_t *icon_surface = cairo_image_surface_create_from_png(icon_path);

    if (cairo_surface_status(icon_surface) == CAIRO_STATUS_SUCCESS) {
        // Draw the actual icon
        cairo_set_source_surface(cr, icon_surface, x, y);
        cairo_paint(cr);
    } else {
        // Fallback: draw a rectangle if the icon can't be loaded
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8);
        cairo_rectangle(cr, x, y, APP_ICON_SIZE, APP_ICON_SIZE);
        cairo_fill(cr);
    }

    cairo_surface_destroy(icon_surface);

    // Draw the app name
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, name, &extents);

    cairo_move_to(cr, x + (APP_ICON_SIZE - extents.width) / 2, y + APP_ICON_SIZE + APP_TEXT_HEIGHT);
    cairo_show_text(cr, name);
}

// Function to draw the app grid
void draw(cairo_t *cr, int width, int height) {
    cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 1);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    int x, y;
    for (int i = 0; i < app_count; ++i) {
        x = (i % APPS_PER_ROW) * (APP_ICON_SIZE + APP_PADDING) + APP_PADDING;
        y = (i / APPS_PER_ROW) * (APP_ICON_SIZE + APP_TEXT_HEIGHT + APP_PADDING) + APP_PADDING;

        draw_icon_with_label(cr, x, y, apps[i].name, apps[i].icon);
    }
}

int main() {
    locus_init(&app, 100, 100);
    locus_create_layer_surface(&app, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, 0, 0);
    locus_set_draw_callback(&app, draw);
    process_desktop_directory("/usr/share/applications");
    locus_run(&app);
    return 0;
}
