/*
 * actkbd-nuklear - A gui for actkbd
 *
 * Copyright (c) 2021-2022 Astolfo@gmx.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/* nuklear - public domain */
#include <GL/glew.h>

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include "Nuklear/nuklear.h"
#include "Nuklear/demo/glfw_opengl3/nuklear_glfw_gl3.h"

#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

static char* default_config = "/etc/actkbd.conf";

typedef struct {
    char** const buffer;
    int read;
    int write;
    const int maxlen;
} circ_bbuf_t;

void circ_bbuf_push (circ_bbuf_t *c, char* data) {
    
    c->buffer[c->write] = data;
    c->write = (c->write + 1) % c->maxlen;
    if (c->write == c->read)
        c->read = (c->read + 1) % c->maxlen;
    return;
}

char* circ_bbuf_pop (circ_bbuf_t* c) {
    char** p = NULL;
    if (c->read != c->write) {
        p = c->buffer + c->read;
        c->read = (c->read + 1) % c->maxlen;
        return *p;
    } else {
        return NULL;
    }
}

bool circ_bbuf_is_empty(circ_bbuf_t* c) {
    return c->read == c->write;
}

bool circ_bbuf_is_full (circ_bbuf_t* c) {
    return (c->write + 1) % c->maxlen == c->read;
}

#define CIRC_BBUF_DEF(x,y)                \
char* x##_data_space[y];              \
circ_bbuf_t x = {                     \
.buffer = x##_data_space,         \
.read = 0,                        \
.write = 0,                       \
.maxlen = y + 1                   \
}

struct media {
    struct nk_font *font_14;
    struct nk_font *font_18;
    struct nk_font *font_20;
    struct nk_font *font_22;
    
    struct nk_image unchecked;
    struct nk_image checked;
};

struct actkbd_options {
    char* config_file;
    char* device;
    int grab;
    int quiet;
    int show_exec;
    int show_key;
    int system_log;
    int permanent;
};

void actkbd_options_init(struct actkbd_options* options) {
    options->config_file = default_config;
    options->device = NULL;
    options->grab = nk_false;
    options->quiet = nk_false;
    options->show_exec = nk_false;
    options->show_key = nk_false;
    options->system_log = nk_false;
}

struct combobox_dirlist {
    char items[64][256];
    DIR* d;
    int itemct;
    int selected_item;
};

void combobox_dirlist_init (struct combobox_dirlist* combobox_dirlist) {
    combobox_dirlist->d = NULL;
    combobox_dirlist->itemct = 0;
    combobox_dirlist->selected_item = 0;
}

static void error_callback (int e, const char *d)
{printf("Error %d: %s\n", e, d);}

static void ui_header (struct nk_context *ctx, struct media *media, const char *title) {
    //nk_style_set_font(ctx, &media->font_18->handle);
    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, title, NK_TEXT_LEFT);
}

static void ui_widget (struct nk_context *ctx, struct media *media, float height) {
    static const float ratio[] = {0.15f, 0.85f};
    //nk_style_set_font(ctx, &media->font_22->handle);
    nk_layout_row(ctx, NK_DYNAMIC, height, 2, ratio);
    nk_spacing(ctx, 1);
}

static void ui_widget_centered (struct nk_context *ctx, struct media *media, float height) {
    static const float ratio[] = {0.15f, 0.50f, 0.35f};
    //nk_style_set_font(ctx, &media->font_22->handle);
    nk_layout_row(ctx, NK_DYNAMIC, height, 3, ratio);
    nk_spacing(ctx, 1);
}



static void ui_combobox_dirlist (struct nk_context* ctx, struct media* media, struct combobox_dirlist* combobox_dirlist, char* directory, unsigned char d_type) {
    // NOTE: How could i make "items" more dynamic? it doesnt expect me to a maximum file name size does it?
    if (nk_combo_begin_label(ctx, combobox_dirlist->items[combobox_dirlist->selected_item], nk_vec2(nk_widget_width(ctx), 200))) {
        if (combobox_dirlist->d == NULL) {
            combobox_dirlist->itemct = 0;
            combobox_dirlist->d = opendir(directory);
            if (combobox_dirlist->d) {
                struct dirent* dir;
                while ((dir = readdir(combobox_dirlist->d)) != NULL) {
                    if (dir->d_type == d_type)
                        sprintf(combobox_dirlist->items[combobox_dirlist->itemct++], "%s", dir->d_name);
                }
            }
            
        }
        nk_layout_row_dynamic(ctx, 20, 1);
        for (int i = 0; i < combobox_dirlist->itemct; ++i)
            if (nk_combo_item_label(ctx, combobox_dirlist->items[i], NK_TEXT_LEFT))
            combobox_dirlist->selected_item = i;
        
        nk_combo_end(ctx);
    } else {
        if (combobox_dirlist->d != NULL) {
            closedir(combobox_dirlist->d);
            combobox_dirlist->d = NULL;
        }
    }
}

int check_if_number (char *str)
{
    int i;
    for (i=0; str[i] != '\0'; i++)
    {
        if (!isdigit (str[i]))
        {
            return 0;
        }
    }
    return 1;
}


#define INITIAL_ALLOC 512


char* read_line(FILE *fin) {
    char *buffer;
    char *tmp;
    int read_chars = 0;
    int bufsize = INITIAL_ALLOC;
    char *line = malloc(bufsize);
    
    if ( !line ) {
        return NULL;
    }
    
    buffer = line;
    
    while ( fgets(buffer, bufsize - read_chars, fin) ) {
        read_chars = strlen(line);
        
        if ( line[read_chars - 1] == '\n' ) {
            line[read_chars - 1] = '\0';
            return line;
        }
        
        else {
            bufsize = 2 * bufsize;
            tmp = realloc(line, bufsize);
            if ( tmp ) {
                line = tmp;
                buffer = line + read_chars;
            }
            else {
                free(line);
                return NULL;
            }
        }
    }
    return NULL;
}

#define MAX_BUF 1024
#define PID_LIST_BLOCK 32

int* pidof (char* pname) {
    DIR* dirp;
    FILE* fp;
    struct dirent* entry;
    int* pidlist, pidlist_index = 0, pidlist_realloc_count = 1;
    char path[MAX_BUF], read_buf[MAX_BUF];
    
    dirp = opendir ("/proc/");
    if (dirp == NULL) {
        perror ("Fail");
        return NULL;
    }
    
    pidlist = malloc (sizeof (int) * PID_LIST_BLOCK);
    if (pidlist == NULL) {
        return NULL;
    }
    
    while ((entry = readdir (dirp)) != NULL) {
        if (check_if_number (entry->d_name)) {
            strcpy (path, "/proc/");
            strcat (path, entry->d_name);
            strcat (path, "/comm");
            
            /* A file may not exist, it may have been removed.
             * dut to termination of the process. Actually we need to
             * make sure the error is actually file does not exist to
             * be accurate.
             */
            fp = fopen (path, "r");
            if (fp != NULL) {
                fscanf (fp, "%s", read_buf);
                if (strcmp (read_buf, pname) == 0) {
                    /* add to list and expand list if needed */
                    pidlist[pidlist_index++] = atoi (entry->d_name);
                    if (pidlist_index == PID_LIST_BLOCK * pidlist_realloc_count) {
                        pidlist_realloc_count++;
                        pidlist = realloc (pidlist, sizeof (int) * PID_LIST_BLOCK * pidlist_realloc_count); //Error check todo
                        if (pidlist == NULL) {
                            return NULL;
                        }
                    }
                }
                fclose (fp);
            }
        }
    }
    
    
    closedir (dirp);
    pidlist[pidlist_index] = -1; /* indicates end of list */
    return pidlist;
}

void gui (void) {
    /* Platform */
    int win_width = 1000;
    int win_height = 700;
    
    struct media* media;
    
    /* GLFW */
    struct nk_glfw glfw = {0};
    static GLFWwindow *win;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) 
    {fprintf(stdout, "[GFLW] failed to init!\n");exit(1);}
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    win = glfwCreateWindow(win_width, win_height, "actkbd gui", NULL, NULL);
    glfwMakeContextCurrent(win);
    
    /* Glew */
    glewExperimental = 1;
    if (glewInit() != GLEW_OK) 
    {fprintf(stderr, "Failed to setup GLEW\n");exit(1);}
    
	/* create context */
    struct nk_context *ctx = nk_glfw3_init(&glfw, win, NK_GLFW3_INSTALL_CALLBACKS);
    
    {struct nk_font_atlas *atlas;
        nk_glfw3_font_stash_begin(&glfw, &atlas);
        nk_glfw3_font_stash_end(&glfw);}
    
    struct combobox_dirlist combobox_dirlist;
    combobox_dirlist_init(&combobox_dirlist);
    struct actkbd_options options;
    actkbd_options_init(&options);
    
    char path[512];
    CIRC_BBUF_DEF(error_log_buffer, 16);
    
    while (!glfwWindowShouldClose(win))
    {
        /* Input */
        glfwPollEvents();
        nk_glfw3_new_frame(&glfw);
        glfwGetWindowSize(win, &win_width, &win_height);
        
        
        
        /* GUI */
        if (nk_begin(ctx, "", nk_rect(0, 0, win_width, win_height), 0))
        {
            
    	    /*------------------------------------------------
     	    *                  COMBOBOX
     	    *------------------------------------------------*/
    	    ui_header(ctx, media, "input device list");
    	    ui_widget_centered(ctx, media, 20);
            ui_combobox_dirlist(ctx, media, &combobox_dirlist, "/dev/input/by-id/", DT_LNK);
            if(options.device != combobox_dirlist.items[combobox_dirlist.selected_item] && combobox_dirlist.items[combobox_dirlist.selected_item] != NULL) {
                options.device = combobox_dirlist.items[combobox_dirlist.selected_item];
            }
            
            ui_header(ctx, media, "config file location (default is /etc/actkbd.conf)");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, path, sizeof(path)-1, nk_filter_default);
            if (nk_button_label (ctx, "Validate")) {
                if( access( path, R_OK|W_OK ) == 0 ) {
                    options.config_file = path;
                } else {
                    options.config_file = default_config;
                    circ_bbuf_push(&error_log_buffer, "File does not have read and write access!");
                }
            }
            
            ui_header(ctx, media, "options");
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "grab device?", &options.grab);
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "suppress all console messages?", &options.quiet);
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "show executed commands?", &options.show_exec);
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "show keypresses?", &options.show_key);
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "use syslog for logging?", &options.system_log);
            ui_widget(ctx, media, 20);
            nk_checkbox_label(ctx, "make permanent?", &options.permanent);
            if (nk_button_label (ctx, "Run actkbd!")) {
                int* list = pidof("actkbd");
                bool exists = false;
                for (int i = 0; list[i] != -1; i++) {
                    exists = true;
                    break;
                }
                if (exists) {
                    circ_bbuf_push(&error_log_buffer, "Instance of actkbd already running!");
                } else {
                    
                    // TODO: Need better way of gaining access to keyboard, and making it more permanent (udev config writer probably)
                    char cmd[65536] = "actkbd";
                    if (options.config_file != NULL) {
                        strcat(cmd, " -c ");
                        strcat(cmd, options.config_file);
                    }
                    if (options.device != NULL) {
                        strcat(cmd, " -d /dev/input/by-id/");
                        strcat(cmd, options.device);
                    }
                    if (options.grab == nk_true) {
                        strcat(cmd, " -g");
                    }
                    if (options.quiet == nk_true) {
                        strcat(cmd, " -q");
                    }
                    if (options.show_exec == nk_true) {
                        strcat(cmd, " -x");
                    }
                    if (options.show_key == nk_true) {
                        strcat(cmd, " -s");
                    }
                    if (options.system_log == nk_true) {
                        strcat(cmd, " -l");
                    }
                    
                    strcat(cmd, " -D &");
                    fprintf(stdout, cmd);
                    // TODO(Astolfo): pkexec doesnt exist on all linux platforms...
                    char cmd_chmod[65535] = "pkexec chmod 0777 /dev/input/by-id/";
                    strcat(cmd_chmod, options.device);
                    system(cmd_chmod);
                    system(cmd);
                    
                    // TODO(Astolfo): currently, it detects if the command is just there, need to add modification of settings
                    if (options.permanent == nk_true) {
                        FILE* actkbd_auto;
                        char* line;
                        char file_name[256];
                        strcat(strcpy(file_name, getenv("HOME")), "/.profile");
                        actkbd_auto = fopen(file_name, "a+");
                        if (actkbd_auto != NULL) {
                            bool found = false;
                            while ((line = read_line(actkbd_auto))) {
                                if (strstr(line, options.device)){
                                    found = true;
                                    circ_bbuf_push(&error_log_buffer, "Config was already made permanent!");
                                    break;
                                }
                                free(line);
                            }
                            if (!found) {
                                fprintf(actkbd_auto, "\n%s\n%s", cmd_chmod, cmd);
                            }
                        } else {
                            circ_bbuf_push(&error_log_buffer, "File has insufficient permission or does not exist!");
                        }
                        fclose(actkbd_auto);
                    }
                }
            }
            
            ui_header(ctx, media, "application log");
            int read = error_log_buffer.read, write = error_log_buffer.write;
            while (read != write) {
                nk_label_wrap(ctx, *(error_log_buffer.buffer + read));
                read = (read + 1) % error_log_buffer.maxlen;
            }
            // NOTE: should i be using this instead of nk_label_wrap?
            //nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_EDITOR, error_log_buffer, &error_log_size, error_log_size, nk_filter_default);
        }
        
        nk_end(ctx);
        
        /* Draw */
        glViewport(0, 0, win_width, win_height);
        glClear(GL_COLOR_BUFFER_BIT);
        nk_glfw3_render(&glfw, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(win);
    }
    
    //free(error_log_buffer);
    nk_glfw3_shutdown(&glfw);
    glfwTerminate();
}

int main(int argc, char** argv) {
    gui();
    return 0;
}
