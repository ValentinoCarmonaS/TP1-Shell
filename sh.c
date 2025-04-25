#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"

char prompt[PRMTLEN] = { 0 };
FILE *shell_history = NULL;

#define SIGSTACK_SZ (SIGSTKSZ)

static stack_t alt_stack;

// runs a shell command
static void
run_shell()
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL) {
		// Se escribe en el history el comando
		if (shell_history != NULL) {
			fprintf(shell_history, "%s\n", cmd);
			fflush(shell_history);
		}

		if (run_cmd(cmd) == EXIT_SHELL) {
			free(alt_stack.ss_sp);
			fclose(shell_history);
			return;
		}
	}
}

static void
sigchld_handler(int sig)
{
	int saved_errno = errno;
	pid_t pid;
	int status;

	// Buffer para el mensaje
	char buf[64];


	while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
		char *p = buf;

		// Armar string async-signal-safe
		const char *prefix = "==> terminado: PID=";
		size_t L = strlen(prefix);
		memcpy(p, prefix, L);
		p += L;

		// Contruir numero
		char digits[20];
		int n = 0;
		pid_t x = pid;
		if (x == 0) {
			digits[n++] = '0';
		} else {
			while (x > 0 && n < (int) sizeof(digits)) {
				digits[n++] = '0' + (x % 10);
				x /= 10;
			}
		}

		for (int i = 0; i < n / 2; ++i) {
			char tmp = digits[i];
			digits[i] = digits[n - 1 - i];
			digits[n - 1 - i] = tmp;
		}

		memcpy(p, digits, n);
		p += n;


		*p++ = '\n';


		write(STDOUT_FILENO, buf, p - buf);
	}

	errno = saved_errno;
}

// initializes the shell
// with the "HOME" directory
static void
init_shell()
{
	// Se obtiene la direccion del repositorio.
	char shell_history_path[PATH_MAX];

	if (getcwd(shell_history_path, sizeof(shell_history_path)) == NULL) {
		perror("Error al obtener la direccion del historial de la "
		       "shell");
		exit(EXIT_FAILURE);
	}
	strncat(shell_history_path,
	        "/.shell_history",
	        PATH_MAX - strlen(shell_history_path) - 1);

	// Se crea el historial de la shell en la direccion del repositorio, en vez de en home.
	shell_history = fopen(shell_history_path, "a+");
	if (shell_history == NULL) {
		perror("Error al crear el historial de la shell");
		exit(EXIT_FAILURE);
	}

	char buf[BUFLEN] = { 0 };
	char *home = getenv("HOME");

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(prompt, sizeof prompt, "(%s)", home);
	}

	// inicializar stack alternativo

	alt_stack.ss_sp = malloc(SIGSTKSZ);
	if (!alt_stack.ss_sp) {
		perror("malloc sigaltstack");
		exit(EXIT_FAILURE);
	}
	alt_stack.ss_size = SIGSTKSZ;
	alt_stack.ss_flags = 0;
	if (sigaltstack(&alt_stack, NULL) < 0) {
		perror("sigaltstack");
		exit(EXIT_FAILURE);
	}

	// Llamar handler usando la stack alternativo
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_ONSTACK;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

int
main(void)
{
	init_shell();

	run_shell();

	return 0;
}
