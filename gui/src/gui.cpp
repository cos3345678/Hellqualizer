#include <gui.h>
#include <stdio.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>


#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl2.h"

#define WINDOW_WIDTH 704
#define WINDOW_HEIGHT 576

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])



using namespace std;

static void error_callback(int e, const char *d)
{printf("Error %d: %s\n", e, d);}


GUI::GUI(char* src_file_name, HQ_Context *ctx)
{
   m_src_file_name=src_file_name;
   m_ctx=ctx;
}

void *GUI::gui_thread(void *x_void_ptr)
{
    /* Platform */
    static GLFWwindow *win;
    int width = 0, height = 0;
    struct nk_context *ctx;
    struct nk_color background;
    float G0=1.0,G1=1.0,G2=1.0,G3=1.0,G4=1.0;

    /* GLFW */
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stdout, "[GFLW] failed to init!\n");
        exit(1);
    }
    win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Hellqualizer", NULL, NULL);
    glfwMakeContextCurrent(win);
    glfwGetWindowSize(win, &width, &height);

    /* GUI */
    ctx = nk_glfw3_init(win, NK_GLFW3_INSTALL_CALLBACKS);
    /* Load Fonts: if none of these are loaded a default font will be used  */
    /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    {struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&atlas);
    /*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);*/
    /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
    /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
    /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
    /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
    /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
    nk_glfw3_font_stash_end();
    /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
    /*nk_style_set_font(ctx, &droid->handle);*/}

    /* style.c */
    /*set_style(ctx, THEME_WHITE);*/
    /*set_style(ctx, THEME_RED);*/
    /*set_style(ctx, THEME_BLUE);*/
    /*set_style(ctx, THEME_DARK);*/


    background = nk_rgb(28,48,62);
    while (!glfwWindowShouldClose(win))
    {
        /* Input */
        glfwPollEvents();
        nk_glfw3_new_frame();

        /* GUI */
        if (nk_begin(ctx, "Hellqualizer", nk_rect(50, 50, 300, 350),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_static(ctx, 30, 150, 1);
            if (nk_button_label(ctx, "Play/Pause")){
                //fprintf(stdout, "button pressed\n");
                if(m_ctx->state==PLAY){
                    printf("Pause \n");
                    m_ctx->state=PAUSE;
                }else if(m_ctx->state==PAUSE){
                    printf("Play \n");
                    m_ctx->state=PLAY;
                }
            }

            if (nk_button_label(ctx, "Enable/Disable EQ")){
               m_ctx->proc_opt.do_process=(m_ctx->proc_opt.do_process==1)?0:1;
            }


            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_float(ctx, "Gain 0 - 2000 Hz:", 0, &G0, 10.0, 0.1, 0.1);
            nk_property_float(ctx, "Gain 2000 - 4000 Hz:", 0, &G1, 10.0, 0.1, 0.1);
            nk_property_float(ctx, "Gain 4000 - 6000 Hz:", 0, &G2, 10.0, 0.1, 0.1);
            nk_property_float(ctx, "Gain 6000 - 10000 Hz:", 0, &G3, 10.0, 0.1, 0.1);
            nk_property_float(ctx, "Gain 10000 - 22000 Hz:", 0, &G4, 10.0, 0.1, 0.1);
        }
        nk_end(ctx);

        /* -------------- EXAMPLES ---------------- */
        /*calculator(ctx);*/
        /*overview(ctx);*/
        /*node_editor(ctx);*/
        /* ----------------------------------------- */

        /* Draw */
        {float bg[4];
        nk_color_fv(bg, background);
        glfwGetWindowSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg[0], bg[1], bg[2], bg[3]);
        /* IMPORTANT: `nk_glfw_render` modifies some global OpenGL state
         * with blending, scissor, face culling and depth test and defaults everything
         * back into a default state. Make sure to either save and restore or
         * reset your own state after drawing rendering the UI. */
        nk_glfw3_render(NK_ANTI_ALIASING_ON);
        glfwSwapBuffers(win);
        }

        m_ctx->proc_opt.GAIN[0]=G0;
        m_ctx->proc_opt.GAIN[1]=G1;
        m_ctx->proc_opt.GAIN[2]=G2;
        m_ctx->proc_opt.GAIN[3]=G3;
        m_ctx->proc_opt.GAIN[4]=G4;
        usleep(50000);
    }
    nk_glfw3_shutdown();
    glfwTerminate();

}

GUI::~GUI(){

}

void GUI::InternalThreadEntry(){
   this->gui_thread(NULL);
}
