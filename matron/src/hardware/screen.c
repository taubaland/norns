/*
 * screen.c
 *
 * oled
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cairo.h>
#include <cairo-ft.h>

// skip this if you don't want every screen module call to perform null checks
#ifndef CHECK_CR
#define CHECK_CR if (cr == NULL) {return; }
#define CHECK_CRR if (cr == NULL) {return 0; }
#endif

#define NUM_FONTS 14
static char font_path[NUM_FONTS][32];

static float c[16] =
{0, 0.066666666666667, 0.13333333333333, 0.2, 0.26666666666667,
 0.33333333333333,
 0.4, 0.46666666666667, 0.53333333333333, 0.6, 0.66666666666667,
 0.73333333333333, 0.8, 0.86666666666667, 0.93333333333333, 1};

static cairo_surface_t *surface;
static cairo_surface_t *surfacefb;
static cairo_t *cr;
static cairo_t *crfb;
static cairo_font_face_t *ct[NUM_FONTS];
static FT_Library value;
static FT_Error status;
static FT_Face face[NUM_FONTS];
static double text_xy[2];

typedef struct _cairo_linuxfb_device {
    int fb_fd;
    unsigned char *fb_data;
    long fb_screensize;
    struct fb_var_screeninfo fb_vinfo;
    struct fb_fix_screeninfo fb_finfo;
} cairo_linuxfb_device_t;

/* Destroy a cairo surface */
void cairo_linuxfb_surface_destroy(void *device)
{
    cairo_linuxfb_device_t *dev = (cairo_linuxfb_device_t *)device;

    if (dev == NULL) {
        return;
    }

    munmap(dev->fb_data, dev->fb_screensize);
    close(dev->fb_fd);
    free(dev);
}

/* Create a cairo surface using the specified framebuffer */
cairo_surface_t *cairo_linuxfb_surface_create(const char *fb_name)
{
    cairo_linuxfb_device_t *device;
    cairo_surface_t *surface;

    /* Use fb0 if no fram buffer is specified */
    if (fb_name == NULL) {
        fb_name = "/dev/fb0";
    }

    device = malloc( sizeof(*device) );
    if (!device) {
        fprintf(stderr, "ERROR (screen) cannot allocate memory\n");
        return NULL;
    }

    // Open the file for reading and writing
    device->fb_fd = open(fb_name, O_RDWR);
    if (device->fb_fd == -1) {
        fprintf(stderr, "ERROR (screen) cannot open framebuffer device");
        goto handle_allocate_error;
    }

    // Get variable screen information
    if (ioctl(device->fb_fd, FBIOGET_VSCREENINFO, &device->fb_vinfo) == -1) {
        fprintf(stderr, "ERROR (screen) reading variable information");
        goto handle_ioctl_error;
    }

    // Figure out the size of the screen in bytes
    device->fb_screensize = device->fb_vinfo.xres * device->fb_vinfo.yres
                            * device->fb_vinfo.bits_per_pixel / 8;

    // Map the device to memory
    device->fb_data = (unsigned char *)mmap(0, device->fb_screensize,
                                            PROT_READ | PROT_WRITE, MAP_SHARED,
                                            device->fb_fd, 0);

    if ( device->fb_data == (unsigned char *)-1 ) {
        fprintf(stderr, "ERROR (screen) failed to map framebuffer device to memory");
        goto handle_ioctl_error;
    }

    // Get fixed screen information
    if (ioctl(device->fb_fd, FBIOGET_FSCREENINFO, &device->fb_finfo) == -1) {
        fprintf(stderr, "ERROR (screen) reading fixed information");
        goto handle_ioctl_error;
    }

    /* Create the cairo surface which will be used to draw to */
    surface = cairo_image_surface_create_for_data(
        device->fb_data,
        CAIRO_FORMAT_ARGB32,
        device->fb_vinfo.xres,
        device->fb_vinfo.yres,
        cairo_format_stride_for_width(
            CAIRO_FORMAT_ARGB32,
            device
            ->fb_vinfo.xres) );
    cairo_surface_set_user_data(surface, NULL, device,
                                &cairo_linuxfb_surface_destroy);

    return surface;

handle_ioctl_error:
    close(device->fb_fd);
handle_allocate_error:
    free(device);
    return NULL;
}

void screen_init(void) {
    surfacefb = cairo_linuxfb_surface_create("/dev/fb0");
    if(surfacefb == NULL) { return; }
    crfb = cairo_create(surfacefb);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1024,600);
    cr = cairo_create(surface);

    status = FT_Init_FreeType(&value);
    if(status != 0) {
        fprintf(stderr, "ERROR (screen) freetype init\n");
        return;
    }
    
    cairo_scale(cr, 4.5, 6);

    strcpy(font_path[0],"04B_03__.TTF");
    strcpy(font_path[1],"liquid.ttf");
    strcpy(font_path[2],"Roboto-Thin.ttf");
    strcpy(font_path[3],"Roboto-Light.ttf");
    strcpy(font_path[4],"Roboto-Regular.ttf");
    strcpy(font_path[5],"Roboto-Medium.ttf");
    strcpy(font_path[6],"Roboto-Bold.ttf");
    strcpy(font_path[7],"Roboto-Black.ttf");
    strcpy(font_path[8],"Roboto-ThinItalic.ttf");
    strcpy(font_path[9],"Roboto-LightItalic.ttf");
    strcpy(font_path[10],"Roboto-Italic.ttf");
    strcpy(font_path[11],"Roboto-MediumItalic.ttf");
    strcpy(font_path[12],"Roboto-BoldItalic.ttf");
    strcpy(font_path[13],"Roboto-BlackItalic.ttf");

    char filename[256];

    for(int i = 0; i < NUM_FONTS; i++) {
        // FIXME should be path relative to norns/
        snprintf(filename, 256, "%s/norns/resources/%s", getenv(
                     "HOME"), font_path[i]);

        status = FT_New_Face(value, filename, 0, &face[i]);
        if(status != 0) {
            fprintf(stderr, "ERROR (screen) font load: %s\n", filename);
            return;
        }
        else{
            ct[i] = cairo_ft_font_face_create_for_ft_face(face[i], 0);
        }
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_font_options_t *font_options;
    font_options = cairo_font_options_create();
    cairo_font_options_set_antialias(font_options,CAIRO_ANTIALIAS_SUBPIXEL);

    // default font
    cairo_set_font_face (cr, ct[0]);
    cairo_set_font_options(cr, font_options);
    cairo_set_font_size(cr, 8.0);

    // config buffer
    cairo_set_operator(crfb, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(crfb,surface,0,0);
}

void screen_deinit(void) {
    CHECK_CR
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_destroy(crfb);
    cairo_surface_destroy(surfacefb);
}

void screen_update(void) {
    CHECK_CR
    cairo_paint(crfb);
}

void screen_save(void) {
    CHECK_CR
    cairo_save(cr);
}

void screen_restore(void) {
    CHECK_CR
    cairo_restore(cr);
}

void screen_font_face(int i) {
    CHECK_CR
    if( (i >= 0) && (i < NUM_FONTS) ) {
        cairo_set_font_face(cr,ct[i]);
    }
}

void screen_font_size(double z) {
    CHECK_CR
    cairo_set_font_size(cr,z);
}

void screen_aa(int s) {
    CHECK_CR
    if(s == 0) {
        cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
    } else {
        cairo_set_antialias(cr,CAIRO_ANTIALIAS_DEFAULT);
    }
}

void screen_level(int z) {
    CHECK_CR
    cairo_set_source_rgb(cr,c[z],c[z],c[z]);
}

void screen_line_width(double w) {
    CHECK_CR
    cairo_set_line_width(cr,w);
}

void screen_line_cap(const char *style) {
    CHECK_CR
    if(strcmp(style, "round") == 0){
      cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
    }else if(strcmp(style, "square") == 0){
      cairo_set_line_cap(cr,CAIRO_LINE_CAP_SQUARE);
    }else{
      cairo_set_line_cap(cr,CAIRO_LINE_CAP_BUTT);
    }
}

void screen_line_join(const char *style) {
    CHECK_CR
    if(strcmp(style, "round") == 0){
      cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
    }else if(strcmp(style, "bevel") == 0){
      cairo_set_line_join(cr,CAIRO_LINE_JOIN_BEVEL);
    }else{
      cairo_set_line_join(cr,CAIRO_LINE_JOIN_MITER);
    }
}

void screen_miter_limit(double limit) {
    CHECK_CR
    cairo_set_miter_limit(cr,limit);
}

void screen_move(double x, double y) {
    CHECK_CR
    cairo_move_to(cr,x,y);
}

void screen_line(double x, double y) {
    CHECK_CR
    cairo_line_to(cr,x,y);
}

void screen_line_rel(double x, double y) {
    CHECK_CR
    cairo_rel_line_to(cr,x,y);
}

void screen_move_rel(double x, double y) {
    CHECK_CR
    cairo_rel_move_to(cr,x,y);
}

void screen_curve(double x1,
                  double y1,
                  double x2,
                  double y2,
                  double x3,
                  double y3) {
    CHECK_CR
    cairo_curve_to(cr,x1,y1,x2,y2,x3,y3);
}

void screen_curve_rel(double dx1,
                      double dy1,
                      double dx2,
                      double dy2,
                      double dx3,
                      double dy3) {
    CHECK_CR
    cairo_rel_curve_to(cr,dx1,dy1,dx2,dy2,dx3,dy3);
}

void screen_arc(double x, double y, double r, double a1, double a2) {
    CHECK_CR
    cairo_arc(cr,x,y,r,a1,a2);
}

void screen_rect(double x, double y, double w, double h) {
    CHECK_CR
    cairo_rectangle(cr,x,y,w,h);
}

void screen_close_path(void) {
    CHECK_CR
    cairo_close_path(cr);
}

void screen_stroke(void) {
    CHECK_CR
    cairo_stroke(cr);
}

void screen_fill(void) {
    CHECK_CR
    cairo_fill(cr);
}

void screen_text(const char *s) {
    CHECK_CR
    cairo_show_text(cr, s);
}

void screen_clear(void) {
    CHECK_CR
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

double *screen_extents(const char *s) {
    CHECK_CRR
    cairo_text_extents_t extents;
    cairo_text_extents(cr, s, &extents);
    text_xy[0] = extents.width;
    text_xy[1] = extents.height;
    return text_xy;
}
#undef CHECK_CR
#undef CHECK_CRR
