#include "Enigmatic.h"
#include "enigmatic_util.h"
#include <Ecore_File.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static char *
_tool_path_find(const char *tool)
{
   char exe[PATH_MAX], dir[PATH_MAX], path[PATH_MAX * 2];
   ssize_t len;

   len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
   if (len <= 0)
     return strdup(tool);

   exe[len] = '\0';
   snprintf(dir, sizeof(dir), "%s", exe);

   char *slash = strrchr(dir, '/');
   if (!slash)
     return strdup(tool);
   *slash = '\0';

   snprintf(path, sizeof(path), "%s/%s", dir, tool);
   if (!access(path, X_OK))
     return strdup(path);

   snprintf(path, sizeof(path), "%s/../%s", dir, tool);
   if (!access(path, X_OK))
     return strdup(path);

   return strdup(tool);
}

static Eina_Bool
_tool_run(const char *tool, const char *arg)
{
   pid_t pid;
   int status = 0;
   char *path = _tool_path_find(tool);

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, 0);

   pid = fork();
   if (pid == -1)
     {
        free(path);
        return 0;
     }
   else if (!pid)
     {
        if (arg)
          execlp(path, path, arg, NULL);
        else
          execlp(path, path, NULL);
        _exit(127);
     }

   waitpid(pid, &status, 0);
   free(path);

   return (WIFEXITED(status) && (WEXITSTATUS(status) == 0));
}

static char *
_pidfile_path(void)
{
   char path[PATH_MAX * 2 + 1];
   snprintf(path, sizeof(path), "%s/%s/%s", enigmatic_cache_dir_get(), PACKAGE, PID_FILE_NAME);
   return strdup(path);
}

char *
enigmatic_pidfile_path(void)
{
   return _pidfile_path();
}

// Store only (see enigmatic_log_lock).
Eina_Bool
enigmatic_pidfile_create(Enigmatic *enigmatic)
{
   char path[PATH_MAX * 2 + 1], *tmp;
   FILE *f;

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkdir(path);

   tmp = _pidfile_path();
   f = fopen(tmp, "w");
   if (!f)
     {
        free(tmp);
        return 0;
     }

   fprintf(f, "%i", enigmatic->pid);
   fclose(f);

   enigmatic->pidfile_path = tmp;

   return 1;
}

const char *
enigmatic_cache_dir_get(void)
{
   static char dir[PATH_MAX];
   static Eina_Bool found = 0;
   char *homedir;

   if (found)
     return dir;
   else
     {
        homedir = getenv("HOME");
        snprintf(dir, sizeof(dir), "%s/.cache", homedir);
        found = 1;
     }
   return dir;
}

void
enigmatic_pidfile_delete(Enigmatic *enigmatic)
{
   ecore_file_remove(enigmatic->pidfile_path);
   free(enigmatic->pidfile_path);
}

uint32_t
enigmatic_pidfile_pid_get(const char *path)
{
   pid_t pid;
   char buf[32];
   FILE *f = fopen(path, "r");
   if (!f)
     return 0;

   if (fgets(buf, sizeof(buf), f))
     pid = atoll(buf);
   else
     {
        fclose(f);
        return 0;
     }

   fclose(f);

   return pid;
}

static Eina_Bool
_pid_is_enigmatic(pid_t pid)
{
#if defined(__linux__)
   char path[PATH_MAX];
   char name[32];
   FILE *f;

   if (pid <= 1) return 0;

   snprintf(path, sizeof(path), "/proc/%d/comm", pid);
   f = fopen(path, "r");
   if (!f) return 0;

   if (!fgets(name, sizeof(name), f))
     {
        fclose(f);
        return 0;
     }
   fclose(f);

   char *nl = strchr(name, '\n');
   if (nl) *nl = '\0';

   return !strcmp(name, PACKAGE);
#else
   return pid > 1;
#endif
}

Eina_Bool
enigmatic_terminate(void)
{
   char *path = _pidfile_path();
   pid_t pid;
   Eina_Bool ok = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, 0);

   if (!ecore_file_exists(path))
     {
        free(path);
        return 0;
     }

   pid = enigmatic_pidfile_pid_get(path);
   if (pid <= 1)
     goto done;

   if (!_pid_is_enigmatic(pid))
     goto done;

   if (kill(pid, SIGTERM) == -1)
     {
        if (errno == ESRCH)
          {
             ecore_file_remove(path);
             ok = 1;
          }
        goto done;
     }

   for (int i = 0; i < 50; i++)
     {
        if (kill(pid, 0) == -1)
          {
             if (errno == ESRCH)
               {
                  ecore_file_remove(path);
                  ok = 1;
               }
             break;
          }
        usleep(100000);
     }

done:
   free(path);
   return ok;
}

Eina_Bool
enigmatic_launch(void)
{
   return _tool_run(PACKAGE"_start", NULL);
}

Eina_Bool
enigmatic_running(void)
{
   return _tool_run(PACKAGE, "-p");
}

char *
enigmatic_log_path(void)
{
   char path[PATH_MAX * 2 + 1];

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkdir(path);

   snprintf(path, sizeof(path), "%s/%s/%s", enigmatic_cache_dir_get(), PACKAGE, LOG_FILE_NAME);

   return strdup(path);
}

char *
enigmatic_log_directory(void)
{
   char path[PATH_MAX * 2 + 1];

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);

   return strdup(path);
}

void
enigmatic_about(void)
{
   printf("\n"
          "          _.,----,._\n"
          "        .:'        `:.\n"
          "      .'              `.\n"
          "     .'                `.\n"
          "     :                  :\n"
          "     `    .'`':'`'`/    '\n"
          "      `.   \\  |   /   ,'\n"
          "        \\   \\ |  /   /\n"
          "         `\\_..,,.._/'\n"
          "          {`'-,_`'-}\n"
          "          {`'-,_`'-}\n"
          "          {`'-,_`'-}\n"
          "           `YXXXXY'\n"
          "             ~^^~\n\n"
          "%s (%s)\n", PACKAGE, PACKAGE_VERSION);
   exit(0);
}
