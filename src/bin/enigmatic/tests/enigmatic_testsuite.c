#include <Elementary.h>
#include "Enigmatic.h"
#include "enigmatic_log.h"

static Eina_Bool
test_log_compress(Eina_Bool staggered)
{
   char buf[PATH_MAX], buf2[PATH_MAX];
   char *tok, *paths = getenv("PATH");
   char *path = NULL, *diffpath = NULL;
   uint32_t sz = 0;
   Eina_Bool ret;

   tok = strtok(paths, ":");
   while (tok)
     {
        snprintf(buf, sizeof(buf), "%s/enlightenment", tok);
        if (ecore_file_exists(buf))
          path = strdup(buf);
        snprintf(buf, sizeof(buf), "%s/diff", tok);
        if (ecore_file_exists(buf))
          diffpath = strdup(buf);

        if ((path) && (diffpath)) break;
        tok = strtok(NULL, ":");
     }

   if (!path)
      {
         fprintf(stderr, "enlightenment could not be found.\n");
         return EINA_FALSE;
      }
    if (!diffpath)
      {
         fprintf(stderr, "diff coult not be found.\n");
         return EINA_FALSE;
      }

    getcwd(buf2, PATH_MAX);
    Eina_Slstr *tmp = eina_slstr_printf("%s/enlightenment", buf2);
    ecore_file_cp(path, tmp);

    enigmatic_log_compress(tmp, staggered);
    ecore_file_remove(tmp);

    char *blob = enigmatic_log_decompress(eina_slstr_printf("%s.lz4", tmp), &sz);
    EINA_SAFETY_ON_NULL_RETURN_VAL(blob, EINA_FALSE);

    FILE *f = fopen("enlightenment", "wb");
    if (f)
      {
         fwrite(blob, sz, 1, f);
         fclose(f);
      }
    free(blob);

    ret = !system(eina_slstr_printf("%s %s %s", diffpath, path, "enlightenment"));

    free(path);
    free(diffpath);

    return ret;
}

static void
clear_tmp(void)
{
   Eina_List *files;
   char *file;

   printf("\nCleaning up...\n");
   files = ecore_file_ls("tmp");
   EINA_LIST_FREE(files, file)
     {
        const char *path = eina_slstr_printf("tmp/%s", file);
        printf("DEL => %s\n", path);
        ecore_file_remove(path);
     }
   printf("DEL => tmp/\n");
   ecore_file_rmdir("tmp");
}

int elm_main(int argc, char **argv)
{
    char path[PATH_MAX];
    Eina_Bool staggered = EINA_FALSE;

    ecore_file_mkdir("tmp");
    getcwd(path, PATH_MAX);
    chdir("tmp");
    puts("Begin testing...\n");

    printf("test_log_compress => (%s) => ", staggered == EINA_TRUE ? "staggered" : "default");
    fflush(stdout);
    printf("%s\n", test_log_compress(staggered) == EINA_TRUE ? "OK!" : "FAIL!" );

    chdir(path);
    clear_tmp();
    puts("Bye!");

    return 0;
}

ELM_MAIN();
