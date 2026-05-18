/*
 * lsh.c — Little Shell
 * Based on Stephen Brennan's tutorial:
 * https://brennan.io/2015/01/16/write-a-shell-in-c/
 *
 * Added builtins: pwd, echo, history, env
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      /* fork, execvp, getcwd, chdir */
#include <sys/wait.h>    /* waitpid */

 /* ──────────────────────────────────────────────
    HISTORY  (max 100 entries)
    ────────────────────────────────────────────── */
#define HISTORY_MAX 100

static char* history[HISTORY_MAX];   /* مصفوفة مؤشرات للأوامر */
static int   history_count = 0;      /* عدد الأوامر المحفوظة  */

/* إضافة أمر لتاريخ الأوامر */
void history_add(const char* line) {
    if (history_count < HISTORY_MAX) {
        history[history_count++] = strdup(line); /* نسخة في الـ heap */
    }
    else {
        /* لو امتلأت: نحذف الأقدم ونحرّك الباقي */
        free(history[0]);
        memmove(&history[0], &history[1], (HISTORY_MAX - 1) * sizeof(char*));
        history[HISTORY_MAX - 1] = strdup(line);
    }
}

/* تحرير الذاكرة الخاصة بالسجل عند الخروج */
void history_free(void) {
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
        history[i] = NULL;
    }
    history_count = 0;
}

/* ──────────────────────────────────────────────
   FORWARD DECLARATIONS للـ builtins
   ────────────────────────────────────────────── */
int lsh_cd(char** args);
int lsh_help(char** args);
int lsh_exit(char** args);
int lsh_pwd(char** args);
int lsh_echo(char** args);
int lsh_history(char** args);
int lsh_env(char** args);

/* ──────────────────────────────────────────────
   جداول الـ BUILTINS
   ────────────────────────────────────────────── */

char* builtin_str[] = {
    "cd", "help", "exit", "pwd", "echo", "history", "env"
};

int (*builtin_func[])(char**) = {
    &lsh_cd, &lsh_help, &lsh_exit, &lsh_pwd, &lsh_echo, &lsh_history, &lsh_env
};

int lsh_num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char*);
}

/* ──────────────────────────────────────────────
   BUILTIN COMMANDS IMPLEMENTATION
   ────────────────────────────────────────────── */

   /* BUILTIN: cd */
int lsh_cd(char** args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: cd: missing argument\n");
    }
    else if (args[2] != NULL) {
        fprintf(stderr, "lsh: cd: too many arguments\n");
    }
    else {
        if (chdir(args[1]) != 0) {
            perror("lsh: cd");
        }
    }
    return 1;
}

/* BUILTIN: help */
int lsh_help(char** args) {
    (void)args;
    printf("=== Little Shell (lsh) ===\n");
    printf("Type program names and arguments, then press Enter.\n");
    printf("The following commands are built in:\n");
    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }
    printf("Use 'man' for info on other programs.\n");
    return 1;
}

/* BUILTIN: exit */
int lsh_exit(char** args) {
    (void)args;
    history_free(); /* نظّف الذاكرة قبل الخروج */
    return 0;       /* اخرج من الـ loop الرئيسي */
}

/* BUILTIN: pwd */
int lsh_pwd(char** args) {
    /* التحقق من وجود وسائط زائدة */
    if (args[1] != NULL) {
        fprintf(stderr, "lsh: pwd: too many arguments\n");
        return 1;
    }

    char* cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("lsh: pwd");
    }
    else {
        printf("%s\n", cwd);
        free(cwd); /* تحرير الذاكرة المخصصة بواسطة getcwd */
    }
    return 1;
}

/* BUILTIN: echo */
int lsh_echo(char** args) {
    if (args[1] == NULL) {
        printf("\n");
        return 1;
    }

    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) printf(" ");
        printf("%s", args[i]);
    }
    printf("\n");
    return 1;
}

/* BUILTIN: history */
int lsh_history(char** args) {
    /* التحقق من وجود وسائط زائدة */
    if (args[1] != NULL) {
        fprintf(stderr, "lsh: history: too many arguments\n");
        return 1;
    }

    if (history_count == 0) {
        printf("(no history yet)\n");
        return 1;
    }

    for (int i = 0; i < history_count; i++) {
        printf("%4d  %s\n", i + 1, history[i]);
    }
    return 1;
}

/* BUILTIN: env */
extern char** environ;

int lsh_env(char** args) {
    /* التحقق من وجود وسائط زائدة */
    if (args[1] != NULL) {
        fprintf(stderr, "lsh: env: too many arguments\n");
        return 1;
    }

    for (int i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
    return 1;
}

/* ──────────────────────────────────────────────
   EXECUTION & LAUNCHING
   ────────────────────────────────────────────── */

int lsh_launch(char** args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        /* الـ CHILD PROCESS */
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        /* فشل الـ fork */
        perror("lsh: fork");
    }
    else {
        /* الـ PARENT PROCESS */
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int lsh_execute(char** args) {
    if (args[0] == NULL) {
        return 1; /* أمر فارغ */
    }

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return lsh_launch(args);
}

/* ──────────────────────────────────────────────
   READING & TOKENIZATION
   ────────────────────────────────────────────── */
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM   " \t\r\n\a"

char* lsh_read_line(void) {
    char* line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("\n");
            exit(EXIT_SUCCESS); /* Ctrl+D خروج نظيف عند الـ */
        }
        else {
            perror("lsh: getline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char** lsh_split_line(char* line) {
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char** tokens = malloc(bufsize * sizeof(char*));
    char* token;

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

/* ──────────────────────────────────────────────
   MAIN LOOP
   ────────────────────────────────────────────── */
void lsh_loop(void) {
    char* line;
    char** args;
    int status;

    do {
        printf("lsh> ");
        fflush(stdout);

        line = lsh_read_line();

        /* الحفاظ على التاريخ بأمان وبدون كراش */
        if (line[0] != '\0' && line[0] != '\n') {
            char* nl = strchr(line, '\n');
            if (nl) *nl = '\0';  /* إزالة الـ newline مؤقتاً قبل الحفظ */

            history_add(line);

            if (nl) *nl = '\n';  /* [تعديل آمن لحماية الـ Pointer] إعادة الـ newline لتمريرها للـ split بشكل سليم */
        }

        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("Little Shell (lsh) — type 'help' for commands\n");
    lsh_loop();
    return EXIT_SUCCESS;
}