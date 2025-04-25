# shell

### Búsqueda en $PATH

1. cuáles son las diferencias entre la syscall `execve(2)` y la familia de wrappers proporcionados por la librería estándar de C (libc) `exec(3)`? \
Una diferencia es que los programas, en lugar de llamar directamente a la syscall `execve(2)`, suelen utilizar las funciones wrapper de la familia `exec(3)`. Estas funciones pertenecen a la biblioteca estándar de C (libc) y tienen como objetivo facilitar el uso de la syscall real `execve(2)`. \
Los wrapper `exec(3)` preparan los argumentos necesarios para llamar a `execve(2)` como el entorno envp o la busqueda del ejecutable en el PATH y ejecutan una instruccion de codigo llamada trap machine instruction que causa que el procesador pase del user mode al kernel mode y ejecute la syscall `execve(2)` que se encargara de reemplazar el proceso actual por otro.

2. ¿Puede la llamada a `exec(3)` fallar? ¿Cómo se comporta la implementación de la shell en ese caso? \
Cualquier llamada de la familia `exec(3)` puede fallar por varios motivos: el archivo no existe, no se tiene permisos de ejecucion, el ejecutable o no es valido o esta corrupto, se paso mal algun argumento o pudo haber fallado por no haber suficiente memoria. Cuando este falla, no se reemplaza el proceso y se sigue con el siguiente codigo despues del exec. \
En nuestra shell, cuando alguna funcion de la familia de `exec(3)` falla, imprimimos el error con perror y terminamos el proceso hijo donde se iba a ejecutar `exec(3)` con `_exit(-1)`


### Procesos en segundo plano
1. ¿Por qué es necesario el uso de señales? /
Es necesario debido a la necesidad de avisar e imprimir el output del proceso en segundo plano justo cuando termina, exactamente en el momento, cosa que es imposible de hacer de otra forma, ya que tendrias que hacer una verificacion atada a una accion como lanzar otro comando. Al ejecutar el Handler, el sistema se queda escuchando para hacer un cambio de contexto cuando SIGCHLD aparezca (en nuestro caso ocurriria especificamente cuando sea el SIGCHLD del pgid esperado), y cuando el proceso termine, lanza SIGCHLD, es captado por el wait y devuelve el resultado. Usar estas signals nos permite tambien manejar un stack alternativo que evita problemas incluso si el stack principal está casi lleno, con esto el handler siempre va a tener espacio para ejecutarse sin riesgo, aisla por completo cualquier problema de espacio que pueda tener el programa.


### Flujo estándar

1. Investigar el significado de 2>&1, explicar cómo funciona su forma general.
    * Mostrar qué sucede con la salida de cat out.txt en el ejemplo.
    * Luego repetirlo, invirtiendo el orden de las redirecciones (es decir, 2>&1 >out.txt). ¿Cambió algo? Compararlo con el comportamiento en bash(1). \

    **Significado de `2>&1` y su forma general**  
    El operador general para redirección de descriptores es  
    ```
    N>&M
    ```  
    que duplica el descriptor de archivo `M` sobre el descriptor `N`. 
    En particular:  
    ```
    2>&1
    ```  
    hace que stderr (2) apunte al mismo destino que stdout (1) en el momento de la redirección.

    - **Ejemplo**:  
        ```bash
        $ ls -C /home /noexiste >out.txt 2>&1
        ```  
        1. Primero `>out.txt` hace que stdout vaya a `out.txt`.  
        2. Luego `2>&1` duplica stderr sobre el mismo destino que stdout (que ya es `out.txt`).  
        ‑> Entonces, tanto la salida estándar como el mensaje de error se escriben en `out.txt`.

    - Para el orden invertido:  
        ```bash
        $ ls -C /home /noexiste 2>&1 >out.txt
        ```  
        1. Primero `2>&1` hace que stderr vaya al *antiguo* stdout (la pantalla).  
        2. Luego `>out.txt` redirige el *actual*stdout a `out.txt`, pero stderr ya fue redireccionado y **sigue** yendo a la pantalla.  
        ‑> Entonces el mensaje de error aparece en la consola, y `out.txt` quedará vacío.

    - **Comparación con bash(1)**  
        El comportamiento es idéntico al de bash:
        ```bash
        $ bash -c 'ls noexiste >f1 2>&1'; cat f1
        ls: no se puede acceder a 'noexiste': No existe el archivo o el directorio

        $ bash -c 'ls noexiste 2>&1 >f2'; cat f2
        # No aparecerá nada, el error se vio en pantalla
        ```


### Tuberías múltiples

1. Investigar qué ocurre con el exit code reportado por la shell si se ejecuta un pipe
    
    * ¿Cambia en algo?
    
    El exit code que reporta la shell (el que se puede consultar con $?) es el del último comando de la tubería. En bash, `set -o pipefail` hace que el exit code del pipeline completo sea el del primer comando que falle
    
    * ¿Qué ocurre si, en un pipe, alguno de los comandos falla? Mostrar evidencia (e.g. salidas de terminal) de este comportamiento usando bash. Comparar con su implementación. \

    Si algunos de los comando falla, se continuara con el procesamiento de las siguientes tareas en la tuberia, escribiendo los errores en stderr.


        ```bash
        $ cat no_existe | wc -c | wc -l
        cat: no_existe: No such file or directory
        2

        *implementacion propia*
        $ ./sh
        $ cat noexiste | wc -l | wc -c
        cat: noexiste: No such file or directory
        2
        ```

### Variables de entorno temporarias

1. ¿Por qué es necesario hacerlo luego de la llamada a `fork(2)`? \
Las variables de entorno temporarias se hacen despues de una llamada a `fork(2)`, especificamente en el proceso hijo, porque de lo contrario se modificaria el entorno del proceso padre permamentemente al cambiar sus variables de entorno. Al la Shell solo desear cambiar el entorno del comando que se va a ejecutar en vez de su entorno, esta  modifica el entorno del hijo antes de ejecutar el nuevo programa con `exec(3)`, para asi solo afectar al nuevo proceso.

2.  En algunos de los wrappers de la familia de funciones de `exec(3)` (las que finalizan con la letra e), se les puede pasar un tercer argumento (o una lista de argumentos dependiendo del caso), con nuevas variables de entorno para la ejecución de ese proceso. Supongamos, entonces, que en vez de utilizar `setenv(3)` por cada una de las variables, se guardan en un arreglo y se lo coloca en el tercer argumento de una de las funciones de `exec(3)`.
    * ¿El comportamiento resultante es el mismo que en el primer caso? Explicar qué sucede y por qué.
    * Describir brevemente (sin implementar) una posible implementación para que el comportamiento sea el mismo. \

    El comportamiento resultante es el mismo. La diferencia es que `setenv(3)` modifica el entorno global del proceso actual, lo que hara que cualquier `exec(3)` que no reciba un `envp` (nombre del tercer argumento de `exec(3)` con las nuevas variables de entorno) herede dichas variables. Mientras tanto, las funciones  como `execle()` o `execvpe()` reciben unicamente el entorno pasado por `envp` y no heredan las variables de entorno del proceso original, lo que permite evitar modificar el entorno global. \
    Una forma de que tengan el mismo comportamiento el usar esas funciones de `exec(3)` y usar `setenv(3)`, es pasandole mediante `envp` un arreglo con las variables del entorno actual (copiando `environ` por ej) y agregando manualmente las variables temporarias antes de pasarlo a la funcion.


### Pseudo-variables

1. Investigar al menos otras tres variables mágicas estándar, y describir su propósito.
    * Incluir un ejemplo de su uso en bash (u otra terminal similar). \
    
    `$$`: Devuelve el Process ID del proceso actual de la shell. Se puede usar para crear archivos temporales unicos
    ```bash
        echo "PID actual: $$"
        touch /tmp/archivo_$$.tmp
    ```  
    `$!`: Devuelve el Process ID del ultimo proceso ejecutado en segundo plano. Puede servir para monitorear o matar un proceso en segundo plano.
    ```bash
        python3 -m http.server 8000 &
        pid=$!

        echo "Servidor iniciado con PID $pid"

        sleep 10
        kill "$pid"
        echo "Servidor detenido despues de 10 segundos"
    ```      
    `$0`: Devuelve el nombre actual del script que se esta ejecutando o el nombre del proceso si esta en modo interactivo. Puede servir para mostrar ayuda contextual, logs o para saber como fue invocado el script.
    ```bash
        echo "Ejecutando el script $0"
    ```      

### Comandos built-in

1. ¿Entre `cd` y `pwd`, alguno de los dos se podría implementar sin necesidad de ser built-in? ¿Por qué? ¿Si la respuesta es sí, cuál es el motivo, entonces, de hacerlo como built-in? (para esta última pregunta pensar en los built-in como true y false ) \
Solo pwd se podria implementar como un comando externo al este solo necesitar consultar la ubicacion actual con `getcwd()`, no modificar nada del entorno del proceso padre y al poder funcionar como un binario separado como `/bin/pwd`. Mientras tanto cd se necesita que sea un built-in, porque si no cambiaria el directorio del proceso hijo que se crearia con fork para ejecutarlo, en vez de el de la shell principal; lo que haria que despues hacer el comando, el directorio actual no cambie.
La razon por la que pwd es un built-in, es la misma razon por la que true y false tambien lo son: es mas rapido al evitar lanzar procesos externos como `fork()` o `exec()` cuando no es necesario. Tambien porque es util para las shells que mantienen un estado interno del directorio por motivos de rendimiento o portabilidad.

### Historial

1. ¿Cuál es la función de los parámetros MIN y TIME del modo no canónico?

2. ¿Qué se logra en el ejemplo dado al establecer a MIN en 1 y a TIME en 0?

---
