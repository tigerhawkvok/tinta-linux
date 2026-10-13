#include <gtk/gtk.h>
#include <time.h>
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e3+ts.tv_nsec/1e6;}
int main(void){double t0=now();gtk_init();double t1=now();
 PangoFontMap*fm=pango_cairo_font_map_get_default();PangoContext*pc=pango_font_map_create_context(fm);
 PangoLayout*l=pango_layout_new(pc);pango_layout_set_text(l,"Hello",-1);int w,h;pango_layout_get_pixel_size(l,&w,&h);double t2=now();
 g_print("gtk_init %.1f ms, first pango layout +%.1f ms\n",t1-t0,t2-t1);return 0;}
