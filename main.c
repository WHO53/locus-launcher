#include "proto/wlr-layer-shell-unstable-v1-client-protocol.h"
#include <locus.h>
#include <cairo.h>
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <librsvg/rsvg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

Locus app;

#define APP_ICON_SIZE ((int)(app.width * 0.125))
#define APP_PADDING ((int)(app.width * 0.0625))
#define APP_TEXT_HEIGHT ((int)(app.height * 0.012))
#define APPS_PER_ROW (app.width / (APP_ICON_SIZE + APP_PADDING))

typedef struct {
    char name[1024];
    char icon[1024];
    char exec[1024];
} App;

App *apps = NULL;
int app_count = 0;
int app_capacity = 10;

int starts_with(const char *line, const char *key) {
    return strncmp(line, key, strlen(key)) == 0;
}

char *trim_whitespace(char *str) {
    char *end;
    while (*str == ' ') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = '\0';
    return str;
}

void add_app(const char *name, const char *icon_name, const char *exec_cmd) {
    if (app_count >= app_capacity) {
        app_capacity *= 2;
        apps = realloc(apps, app_capacity * sizeof(App));
        if (!apps) {
            perror("Failed to reallocate memory for apps");
            exit(EXIT_FAILURE);
        }
    }
    strncpy(apps[app_count].name, name, sizeof(apps[app_count].name));
    strncpy(apps[app_count].icon, icon_name, sizeof(apps[app_count].icon));
    strncpy(apps[app_count].exec, exec_cmd, sizeof(apps[app_count].exec));
    app_count++;
}

void process_desktop_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char line[1024];
    char app_name[1024] = "";
    char icon_name[1024] = "";
    char exec_name[1024] = "";
    int no_display = 0;
    int name_found = 0;
    int icon_found = 0;
    int exec_found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (starts_with(line, "NoDisplay=") && strstr(line, "true")) {
            no_display = 1;
            break;
        }
        if (!name_found && starts_with(line, "Name=")) {
            strncpy(app_name, trim_whitespace(line + 5), sizeof(app_name));
            name_found = 1;
        }
        if (!icon_found && starts_with(line, "Icon=")) {
            strncpy(icon_name, trim_whitespace(line + 5), sizeof(icon_name));
            icon_found = 1;
        }
        if (!exec_found && starts_with(line, "Exec=")) {
            strncpy(exec_name, trim_whitespace(line + 5), sizeof(exec_name));
            char *percent_u = strstr(exec_name, "%U");
            if (percent_u) {
                *percent_u = '\0';
            }
            exec_found = 1;
        }
    }

    fclose(file);

    if (!no_display && strlen(app_name) > 0 && strlen(icon_name) > 0 && strlen(exec_name) > 0) {
        add_app(app_name, icon_name, exec_name);
    }
}

void process_desktop_directory(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    char filepath[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".desktop")) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            process_desktop_file(filepath);
        }
    }

    closedir(dir);
}

int file_exists(const char *path) {
    FILE *file = fopen(path, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

char *find_icon(const char *icon_name) {
    static char path[1024];
    const char *icon_dirs[] = {
        "/home/droidian/temp/Fluent-grey/scalable/apps/", // temp
        "/home/droidian/temp/candy-icons/apps/scalable/", // for fish
        "/usr/share/icons/hicolor/scalable/apps/",
        "/usr/share/pixmaps/",
        NULL
    };
    
    for (int i = 0; icon_dirs[i] != NULL; ++i) {
        snprintf(path, sizeof(path), "%s%s.png", icon_dirs[i], icon_name);
        if (file_exists(path)) return path;
        snprintf(path, sizeof(path), "%s%s.svg", icon_dirs[i], icon_name);
        if (file_exists(path)) return path;
        snprintf(path, sizeof(path), "%s%s-symbolic.svg", icon_dirs[i], icon_name);
        if (file_exists(path)) return path;
    }

    return NULL;
}

void draw_icon_with_label(cairo_t *cr, int x, int y, const char *name, const char *icon_name) {
    char *icon_path = find_icon(icon_name);
    cairo_surface_t *icon_surface = NULL;

    if (icon_path != NULL) {
        if (strstr(icon_path, ".svg")) {
            RsvgHandle *svg = rsvg_handle_new_from_file(icon_path, NULL);
            RsvgRectangle viewport = { 0, 0, APP_ICON_SIZE, APP_ICON_SIZE };
            if (svg) {
                cairo_save(cr);
                cairo_translate(cr, x, y);
                rsvg_handle_render_document(svg, cr, &viewport, NULL);
                cairo_restore(cr);
                g_object_unref(svg);
            }
        } else {
            icon_surface = cairo_image_surface_create_from_png(icon_path);
            if (cairo_surface_status(icon_surface) == CAIRO_STATUS_SUCCESS) {
                cairo_set_source_surface(cr, icon_surface, x, y);
                cairo_paint(cr);
                cairo_surface_destroy(icon_surface);
            }
        }
    }

    if (!icon_surface && !icon_path) {
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8);
        cairo_rectangle(cr, x, y, APP_ICON_SIZE, APP_ICON_SIZE);
        cairo_fill(cr);
    }

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, APP_TEXT_HEIGHT);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, name, &extents);

    cairo_move_to(cr, x + (APP_ICON_SIZE - extents.width) / 2, y + APP_ICON_SIZE + APP_TEXT_HEIGHT);
    cairo_show_text(cr, name);
}

int compare_apps(const void *a, const void *b) {
    const App *appA = (const App *)a;
    const App *appB = (const App *)b;
    return strcasecmp(appA->name, appB->name);
}

void launch_app(const char *exec) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"/bin/sh", "-c", (char *)exec, NULL};
        execvp(args[0], args);
        perror("Failed to execute application");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("Failed to fork");
    } 
}

void touch(int32_t id, double x, double y, int state) {
    if (state == 0) {
        for (int i = 0; i < app_count; ++i) {
            int app_x = (i % APPS_PER_ROW) * (APP_ICON_SIZE + APP_PADDING) + APP_PADDING;
            int app_y = (i / APPS_PER_ROW) * (APP_ICON_SIZE + APP_TEXT_HEIGHT + APP_PADDING) + APP_PADDING;

            if (x >= app_x && x <= app_x + APP_ICON_SIZE && y >= app_y && y <= app_y + APP_ICON_SIZE) {
                launch_app(apps[i].exec);
                break;
            }
        }
    }
}

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
    apps = malloc(app_capacity * sizeof(App));
    if (!apps) {
        perror("Failed to allocate memory for apps");
        return EXIT_FAILURE;
    }

    locus_init(&app, 100, 100);
    locus_create_layer_surface(&app, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, 0, 0);
    locus_set_draw_callback(&app, draw);
    locus_set_touch_callback(&app, touch);
    process_desktop_directory("/usr/share/applications");
    qsort(apps, app_count, sizeof(App), compare_apps);
    locus_run(&app);

    free(apps);
    return 0;
}
