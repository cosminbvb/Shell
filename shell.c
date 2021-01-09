#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/history.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

void clear()
{
    printf("\e[1;1H\e[2J\n");
}

int read_command(char **command)
{
    int size = 0;
    int realloc_size = 4;
    *command = (char *)malloc(realloc_size * sizeof(char) + 1);
    char c;
    char *temp;

    while (1)
    {
        if (scanf("%c", &((*command)[size++])) == 1)
        {
            if ((*command)[size - 1] == '\\')
            {
                scanf("%c", &(*command)[size - 1]);
                fprintf(stderr, "> ");
                scanf("%c", &(*command)[size - 1]);
            }
            if ((*command)[size - 1] == '\n')
                break;

            if (size == realloc_size)
            {
                realloc_size *= 2;
                temp = (char *)realloc(*command, realloc_size * sizeof(char) + 1);
                if (!temp)
                {
                    perror(NULL);
                    return -1;
                }
                *command = temp;
            }
        }
        else
            break;
    }

    (*command)[--size] = '\0';
    return size;
}

int split(char *command, char ***argv)
{
    int argc = 0;
    int realloc_size = 4;
    *argv = (char **)malloc(realloc_size * sizeof(char *));

    char *token = strtok(command, " ");
    while (token != NULL)
    {
        (*argv)[argc++] = token;
        if (argc == realloc_size)
        {
            realloc_size *= 2;
            *argv = (char **)realloc(*argv, realloc_size * sizeof(char *) + 1);
        }
        token = strtok(NULL, " ");
    }

    (*argv)[argc] = NULL;
    return argc;
}

int hasPipe(char *command, char **pipe_split)
{
    //return values:
    //1 if pipe is found
    //0 otherwise
    //the functions also splits the command

    for (int i = 0; i < 2; i++)
    {
        pipe_split[i] = strsep(&command, "|");
        if (!pipe_split[i])
            break;
    }
    if (!pipe_split[1])
        return 0;
    return 1;
}

int execute(char *command, int command_size)
{
    char *copy = (char *)malloc(sizeof(char) * command_size + 1);
    strcpy(copy, command);

    char *pipe_split[2];
    if (hasPipe(command, pipe_split))
    {
        free(copy); //we only need it on else

        char **argvPipe1 = NULL;
        char **argvPipe2 = NULL;
        int argc1 = split(pipe_split[0], &argvPipe1);
        int argc2 = split(pipe_split[1], &argvPipe2);

        int pipefd[2];
        //pipe[0] - refers to the read end of the pipe
        //pipe[1] - refers to the write end of the pipe

        if (pipe(pipefd) == -1)
        {
            perror(NULL);
            return -1;
        }

        pid_t child1, child2;
        child1 = fork();

        if (child1 < 0)
        {
            perror(NULL);
            return -1;
        }
        if (child1 == 0) //child 1 writes
        {
            close(pipefd[0]);               //close unused read end
            dup2(pipefd[1], STDOUT_FILENO); //copy file descriptor 1 to stdout.
            close(pipefd[1]);
            if (execvp(argvPipe1[0], argvPipe1) == -1)
            {
                puts("Command failed");
                exit(EXIT_FAILURE); // kill child
            }
        }
        else
        {
            child2 = fork();
            if (child2 < 0)
            {
                perror(NULL);
                return -1;
            }
            if (child2 == 0)
            {
                close(pipefd[1]);              //close unused write end
                dup2(pipefd[0], STDIN_FILENO); //copy file descriptor 0 to stdin.
                close(pipefd[0]);
                if (execvp(argvPipe2[0], argvPipe2) == -1)
                {
                    puts("Command failed");
                    exit(EXIT_FAILURE); // kill child
                }
            }
            else
            {
                wait(NULL);
                wait(NULL);
                //the parent waits for his 2 children

                free(argvPipe1);
                free(argvPipe2);
            }
        }
    }
    else
    {
        char **argv = NULL;
        int argc = split(copy, &argv);

        //builtin cd
        if (!strcmp(argv[0], "cd"))
        {
            if (argc > 2)
            {
                puts("too many arguments");
                return -1;
            }
            if (argc < 2)
            {
                puts("not enough arguments");
                return -1;
            }
            chdir(argv[1]);
            free(copy);
            free(argv);
            return 0;
        }

        pid_t child = fork();
        if (child < 0)
        {
            perror(NULL);
            return -1;
        }
        if (child == 0)
        {
            if (execvp(argv[0], argv) == -1)
            {
                puts("Command failed");
                exit(EXIT_FAILURE); // kill child
            }
            //if execvp worked the process ends
        }
        else
        {
            wait(NULL);
            free(copy);
            free(argv);
        }
    }

    return 0;
}

int main()
{
    char *command = NULL;
    char cwd[4096];

    clear();

    while (1)
    {
        char *username = getenv("USER");

        getcwd(cwd, sizeof(cwd));

        fprintf(stderr, "%s:~%s$ ", username, cwd);

        int size = read_command(&command);

        if(!size) continue;

        if (strcmp(command, "exit") == 0)
        {
            free(command);
            break;
        }

        int execution_result = execute(command, size);

        free(command);
    }

    return 0;
}

// OBS:
/*
why not free(argv[i])?
"strtok only returns a pointer to a location inside the string you give it 
as input-- it doesn't allocate new memory for you, so shouldn't need to call
free on any of the pointers it gives you back in return."
*/