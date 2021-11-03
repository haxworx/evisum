#include "ui_cpu.h"
#include "config.h"

// Templates for visualisations.
#include "visuals/visuals.x"

// config for colors/sizing
#define COLOR_CPU_NUM 5
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 },
   {  25, 0xff2030a0 },
   {  50, 0xffa040a0 },
   {  75, 0xffff9040 },
   { 100, 0xffffffff },
   { 256, 0xffffffff }
};
#define COLOR_FREQ_NUM 4
static const Color_Point freq_colormap_in[] = {
   {   0, 0xff202020 },
   {  33, 0xff285020 },
   {  67, 0xff30a060 },
   { 100, 0xffa0ff80 },
   { 256, 0xffa0ff80 }
};

#define COLOR_TEMP_NUM 5
static const Color_Point temp_colormap_in[] = {
   {  0,  0xff57bb8a },
   {  25, 0xffa4c073 },
   {  50, 0xfff5ce62 },
   {  75, 0xffe9a268 },
   { 100, 0xffdd776e },
   { 256, 0xffdd776e }
};

unsigned int cpu_colormap[256];
unsigned int freq_colormap[256];
unsigned int temp_colormap[256];

static void
_win_mouse_move_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Coord w, h;
   Evas_Event_Mouse_Move *ev;
   Ui_Cpu_Data *pd = data;

   ev = event_info;
   evas_object_geometry_get(obj, NULL, NULL, &w, &h);

   if ((ev->cur.canvas.x >= (w - 128)) && (ev->cur.canvas.y <= 128))
     {
       if (!pd->btn_visible)
         {
            elm_object_signal_emit(pd->btn_menu, "menu,show", "evisum/menu");
            pd->btn_visible = 1;
         }
     }
   else if ((pd->btn_visible) && (!pd->menu))
    {
       elm_object_signal_emit(pd->btn_menu, "menu,hide", "evisum/menu");
       pd->btn_visible = 0;
    }
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Evisum_Ui *ui;

   ui = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     {
        evas_object_del(ui->cpu.win);
     }
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Ui_Cpu_Data *pd = data;

   ui = pd->ui;
   if (!pd->menu)
     pd->menu = evisum_ui_main_menu_create(ui, ui->cpu.win, obj);
   else
     {
        evas_object_del(pd->menu);
        pd->menu = NULL;
     }
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, NULL, NULL, &ui->cpu.width, &ui->cpu.height);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, &ui->cpu.x, &ui->cpu.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Cpu_Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);

   if (pd->ext_free_cb)
      pd->ext_free_cb(pd->ext);

   free(pd);
   ui->cpu.win = NULL;
}

static void
_color_init(const Color_Point *col_in, unsigned int n, unsigned int *col)
{
   unsigned int pos, interp, val, dist, d;
   unsigned int a, r, g, b;
   unsigned int a1, r1, g1, b1, v1;
   unsigned int a2, r2, g2, b2, v2;

   // wal colormap_in until colormap table is full
   for (pos = 0, val = 0; pos < n; pos++)
     {
        // get first color and value position
        v1 = col_in[pos].val;
        a1 = AVAL(col_in[pos].color);
        r1 = RVAL(col_in[pos].color);
        g1 = GVAL(col_in[pos].color);
        b1 = BVAL(col_in[pos].color);
        // get second color and valuje position
        v2 = col_in[pos + 1].val;
        a2 = AVAL(col_in[pos + 1].color);
        r2 = RVAL(col_in[pos + 1].color);
        g2 = GVAL(col_in[pos + 1].color);
        b2 = BVAL(col_in[pos + 1].color);
        // get distance between values (how many entires to fill)
        dist = v2 - v1;
        // walk over the span of colors from point a to point b
        for (interp = v1; interp < v2; interp++)
          {
             // distance from starting point
             d = interp - v1;
             // calculate linear interpolation between start and given d
             a = ((d * a2) + ((dist - d) * a1)) / dist;
             r = ((d * r2) + ((dist - d) * r1)) / dist;
             g = ((d * g2) + ((dist - d) * g1)) / dist;
             b = ((d * b2) + ((dist - d) * b1)) / dist;
             // write out resulting color value
             col[val] = ARGB(a, r, g, b);
             val++;
          }
     }
}

Eina_List *
ui_cpu_visuals_get(void)
{
   Eina_List *l = NULL;

   for (int i = 0; (i < sizeof(visualizations) / sizeof(Cpu_Visual)); i++)
     {
        l = eina_list_append(l, strdup(visualizations[i].name));
     }

   return l;
}

Cpu_Visual *
ui_cpu_visual_by_name(const char *name)
{
   for (int i = 0; (i < sizeof(visualizations) / sizeof(Cpu_Visual)); i++)
     {
        if (!strcmp(name, visualizations[i].name))
          return &visualizations[i];
     }
   return NULL;
}

void
ui_cpu_win_restart(Evisum_Ui *ui)
{
   evas_object_del(ui->cpu.win);
   ui_cpu_win_add(ui);
}

void
ui_cpu_win_add(Evisum_Ui *ui)
{
   Ui_Cpu_Data *pd;
   Evas_Object *win, *box, *scr, *btn, *ic;
   Evas_Object *tb;
   Elm_Layout *lay;
   Cpu_Visual *vis;
   static Eina_Bool init = 0;

   if (ui->cpu.win)
     {
        elm_win_raise(ui->cpu.win);
        return;
     }

   if (!init)
     {
        // init colormaps from a small # of points
        _color_init(cpu_colormap_in, COLOR_CPU_NUM, cpu_colormap);
        _color_init(freq_colormap_in, COLOR_FREQ_NUM, freq_colormap);
        _color_init(temp_colormap_in, COLOR_TEMP_NUM, temp_colormap);
        init = 1;
     }

   ui->cpu.win = win = elm_win_util_standard_add("evisum",
                   _("CPU Activity"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   scr = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scr, FILL, FILL);
   elm_scroller_policy_set(scr,
                           ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scr);

   tb = elm_table_add(win);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_show(tb);

   box = elm_box_add(win);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   elm_table_pack(tb, box, 0, 0, 1, 1);

   vis = ui_cpu_visual_by_name(ui->cpu.visual);
   if (!vis)
     {
        fprintf(stderr, "FATAL: unknown CPU visual (check your config)\n");
        exit(1);
     }
   pd = vis->func(box);
   pd->ui = ui;

   elm_object_content_set(scr, tb);
   elm_object_content_set(win, scr);

   btn = elm_button_add(win);
   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get("menu"));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);
   elm_object_focus_allow_set(btn, 0);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   evas_object_smart_callback_add(btn, "clicked", _btn_menu_clicked_cb, pd);

   pd->btn_menu = lay = elm_layout_add(win);
   evas_object_size_hint_weight_set(lay, 1.0, 1.0);
   evas_object_size_hint_align_set(lay, 0.99, 0.01);
   elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
   elm_layout_content_set(lay, "evisum/menu", btn);
   elm_table_pack(tb, lay, 0, 0, 1, 1);
   evas_object_show(lay);

   evas_object_event_callback_add(scr, EVAS_CALLBACK_MOUSE_MOVE, _win_mouse_move_cb, pd);
   evas_object_event_callback_add(scr, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE,  _win_move_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);

   if ((ui->cpu.width > 0) && (ui->cpu.height > 0))
     evas_object_resize(win, ui->cpu.width, ui->cpu.height);
   else
     evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.8), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT));

   if ((ui->cpu.x > 0) && (ui->cpu.y > 0))
     evas_object_move(win, ui->cpu.x, ui->cpu.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);
}

