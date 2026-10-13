// Time from main() to first painted frame for a GTK4 window whose drawing
// area lays out and draws ~one screen of text with Pango/Cairo.
#include <gtk/gtk.h>
#include <time.h>
static double t0;
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e3+ts.tv_nsec/1e6;}
static const char *para="Tinta is a fast, lightweight Markdown and Mermaid viewer. It uses the GPU-accelerated text stack already built into the platform, opening instantly with no web engine. ";
static int drawn=0;
static void draw(GtkDrawingArea*a,cairo_t*cr,int w,int h,gpointer u){
  double s=now();
  PangoLayout*l=pango_cairo_create_layout(cr);
  PangoFontDescription*fd=pango_font_description_from_string("Sans 11");
  pango_layout_set_font_description(l,fd);
  pango_layout_set_width(l,(w-40)*PANGO_SCALE);
  pango_layout_set_wrap(l,PANGO_WRAP_WORD_CHAR);
  for(int i=0;i<12;i++){pango_layout_set_text(l,para,-1);cairo_move_to(cr,20,20+i*60);pango_cairo_show_layout(cr,l);}
  g_object_unref(l);pango_font_description_free(fd);
  if(!drawn++) g_print("first draw callback at %.1f ms (draw itself %.1f ms)\n",s-t0,now()-s);
}
static void after_paint(GdkFrameClock*c,gpointer app){
  if(drawn){g_print("first frame painted at %.1f ms\n",now()-t0);g_application_quit(G_APPLICATION(app));}
}
static void realize_cb(GtkWidget*w,gpointer app){
  GdkFrameClock*c=gtk_widget_get_frame_clock(w);
  g_signal_connect(c,"after-paint",G_CALLBACK(after_paint),app);
}
static void activate(GtkApplication*app,gpointer u){
  g_print("activate at %.1f ms\n",now()-t0);
  GtkWidget*win=gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(win),1000,800);
  GtkWidget*da=gtk_drawing_area_new();
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da),draw,NULL,NULL);
  gtk_window_set_child(GTK_WINDOW(win),da);
  g_signal_connect(win,"realize",G_CALLBACK(realize_cb),app);
  gtk_window_present(GTK_WINDOW(win));
}
int main(int argc,char**argv){
  t0=now();
  GtkApplication*app=gtk_application_new("cc.tinta.spike",G_APPLICATION_NON_UNIQUE);
  g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);
  int r=g_application_run(G_APPLICATION(app),argc,argv);
  g_print("exit at %.1f ms\n",now()-t0);
  return r;
}
