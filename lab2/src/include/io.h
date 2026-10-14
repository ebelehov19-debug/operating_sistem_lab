#ifndef IO_H
#define IO_H

#include <sys/types.h>
#include <stddef.h>

void die(const char *msg);

pid_t safe_fork(void);
void safe_pipe(int fd[2]);
void safe_dup2(int oldfd, int newfd);
void safe_close(int fd);
int safe_open_write(const char *path);

void safe_write_all(int fd, const void *buf, size_t count);

int safe_waitpid(pid_t pid, const char *who);

size_t utf8_length(const char *s, size_t bytes);

void remove_vowels(char *s);

#endif
