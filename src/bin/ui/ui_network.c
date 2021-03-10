#include "ui_network.h"
#include "system/machine.h"

typedef struct
{
   Ecore_Thread          *thread;
   Eina_List             *interfaces;
   Evas_Object           *win;
   Evas_Object           *glist;
   Eina_List             *purge;
   Eina_Bool              skip_wait;
   Elm_Genlist_Item_Class itc, itc2;
   Ui                    *ui;
} Ui_Data;

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
} Network_Interface;


static void
_interface_gone(net_iface_t **ifaces, int n, Eina_List *list, Ui_Data *pd)
{
   Eina_List *l, *l2;
   Network_Interface *iface;

   EINA_LIST_FOREACH_SAFE(list, l, l2, iface)
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
          {
             if (iface->it)
               pd->purge = eina_list_append(pd->purge, iface->it);
             if (iface->it2)
               pd->purge = eina_list_append(pd->purge, iface->it2);
             free(iface);
             pd->interfaces = eina_list_remove_list(pd->interfaces, l);
          }
     }
}

static void
_network_update(void *data, Ecore_Thread *thread)
{
   Ui_Data *pd = data;
   int n;

   while (!ecore_thread_check(thread))
     {
        Eina_List *l;
        Network_Interface *iface, *iface2;

        net_iface_t *nwif, **ifaces = system_network_ifaces_get(&n);

	_interface_gone(ifaces, n, pd->interfaces, pd);

        for (int i = 0; i < n; i++)
           {
             nwif = ifaces[i];
             iface = NULL;
             EINA_LIST_FOREACH(pd->interfaces, l, iface2)
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
                  pd->interfaces = eina_list_append(pd->interfaces, iface);
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

        ecore_thread_feedback(thread, pd->interfaces);
        for (int i = 0; i < 8; i++)
          {
             if (pd->skip_wait || ecore_thread_check(thread)) break;
             usleep(250000);
          }
        pd->skip_wait = 0;
     }
}

static Evas_Object *
_lb_add(Evas_Object *obj, const char *txt)
{
   Evas_Object *lb = elm_label_add(obj);
   evas_object_size_hint_weight_set(lb, 1.0, 1.0);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   elm_object_text_set(lb, txt);
   evas_object_show(lb);

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
   Evas_Object *bx, *tb, *lb;
   Network_Interface *iface;

   if (strcmp(part, "elm.swallow.content")) return NULL;

   iface = data;

   bx = elm_box_add(obj);
   evas_object_size_hint_weight_set(bx, 1.0, 1.0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   iface->obj = tb = elm_table_add(obj);
   evas_object_size_hint_weight_set(tb, 1.0, 0);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   lb = _lb_add(obj, _("Total In/Out"));
   elm_table_pack(tb, lb, 0, 1, 1, 1);
   lb = _lb_add(obj, "");
   evas_object_data_set(tb, "total", lb);
   elm_table_pack(tb, lb, 1, 1, 1, 1);

   lb = _lb_add(obj, _("Peak In/Out"));
   elm_table_pack(tb, lb, 0, 2, 1, 1);
   lb = _lb_add(obj, "");
   evas_object_data_set(tb, "peak", lb);
   elm_table_pack(tb, lb, 1, 2, 1, 1);

   lb = _lb_add(obj, _("In/Out"));
   elm_table_pack(tb, lb, 0, 3, 1, 1);
   lb = _lb_add(obj, "");
   evas_object_data_set(tb, "inout", lb);
   elm_table_pack(tb, lb, 1, 3, 1, 1);

   elm_box_pack_end(bx, tb);

   return bx;
}

static char *
_network_transfer_format(double rate)
{
   const char *unit = "B/s";

   if (rate > 1048576)
     {
        rate /= 1048576;
        unit = "MB/s";
     }
   else if (rate > 1024 && rate < 1048576)
     {
        rate /= 1024;
        unit = "KB/s";
     }

   return strdup(eina_slstr_printf("%.2f %s", rate, unit));
}

static void
_network_update_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata EINA_UNUSED)
{
   Network_Interface *iface;
   Evas_Object *obj;
   Elm_Object_Item *it;
   Eina_List *l;
   char *s;
   Eina_Strbuf *buf;
   Ui_Data *pd = data;

   EINA_LIST_FREE(pd->purge, it)
     elm_object_item_del(it);

   buf = eina_strbuf_new();

   EINA_LIST_FOREACH(pd->interfaces, l, iface)
     {
        if (iface->is_new)
          {
             iface->it = elm_genlist_item_append(pd->glist, &pd->itc2, iface->name, NULL, ELM_GENLIST_ITEM_GROUP, NULL, NULL);
	     iface->it2 = elm_genlist_item_append(pd->glist, &pd->itc, iface, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
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
   Ui_Data *pd;

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
   Ui_Data *pd;
   Ui *ui;

   pd = data;
   ui = pd->ui;

   evas_object_geometry_get(obj, &ui->network.x, &ui->network.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;
   Network_Interface *iface;

   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);
   eina_list_free(pd->purge);
   EINA_LIST_FREE(pd->interfaces, iface)
     free(iface);
   ui->network.win = NULL;
   free(pd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   evas_object_geometry_get(obj, NULL, NULL, &ui->network.width, &ui->network.height);
}

void
ui_network_win_add(Ui *ui)
{
   Evas_Object *win, *bx, *glist;;
   Elm_Genlist_Item_Class *itc;

   if (ui->network.win)
     {
        elm_win_raise(ui->network.win);
        return;
     }

   Ui_Data *pd = calloc(1, sizeof(Ui_Data));
   if (!pd) return;
   pd->ui = ui;

   ui->network.win = pd->win = win = elm_win_util_standard_add("evisum", _("Network"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, pd);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, 1.0, 1.0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   pd->glist = glist = elm_genlist_add(win);
   elm_genlist_homogeneous_set(glist, 1);
   evas_object_size_hint_weight_set(glist, 1.0, 1.0);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_show(glist);
   elm_box_pack_end(bx, glist);

   itc = &pd->itc;
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _iface_obj_add;
   itc->func.filter_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   itc = &pd->itc2;
   itc->item_style = "group_index";
   itc->func.text_get = _text_get;
   itc->func.content_get = NULL;
   itc->func.filter_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   elm_object_content_set(win, bx);

   if (ui->network.width > 0 && ui->network.height > 0)
     evas_object_resize(win, ui->network.width, ui->network.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);

   if (ui->network.x > 0 && ui->network.y > 0)
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

