// Sara Spagnoletto 556693885
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define BUFF_SIZE 100
#define PATH_MAX 4096
#define DELIM " "

typedef struct {
    char *line;
    pid_t id;
    int background;
} job;

job jobs[100]; // the array of all commands executed from program start
int jobs_number = 0;


int command_jobs(char **args, char *line);
int command_history(char **args, char *line);
int command_cd(char **args, char *line);
int command_exit(char **args, char *line);

char *builtin_str[] = {"jobs", "history", "cd", "exit"};
int (*builtin_func[])(char **, char *) = {&command_jobs, &command_history, &command_cd, &command_exit};

int num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

void free_jobs() {
    int i;
    for (i = 0; i < jobs_number; i++) {
        free(jobs[i].line);
    }
}

void add_job(char *line, pid_t id, int background) {
    jobs[jobs_number].line = (char *) malloc(BUFF_SIZE * sizeof(char));
    strcpy(jobs[jobs_number].line, line);
    jobs[jobs_number].id = id;
    jobs[jobs_number].background = background;
    jobs_number++;
}

void print_jobs() {
    int status, i;
    for (i = 0; i < jobs_number; i++) {
        if (jobs[i].background == 1) {
            pid_t result = waitpid(jobs[i].id, &status, WNOHANG);
            if (result == 0) {
                printf("%s\n", jobs[i].line);
            } else if (result > 0) {
                jobs[i].background = 0;
            } else {
                printf("An error occurred\n");
            }
        }
    }
}

int command_jobs(char **args, char *line) {
    print_jobs();
    add_job("jobs", 0, 0);
    return 1;
}

void print_history() {
    int status, i;
    for (i = 0; i < jobs_number; i++) {
        printf("%s ", jobs[i].line);
        fflush(stdout);
        if (jobs[i].background == 1) {
            pid_t result = waitpid(jobs[i].id, &status, WNOHANG);
            if (result == 0) {
                printf("RUNNING\n");
            } else if (result > 0) {
                printf("DONE\n");
                jobs[i].background = 0;
            } else {
                printf("An error occurred\n");
            }
        } else {
            printf("DONE\n");
        }
    }
    printf("history RUNNING\n");
}

int command_history(char **args, char *line) {
    print_history();
    add_job("history", 0, 0);
    return 1;
}

int command_cd(char **args, char *line) {
    add_job(line, 0, 0);
    int return_value;
    char path[PATH_MAX];
    // checking all cd cases
    if (args[2] != NULL) {
        printf("Too many arguments\n");
        return 1;
    }
    if (args[1] == NULL || !strcmp(args[1], "~")) {
        return_value = chdir(getenv("HOME"));
    } else if (args[1][0] == '~') {
        strcpy(path, getenv("HOME"));
        strcat(path, args[1] + 1);
        return_value = chdir(path);
    } else if (!strcmp(args[1], "-")) {
        return_value = chdir(getenv("OLDPWD"));
    } else {
        return_value = chdir(args[1]);
    }
    if (return_value == -1) {
        printf("chdir failed\n");
    } else {
        // setting old path and new path to use '-'
        setenv("OLDPWD", getenv("PWD"), 1);
        getcwd(path, sizeof(path));
        setenv("PWD", path, 1);
    }
    return 1;
}

int command_exit(char **args, char *line) {
    return 0;
}

// function for simple not builtin commands
int not_builtin(char **args, int background, char *line) {
    int status;
    pid_t pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
    } else if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            printf("exec failed\n");
        }
        return 0;
    } else {
        if (!background) {
            if (waitpid(pid, &status, 0) < 0) {
                printf("An error occurred\n");
            }
        }
        add_job(line, pid, background);
    }
    return 1;
}

// function to execute a command
int execute(char **args, int background, char *line) {
    int i, n = num_builtins();
    // calling builtin commands
    for (i = 0; i < n; i++) {
        if (!strcmp(args[0], builtin_str[i])) {
            return (*builtin_func[i])(args, line);
        }
    }
    // calling not builtin commands
    return not_builtin(args, background, line);
}

void free_all(char *line, char *line_copy, char **args) {
    free(line);
    free(line_copy);
    free(args);
}

// coping the line string to use it later
char *copy_line(char *line) {
    char *line_copy = malloc(BUFF_SIZE * sizeof(char *));
    strcpy(line_copy, line);
    return line_copy;
}

// function to remove "" in case the command is echo
void deal_with_echo(char *line_copy) {
    int i, j;
    if (!strncmp(line_copy, "echo", 4)) {
        for (i = 4; i < strlen(line_copy); i++) {
            if (line_copy[i] == '"') {
                for (j = i-- + 1; j <= strlen(line_copy); j++) {
                    line_copy[j - 1] = line_copy[j];
                }
            }
        }
    }
}

// function to parse the line into an array of args
char **parse_line(char *line, char *line_copy, int *background) {
    char **tokens = malloc(BUFF_SIZE * sizeof(char *));
    if (!tokens) {
        printf("An error occurred\n");
        exit(EXIT_FAILURE);
    }
    deal_with_echo(line_copy);
    int position = 0;
    char *token = strtok(line_copy, DELIM);
    while (token) {
        tokens[position] = token;
        position++;
        token = strtok(NULL, DELIM);
    }
    // changing the background option
    if (position > 0 && !strcmp(tokens[position - 1], "&")) {
        *background = 1;
        tokens[position - 1] = NULL;
        line[strlen(line) - 2] = '\0';
    } else {
        *background = 0;
        tokens[position] = NULL;
    }
    return tokens;
}

// function to read and return one line of input as string
char *read_line() {
    int i = 0, c;
    char *buffer = malloc(BUFF_SIZE * sizeof(char));
    if (!buffer) {
        printf("An error occurred");
        exit(EXIT_FAILURE);
    }
    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            while (i > 1 && isspace((unsigned char) *(buffer + i - 1))) i--;
            *(buffer + i) = '\0';
            return buffer;
        } else {
            buffer[i] = (char) c;
        }
        i++;
    }
}

void run_shell() {
    char *line, *line_copy, **args;
    int status, background;
    do {
        printf("$ ");
        fflush(stdout);
        line = read_line();                                 // reading the input
        line_copy = copy_line(line);                        // saving the line string
        args = parse_line(line, line_copy, &background);    // parsing the array of args
        status = execute(args, background, line);           // executing command
        free_all(line, line_copy, args);                    // freeing allocated memory
    } while (status);
}

int main() {
    run_shell();
    free_jobs();
    return 0;
}