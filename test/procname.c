
/*
用于进程重命名，主进程、子进程使用不同的命令，便于命令ps -ef查看。
*/

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

# define MAXLINE 2048

#ifdef Linux
  #include <sys/prctl.h>
#endif

extern char **environ;

static char                **g_main_Argv = NULL;                /* pointer to argument vector */
static char                *g_main_LastArgv = NULL;        /* end of argv */

void prename_setproctitle_init(int argc, char **argv, char **envp)
{
    int i;

    for (i = 0; envp[i] != NULL; i++)
        continue;
    environ = (char **) malloc(sizeof (char *) * (i + 1));
    for (i = 0; envp[i] != NULL; i++)
        environ[i] = strdup(envp[i]);//xstrdup(envp[i]);
    environ[i] = NULL;

    g_main_Argv = argv;
    if (i > 0)
      g_main_LastArgv = envp[i - 1] + strlen(envp[i - 1]);
    else
      g_main_LastArgv = argv[argc - 1] + strlen(argv[argc - 1]);
}

void prename_setproctitle(const char *fmt, ...)
{
        char *p;
        int i;
        char buf[MAXLINE];

        extern char **g_main_Argv;
        extern char *g_main_LastArgv;
    va_list ap;
        p = buf;

    va_start(ap, fmt);
        vsprintf(p, fmt, ap);
        va_end(ap);


        i = strlen(buf);

        if (i > g_main_LastArgv - g_main_Argv[0] - 2)
        {
                i = g_main_LastArgv - g_main_Argv[0] - 2;
                buf[i] = '\0';
        }
        (void) strcpy(g_main_Argv[0], buf);
        p = &g_main_Argv[0][i];
        while (p < g_main_LastArgv)
                *p++ = '\0';//SPT_PADCHAR;
        g_main_Argv[1] = NULL;

#ifdef Linux
     prctl(PR_SET_NAME,buf);
#endif

}

int main(int argc, char *argv[], char *envp[])
{
    prename_setproctitle_init(argc, argv, envp);

    prename_setproctitle("%s@%s", "test_very_long_user_name_in_process_name", "192.168.123.145");

    while(1)
        sleep(60);

    return 0;
}

