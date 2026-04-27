#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include "enigmatic_util.h"

static char *
_enigmatic_path_find(void)
{
   char exe[PATH_MAX], dir[PATH_MAX], path[PATH_MAX * 2];
   ssize_t len;

   len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
   if (len <= 0)
     return strdup("enigmatic");

   exe[len] = '\0';
   snprintf(dir, sizeof(dir), "%s", exe);

   char *slash = strrchr(dir, '/');
   if (!slash)
     return strdup("enigmatic");
   *slash = '\0';

   snprintf(path, sizeof(path), "%s/enigmatic", dir);
   if (!access(path, X_OK))
     return strdup(path);

   snprintf(path, sizeof(path), "%s/../enigmatic", dir);
   if (!access(path, X_OK))
     return strdup(path);

   return strdup("enigmatic");
}

static Eina_Bool
_enigmatic_ping(void)
{
   pid_t pid;
   int status = 0;
   char *path = _enigmatic_path_find();

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, 0);

   pid = fork();
   if (pid == -1)
     {
        free(path);
        return 0;
     }
   else if (!pid)
     {
        execlp(path, path, "-p", NULL);
        _exit(127);
     }

   waitpid(pid, &status, 0);
   free(path);

   return (WIFEXITED(status) && (WEXITSTATUS(status) == 0));
}

static void
launch(void)
{
   char *path = _enigmatic_path_find();
   if (!path) _exit(1);
   execlp(path, path, NULL);
   free(path);
   _exit(1);
}

static Eina_Bool
_pidfile_proc_alive(void)
{
   FILE *f;
   char *path;
   char buf[32];
   long pid;
   Eina_Bool alive = 0;

   path = enigmatic_pidfile_path();
   if (!path) return 0;

   f = fopen(path, "r");
   if (!f)
     {
        free(path);
        return 0;
     }

   if (fgets(buf, sizeof(buf), f))
     {
        pid = strtol(buf, NULL, 10);
        if (pid > 1)
          {
             if (kill((pid_t) pid, 0) == 0 || errno == EPERM)
               alive = 1;
          }
     }

   fclose(f);
   free(path);
   return alive;
}

static Eina_Bool
_daemon_ready(void)
{
   if (!_pidfile_proc_alive()) return 0;
   return _enigmatic_ping();
}

int
main(int argc, char **argv)
{
   pid_t pid;
   int running = 0;

   signal(SIGCHLD, SIG_IGN);

   if (_daemon_ready()) return 0;

   /* If an old instance is shutting down, wait briefly for it to disappear. */
   for (int i = 0; i < 20; i++)
     {
        if (_daemon_ready()) return 0;
        if (!_pidfile_proc_alive()) break;
        usleep(100000);
     }

   pid = fork();
   if (pid == -1)
     {
        fprintf(stderr, "fork()\n");
        exit(1);
     }
   else if (pid == 0)
     {
        close(fileno(stdin));
        close(fileno(stdout));
        close(fileno(stderr));
        launch();
     }
   else
     {
        for (int i = 0; i < 60; i++)
          {
             if (_daemon_ready())
               running = 1;
             else
               running = 0;
             if (running) break;
             usleep(1000000 / 10);
          }
     }

   return !running;
}
