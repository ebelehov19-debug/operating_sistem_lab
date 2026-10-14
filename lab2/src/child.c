#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"

int main(void)
{
    fprintf(stderr, "[child pid=%d] запущен, результат записывается в стандартный вывод\n",
            getpid());

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

        fprintf(stderr, "[child pid=%d] -> \"%s\"\n", getpid(), line);

        safe_write_all(STDOUT_FILENO, line, (size_t)strlen(line));
        safe_write_all(STDOUT_FILENO, "\n", 1);
    }

    if (ferror(stdin))
        die("getline");

    free(line);
    safe_close(STDOUT_FILENO);

    fprintf(stderr, "[child pid=%d] поток ввода закрыт родителем, завершаюсь\n", getpid());

    return EXIT_SUCCESS;
}
