#include "cpu_bars.h"

#define BAR_WIDTH 16

typedef struct {
   int          cpu_count;
   int         *cpu_order;
   Eina_List   *objects;
   Evas_Object *tb;
   Evas_Object *bg;
} Ext;

static void
_core_times_main_cb(void *data, Ecore_Thread *thread)
{
   int ncpu;
   Ui_Cpu_Data *pd = data;
   Ext *ext = pd->ext;

   while (!ecore_thread_check(thread))
     {
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 100000);
        Core *cores_out = calloc(ncpu, sizeof(Core));

        if (cores_out)
          {
             for (int n = 0; n < ncpu; n++)
               {
                  int id = ext->cpu_order[n];
                  Core *core = &(cores_out[n]);
                  core->id = id;
                  core->percent = cores[id]->percent;
                  free(cores[id]);
               }
             ecore_thread_feedback(thread, cores_out);
          }
        free(cores);
     }
}

static void
_core_times_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Ui_Cpu_Data *pd;
   Core *cores;
   Ext *ext;
   Evas_Coord oh, ow, ox, oy;
   int step = 0;

   pd = data;
   ext = pd->ext;
   cores = msgdata;

   evas_object_geometry_get(ext->tb, NULL, &oy, &ow, &oh);
   evas_object_geometry_get(pd->win, NULL, NULL, NULL, &oh);

   step = (oh / 100);

   for (int i = 0; i < ext->cpu_count; i++)
     {
        Core *core = &cores[i];
        Evas_Object *rec = eina_list_nth(ext->objects, i);
        evas_object_geometry_get(rec, &ox, NULL, NULL, NULL);
        int c = temp_colormap[core->percent & 0xff];
        int h = core->percent * step;
        if (!h) h = 1;
        evas_object_color_set(rec, RVAL(c), GVAL(c), BVAL(c), AVAL(c));
        evas_object_resize(rec, ELM_SCALE_SIZE(BAR_WIDTH), ELM_SCALE_SIZE(h));
        evas_object_move(rec, ox, oy - ELM_SCALE_SIZE(h));
        elm_table_align_set(ext->tb, 0.0, 1.0);
     }

   free(cores);
}

static void
_cb_free(void *data)
{
   Ext *ext = data;
   
   eina_list_free(ext->objects);

   free(ext->cpu_order);
   free(ext);
}

Ui_Cpu_Data *
cpu_visual_bars(Evas_Object *parent_bx)
{
   Evas_Object *tb, *rec;
   Ext *ext;
   int i;

   Ui_Cpu_Data *pd = calloc(1, sizeof(Ui_Cpu_Data));
   if (!pd) return NULL;

   pd->ext = ext = calloc(1, sizeof(Ext));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ext, NULL);
   pd->ext_free_cb = _cb_free;

   /* Populate lookup table to match id with topology core id */
   ext->cpu_count = system_cpu_count_get();
   ext->cpu_order = malloc((ext->cpu_count) * sizeof(int));
   for (i = 0; i < ext->cpu_count; i++)
     ext->cpu_order[i] = i;
   system_cpu_topology_get(ext->cpu_order, ext->cpu_count);

   ext->tb = tb = elm_table_add(parent_bx);
   elm_table_padding_set(tb, ELM_SCALE_SIZE(2), ELM_SCALE_SIZE(2));
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, 0.5, 1.0);
   evas_object_show(tb);

   for (i = 0; i < ext->cpu_count; i++)
     {
        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BAR_WIDTH), 1);
        elm_table_pack(tb, rec, i, 0, 1, 1);
        evas_object_show(rec);
        ext->objects = eina_list_append(ext->objects, rec);
     }

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
   evas_object_size_hint_align_set(rec, FILL, FILL);
   elm_table_pack(tb, rec, 0, 0, i, 2);
  
   elm_box_pack_end(parent_bx, tb);

   pd->thread = ecore_thread_feedback_run(_core_times_main_cb,
                                          _core_times_feedback_cb,
                                          NULL,
                                          NULL,
                                          pd, 1);
   return pd;
}

