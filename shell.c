#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define INPUT 0
#define OUTPUT 1
#define APPEND 2

void removeWhiteSpace(char* buf) {
    if (buf[strlen(buf) - 1] == ' ' || buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';
    if (buf[0] == ' ' || buf[0] == '\n') memmove(buf, buf + 1, strlen(buf));
}

void tokenize_buffer(char** param, int *nr, char *buf, const char *c) {
    char *token;
    token = strtok(buf, c);
    int pc = -1;
    while (token) {
        param[++pc] = malloc(sizeof(token) + 1);
        strcpy(param[pc], token);
        removeWhiteSpace(param[pc]);
        token = strtok(NULL, c);
    }
    param[++pc] = NULL;
    *nr = pc;
}

void print_params(char ** param) {
    while (*param) {
        printf("param=%s..\n", *param++);
    }
}

void executeBasic(char** argv) {
    if (fork() > 0) {
        wait(NULL);
    } else {
        if (strcmp(argv[0], "sa") == 0) strcpy(argv[0], "ls");
        if (strcmp(argv[0], "noise") == 0) strcpy(argv[0], "echo");
        execvp(argv[0], argv);
        perror(ANSI_COLOR_RED "invalid input" ANSI_COLOR_RESET "\n");
        exit(1);
    }
}

void executePiped(char** buf, int nr) {
    if (nr > 10) return;

    int fd[10][2], i, pc;
    char *argv[100];

    for (i = 0; i < nr; i++) {
        tokenize_buffer(argv, &pc, buf[i], " ");
        if (i != nr - 1) {
            if (pipe(fd[i]) < 0) {
                perror("pipe creating was not successful\n");
                return;
            }
        }
        if (fork() == 0) {
            if (strcmp(argv[0], "sa") == 0) strcpy(argv[0], "ls");
            if (strcmp(argv[0], "noise") == 0) strcpy(argv[0], "echo");
            if (i != nr - 1) {
                dup2(fd[i][1], 1);
                close(fd[i][0]);
                close(fd[i][1]);
            }
            if (i != 0) {
                dup2(fd[i - 1][0], 0);
                close(fd[i - 1][1]);
                close(fd[i - 1][0]);
            }
            execvp(argv[0], argv);
            perror("invalid input ");
            exit(1);
        }
        if (i != 0) {
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL);
    }
}

void executeAsync(char** buf, int nr) {
    int i, pc;
    char *argv[100];
    for (i = 0; i < nr; i++) {
        tokenize_buffer(argv, &pc, buf[i], " ");
        if (fork() == 0) {
            if (strcmp(argv[0], "sa") == 0) strcpy(argv[0], "ls");
            if (strcmp(argv[0], "noise") == 0) strcpy(argv[0], "echo");
            execvp(argv[0], argv);
            perror("invalid input ");
            exit(1);
        }
    }
    for (i = 0; i < nr; i++) {
        wait(NULL);
    }
}

void executeRedirect(char** buf, int nr, int mode) {
    int pc, fd;
    char *argv[100];
    removeWhiteSpace(buf[1]);
    tokenize_buffer(argv, &pc, buf[0], " ");
    if (fork() == 0) {
        if (strcmp(argv[0], "sa") == 0) strcpy(argv[0], "ls");
        if (strcmp(argv[0], "noise") == 0) strcpy(argv[0], "echo");

        switch (mode) {
            case INPUT:  fd = open(buf[1], O_RDONLY); break;
            case OUTPUT: fd = open(buf[1], O_WRONLY); break;
            case APPEND: fd = open(buf[1], O_WRONLY | O_APPEND); break;
            default: return;
        }

        if (fd < 0) {
            perror("cannot open file\n");
            return;
        }

        switch (mode) {
            case INPUT: dup2(fd, 0); break;
            case OUTPUT: dup2(fd, 1); break;
            case APPEND: dup2(fd, 1); break;
            default: return;
        }
        execvp(argv[0], argv);
        perror("invalid input ");
        exit(1);
    }
    wait(NULL);
}

void showHelp() {
    printf(ANSI_COLOR_GREEN "----------Help--------" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Supported commands: sa (ls), noise (echo), cd, pwd, exit " ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Commands can be piped, redirected, or run asynchronously.\n" ANSI_COLOR_RESET);
}

int main(char** argv, int argc) {
    char buf[500], *buffer[100], cwd[1024];
    int nr = 0;

    printf(ANSI_COLOR_GREEN "**************************CUSTOM SHELL***************************" ANSI_COLOR_RESET "\n");

    while (1) {
        printf(ANSI_COLOR_BLUE "Enter command (or 'exit' to exit): " ANSI_COLOR_RESET "\n");

        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf(ANSI_COLOR_GREEN "%s " ANSI_COLOR_RESET, cwd);
        else
            perror("getcwd failed\n");

        fgets(buf, 500, stdin);

        if (strchr(buf, '|')) {
            tokenize_buffer(buffer, &nr, buf, "|");
            executePiped(buffer, nr);
        } else if (strchr(buf, '&')) {
            tokenize_buffer(buffer, &nr, buf, "&");
            executeAsync(buffer, nr);
        } else if (strstr(buf, ">>")) {
            tokenize_buffer(buffer, &nr, buf, ">>");
            if (nr == 2) executeRedirect(buffer, nr, APPEND);
            else printf("Incorrect output redirection!(command >> file)\n");
        } else if (strchr(buf, '>')) {
            tokenize_buffer(buffer, &nr, buf, ">");
            if (nr == 2) executeRedirect(buffer, nr, OUTPUT);
            else printf("Incorrect output redirection!(command > file)\n");
        } else if (strchr(buf, '<')) {
            tokenize_buffer(buffer, &nr, buf, "<");
            if (nr == 2) executeRedirect(buffer, nr, INPUT);
            else printf("Incorrect input redirection!(command < file)\n");
        } else {
            char *params[100];
            tokenize_buffer(params, &nr, buf, " ");
            if (strstr(params[0], "cd")) {
                chdir(params[1]);
            } else if (strstr(params[0], "help")) {
                showHelp();
            } else if (strstr(params[0], "exit")) {
                exit(0);
            } else {
                executeBasic(params);
            }
        }
    }
    return 0;
}
