#include "ui_network.h"
#include "system/machine.h"

typedef struct
{
   Ecore_Thread           *thread;
   Evas_Object            *win;
   Evas_Object            *glist;
   Elm_Genlist_Item_Class *itc, *itc2;

   Evisum_Ui              *ui;
} Data;

typedef struct
{
   char     name[255];
   uint64_t total_in;
   uint64_t total_out;

   uint64_t peak_in;
   uint64_t peak_out;

   uint64_t in;
   uint64_t out;

   Evas_Object     *obj;
   Elm_Object_Item *it, *it2;
   Eina_Bool        is_new;
   Eina_Bool        delete_me;
} Network_Interface;

static void
_interface_gone(net_iface_t **ifaces, int n, Eina_List *list, Data *pd)
{
   Eina_List *l;
   Network_Interface *iface;

   EINA_LIST_FOREACH(list, l, iface)
     {
        Eina_Bool found = 0;
        for (int i = 0; i < n; i++)
          {
             net_iface_t *nwif = ifaces[i];
             if (!strcmp(nwif->name, iface->name))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          iface->delete_me = 1;
     }
}

static void
_network_update(void *data, Ecore_Thread *thread)
{
   Data *pd = data;
   Eina_List *interfaces = NULL;
   Network_Interface *iface;

   while (!ecore_thread_check(thread))
     {
        int n;
        net_iface_t *nwif, **ifaces = system_network_ifaces_get(&n);

	_interface_gone(ifaces, n, interfaces, pd);

        for (int i = 0; i < n; i++)
           {
             Network_Interface *iface2;
             Eina_List *l;

             nwif = ifaces[i];
             iface = NULL;
             EINA_LIST_FOREACH(interfaces, l, iface2)
               {
                  if (!strcmp(nwif->name, iface2->name))
                    {
                       iface = iface2;
                       break;
                    }
               }
             if (!iface)
               {
                  iface = calloc(1, sizeof(Network_Interface));
                  iface->is_new = 1;
                  snprintf(iface->name, sizeof(iface->name), "%s", nwif->name);
                  interfaces = eina_list_append(interfaces, iface);
               }
             else
               {
                  iface->is_new = 0;
                  if ((nwif->xfer.in == iface->total_in) || (iface->total_in == 0))
                    iface->in = 0;
                  else
                    iface->in =  (nwif->xfer.in - iface->total_in);
                  if ((nwif->xfer.out == iface->total_out) || (iface->total_out == 0))
                    iface->out = 0;
                  else
                    iface->out = (nwif->xfer.out - iface->total_out);

                  if (iface->in > iface->peak_in)
                    iface->peak_in = iface->in;
                  if (iface->out > iface->peak_out)
                    iface->peak_out = iface->out;
                  iface->total_in = nwif->xfer.in;
                  iface->total_out = nwif->xfer.out;
               }
             free(nwif);
          }
        free(ifaces);

        ecore_thread_feedback(thread, interfaces);
        for (int i = 0; i < 8; i++)
          {
             if (ecore_thread_check(thread))
               break;
             usleep(250000);
          }
     }
   EINA_LIST_FREE(interfaces, iface)
     free(iface);
}

static Evas_Object *
_lb_add(Evas_Object *obj, const char *txt)
{
   Evas_Object *lb = elm_label_add(obj);
   elm_object_text_set(lb, txt);
   return lb;
}

static char *
_text_get(void *data, Evas_Object *obj, const char *part)
{
   char *buf;

   if (strcmp(part, "elm.text")) return NULL;

   buf = data;

   return strdup(buf);
}

static Evas_Object *
_iface_obj_add(void *data, Evas_Object *obj, const char *part)
{
   Evas_Object *tb, *lb, *fr;
   Network_Interface *iface;

   if (strcmp(part, "elm.swallow.content")) return NULL;

   iface = data;

   iface->obj = tb = elm_table_add(obj);
   elm_table_padding_set(tb,
                         8 * elm_config_scale_get(),
                         4 * elm_config_scale_get());
   evas_object_size_hint_weight_set(tb, 1.0, 0);
   evas_object_size_hint_align_set(tb, FILL, 0.0);

   lb = _lb_add(obj, _("<hilight>Total In/Out</>"));
   evas_object_size_hint_align_set(lb, 1.0, 0.5);
   elm_table_pack(tb, lb, 0, 0, 1, 1);
   evas_object_show(lb);
   lb = _lb_add(obj, "");
   evas_object_size_hint_weight_set(lb, 1.0, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_data_set(tb, "total", lb);
   elm_table_pack(tb, lb, 1, 0, 1, 1);
   evas_object_show(lb);

   lb = _lb_add(obj, _("<hilight>Peak In/Out</>"));
   evas_object_size_hint_align_set(lb, 1.0, 0.5);
   elm_table_pack(tb, lb, 0, 1, 1, 1);
   evas_object_show(lb);
   lb = _lb_add(obj, "");
   evas_object_size_hint_weight_set(lb, 1.0, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_data_set(tb, "peak", lb);
   elm_table_pack(tb, lb, 1, 1, 1, 1);
   evas_object_show(lb);

   lb = _lb_add(obj, _("<hilight>In/Out</>"));
   evas_object_size_hint_align_set(lb, 1.0, 0.5);
   elm_table_pack(tb, lb, 0, 2, 1, 1);
   evas_object_show(lb);
   lb = _lb_add(obj, "");
   evas_object_size_hint_weight_set(lb, 1.0, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_data_set(tb, "inout", lb);
   elm_table_pack(tb, lb, 1, 2, 1, 1);
   evas_object_show(lb);

   return tb;
}

static char *
_network_transfer_format(double rate)
{
   const char *unit = "B/s";
   char buf[256];

   if (rate > 1048576)
     {
        rate /= 1048576;
        unit = "MB/s";
     }
   else if ((rate > 1024) && (rate < 1048576))
     {
        rate /= 1024;
        unit = "KB/s";
     }

   snprintf(buf, sizeof(buf), "%.2f %s", rate, unit);
   return strdup(buf);
}

static void
_network_update_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata EINA_UNUSED)
{
   Eina_List *interfaces;
   Network_Interface *iface;
   Data *pd;
   Evas_Object *obj;
   Eina_List *l, *l2;
   char *s;
   Eina_Strbuf *buf;

   interfaces = msgdata;
   pd = data;

   buf = eina_strbuf_new();

   EINA_LIST_FOREACH_SAFE(interfaces, l, l2, iface)
     {
        if (iface->delete_me)
          {
             elm_object_item_del(iface->it);
             elm_object_item_del(iface->it2);
             free(iface);
             interfaces = eina_list_remove_list(interfaces, l);
          }
	else if (iface->is_new)
          {
             iface->it = elm_genlist_item_append(pd->glist, pd->itc2, iface->name, NULL, ELM_GENLIST_ITEM_GROUP, NULL, NULL);
	     iface->it2 = elm_genlist_item_append(pd->glist, pd->itc, iface, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
          }
        else
          {
             obj = evas_object_data_get(iface->obj, "total");
             elm_object_text_set(obj, eina_slstr_printf("%s / %s",
                                 evisum_size_format(iface->total_in),
                                 evisum_size_format(iface->total_out)));

             obj = evas_object_data_get(iface->obj, "peak");
             s = _network_transfer_format(iface->peak_in);
             eina_strbuf_append(buf, s);
             free(s);
             s = _network_transfer_format(iface->peak_out);
             eina_strbuf_append_printf(buf, " / %s", s);
             free(s);
             elm_object_text_set(obj, eina_strbuf_string_get(buf));
             eina_strbuf_reset(buf);

             obj = evas_object_data_get(iface->obj, "inout");
             s = _network_transfer_format(iface->in);
             eina_strbuf_append(buf, s);
             free(s);
             s = _network_transfer_format(iface->out);
             eina_strbuf_append_printf(buf, " / %s", s);
             free(s);
             elm_object_text_set(obj, eina_strbuf_string_get(buf));
             eina_strbuf_reset(buf);
          }
     }
   eina_strbuf_free(buf);
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Data *pd;

   pd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(pd->ui->network.win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Data *pd;
   Evisum_Ui *ui;

   pd = data;
   ui = pd->ui;

   evas_object_geometry_get(obj, &ui->network.x, &ui->network.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   elm_genlist_item_class_free(pd->itc);
   elm_genlist_item_class_free(pd->itc2);

   evisum_ui_config_save(ui);
   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);
   ui->network.win = NULL;
   free(pd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   evas_object_geometry_get(obj, NULL, NULL, &ui->network.width, &ui->network.height);
}

void
ui_network_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *bx, *glist;;
   Elm_Genlist_Item_Class *itc;

   if (ui->network.win)
     {
        elm_win_raise(ui->network.win);
        return;
     }

   Data *pd = calloc(1, sizeof(Data));
   if (!pd) return;
   pd->ui = ui;

   ui->network.win = pd->win = win = elm_win_util_standard_add("evisum", _("Network"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, 1.0, 1.0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, pd);

   pd->glist = glist = elm_genlist_add(win);
   elm_genlist_homogeneous_set(glist, 1);
   evas_object_size_hint_weight_set(glist, 1.0, 1.0);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_NONE);
   elm_box_pack_end(bx, glist);
   evas_object_show(glist);

   itc = elm_genlist_item_class_new();
   pd->itc = itc;
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _iface_obj_add;
   itc->func.filter_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   itc = elm_genlist_item_class_new();
   pd->itc2 = itc;
   itc->item_style = "group_index";
   itc->func.text_get = _text_get;
   itc->func.content_get = NULL;
   itc->func.filter_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   elm_win_resize_object_add(win, bx);
   evas_object_show(bx);

   if ((ui->network.width) > 0 && (ui->network.height > 0))
     evas_object_resize(win, ui->network.width, ui->network.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);

   if ((ui->network.x > 0) && (ui->network.y > 0))
     evas_object_move(win, ui->network.x, ui->network.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);

   pd->thread = ecore_thread_feedback_run(_network_update,
                                          _network_update_feedback_cb,
                                          NULL,
                                          NULL,
                                          pd, 1);
}

