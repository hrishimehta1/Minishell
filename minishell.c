

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COLOR_BLUE "\x1b[34;1m"
#define COLOR_RESET "\x1b[0m"
#define MAX_CMD_LEN 1024
#define MAX_ARGS 256
#define HISTORY_SIZE 100
#define MAX_JOBS 100

typedef struct {
    pid_t pid;
    char cmd[MAX_CMD_LEN];
    int active;
} Job;

char *history[HISTORY_SIZE];
int history_count = 0;
Job jobs[MAX_JOBS];
int job_count = 0;

volatile sig_atomic_t interrupt_flag = 0;
volatile sig_atomic_t child_active = false;

void add_to_history(const char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    } else {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(cmd);
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

void handle_interrupt(int sig_num) {
    if (!child_active) {
        write(STDOUT_FILENO, "\n", 1);
    }
    interrupt_flag = 1;
}

void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                jobs[i].active = 0;
                break;
            }
        }
    }

    errno = saved_errno;
}

void add_job(pid_t pid, const char *cmd) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].cmd, cmd, MAX_CMD_LEN);
        jobs[job_count].active = 1;
        job_count++;
    }
}

void print_jobs() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            printf("[%d] %d %s\n", i + 1, jobs[i].pid, jobs[i].cmd);
        }
    }
}

void fg(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "miniShell: expected argument to \"fg\"\n");
        return;
    }
    int job_id = atoi(args[1]) - 1;
    if (job_id < 0 || job_id >= job_count || !jobs[job_id].active) {
        fprintf(stderr, "miniShell: no such job\n");
        return;
    }
    int status;
    waitpid(jobs[job_id].pid, &status, 0);
    jobs[job_id].active = 0;
}

void bg(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "miniShell: expected argument to \"bg\"\n");
        return;
    }
    int job_id = atoi(args[1]) - 1;
    if (job_id < 0 || job_id >= job_count || !jobs[job_id].active) {
        fprintf(stderr, "miniShell: no such job\n");
        return;
    }
    kill(jobs[job_id].pid, SIGCONT);
}

void cd(char *args[], int arg_count) {
    if (arg_count > 2) {
        fprintf(stderr, "Error: Too many arguments to cd.\n");
        return;
    }

    char *home_dir = getenv("HOME");
    char *arg = args[1];
    char fullPath[1024];

    if (arg == NULL) {
        arg = home_dir;
    } else if (arg[0] == '~') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", home_dir, arg + 1);
        arg = fullPath;
    }

    if (chdir(arg) != 0) {
        fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", arg, strerror(errno));
    }
}

void pwd() {
    char *curr_dir = getcwd(NULL, 0);
    if (curr_dir != NULL) {
        printf("%s\n", curr_dir);
        free(curr_dir);
    } else {
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
}

void lf() {
    DIR *dir;
    struct dirent *entry;

    char *curr_dir = getcwd(NULL, 0);
    if (curr_dir == NULL) {
        fprintf(stderr, "Error: Cannot open current directory. %s.\n", strerror(errno));
        return;
    }

    dir = opendir(curr_dir);
    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'. %s.\n", curr_dir, strerror(errno));
        free(curr_dir);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    free(curr_dir);
}

void lp() {
    DIR *dir;
    struct dirent *entry;
    char path[1024];
    char command_line[256];
    char user[256];

    if ((dir = opendir("/proc")) == NULL) {
        fprintf(stderr, "Error: Cannot open /proc directory. %s.\n", strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (isdigit(entry->d_name[0])) {
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                ssize_t bytes = read(fd, command_line, sizeof(command_line) - 1);
                close(fd);
                if (bytes > 0) {
                    command_line[bytes] = '\0';
                } else {
                    command_line[0] = '\0';
                }
            } else {
                command_line[0] = '\0';
            }

            snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);
            FILE *status_file = fopen(path, "r");
            if (status_file) {
                char line[256];
                while (fgets(line, sizeof(line), status_file) != NULL) {
                    if (strncmp(line, "Uid:", 4) == 0) {
                        int uid;
                        sscanf(line, "Uid: %d", &uid);
                        struct passwd *pw = getpwuid(uid);
                        if (pw != NULL) {
                            strncpy(user, pw->pw_name, sizeof(user) - 1);
                            user[sizeof(user) - 1] = '\0';
                        } else {
                            strncpy(user, "unknown", sizeof(user) - 1);
                            user[sizeof(user) - 1] = '\0';
                        }
                        break;
                    }
                }
                fclose(status_file);
            } else {
                strncpy(user, "unknown", sizeof(user) - 1);
                user[sizeof(user) - 1] = '\0';
            }

            printf("%s %s %s\n", entry->d_name, user, command_line);
        }
    }

    closedir(dir);
}

void execute_piped_command(char **args, int num_pipes) {
    int pipefds[2 * num_pipes];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    int j = 0;
    int command_index = 0;
    char *command[MAX_ARGS];
    int pipe_index = 0;

    while (args[j] != NULL) {
        if (strcmp(args[j], "|") == 0 || args[j + 1] == NULL) {
            if (args[j + 1] == NULL) {
                command[command_index++] = args[j];
            }
            command[command_index] = NULL;
            pid_t pid = fork();

            if (pid == 0) {
                if (pipe_index != 0) {
                    if (dup2(pipefds[(pipe_index - 1) * 2], 0) < 0) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                if (pipe_index < num_pipes) {
                    if (dup2(pipefds[pipe_index * 2 + 1], 1) < 0) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }

                for (int i = 0; i < 2 * num_pipes; i++) {
                    close(pipefds[i]);
                }

                if (execvp(command[0], command) == -1) {
                    perror("miniShell");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            pipe_index++;
            command_index = 0;
        } else {
            command[command_index++] = args[j];
        }
        j++;
    }

    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    for (int i = 0; i < num_pipes + 1; i++) {
        wait(NULL);
    }
}

void execute_command(char **args) {
    int num_pipes = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            num_pipes++;
        }
    }

    if (num_pipes > 0) {
        execute_piped_command(args, num_pipes);
    } else {
        int background = 0;
        int input_redirection = 0;
        int output_redirection = 0;
        int append_redirection = 0;
        char *input_file = NULL;
        char *output_file = NULL;

        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&") == 0) {
                background = 1;
                args[i] = NULL;
            } else if (strcmp(args[i], "<") == 0) {
                input_redirection = 1;
                input_file = args[i + 1];
                args[i] = NULL;
            } else if (strcmp(args[i], ">") == 0) {
                output_redirection = 1;
                output_file = args[i + 1];
                args[i] = NULL;
            } else if (strcmp(args[i], ">>") == 0) {
                append_redirection = 1;
                output_file = args[i + 1];
                args[i] = NULL;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
        } else if (pid == 0) {
            if (input_redirection) {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }
            if (output_redirection) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }
            if (append_redirection) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }
            if (execvp(args[0], args) == -1) {
                perror("miniShell");
            }
            exit(EXIT_FAILURE);
        } else {
            if (background) {
                add_job(pid, args[0]);
                signal(SIGCHLD, sigchld_handler);
            } else {
                int status;
                waitpid(pid, &status, 0);
            }
        }
    }
}

int is_builtin_command(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        cd(args, MAX_ARGS);
        return 1;
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(args[0], "history") == 0) {
        print_history();
        return 1;
    } else if (strcmp(args[0], "setenv") == 0) {
        if (args[1] == NULL || args[2] == NULL) {
            fprintf(stderr, "miniShell: expected arguments to \"setenv\"\n");
        } else {
            if (setenv(args[1], args[2], 1) != 0) {
                perror("miniShell");
            }
        }
        return 1;
    } else if (strcmp(args[0], "unsetenv") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "miniShell: expected argument to \"unsetenv\"\n");
        } else {
            if (unsetenv(args[1]) != 0) {
                perror("miniShell");
            }
        }
        return 1;
    } else if (strcmp(args[0], "jobs") == 0) {
        print_jobs();
        return 1;
    } else if (strcmp(args[0], "fg") == 0) {
        fg(args);
        return 1;
    } else if (strcmp(args[0], "bg") == 0) {
        bg(args);
        return 1;
    } else if (strcmp(args[0], "lf") == 0) {
        lf();
        return 1;
    } else if (strcmp(args[0], "lp") == 0) {
        lp();
        return 1;
    } else if (strcmp(args[0], "pwd") == 0) {
        pwd();
        return 1;
    }
    return 0;
}

void parse_command(char *cmd, char **args) {
    for (int i = 0; i < MAX_ARGS; i++) {
        args[i] = strsep(&cmd, " ");
        if (args[i] == NULL) break;
        if (strlen(args[i]) == 0) i--;
    }
}

int main() {
    char cmd[MAX_CMD_LEN];
    char *args[MAX_ARGS];
    struct sigaction sigact;
    char cwd[1024];

    memset(&sigact, 0, sizeof(struct sigaction));
    sigact.sa_handler = handle_interrupt;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigact, NULL);

    while (true) {
        if (interrupt_flag && !child_active) {
            printf("\n");
            interrupt_flag = 0;
        }

        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
            continue;
        }

        printf("%s[%s]%s$ ", COLOR_BLUE, cwd, COLOR_RESET);
        fflush(stdout);

        if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
            continue;
        }

        cmd[strcspn(cmd, "\n")] = 0; // Remove the newline character
        add_to_history(cmd);
        parse_command(cmd, args);

        if (args[0] == NULL) continue;

        if (!is_builtin_command(args)) {
            execute_command(args);
        }
    }

    return 0;
}
