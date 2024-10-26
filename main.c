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

Locus launcher;

#define APP_TEXT_HEIGHT ((int)(launcher.height * 0.013))

typedef struct {
    char name[1024];
    char icon[1024];
    char exec[1024];
} App;

App *apps = NULL;
int app_count = 0;
int app_capacity = 10;
int app_icon_size;
int app_padding;

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

void process_desktop_directory() {
    const char *homeDir = getenv("HOME");
    char userDir[512];
    snprintf(userDir, sizeof(userDir), "%s/.local/share/applications", homeDir);
    const char *dirs[] = {
        "/usr/share/applications",
        userDir
    };

    char filepath[1024];
    DIR *dir;
    struct dirent *entry;

    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        dir = opendir(dirs[i]);
        if (!dir) {
            perror("Failed to open directory");
            continue;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".desktop")) {
                snprintf(filepath, sizeof(filepath), "%s/%s", dirs[i], entry->d_name);
                process_desktop_file(filepath);
            }
        }

        closedir(dir);
    }
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
        "/usr/share/icons/hicolor/scalable/apps/",
        "/usr/share/icons/hicolor/128x128/apps/",
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
            RsvgRectangle viewport = { 0, 0, app_icon_size, app_icon_size };
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
                double icon_width = cairo_image_surface_get_width(icon_surface);
                double icon_height = cairo_image_surface_get_height(icon_surface);
                cairo_save(cr);
                cairo_translate(cr, x, y);
                cairo_scale(cr, (double)app_icon_size / icon_width, (double)app_icon_size / icon_height);
                cairo_set_source_surface(cr, icon_surface, 0, 0);
                cairo_paint(cr);
                cairo_restore(cr);
                cairo_surface_destroy(icon_surface);
                }
        }
    }

    if (!icon_surface && !icon_path) {
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8);
        cairo_rectangle(cr, x, y, app_icon_size, app_icon_size);
        cairo_fill(cr);
    }

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, APP_TEXT_HEIGHT);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, name, &extents);

    cairo_move_to(cr, x + (app_icon_size - extents.width) / 2, y + app_icon_size + APP_TEXT_HEIGHT);
    cairo_show_text(cr, name);
}

int compare_apps(const void *a, const void *b) {
    const App *appA = (const App *)a;
    const App *appB = (const App *)b;
    return strcasecmp(appA->name, appB->name);
}

void launch_app(const char *exec) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        return;
    }

    if (pid == 0) {
        char *args[512];
        char *token;
        int i = 0;

        char *exec_copy = strdup(exec);
        if (exec_copy == NULL) {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }

        token = strtok(exec_copy, " ");
        while (token != NULL && i < 255) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        execvp(args[0], args);
        perror("Failed to execute application");
        free(exec_copy);
        exit(EXIT_FAILURE);
    }
}
int calculate_apps_per_row(int icon_size, int padding) {
    return launcher.width / (icon_size + padding);
}

int calculate_total_rows(int apps_per_row) {
    return (app_count + apps_per_row - 1) / apps_per_row;
}

void touch(int32_t id, double x, double y, int state) {
    if (state == 0) {
        int apps_per_row = calculate_apps_per_row(app_icon_size, app_padding);
        int total_width_used = apps_per_row * (app_icon_size + app_padding) - app_padding;
        int extra_padding = (launcher.width - total_width_used) / 2;

        for (int i = 0; i < app_count; ++i) {
            int app_x = (i % apps_per_row) * (app_icon_size + app_padding) + extra_padding;
            int app_y = (i / apps_per_row) * (app_icon_size + APP_TEXT_HEIGHT + app_padding) + app_padding;

            if (x >= app_x && x <= app_x + app_icon_size && y >= app_y && y <= app_y + app_icon_size) {
                launch_app(apps[i].exec);
                break;
            }
        }
    }
}

void adjust_icon_size_and_padding() {
    app_icon_size = (int)(launcher.width * 0.150);
    app_padding = (int)(launcher.width * 0.0620);
    
    int apps_per_row = launcher.width / (app_icon_size + app_padding);
    int total_rows = (app_count + apps_per_row - 1) / apps_per_row; 
    
    while ((total_rows * (app_icon_size + APP_TEXT_HEIGHT + app_padding) + app_padding) > launcher.height) {
        app_icon_size *= 0.9;
        app_padding *= 0.9;
        apps_per_row = calculate_apps_per_row(app_icon_size, app_padding);
        total_rows = calculate_total_rows(apps_per_row);
        
        if (app_icon_size < 40 || app_padding < 10) {
            break;
        }
    }
}

void draw(cairo_t *cr, int width, int height) {
    adjust_icon_size_and_padding();

    const char *home_dir = getenv("HOME");
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/locus/wallpaper.png", home_dir);
    
    cairo_surface_t *image = cairo_image_surface_create_from_png(path);

    if (cairo_surface_status(image) == CAIRO_STATUS_SUCCESS) {
        int img_width = cairo_image_surface_get_width(image);
        int img_height = cairo_image_surface_get_height(image);

        double scale_x = (double)width / img_width;
        double scale_y = (double)height / img_height;
        double scale = (scale_x > scale_y) ? scale_x : scale_y;

        cairo_save(cr);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, (width / 2.0 / scale) - (img_width / 2.0), 
                             (height / 2.0 / scale) - (img_height / 2.0));
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_rectangle(cr, 0, 0, img_width, img_height);
        cairo_fill(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 1);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
    }
    cairo_surface_destroy(image);

    int apps_per_row = calculate_apps_per_row(app_icon_size, app_padding);
    int total_width_used = apps_per_row * (app_icon_size + app_padding) - app_padding;

    int extra_padding = (launcher.width - total_width_used) / 2;

    int x, y;
    for (int i = 0; i < app_count; ++i) {
        x = (i % apps_per_row) * (app_icon_size + app_padding) + extra_padding;
        y = (i / apps_per_row) * (app_icon_size + APP_TEXT_HEIGHT + app_padding) + app_padding;

        draw_icon_with_label(cr, x, y, apps[i].name, apps[i].icon);
    }
}

int main() {
    apps = malloc(app_capacity * sizeof(App));
    if (!apps) {
        perror("Failed to allocate memory for apps");
        return EXIT_FAILURE;
    }

    locus_init(&launcher, 100, 100);
    locus_create_layer_surface(&launcher, "locus-launcher", ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, 0, 0);
    locus_set_draw_callback(&launcher, draw);
    locus_set_touch_callback(&launcher, touch);
    process_desktop_directory();
    qsort(apps, app_count, sizeof(App), compare_apps);
    locus_run(&launcher);

    free(apps);
    return 0;
}
