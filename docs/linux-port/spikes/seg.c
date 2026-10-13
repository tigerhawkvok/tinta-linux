// Tinta's inline pass: one measuring layout per styled element (cluster
// metrics), then one unwrapped layout per line segment. Approximate with
// 80-char segments and a cluster walk over each block.
#include <pango/pangocairo.h>
#include <time.h>
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e3+ts.tv_nsec/1e6;}
int main(int argc,char**argv){
  gchar*buf;gsize n;g_file_get_contents(argv[1],&buf,&n,NULL);
  gchar**paras=g_regex_split_simple("\\n\\s*\\n",buf,0,0);
  cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,10,10);cairo_t*cr=cairo_create(s);
  PangoContext*pc=pango_cairo_create_context(cr);
  PangoFontDescription*fd=pango_font_description_from_string("Sans 11");
  for(int pass=0;pass<3;pass++){
    double t=now();int layouts=0;long clusters=0;
    for(gchar**p=paras;*p;p++){
      PangoLayout*m=pango_layout_new(pc);pango_layout_set_font_description(m,fd);pango_layout_set_text(m,*p,-1);
      PangoLayoutIter*it=pango_layout_get_iter(m);do{PangoRectangle r;pango_layout_iter_get_cluster_extents(it,NULL,&r);clusters++;}while(pango_layout_iter_next_cluster(it));pango_layout_iter_free(it);
      int nattrs;const PangoLogAttr*la=pango_layout_get_log_attrs_readonly(m,&nattrs);(void)la;
      g_object_unref(m);layouts++;
      glong len=g_utf8_strlen(*p,-1);
      for(glong i=0;i<len;i+=80){const gchar*a=g_utf8_offset_to_pointer(*p,i);const gchar*b=g_utf8_offset_to_pointer(*p,MIN(len,i+80));
        PangoLayout*l=pango_layout_new(pc);pango_layout_set_font_description(l,fd);pango_layout_set_text(l,a,b-a);
        PangoRectangle log;pango_layout_get_extents(l,NULL,&log);g_object_unref(l);layouts++;}}
    g_print("pass %d: %d layouts, %ld clusters, %.1f ms (%.1f us/layout)\n",pass,layouts,clusters,now()-t,(now()-t)*1000/layouts);}
  return 0;}
