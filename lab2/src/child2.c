#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Использование: %s <файл_для_записи_результата>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *out_path = argv[1];
    int out_fd = safe_open_write(out_path);
    safe_dup2(out_fd, STDOUT_FILENO);
    if (out_fd != STDOUT_FILENO)
        safe_close(out_fd);

    fprintf(stderr, "[child2 pid=%d] запущен, результаты пишутся в файл '%s'\n",
            getpid(), out_path);

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, stdin)) != -1)
    {
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
            len--;
        }

        remove_vowels(line);

        fprintf(stderr, "[child2 pid=%d] -> \"%s\"\n", getpid(), line);

        safe_write_all(STDOUT_FILENO, line, (size_t)strlen(line));
        safe_write_all(STDOUT_FILENO, "\n", 1);
    }

    if (ferror(stdin))
        die("getline");

    free(line);
    safe_close(STDOUT_FILENO);

    fprintf(stderr, "[child2 pid=%d] поток ввода закрыт родителем, завершаюсь\n", getpid());

    return EXIT_SUCCESS;
}
