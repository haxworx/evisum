#include "evisum_ui_cpu.h"
#include "config.h"

// Templates for visualisations.
#include "visuals/visuals.x"

static void
_cpu_description_add(Evas_Object *parent, const Evisum_Ui_Cpu_Visual *vis)
{
   Evas_Object *fr, *box, *lb;
   char *author;

   if (!vis->description) return;

   fr = elm_frame_add(parent);
   elm_object_text_set(fr, _("Description"));
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0.0);
   elm_box_pack_end(parent, fr);
   evas_object_show(fr);

   box = elm_box_add(fr);
   elm_box_padding_set(box, 0, ELM_SCALE_SIZE(8));
   elm_object_content_set(fr, box);
   evas_object_show(box);

   lb = elm_label_add(box);
   elm_label_line_wrap_set(lb, ELM_WRAP_WORD);
   elm_object_text_set(lb, eina_slstr_printf("<align=left>%s</align>", _(vis->description)));
   evas_object_size_hint_align_set(lb, FILL, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, 0.0);
   elm_box_pack_end(box, lb);
   evas_object_show(lb);

   if (!vis->author) return;
   author = elm_entry_utf8_to_markup(vis->author);
   if (!author) return;
   lb = elm_label_add(box);
   elm_label_line_wrap_set(lb, ELM_WRAP_WORD);
   elm_object_text_set(lb, eina_slstr_printf("<align=right><i>%s</i></align>",
                                           eina_slstr_printf(_("By %s"), author)));
   free(author);
   evas_object_size_hint_align_set(lb, FILL, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, 0.0);
   elm_box_pack_end(box, lb);
   evas_object_show(lb);
}

// config for colors/sizing
static void
_evisum_ui_cpu_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Evisum_Ui_Cpu_Visual_Data *pd = data;

   evisum_ui_menu_hover_update(pd->btn_menu, obj, pd->menu,
                               &pd->btn_visible, event_info);
}

static void
_evisum_ui_cpu_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Evisum_Ui *ui;

   ui = data;
   ev = event_info;

   if (!ev || !ev->keyname) return;

   if (!strcmp(ev->keyname, "Escape"))
     {
        evas_object_del(ui->cpu.win);
     }
}

static void
_evisum_ui_cpu_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Evisum_Ui_Cpu_Visual_Data *pd = data;

   ui = pd->ui;
   if (!pd->menu) pd->menu = evisum_ui_main_menu_create(ui, ui->cpu.win, obj);
   else
     {
        evas_object_del(pd->menu);
        pd->menu = NULL;
     }
}

static void
_evisum_ui_cpu_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, NULL, NULL, &ui->cpu.width, &ui->cpu.height);
}

static void
_evisum_ui_cpu_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, &ui->cpu.x, &ui->cpu.y, NULL, NULL);
}

static void
_evisum_ui_cpu_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evisum_Ui_Cpu_Visual_Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);

   if (pd->ext_free_cb) pd->ext_free_cb(pd->ext);

   free(pd);
   ui->cpu.win = NULL;
}

Eina_List *
evisum_ui_cpu_visuals_get(void)
{
   Eina_List *l = NULL;

   for (int i = 0; (i < sizeof(visualizations) / sizeof(Evisum_Ui_Cpu_Visual)); i++)
     {
        l = eina_list_append(l, strdup(visualizations[i].name));
     }

   return l;
}

Evisum_Ui_Cpu_Visual *
evisum_ui_cpu_visual_by_name(const char *name)
{
   for (int i = 0; (i < sizeof(visualizations) / sizeof(Evisum_Ui_Cpu_Visual)); i++)
     {
        if (!strcmp(name, visualizations[i].name)) return &visualizations[i];
     }
   return NULL;
}

void
evisum_ui_cpu_win_restart(Evisum_Ui *ui)
{
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_NONE);
   evas_object_del(ui->cpu.win);
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   evisum_ui_cpu_win_add(ui);
}

void
evisum_ui_cpu_win_add(Evisum_Ui *ui)
{
   Evisum_Ui_Cpu_Visual_Data *pd;
   Evas_Object *win, *box, *scr, *bg;
   Evas_Object *tb;
   Elm_Layout *lay;
   Evisum_Ui_Cpu_Visual *vis;
   uint8_t bg_r, bg_g, bg_b;

   if (ui->cpu.win)
     {
        elm_win_raise(ui->cpu.win);
        return;
     }

   ui->cpu.win = win = elm_win_util_standard_add("evisum", _("CPU Activity"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   evisum_graph_widget_colors_get(&bg_r, &bg_g, &bg_b, NULL, NULL, NULL, NULL, NULL, NULL);
   bg = elm_bg_add(win);
   elm_bg_color_set(bg, bg_r, bg_g, bg_b);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   scr = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scr, FILL, FILL);
   elm_scroller_policy_set(scr, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   if (!strcmp(ui->cpu.visual, "default"))
      elm_scroller_content_min_limit(scr, EINA_TRUE, EINA_TRUE);
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

   vis = evisum_ui_cpu_visual_by_name(ui->cpu.visual);
   if (!vis)
     {
        fprintf(stderr, "FATAL: unknown CPU visual (check your config)\n");
        exit(1);
     }
   pd = vis->func(box);
   if (!pd)
     {
        evas_object_del(win);
        ui->cpu.win = NULL;
        return;
     }
   pd->win = win;
   pd->ui = ui;
   _cpu_description_add(box, vis);

   elm_object_content_set(scr, tb);
   elm_object_content_set(win, scr);

   pd->btn_menu = lay = evisum_ui_menu_button_add(win, _evisum_ui_cpu_btn_menu_clicked_cb, pd);
   elm_table_pack(tb, lay, 0, 0, 1, 1);
   evas_object_show(lay);

   evas_object_event_callback_add(scr, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_cpu_win_mouse_move_cb, pd);
   evas_object_event_callback_add(scr, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_cpu_win_key_down_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_cpu_win_resize_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _evisum_ui_cpu_win_move_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_cpu_win_del_cb, pd);

   if ((ui->cpu.width > 0) && (ui->cpu.height > 0)) evas_object_resize(win, ui->cpu.width, ui->cpu.height);
   else evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.8), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT * 1.2));

   if ((ui->cpu.x > 0) && (ui->cpu.y > 0)) evas_object_move(win, ui->cpu.x, ui->cpu.y);
   else elm_win_center(win, 1, 1);

   evas_object_show(win);
}
