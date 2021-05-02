/*
 * actkbd-nuklear - A gui for actkbd
 *
 * Copyright (c) 2021-2022 RealAstolfo@gmx.com
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

struct media {
    struct nk_font *font_14;
    struct nk_font *font_18;
    struct nk_font *font_20;
    struct nk_font *font_22;

    struct nk_image unchecked;
    struct nk_image checked;
};

static void error_callback(int e, const char *d)
{printf("Error %d: %s\n", e, d);}

static void
ui_header(struct nk_context *ctx, struct media *media, const char *title)
{
    //nk_style_set_font(ctx, &media->font_18->handle);
    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, title, NK_TEXT_LEFT);
}

static void
ui_widget(struct nk_context *ctx, struct media *media, float height)
{
    static const float ratio[] = {0.15f, 0.85f};
    //nk_style_set_font(ctx, &media->font_22->handle);
    nk_layout_row(ctx, NK_DYNAMIC, height, 2, ratio);
    nk_spacing(ctx, 1);
}

static void
ui_widget_centered(struct nk_context *ctx, struct media *media, float height)
{
    static const float ratio[] = {0.15f, 0.50f, 0.35f};
    //nk_style_set_font(ctx, &media->font_22->handle);
    nk_layout_row(ctx, NK_DYNAMIC, height, 3, ratio);
    nk_spacing(ctx, 1);
}

void gui(void) {
    /* Platform */
    int win_width = 1000;
    int win_height = 700;
    struct media* media;
    static char items[64][64];
    DIR* d;
    struct dirent* dir;

    static int selected_item = 0;

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

    bool opened_combobox = false;
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
    	    ui_header(ctx, media, "");
    	    ui_widget_centered(ctx, media, 20);
	    static int itemct = 0;
	    // How could i make "items" more dynamic? it doesnt expect me to a maximum file name size does it?
    	    if (nk_combo_begin_label(ctx, items[selected_item], nk_vec2(nk_widget_width(ctx), 200))) {
		if (!opened_combobox) {
		    itemct = 0;
		    opened_combobox = true;
		    d = opendir("/dev/input/by-id/");
		    if (d) {
			while ((dir = readdir(d)) != NULL) {
			    sprintf(items[itemct++], "%s", dir->d_name);
			}
		    }
		}
            	nk_layout_row_dynamic(ctx, 20, 1);
            	for (int i = 0; i < itemct; ++i)
           	    if (nk_combo_item_label(ctx, items[i], NK_TEXT_LEFT))
               	    	selected_item = i;
        	nk_combo_end(ctx);
    	    } else {
		//combo box closed
		if (opened_combobox) {
		    opened_combobox = false;
		    closedir(d);
		}
	    }
	/*
	    nk_layout_row_static(ctx,  win_height, 200, 2);
	    if (nk_group_begin(ctx, "Input Devices", 0)) {
            	static int selected[32];
    	    	nk_layout_row_static(ctx, 18, 200, 1);
    	    	for (int i = 0; i < 32; ++i)
        	    nk_selectable_label(ctx, (selected[i]) ? "Selected": "Unselected", NK_TEXT_CENTERED, &selected[i]);
	    } nk_group_end(ctx);

	 */
        }
        nk_end(ctx);
        

        /* Draw */
        glViewport(0, 0, win_width, win_height);
        glClear(GL_COLOR_BUFFER_BIT);
        nk_glfw3_render(&glfw, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(win);
    }
    nk_glfw3_shutdown(&glfw);
    glfwTerminate();
}

int main(int argc, char** argv) {
	gui();
	return 0;
}
