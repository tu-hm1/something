#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#if defined(__unix__) || defined(__unix)
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#define wait_key getchar
#else
#include <windows.h>
#include <psapi.h>
#include <conio.h>
#define wait_key getch
#endif

#ifdef __MINGW32__
int _CRT_glob = 0;
#endif

int ret;
long long mem;
timeval tv;
double cl;

bool chk_space(const char *str)
{
    char last = 0;
    while (str && *str)
    {
        if ((*str == ' ' || *str == '\t') && last != '\\')
            return true;
        last = *str++;
    }
    return false;
}

#if defined(__unix__) || defined(__unix)
void get_mem_info(char *path, long long &mem)
{
    int fd, data, dum;
    char buf[128];

    if ((fd = open(path, O_RDONLY)) < 0)
        return;

    read(fd, buf, 128);
    buf[128] = '\0';
    close(fd);

    sscanf(buf, "%d %d %d %d %d %d %d",
           &dum, &dum, &dum, &dum, &dum, &data, &dum);

    if (mem < data)
        mem = data;
}

void exec_unix(char *path, char *cmdline)
{
    char *f = strrchr(path, '/') + 1, *name = new char[strlen(path)];
    strncpy(name, f, sizeof(f));
    pid_t pid = vfork();

    gettimeofday(&tv, NULL);
    cl = tv.tv_sec + (double)tv.tv_usec / 1000000;

    if (pid < 0)
        exit(1);
    else if (pid == 0)
        execl(path, name, cmdline, (char *)0);
    else
    {
        pid_t pid2;
        struct rusage usage;
        char path[NAME_MAX];
        sprintf(path, "/proc/%d/statm", pid);

        do
        {
            get_mem_info(path, mem);
            pid2 = wait4(pid, &ret, WNOHANG, &usage);
        } while (pid2 == 0);
    }

    gettimeofday(&tv, NULL);
    cl = (tv.tv_sec + (double)tv.tv_usec / 1000000) - cl;

    mem = mem * sysconf(_SC_PAGESIZE);

    if (WIFEXITED(ret))
        ret = WEXITSTATUS(ret);
    else
        ret = -1;
}
#else
void exec_win(char *cmdline)
{
    SetConsoleTitle(cmdline);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    PROCESS_MEMORY_COUNTERS pmc;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&pmc, sizeof(pmc));

    gettimeofday(&tv, NULL);
    cl = tv.tv_sec + (double)tv.tv_usec / 1000000;
    CreateProcess(NULL, TEXT(cmdline), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    WaitForSingleObject(pi.hProcess, INFINITE);
    gettimeofday(&tv, NULL);
    cl = (tv.tv_sec + (double)tv.tv_usec / 1000000) - cl;

    DWORD tmp;
    GetExitCodeProcess(pi.hProcess, &tmp);
    ret = tmp;

    GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc));
    mem = pmc.PeakPagefileUsage;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}
#endif

int main(int argc, char **argv)
{

    if (argc < 2)
    {
        printf("Usage: runner <filename> <args ...>\n");
        return 1;
    }

    size_t fullsize = 0;
    for (int i = 1; i < argc; ++i)
        fullsize += strlen(argv[i]);

    fullsize += argc + 32;

    char *cmdline = new char[fullsize];
    memset(cmdline, 0, fullsize);

#if !defined(__unix__) && !defined(__unix)
    bool sp = chk_space(argv[1]);
    if (sp)
        strcat(cmdline, "\"");
    strcat(cmdline, argv[1]);
    if (sp)
        strcat(cmdline, "\"");
    strcat(cmdline, " ");
#endif

    for (int i = 2; i < argc; ++i)
    {
        bool sp = chk_space(argv[i]);
        if (sp)
            strcat(cmdline, "\"");
        strcat(cmdline, argv[i]);
        if (sp)
            strcat(cmdline, "\"");
        strcat(cmdline, " ");
    }

#if defined(__unix__) || defined(__unix)
    exec_unix(argv[1], cmdline);
#else
    exec_win(cmdline);
#endif

    printf("\n=====\n");
    printf("Process returned with exit code %d (0x%08X).\n", ret, ret);
    printf("Execution time: %0.3f s, memory used: %lld KB.\n", cl, mem / 1024);
    printf(
        "Press "
#if defined(__unix__) || defined(__unix)
        "ENTER"
#else
        "any key"
#endif
        " to continue ... ");

    wait_key();

    delete[] cmdline;
    return ret;
}