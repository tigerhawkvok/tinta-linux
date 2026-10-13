// Pango layout throughput: one PangoLayout per blank-line-separated block,
// wrapped at 800 px, measuring extents + line iteration (what Tinta's
// layout pass needs per text run).
#include <pango/pangocairo.h>
#include <time.h>
#include <string.h>
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e3+ts.tv_nsec/1e6;}
int main(int argc,char**argv){
  gchar*buf;gsize n;if(!g_file_get_contents(argv[1],&buf,&n,NULL))return 1;
  gchar**paras=g_regex_split_simple("\\n\\s*\\n",buf,0,0);
  cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,10,10);cairo_t*cr=cairo_create(s);
  PangoContext*pc=pango_cairo_create_context(cr);
  PangoFontDescription*fd=pango_font_description_from_string("Sans 11");
  for(int pass=0;pass<3;pass++){
    double t=now();int count=0,lines=0;double total=0;
    for(gchar**p=paras;*p;p++){
      PangoLayout*l=pango_layout_new(pc);pango_layout_set_font_description(l,fd);
      pango_layout_set_width(l,800*PANGO_SCALE);pango_layout_set_wrap(l,PANGO_WRAP_WORD_CHAR);
      pango_layout_set_text(l,*p,-1);PangoRectangle ink,log;pango_layout_get_pixel_extents(l,&ink,&log);
      lines+=pango_layout_get_line_count(l);total+=log.height;count++;g_object_unref(l);}
    g_print("pass %d: %d blocks, %d lines, %.0f px tall, %.1f ms\n",pass,count,lines,total,now()-t);}
  return 0;}
