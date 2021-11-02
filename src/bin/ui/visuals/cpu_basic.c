#include "cpu_basic.h"

typedef struct {
   int        cpu_count;
   int       *cpu_order;
   Eina_List *objects;
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

   pd = data;
   ext = pd->ext;
   cores = msgdata;

   for (int i = 0; i < ext->cpu_count; i++)
     {
        Core *core = &cores[i];
        Evas_Object *lb = eina_list_nth(ext->objects, i);
        Evas_Object *rec = evas_object_data_get(lb, "r");
        int c = cpu_colormap[core->percent & 0xff];
        evas_object_color_set(rec, RVAL(c), GVAL(c), BVAL(c), AVAL(c));
  
        elm_object_text_set(lb, eina_slstr_printf("%d%%", core->percent));
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
cpu_visual_basic(Evas_Object *parent_bx)
{
   Evas_Object *tb;
   Ext *ext;

   Ui_Cpu_Data *pd = calloc(1, sizeof(Ui_Cpu_Data));
   if (!pd) return NULL;

   pd->ext = ext = calloc(1, sizeof(Ext));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ext, NULL);
   pd->ext_free_cb = _cb_free;

   /* Populate lookup table to match id with topology core id */
   ext->cpu_count = system_cpu_count_get();
   ext->cpu_order = malloc((ext->cpu_count) * sizeof(int));
   for (int i = 0; i < ext->cpu_count; i++)
     ext->cpu_order[i] = i;
   system_cpu_topology_get(ext->cpu_order, ext->cpu_count);

   tb = elm_table_add(parent_bx);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, 0.5, 0.5);
   evas_object_show(tb);

   int row = 0, col = 0;
   int w = sqrt(ext->cpu_count);
   for (int i = 0; i < ext->cpu_count; i++)
     {
        if (!(i % w))
          {
             row++;
             col = 0;
          }
        else col++;
        Evas_Object *rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(64), ELM_SCALE_SIZE(64));
        elm_table_pack(tb, rec, col, row, 1, 1);
        evas_object_show(rec);
        Evas_Object *lb = elm_label_add(tb);
        evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
        evas_object_size_hint_align_set(lb, FILL, FILL);
        elm_table_pack(tb, lb, col, row, 1, 1);
        evas_object_show(lb);
        evas_object_data_set(lb, "r", rec);
        ext->objects = eina_list_append(ext->objects, lb);
     }

   elm_box_pack_end(parent_bx, tb);

   pd->thread = ecore_thread_feedback_run(_core_times_main_cb,
                                           _core_times_feedback_cb,
                                           NULL,
                                           NULL,
                                           pd, 1);
   return pd;
}

