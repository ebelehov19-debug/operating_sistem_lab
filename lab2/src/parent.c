#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "io.h"

#define FILTER_LEN_THRESHOLD 10

static pid_t owner_pid;
static pid_t children[2] = {-1, -1};
static int channels[2][2] = {{-1, -1}, {-1, -1}};
static int output_fds[2] = {-1, -1};

static void cleanup(void)
{
    if (getpid() != owner_pid)
        return;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            if (channels[i][j] >= 0)
                close(channels[i][j]);
    for (int i = 0; i < 2; ++i)
        if (output_fds[i] >= 0 && close(output_fds[i]) == -1)
            perror("close (выходной файл)");
    for (int i = 0; i < 2; ++i)
        if (children[i] > 0)
            while (waitpid(children[i], NULL, 0) == -1 && errno == EINTR) {}
}

static void read_filename(const char *prompt, char **out_line)
{
    printf("%s", prompt);
    fflush(stdout);

    size_t cap = 0;
    ssize_t len = getline(out_line, &cap, stdin);
    if (len == -1)
    {
        if (ferror(stdin))
            die("getline (имя файла)");
        fprintf(stderr, "Не введено имя выходного файла\n");
        exit(EXIT_FAILURE);
    }

    if (len > 0 && (*out_line)[len - 1] == '\n')
        (*out_line)[len - 1] = '\0';
    if ((*out_line)[0] == '\0')
    {
        fprintf(stderr, "Имя выходного файла не должно быть пустым\n");
        exit(EXIT_FAILURE);
    }
}

static pid_t spawn_child(int index, const char *out_file, const char *label)
{
    if (fflush(stdout) == EOF)
        die("fflush");
    pid_t pid = safe_fork();
    if (pid == 0)
    {
        safe_dup2(channels[index][0], STDIN_FILENO);
        safe_dup2(output_fds[index], STDOUT_FILENO);

        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j)
                if (channels[i][j] > STDERR_FILENO)
                    safe_close(channels[i][j]);
            if (output_fds[i] > STDERR_FILENO)
                safe_close(output_fds[i]);
        }

        char *argv_child[] = { "child", NULL };
        execv("./child", argv_child);

        perror("execv(./child)");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "[parent %d] создан %s, pid=%d, файл результатов='%s'\n",
            getpid(), label, pid, out_file);
    return pid;
}

int main(void)
{
    owner_pid = getpid();
    if (atexit(cleanup) != 0)
    {
        fprintf(stderr, "Не удалось зарегистрировать очистку ресурсов\n");
        return EXIT_FAILURE;
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        die("signal(SIGPIPE)");
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd)
        if (fcntl(fd, F_GETFD) == -1)
            die("fcntl (стандартный дескриптор)");
    printf("[parent %d] запущен\n", getpid());

    char *file1 = NULL, *file2 = NULL;
    read_filename("Введите имя файла для результатов child1: ", &file1);
    read_filename("Введите имя файла для результатов child2: ", &file2);
    if (strcmp(file1, file2) == 0)
    {
        fprintf(stderr, "Для дочерних процессов нужны разные выходные файлы\n");
        free(file1);
        free(file2);
        return EXIT_FAILURE;
    }

    output_fds[0] = safe_open_write(file1);
    output_fds[1] = safe_open_write(file2);

    int *pipe1 = channels[0];
    int *pipe2 = channels[1];
    safe_pipe(pipe1);
    safe_pipe(pipe2);

    pid_t pid1 = spawn_child(0, file1, "child1");
    children[0] = pid1;
    pid_t pid2 = spawn_child(1, file2, "child2");
    children[1] = pid2;

    for (int i = 0; i < 2; ++i)
    {
        safe_close(output_fds[i]);
        output_fds[i] = -1;
    }

    safe_close(pipe1[0]);
    pipe1[0] = -1;
    safe_close(pipe2[0]);
    pipe2[0] = -1;

    printf("[parent %d] введите строки (Ctrl+D для завершения). "
           "Строки длиннее %d символов уходят в child2, остальные - в child1.\n",
           getpid(), FILTER_LEN_THRESHOLD);

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) != -1)
    {
        int has_newline = (len > 0 && line[len - 1] == '\n');
        size_t content_len = has_newline ? (size_t)(len - 1) : (size_t)len;
        size_t character_count = utf8_length(line, content_len);

        int target_fd = (character_count > FILTER_LEN_THRESHOLD) ? pipe2[1] : pipe1[1];
        const char *target_name = (character_count > FILTER_LEN_THRESHOLD) ? "pipe2 (child2)" : "pipe1 (child1)";

        safe_write_all(target_fd, line, has_newline ? (size_t)len : content_len);
        if (!has_newline)
            safe_write_all(target_fd, "\n", 1);

        printf("[parent %d] строка длиной %zu символов отправлена в %s\n",
               getpid(), character_count, target_name);
    }

    if (ferror(stdin))
        die("getline (ввод пользователя)");

    free(line);
    free(file1);
    free(file2);

    safe_close(pipe1[1]);
    pipe1[1] = -1;
    safe_close(pipe2[1]);
    pipe2[1] = -1;

    int status1 = safe_waitpid(pid1, "child1");
    children[0] = -1;
    int status2 = safe_waitpid(pid2, "child2");
    children[1] = -1;

    printf("[parent %d] оба дочерних процесса завершены, выход\n", getpid());
    return status1 == EXIT_SUCCESS && status2 == EXIT_SUCCESS
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
