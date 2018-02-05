#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_TOKENS 100
#define MAX_LINE_LEN 1024
#define STDIN 0
#define STDOUT 1

// Global shell variables
char ttyname2[64] = "mysh";
int exitstatus = 0;
char cwd[MAX_LINE_LEN];
bool istty;
bool backgroundprocess;
bool redirectout;
char outpath[256];
bool redirectin;
char inpath[256];
int stdin_org;
int stdout_org;
bool prompt;


// Misc
int tokennum;
char *tokens[MAX_TOKENS];
int pipetokennum;
char *pipetokens[MAX_TOKENS];

// Prototype
void executecommand();

void readtoken(char* line, int* index, char delim) {
    char *token = malloc(256 * sizeof(char));
    char current = line[*index];
    int tokenIndex = 0;

    while (current != delim && current != '\0') {
        token[tokenIndex] = current;
        (*index)++;
        tokenIndex++;
        current = line[*index];
    }
    token[tokenIndex] = '\0';
    tokens[tokennum] = token;
}

void tokenize(char *line, bool check) {
    // Remove \n
    size_t len = strlen(line);
    if (line[len - 1] == '\n') line[len - 1] = '\0';
    len--;
    tokennum = 0;

    for (int i = 0; i < len; ++i) {
       if (isspace(line[i])) {
           continue;
       } else if (line[i] == '"') {
           i++;
           readtoken(line, &i, '"');
           i++;
           tokennum++;
       } else {
           readtoken(line, &i, ' ');
           tokennum++;
       }
    }

    // Check for redirects
    if (check) {
        int numofargs;
        if (tokennum == 2) numofargs = 1;
        else numofargs = 3;

        for (int j = 1; j <= numofargs; ++j) {
            if (tokennum >= 2) {
                char *tok = tokens[tokennum - j];

                if (tok[0] == '<') {
                    redirectin = true;
                    strncpy(inpath, tok + 1, strlen(tok));
                }

                if (tok[0] == '>') {
                    redirectout = true;
                    strncpy(outpath, tok + 1, strlen(tok));
                }
            }
        }

        // Check for background jobs
        char *lasttok = tokens[tokennum - 1];
        backgroundprocess = lasttok[0] == '&';

        // Remove tokens
        int offset = redirectin + redirectout + backgroundprocess;
        tokennum -= offset;
    }
}

int ignore(char *line) {
    if (line[0] == '#') return 1;

    size_t len = strlen(line);
    for (int i = 0; i < len; ++i) {
        if (!isspace(line[i])) {
            if (line[i] == '#') return 1;
            return 0;
        }
    }

    return 1;
}

void name(char *name) {
    if (tokennum > 1) strcpy(ttyname2, name);
    else printf("%s\n", ttyname2);
}

void status() {
    printf("%d\n", exitstatus);
}

void exit_() {
    int shellexitstatus;
    if (tokennum > 1) shellexitstatus = atoi(tokens[1]);
    else shellexitstatus = 0;
    exit(shellexitstatus);
}

void print() {
    for (int i = 1; i < tokennum - 1; ++i) {
        printf("%s ", tokens[i]);
    }
    printf("%s", tokens[tokennum - 1]);
}

void echo() {
    print();
    printf("\n");
}

void pid() {
    printf("%d\n", getpid());
}

void ppid() {
    printf("%d\n", getppid());
}

void help() {
    printf("      name - Print or change shell name\n      help - Print short help\n    status - Print last command status\n      exit - Exit from shell\n     print - Print arguments\n      echo - Print arguments and newline\n       pid - Print PID\n      ppid - Print PPID\n dirchange - Change directory\n  dirwhere - Print current working directory\n   dirmake - Make directory\n dirremove - Remove directory\n   dirlist - List directory\ndirinspect - Inspect directory\n  linkhard - Create hard link\n  linksoft - Create symbolic/soft link\n  linkread - Print symbolic link target\n  linklist - Print hard links\n    unlink - Unlink file\n    rename - Rename file\n    remove - Remove file or directory\n     cpcat - Copy file\n     pipes - Create pipeline\n");
}

void dirwhere() {
    printf("%s\n", cwd);
}

void dirchange() {
    int result;
    if (tokennum > 1) {
        result = chdir(tokens[1]);
    } else {
        result = chdir("/");
    }

    if (result != 0) {
        exitstatus = errno;
        perror("dirchange");
    } else {
        getcwd(cwd, MAX_LINE_LEN);
        exitstatus = 0;
    }
}

void dirmake() {
    char path[MAX_LINE_LEN];
    strcpy(path, cwd);
    strcat(path, "/");
    strcat(path, tokens[1]);

    int result = mkdir(path, S_IRWXU);

    if (result != 0) {
        exitstatus = errno;
        perror("dirmake");
    } else {
        exitstatus = 0;
    }
}

void unlink_(char *str) {
    char path[256];
    if (str == NULL) {
        strcpy(path, cwd);
        strcat(path, "/");
        strcat(path, tokens[1]);
    } else {
        strcpy(path, str);
    }

    int result = unlink(tokens[1]);
    if (result != 0) {
        exitstatus = errno;
        perror("unlink");
    } else {
        exitstatus = 0;
    }
}

void dirremove() {
    char path[MAX_LINE_LEN];
    strcpy(path, cwd);
    strcat(path, "/");
    strcat(path, tokens[1]);

    // Try to delete dir
    int result = rmdir(path);

    if (result != 0) {
        exitstatus = errno;
        perror("dirremove");
    } else {
        exitstatus = 0;
    }
}

void dirlist() {
    char path[MAX_LINE_LEN];
    strcpy(path, cwd);

    if (tokennum > 1) {
        strcat(path, "/");
        strcat(path, tokens[1]);
    }

    DIR *dir = opendir(path);

    if (dir == NULL) {
        exitstatus = errno;
        perror("dirlist");
    } else {
        struct dirent *directoryEntry = readdir(dir);
        while (directoryEntry != NULL) {
            printf("%s  ", directoryEntry->d_name);
            directoryEntry = readdir(dir);
        }
        printf("\n");
        closedir(dir);
    }
}

void linkhard() {
    int result = link(tokens[1], tokens[2]);
    if (result != 0) {
        exitstatus = errno;
        perror("linkhard");
    } else {
        exitstatus = 0;
    }
}

void linksoft() {
    int result = symlink(tokens[1], tokens[2]);
    if (result != 0) {
        exitstatus = errno;
        perror("linksoft");
    } else {
        exitstatus = 0;
    }
}

void linkread() {
    char path[256];
    strcpy(path, cwd);
    strcat(path, "/");
    strcat(path, tokens[1]);

    size_t bufsize = 256;
    char buf[bufsize];
    int result = readlink(path, buf, bufsize);
    buf[result] = '\0';

    if (result == -1) {
        exitstatus = errno;
        perror("linkread");
    } else {
        printf("%s\n", buf);
        exitstatus = 0;
    }
}

void rename_() {
    int result = rename(tokens[1], tokens[2]);
    if (result != 0) {
        exitstatus = errno;
        perror("rename");
    } else {
        exitstatus = 0;
    }
}

void cpcat() {
    int src, dst;

    if (tokens[1][0] == '-'){
        src = STDIN_FILENO;
        dst = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    } else if (tokennum == 2) {
        src = open(tokens[1], O_RDONLY);
        dst = STDOUT_FILENO;
    } else if (tokennum == 3) {
        src = open(tokens[1], O_RDONLY);
        dst = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    } else {
        src = STDIN_FILENO;
        dst = STDOUT_FILENO;
    }

    if (src == -1 || dst == -1) {
        exitstatus = errno;
        perror("cpcat");
    } else {
        char buf[1];
        while (read(src, (void*) &buf, sizeof(char))) {
            write(dst, (void*) buf, sizeof(char));
        }

        close(src);
        close(dst);
    }
}

void linklist() {
    DIR *dir = opendir(cwd);
    struct dirent *directory_entry = readdir(dir);
    struct stat filestat;

    // Find original file inode
    int result = lstat(tokens[1], &filestat);
    if (result == -1) {
        exitstatus = errno;
        perror("linklist");
    } else {
        int target_ino = filestat.st_ino;
        int target_nlink = filestat.st_nlink;

        bool nomatch = true;
        while (directory_entry != NULL) {
            lstat(directory_entry->d_name, &filestat);

            if (target_ino == filestat.st_ino && filestat.st_nlink > 1 && filestat.st_nlink == target_nlink) {
                printf("%s ", directory_entry->d_name);
                nomatch = false;
            }

            directory_entry = readdir(dir);
        }

        if (!nomatch) printf("\n");

        closedir(dir);
        exitstatus = 0;
    }
}

void nonbuiltin() {
    int pid = fork();
    if (pid == 0) {
        char tmp[256] = "/bin/";
        strcat(tmp, tokens[0]);
        tokens[tokennum] = NULL;

        execvp(tokens[0], tokens);
        raise(SIGKILL);
    } else {
        int statloc;
        waitpid(pid, &statloc, 0);
    }
}

pid_t create_process(int in, int out, char **tokens) {
    int pid;

    if ((pid = fork()) == 0) {
        if (in != 0) {
            dup2(in, 0);
            close(in);
        }

        if (out != 1) {
            dup2(out, 1);
            close(out);
        }

        executecommand();
        raise(SIGKILL);
    }

    return pid;
}

void pipes() {
    pid_t pid;
    int fd[2];
    int in = STDIN_FILENO;
    prompt = false;
    pid_t pids[tokennum - 1];

    // Save all tokens
    pipetokennum = tokennum;
    for (int i = 0; i < tokennum; ++i) {
        pipetokens[i] = strdup(tokens[i]);
    }

    for (int i = 1; i < pipetokennum - 1; ++i) {
        tokenize(pipetokens[i], false);

        pipe(fd);

        pids[i - 1] = create_process(in, fd[1], tokens);

        close(fd[1]);
        in = fd[0];
    }

    // Final stage
    if (in != 0) dup2(in, 0);
    tokenize(pipetokens[pipetokennum - 1], false);
    pids[pipetokennum - 2] = create_process(in, fd[1], tokens);

    for (int i = 0; i < pipetokennum - 1; ++i) {
        int status;
        waitpid(pids[i], &status, 0);
    }
}

void executecommand() {
    // Simple built-in commands
    if (strcmp(tokens[0], "name") == 0) name(tokens[1]);
    else if (strcmp(tokens[0], "help") == 0) help();
    else if (strcmp(tokens[0], "status") == 0) status();
    else if (strcmp(tokens[0], "exit") == 0) exit_();
    else if (strcmp(tokens[0], "print") == 0) print();
    else if (strcmp(tokens[0], "echo") == 0) echo();
    else if (strcmp(tokens[0], "pid") == 0) pid();
    else if (strcmp(tokens[0], "ppid") == 0) ppid();

        // Built-in directory manipulation commands
    else if (strcmp(tokens[0], "dirwhere") == 0) dirwhere();
    else if (strcmp(tokens[0], "dirchange") == 0) dirchange();
    else if (strcmp(tokens[0], "dirmake") == 0) dirmake();
    else if (strcmp(tokens[0], "dirremove") == 0) dirremove();
    else if (strcmp(tokens[0], "dirlist") == 0) dirlist();

        // Other built-in file manipulations commands
    else if (strcmp(tokens[0], "linkhard") == 0) linkhard();
    else if (strcmp(tokens[0], "linksoft") == 0) linksoft();
    else if (strcmp(tokens[0], "linkread") == 0) linkread();
    else if (strcmp(tokens[0], "unlink") == 0) unlink_(NULL);
    else if (strcmp(tokens[0], "rename") == 0) rename_();
    else if (strcmp(tokens[0], "cpcat") == 0) cpcat();
    else if (strcmp(tokens[0], "linklist") == 0) linklist();
    else if (strcmp(tokens[0], "pipes") == 0) pipes();
    else nonbuiltin();
}

void zombie_handler(int signum) {
    int status;
    waitpid(-1, &status, WNOHANG);
}

void reset() {
    redirectin = false;
    redirectout = false;
    tokennum = 0;
    prompt = istty;

    // Reset stdin, stdout file descriptors
    dup2(stdin_org, STDIN_FILENO);
    dup2(stdout_org, STDOUT_FILENO);
}

void redirect(int desc) {
    fflush(stdout);
    if (desc == STDIN) {
        int file = open(inpath, O_RDONLY);
        dup2(file, STDIN_FILENO);
    } else if (desc == STDOUT) {
        int file = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        dup2(file, STDOUT_FILENO);
        prompt = false;
    }
}

void init() {
    // Initialize global variables
    backgroundprocess = false;
    redirectout = false;
    redirectin = false;

    // Get current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) perror("getcwd error");

    // Check if shell is in interactive mode
    istty = (bool) isatty(1);
    prompt = istty;

    // Set signal handlers
    signal(SIGCHLD, zombie_handler);

    // Save stdin and stdout file descriptors
    stdin_org = dup(STDIN_FILENO);
    stdout_org = dup(STDOUT_FILENO);

    // Disable stdout buffering
    setbuf(stdout, NULL);
}

int main() {

    // Initialize shell
    init();

    // REPL
    while (1) {
        // Reset shell environment
        reset();

        // Prompt
        if (true) printf("%s> ", ttyname2);
        fflush(stdout);

        // Read line
        char line[MAX_LINE_LEN] = "";
        char* res = fgets(line, MAX_LINE_LEN - 1, stdin);
        if (res == NULL) exit(0);

        // Check for whitespace or comments
        if (ignore(line)) continue;

        tokenize(line, true);

        // Fork if command is a background process
        int childpid;
        if (backgroundprocess) {
            childpid = fork();
            if (childpid > 0) continue;
        }

        // Redirect streams
        if (redirectout) redirect(STDOUT);
        if (redirectin) redirect(STDIN);

        executecommand();

        // End background tasks
        if (backgroundprocess && childpid == 0) raise(SIGKILL);
    }

    return 0;
}