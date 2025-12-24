// main.c
#include <ctype.h>
#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h> // access, close, write, dup2
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <sys/stat.h>

#include "strlist.h" // dynamic lists

#define PATH_BUFFER_SIZE 100

int space(int start, char *buffer) {
  while (buffer[start] != '\0' && isspace((unsigned char)buffer[start])) {
    start++;
  }
  return start;
}

static void push_arg(char ***argv, int *argc, const char *token) {
  *argv = realloc(*argv, (*argc + 2) * sizeof(char *));
  (*argv)[*argc] = strdup(token);
  (*argc)++;
  (*argv)[*argc] = NULL;
}

char **parse_input(const char *s, int *out_argc) {
  char **argv = NULL;
  int argc = 0;

  int cap = 64;
  char *tok = malloc(cap);
  size_t len = 0;
  tok[0] = '\0';

  int in_quote = 0;      // '...'
  int double_quote = 0;  // "..."

  for (int i = 0;; i++) {
    char c = s[i];
    int end = (c == '\0');

    if (end || (!in_quote && !double_quote && isspace((unsigned char)c))) {
      if (len > 0) {
        tok[len] = '\0';
        push_arg(&argv, &argc, tok);
        len = 0;
      }
      if (end) break;
      continue;
    }

    if (c == '\'' && !double_quote) {
      in_quote = !in_quote;
      continue;
    }
    if (c == '\"' && !in_quote) {
      double_quote = !double_quote;
      continue;
    }

    if (c == '\\' && !in_quote) {
      char next = s[i + 1];
      if (next == '\0') break;

      if (double_quote) {
        if (next == '"' || next == '\\' || next == '$') {
          if (len + 1 > (size_t)cap) { cap *= 2; tok = realloc(tok, cap); }
          tok[len++] = next;
          tok[len] = '\0';
          i++;
          continue;
        }
        if (len + 1 > (size_t)cap) { cap *= 2; tok = realloc(tok, cap); }
        tok[len++] = '\\';
        tok[len] = '\0';
        continue;
      }

      if (len + 1 > (size_t)cap) { cap *= 2; tok = realloc(tok, cap); }
      tok[len++] = (next == 'n') ? 'n' : next;
      tok[len] = '\0';
      i++;
      continue;
    }

    if (len + 1 > (size_t)cap) {
      cap *= 2;
      tok = realloc(tok, cap);
    }

    tok[len++] = c;
    tok[len] = '\0';
  }

  free(tok);
  if (out_argc) *out_argc = argc;
  return argv;
}

static void free_argv(char **argv) {
  if (!argv) return;
  for (int i = 0; argv[i]; i++) free(argv[i]);
  free(argv);
}

/* ======= AUTOCOMPLETE DATA ======= */
static StrList cmds_storage;
static StrList *cmds = &cmds_storage;

/* this function lists all the files in the PATH and adds them to cmds */
static void list_path_commands(void) {
  char *path = getenv("PATH");

  // reset cmds
  strlist_free(cmds);
  strlist_init(cmds);

  // add builtins
  strlist_add(cmds, "exit");
  strlist_add(cmds, "echo");
  strlist_add(cmds, "type");
  strlist_add(cmds, "pwd");
  strlist_add(cmds, "cd");

  if (!path) return;

  char *path_copy = strdup(path);
  char *token = strtok(path_copy, ":");

  while (token != NULL) {
    DIR *dir = opendir(token);
    if (dir) {
      struct dirent *dp;
      while ((dp = readdir(dir)) != NULL) {
        if (dp->d_name[0] == '.') continue; // skip . and hidden

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", token, dp->d_name);

        if (access(full, X_OK) == 0) {
          strlist_add_unique(cmds, dp->d_name);
        }
      }
      closedir(dir);
    }
    token = strtok(NULL, ":");
  }

  free(path_copy);
}

/* search for a match (FIXED) */
static char *command_generator(const char *text, int state) {
  static size_t idx;
  static size_t len;

  if (state == 0) {
    idx = 0;
    len = strlen(text);
  }

  while (cmds && idx < cmds->size) {
    const char *name = strlist_get(cmds, idx++);
    if (name && strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  return NULL;
}

static char **my_completion(const char *text, int start, int end) {
  (void)end;
  if (start == 0) {
    return rl_completion_matches(text, command_generator);
  }
  return NULL;
}

static char *trim_spaces(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  if (*s == '\0') return s;
  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return s;
}

static void free_pipeline(char ***cmds, int n_cmds) {
  if (!cmds) return;
  for (int i = 0; i < n_cmds; i++) free_argv(cmds[i]);
  free(cmds);
}

char ***build_pipeline_from_buffer(const char *buffer, int *out_n_cmds) {
  if (!buffer || !out_n_cmds) return NULL;

  char *copy = strdup(buffer);
  if (!copy) {
    perror("strdup");
    exit(1);
  }

  int cap = 4; // we accept up to 4 commands
  char ***cmds = malloc(cap * sizeof(char **));
  if (!cmds) {
    perror("malloc");
    exit(1);
  }

  int n = 0;

  char *token = strtok(copy, "|");

  while (token != NULL) {
    token = trim_spaces(token);
    if (*token == '\0') {
      break;
    }

    int argc = 0;
    char **argv = parse_input(token, &argc);
    if (!argv || !argv[0]) {
      free_argv(argv);
      break;
    }

    if (n == cap) {
      free_argv(argv);
      break;
    }

    cmds[n++] = argv;
    token = strtok(NULL, "|");
  }

  free(copy);
  *out_n_cmds = n;
  return cmds;
}

static int is_builtin(const char *cmd) {
  return cmd &&
    (!strcmp(cmd, "echo") ||
     !strcmp(cmd, "pwd")  ||
     !strcmp(cmd, "type") ||
     !strcmp(cmd, "cd")   ||
     !strcmp(cmd, "exit"));
}

/* returns 1 if handled as builtin, 0 if not */
static int run_builtin(char **argv) {
  if (!argv || !argv[0]) return 1;

  if (!strcmp(argv[0], "echo")) {
    for (int i = 1; argv[i]; i++) {
      printf("%s", argv[i]);
      if (argv[i+1]) printf(" ");
    }
    printf("\n");
    return 1;
  }

  if (!strcmp(argv[0], "pwd")) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    else perror("pwd");
    return 1;
  }

  if (!strcmp(argv[0], "cd")) {
    const char *p = argv[1] ? argv[1] : getenv("HOME");
    if (!p) p = "/";
    if (p[0] == '~') {
      const char *home = getenv("HOME");
      if (home) chdir(home);
      else perror("cd");
    } else {
      if (chdir(p) == -1) perror("cd");
    }
    return 1;
  }

  if (!strcmp(argv[0], "type")) {
    printf("%s is a shell builtin\n", argv[1] ? argv[1] : "");
    return 1;
  }

  if (!strcmp(argv[0], "exit")) {
    exit(0);
  }

  return 0;
}

void execute_pipe_cmds(char ***cmds, int n_cmds) {
  int prev_fd = -1;

  for (int i = 0; i < n_cmds; i++) {
    int pipefd[2] = {-1, -1};

    if (i < n_cmds - 1) {
      if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    }

    if (pid == 0) {
      if (prev_fd != -1) {
        if (dup2(prev_fd, STDIN_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
      }

      if (i < n_cmds - 1) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
      }

      if (prev_fd != -1) close(prev_fd);
      if (i < n_cmds - 1) close(pipefd[1]);

      if (is_builtin(cmds[i][0])) {
        run_builtin(cmds[i]);
        exit(0);
      }

      execvp(cmds[i][0], cmds[i]);
      perror("execvp");
      exit(1);
    }

    if (prev_fd != -1) close(prev_fd);

    if (i < n_cmds - 1) {
      close(pipefd[1]);
      prev_fd = pipefd[0];
    }
  }

  for (int i = 0; i < n_cmds; i++) {
    wait(NULL);
  }
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  char *path = getenv("PATH");

  char *commands[] = {"exit", "echo", "type", "pwd", "cd"};
  int cmdnumber = sizeof(commands) / sizeof(commands[0]);

  strlist_init(cmds);
  list_path_commands();
  rl_attempted_completion_function = my_completion;

  int path_change = 0;

  while (1) {
    char *line = readline("$ ");
    if (!line) break;

    if (*line) add_history(line);

    if (path_change == 1) {
      list_path_commands();
    }

    char buff[100];
    snprintf(buff, sizeof(buff), "%s", line);
    free(line);

    int redirect_output = 0;
    int redirect_error = 0;
    int fd_output = -1;
    int fd_error = -1;
    int piping = 0;
    int argc_gen = 0;
    char **argv_gen = parse_input(buff, &argc_gen);

    int out_n_cmds = 0;
    char ***argv_pipes = NULL;

    if (argc_gen == 0 || argv_gen == NULL) {
      free_argv(argv_gen);
      continue;
    }

    for (int i = 0; i < argc_gen; i++) {
      if (strcmp(argv_gen[i], "1>") == 0 || strcmp(argv_gen[i], ">") == 0 ||
          strcmp(argv_gen[i], ">>") == 0 || strcmp(argv_gen[i], "1>>") == 0) {
        if (i + 1 >= argc_gen || argv_gen[i + 1] == NULL) {
          fprintf(stderr, "syntax error: expected filename after >\n");
          redirect_output = 0;
          break;
        }

        redirect_output = 1;
        if (strcmp(argv_gen[i], ">>") == 0 || strcmp(argv_gen[i], "1>>") == 0) {
          fd_output = open(argv_gen[i + 1], O_CREAT | O_WRONLY | O_APPEND, 0644);
        } else {
          fd_output = open(argv_gen[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
        }
        if (fd_output == -1) {
          perror("open");
          redirect_output = 0;
        }
      }

      if (strcmp(argv_gen[i], "2>") == 0 || strcmp(argv_gen[i], "2>>") == 0) {
        if (i + 1 >= argc_gen || argv_gen[i + 1] == NULL) {
          fprintf(stderr, "syntax error: expected filename after 2>\n");
          redirect_error = 0;
          break;
        }

        redirect_error = 1;
        if (strcmp(argv_gen[i], "2>>") == 0) {
          fd_error = open(argv_gen[i + 1], O_CREAT | O_WRONLY | O_APPEND, 0644);
        } else {
          fd_error = open(argv_gen[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
        }
        if (fd_error == -1) {
          perror("open");
          redirect_error = 0;
        }
      }

      if (strcmp(argv_gen[i], "|") == 0) {
        piping = 1;
      }
    }

    if (piping == 1) {
      argv_pipes = build_pipeline_from_buffer(buff, &out_n_cmds);
      execute_pipe_cmds(argv_pipes, out_n_cmds);
      free_pipeline(argv_pipes, out_n_cmds);

      if (fd_output != -1) close(fd_output);
      if (fd_error != -1) close(fd_error);
      free_argv(argv_gen);
      continue;
    }

    if (strncmp(buff, "exit", 4) == 0) {
      free_argv(argv_gen);
      if (fd_output != -1) close(fd_output);
      if (fd_error != -1) close(fd_error);
      free_pipeline(argv_pipes, out_n_cmds);
      break;
    }

    else if (strncmp(buff, "echo", 4) == 0) {
      int cargc = 0;
      char **cargv = parse_input(buff + 4, &cargc);

      if (cargc == 0 || cargv == NULL) {
        free_argv(cargv);
      } else {
        for (int i = 0; i < cargc; i++) {
          if (redirect_output == 0) {
            if (strcmp(cargv[i], "2>") == 0 || strcmp(cargv[i], "2>>") == 0) {
              break;
            }
            printf("%s ", cargv[i]);
          } else {
            if (strcmp(cargv[i], ">") != 0 && strcmp(cargv[i], "1>") != 0 &&
                strcmp(cargv[i], ">>") != 0 && strcmp(cargv[i], "1>>") != 0) {
              write(fd_output, cargv[i], strlen(cargv[i]));
              if (i + 1 < cargc && strcmp(cargv[i + 1], ">") != 0 &&
                  strcmp(cargv[i + 1], "1>") != 0 && strcmp(cargv[i], ">>") != 0 &&
                  strcmp(cargv[i], "1>>") != 0) {
                write(fd_output, " ", 1);
              }
            } else {
              break;
            }
          }
        }
        if (redirect_output == 0) printf("\n");
        else write(fd_output, "\n", 1);

        free_argv(cargv);
      }
    }

    else if (strncmp(buff, "type", 4) == 0) {
      int n = 0;
      int sp = space(4, buff);
      for (int i = 0; i < cmdnumber; i++) {
        if (strncmp(buff + sp, commands[i], strlen(commands[i])) == 0) {
          if (redirect_output == 0) printf("%s is a shell builtin\n", buff + sp);
          else dprintf(fd_output, "%s is a shell builtin\n", buff + sp);
          n = 1;
          break;
        }
      }
      if (n == 0) {
        if (path != NULL) {
          char path_copy[PATH_MAX];
          snprintf(path_copy, sizeof(path_copy), "%s", path);
          char *strPaths = strtok(path_copy, ":");
          while (strPaths != NULL) {
            char pathBuilt[200];
            sprintf(pathBuilt, "%s/%s", strPaths, buff + sp);
            if (access(pathBuilt, X_OK) == 0) {
              n = 1;
              if (redirect_output == 0) printf("%s is %s\n", buff + sp, pathBuilt);
              else dprintf(fd_output, "%s is %s\n", buff + sp, pathBuilt);
              break;
            }
            strPaths = strtok(NULL, ":");
          }
        }
      }
      if (n == 0) {
        if (redirect_output == 0) printf("%s: not found\n", buff + 5);
        else dprintf(fd_output, "%s: not found\n", buff + 5);
      }
    }

    else if (strncmp(buff, "pwd", 3) == 0) {
      char abs_path[PATH_BUFFER_SIZE];
      if (getcwd(abs_path, PATH_BUFFER_SIZE) == NULL) {
        if (redirect_output == 0) {
          printf("couldn't get the working directory");
          if (errno == ERANGE) printf("path buffer size is too small");
          printf("\n");
        } else {
          dprintf(fd_output, "couldn't get the working directory");
          if (errno == ERANGE) dprintf(fd_output, "path buffer size is too small");
          dprintf(fd_output, "\n");
        }
      } else {
        if (redirect_output == 0) printf("%s\n", abs_path);
        else dprintf(fd_output, "%s\n", abs_path);
      }
    }

    else if (strncmp(buff, "cd", 2) == 0) {
      int sp = space(2, buff);
      char cd_path[100];
      strcpy(cd_path, buff + sp);
      if (strncmp(cd_path, "~", sizeof("~")) == 0) {
        chdir(getenv("HOME"));
      } else {
        if (chdir(cd_path) == -1) {
          if (redirect_error == 0) {
            printf("cd: %s: No such file or directory\n", cd_path);
          } else {
            dprintf(fd_error, "cd: %s: No such file or directory\n", cd_path);
          }
        }
      }
    }

    else {
      int n = 0;
      int cargc = 0;
      char **cargv = parse_input(buff, &cargc);

      if (cargc == 0 || cargv == NULL) {
        free_argv(cargv);
      } else {
        for (int k = 0; cargv[k] != NULL; k++) {
          if (strcmp(cargv[k], ">") == 0 ||
              strcmp(cargv[k], "1>") == 0 ||
              strcmp(cargv[k], "2>") == 0 ||
              strcmp(cargv[k], ">>") == 0 ||
              strcmp(cargv[k], "2>>") == 0 ||
              strcmp(cargv[k], "1>>") == 0) {
            cargv[k] = NULL;
            break;
          }
        }

        if (path != NULL) {
          char path_copy[PATH_MAX];
          snprintf(path_copy, sizeof(path_copy), "%s", path);
          char *strPaths = strtok(path_copy, ":");
          while (strPaths != NULL) {
            char pathBuilt[200];
            snprintf(pathBuilt, sizeof(pathBuilt), "%s/%s", strPaths, cargv[0]);
            if (access(pathBuilt, X_OK) == 0) {
              pid_t pid = fork();
              if (pid < 0) {
                perror("fork didn't work");
              } else if (pid == 0) {
                if (redirect_output == 1 && fd_output != -1) {
                  dup2(fd_output, STDOUT_FILENO);
                }
                if (redirect_error == 1 && fd_error != -1) {
                  dup2(fd_error, STDERR_FILENO);
                }

                if (fd_output != -1) close(fd_output);
                if (fd_error != -1) close(fd_error);

                execvp(pathBuilt, cargv);
                perror("execvp");
                exit(1);
              } else {
                n = 1;
                waitpid(pid, NULL, 0);
                break;
              }
            }
            strPaths = strtok(NULL, ":");
          }
          if (n == 0) {
            printf("%s: command not found \n", buff);
          }
        }
        free_argv(cargv);
      }
    }

    if (fd_output != -1) close(fd_output);
    if (fd_error != -1) close(fd_error);
    free_argv(argv_gen);
  }

  strlist_free(cmds);
  return 0;
}
