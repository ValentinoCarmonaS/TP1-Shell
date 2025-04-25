#include "builtin.h"
#include <ctype.h>

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	return (strcmp(cmd, "exit") == 0);
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	// Saltar espacios en blanco al principio
	while (isspace(*cmd)) {
		cmd++;
	}

	// Verifica que comience con "cd"
	if (strncmp(cmd, "cd", 2) != 0)
		return 0;

	// Saltar "cd"
	cmd += 2;

	// Saltar espacios en blanco después de "cd"
	while (isspace(*cmd)) {
		cmd++;
	}


	if (*cmd == '\0') {
		// No hay argumento -> ir a HOME
		char *home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "cd: No se pudo obtener HOME\n");
			return 0;
		}
		if (chdir(home) < 0) {
			perror("cd: error cambiando al directorio HOME");
			return 0;
		}
	} else {
		// Hay argumento -> limpiar espacios al final
		char *start = cmd;
		char *end = cmd + strlen(cmd) - 1;

		// Eliminar espacios al final
		while (end > start && isspace(*end)) {
			*end = '\0';
			end--;
		}

		// Cambiar al directorio especificado
		if (chdir(start) < 0) {
			char buf[BUFLEN];
			snprintf(buf,
			         sizeof buf,
			         "cd: no se puede cambiar a %s",
			         start);
			perror(buf);
			return 0;
		}
	}

	// Actualizar el prompt con el nuevo directorio
	char cwd[BUFLEN];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(prompt, PRMTLEN, "(%s)", cwd);
	}

	return 1;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	// Verifica si el comando es "pwd" o "/bin/pwd"
	if (strcmp(cmd, "pwd") != 0 && strcmp(cmd, "/bin/pwd") != 0) {
		return 0;
	}

	char cwd[BUFLEN];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
		fflush(stdout);  // Asegurar que la salida se escriba inmediatamente
	} else {
		perror("pwd: error obteniendo el directorio actual");
		return 0;
	}

	return 1;
}

// returns true if `history` was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
history(char *cmd)
{
	// Your code here
	if (strcmp(cmd, "history") != 0)
		return 0;

	if (shell_history == NULL) {
		perror("Hubo un error al crear el historial de la shell");
		return 1;
	}

	char comando[BUFLEN];
	int num_comando = 1;
	rewind(shell_history);

	while (fgets(comando, BUFLEN, shell_history) != NULL) {
		printf("%i\t%s", num_comando, comando);
		num_comando++;
	}

	return 1;
}
