#include "exec.h"
#include <string.h>
#include "runcmd.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define SYMLINK_BUFSIZE 1024

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
//
static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
//
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here
static void
set_environ_vars(char **eargv, int eargc)
{
	int x;
	int posicion;
	char value[ARGSIZE];
	char key[ARGSIZE];

	for (x = 0; x < eargc; x++) {
		get_environ_key(eargv[x], key);
		posicion = block_contains(eargv[x], '=');
		get_environ_value(eargv[x], value, posicion);
		setenv(key, value, 1);
	}
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and  S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int flags, int oldFd)
{
	int fd;
	if (file[0] == '&') {
		fd = atoi(file + 1);
		// Verificar que sea un descriptor válido
		int flags2 = fcntl(fd, F_GETFL);

		if ((flags2 & O_ACCMODE) == O_RDONLY) {
			fprintf(stderr, "fd %d es solo lectura\n", fd);
		}

		if (flags2 == -1) {
			fprintf(stderr,
			        "Descriptor %d inválido para redirección "
			        "%d>&%d\n",
			        fd,
			        oldFd,
			        fd);
			_exit(1);
		}
		if (dup2(fd, oldFd) == -1) {
			perror("Error en dup2");
			_exit(1);
		}

		return oldFd;
	} else if (flags >> 2) {
		fd = open(file, flags, S_IWUSR | S_IRUSR);

	} else {
		fd = open(file, flags);
	}
	if (fd < 0) {
		perror("Error al redireccionar");
		_exit(-1);
	}

	if (dup2(fd, oldFd) == -1) {
		perror("Error en dup2");
		_exit(1);
	}

	return fd;
}

// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option
void
exec_cmd(struct cmd *cmd)
{
	// To be used in the different cases
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:

		if (shell_history)
			fclose(shell_history);

		// spawns a command
		e = (struct execcmd *) cmd;
		set_environ_vars(e->eargv, e->eargc);
		if (e->argc > 0) {
			execvp(e->argv[0], e->argv);
			perror("Error en execvp");
			_exit(-1);
		}

		break;

	case BACK: {
		b = (struct backcmd *) cmd;
		exec_cmd(b->c);
		break;
	}

	case REDIR: {
		if (shell_history)
			fclose(shell_history);

		// changes the input/output/stderr flow
		//
		// To check if a redirection has to be performed
		// verify if file name's length (in the execcmd struct)
		// is greater than zero
		//
		r = (struct execcmd *) cmd;
		int fd;


		if (strlen(r->in_file) > 0) {
			int fd = open_redir_fd(r->in_file, O_RDONLY, STDIN_FILENO);
			if (fd != STDIN_FILENO && fd != STDOUT_FILENO &&
			    fd != STDERR_FILENO) {
				close(fd);
			}
		}

		if (strlen(r->out_file) > 0) {
			int fd = open_redir_fd(r->out_file,
			                       O_WRONLY | O_CREAT | O_TRUNC,
			                       STDOUT_FILENO);
			if (fd != STDIN_FILENO && fd != STDOUT_FILENO &&
			    fd != STDERR_FILENO) {
				close(fd);
			}
		}

		if (strlen(r->err_file) > 0) {
			int fd = open_redir_fd(r->err_file,
			                       O_WRONLY | O_CREAT | O_TRUNC,
			                       STDERR_FILENO);
			if (fd != STDIN_FILENO && fd != STDOUT_FILENO &&
			    fd != STDERR_FILENO) {
				close(fd);
			}
		}

		set_environ_vars(r->eargv, r->eargc);
		if (r->argc > 0) {
			execvp(r->argv[0], r->argv);
			perror("Error en execvp al hacer una redireccion");
			_exit(-1);
		}
		break;
	}

	case PIPE: {
		// pipes two commands
		p = (struct pipecmd *) cmd;

		int pipefd[2];
		if (pipe(pipefd) == -1) {
			perror("pipe failed");
			free_command(parsed_pipe);
			exit(EXIT_FAILURE);
		}

		// Fork para el primer comando (izquierda)
		pid_t left_pid = fork();
		if (left_pid == -1) {
			perror("fork left failed");
			free_command(parsed_pipe);
			exit(EXIT_FAILURE);
		}


		if (left_pid == 0) {
			setpgid(0, 0);
			// Proceso hijo izquierdo: produce la salida
			dup2(pipefd[1], STDOUT_FILENO);  // Redirige stdout
			close(pipefd[0]);                // No lee del pipe
			close(pipefd[1]);
			exec_cmd(p->leftcmd);  // Ejecuta el comando izquierdo

			free_command(parsed_pipe);
			exit(0);
		}

		// Fork para el segundo comando (derecha)
		if (left_pid > 0) {
			pid_t right_pid = fork();
			if (right_pid == -1) {
				perror("fork right failed");
				free_command(parsed_pipe);
				exit(EXIT_FAILURE);
			}
			if (right_pid == 0) {
				setpgid(0, 0);
				// Proceso hijo derecho: consume la entrada
				dup2(pipefd[0], STDIN_FILENO);  // Redirige stdin
				close(pipefd[0]);
				close(pipefd[1]);  // No escribe en el pipe

				exec_cmd(p->rightcmd);  // Ejecuta el comando derecho
				free_command(parsed_pipe);
				exit(0);
			}

			// Proceso padre
			close(pipefd[0]);
			close(pipefd[1]);

			// Esperar a ambos hijos
			waitpid(left_pid, NULL, 0);
			waitpid(right_pid, NULL, 0);

			free_command(parsed_pipe);
			exit(0);
		}


		break;
	}
	}
}
