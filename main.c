#include <locus.h>
#include <locus-ui.h>
#include "main.h"
#include <nanovg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <librsvg/rsvg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

Locus launcher;
LocusUI ui;

#define APP_TEXT_HEIGHT ((int)(launcher.height * 0.018))

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

void draw_icon_with_label(int x, int y, const char *name, const char *icon_name) {
    locus_icon(&ui, icon_name, x, y, app_icon_size);

    char display_name[14]; 
    if (strlen(name) > 11) {
        strncpy(display_name, name, 11);
        display_name[11] = '.'; 
        display_name[12] = '.'; 
        display_name[13] = '\0'; 
    } else {
        strncpy(display_name, name, sizeof(display_name));
    }
    
    float textBounds[4];  
    nvgTextBounds(ui.vg, x, y, display_name, NULL, textBounds);
    float textWidth = textBounds[2] - textBounds[0];
    float textX = x - (textWidth / 2) + (app_icon_size * 0.50);

    locus_text(&ui, display_name, textX, y + app_icon_size + APP_TEXT_HEIGHT, APP_TEXT_HEIGHT, 255, 255, 255, 1);
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

void touch(int32_t id, double x, double y, int32_t state) {
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

void draw( void *data) {
    nvgBeginFrame(ui.vg, launcher.width, launcher.height, 1.0f);
    adjust_icon_size_and_padding();

    const char *home_dir = getenv("HOME");
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/locus/wallpaper.png", home_dir);
    
    locus_image(&ui, path, 0, 0, launcher.width, launcher.height);

    int apps_per_row = calculate_apps_per_row(app_icon_size, app_padding);
    int total_width_used = apps_per_row * (app_icon_size + app_padding) - app_padding;

    int extra_padding = (launcher.width - total_width_used) / 2;

    int x, y;
    for (int i = 0; i < app_count; ++i) {
        x = (i % apps_per_row) * (app_icon_size + app_padding) + extra_padding;
        y = (i / apps_per_row) * (app_icon_size + APP_TEXT_HEIGHT + app_padding) + app_padding;

        draw_icon_with_label(x, y, apps[i].name, apps[i].icon);
    }
    nvgEndFrame(ui.vg);
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
    locus_setup_ui(&ui);
    locus_run(&launcher);

    free(apps);
    locus_cleanup_ui(&ui);
    locus_cleanup(&launcher);
    return 0;
}
