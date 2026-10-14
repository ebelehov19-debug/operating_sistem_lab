#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

pid_t safe_fork(void)
{
    pid_t pid = fork();
    if (pid == -1)
        die("fork");
    return pid;
}

void safe_pipe(int fd[2])
{
    if (pipe(fd) == -1)
        die("pipe");
}

void safe_dup2(int oldfd, int newfd)
{
    if (dup2(oldfd, newfd) == -1)
        die("dup2");
}

void safe_close(int fd)
{
    if (close(fd) == -1)
        die("close");
}

int safe_open_write(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
        die("open");
    return fd;
}

void safe_write_all(int fd, const void *buf, size_t count)
{
    const char *p = (const char *)buf;
    size_t written = 0;
    while (written < count)
    {
        ssize_t n = write(fd, p + written, count - written);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            die("write");
        }
        if (n == 0)
        {
            errno = EIO;
            die("write (нулевая запись)");
        }
        written += (size_t)n;
    }
}

int safe_waitpid(pid_t pid, const char *who)
{
    int status = 0;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result == -1)
    {
        die("waitpid");
    }
    if (WIFEXITED(status))
    {
        fprintf(stderr, "[parent %d] процесс %s (pid %d) завершился с кодом %d\n",
                getpid(), who, pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        fprintf(stderr, "[parent %d] процесс %s (pid %d) завершён сигналом %d\n",
                getpid(), who, pid, WTERMSIG(status));
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* В корректном UTF-8 каждый символ начинается с байта, отличного от 10xxxxxx. */
size_t utf8_length(const char *s, size_t bytes)
{
    size_t length = 0;
    for (size_t i = 0; i < bytes; ++i)
        if (((unsigned char)s[i] & 0xC0) != 0x80)
            ++length;
    return length;
}

void remove_vowels(char *s)
{
    static const char *vowels = "aeiouAEIOU";
    char *src = s;
    char *dst = s;
    while (*src != '\0')
    {
        if (strchr(vowels, *src) == NULL)
        {
            *dst = *src;
            dst++;
        }
        src++;
    }
    *dst = '\0';
}
