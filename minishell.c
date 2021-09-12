#include "parser.h"
#define _GNU_SOURCE
#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void UnEjecucion(tline *line);
void DosEjecucion(tline *line);
void nEjecucion(tline *line);

void Cd(tline *line, char *cwd);

void RedireccionEntrada(tline *line);
void RedireccionSalida(tline *line);

void handler(int SIGHLD){
	waitpid(WAIT_ANY,NULL,WNOHANG); // para esperar los procesos en background
	
}
 

int main(void){
	
	signal (SIGINT, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	
	signal(SIGCHLD, handler);
	

	char buffe[1024];
	char cwd[1024];
	
	tline * line;
	
	//Prompt
	printf("%s $ ", getcwd(cwd, sizeof(cwd)));	

	while(fgets(buffe,1024, stdin)) {	 //Lectura de Mandatos
		
		line = tokenize(buffe); 
		
	  if(sizeof(line) == 0){
		  continue; 		//prompt de nuevo (no realizamos nada)
		}
		
	  if (line->ncommands == 1){ //si tiene 1 unico mandato.
		if ( strcmp(line->commands[0].argv[0], "cd") ==0 ){ 	//debemos comprobar el mandato si es cd
	  			Cd(line, cwd);
	  		}
	  		else{
	  			UnEjecucion(line);
	  		}
		}
	
		if (line->ncommands == 2){  	//En caso de tener 2 mandatos.
			DosEjecucion(line);
		}
		
		if (line->ncommands > 2){  //cuando la linea tiene + de 2 mandatos
			nEjecucion(line);
		}
		printf("%s $ ", getcwd(cwd, sizeof(cwd)));	//Prompt
	}
	return 0;
}

void Cd(tline *line, char *cwd) {


	
  //Posibles situaciones que pueden darse:
  if( line->commands[0].argv[1] == NULL ){	// 1º Si es solo cd =  HOME: 
		
		chdir(getenv("HOME"));
  }
	
  else if(line->commands[0].argc > 1){ 	// 2º en caso de que es cd  x 
	if (strcmp(line->commands[0].argv[1], "..") == 0){ 	//3º en caso de: x == ".." ir al padre 
		chdir(dirname(cwd));							//entonces hacer printf("PWD = %s\n", dirname(getenv()));
	}
	else { 
		if(chdir(line->commands[0].argv[1])<0)
			fprintf(stderr,"error cd\n");
	}
  }
}

/* Cuando se produce redireccion de entrada o salida se debe comprobar que el archivo existe, 
se habrira y redirecionara una entrada o salida dup2 */

void RedireccionSalida (tline *line){
   
   int ficheD; // (redirect_output) fichero descrp de redireccion de salida 

	if (line->redirect_output != NULL) { //en caso de redirec d salida
		ficheD = open(line->redirect_output,O_WRONLY|O_CREAT,S_IRWXU); 	//se abre dicho archivo para modificarlo
		if (ficheD != -1){ //ahora contemplamos posibles errores de redirec d salida.
			dup2(ficheD,1); //En caso de haber error deberemos cambiar la entrada standar x el fichero
		
		}
		else {
		  printf("No se ha creado o abierto el archivo ");
			
		}		
		ficheD = close (ficheD); // Cuando se finalice e cerrara el fichero
		
		if (ficheD == -1){
			
		  printf ("No se cerro correcmente el archivo");
		}
	}
	
	if (line->redirect_error != NULL) { //para saber si contiene algun error de redirec
		
		printf("Error de redireccion : %s\n", line->redirect_error);
	}
}

void RedireccionEntrada (tline *line){
	
	int RDE;  // nombre que asiganamos al archivo de redireccion de entrada

	if(line->redirect_input != NULL){ //Si se produce redireccion de entrada
	  RDE = open(line->redirect_input,O_RDONLY);
					
	  if(RDE != -1) { //Si tiene errores redireccion de entrada:
		dup2(RDE,0); //pasamos la entrada estandar x el fichero.
	  }
		else{
			
			printf("No se puede abrir el archivo");
		}
		RDE = close (RDE); 	//cerramos el fichero cuando termine
		
		if(RDE == -1){
			printf ("No se ha cerrado correcamente el archivo");
		}
	}
	if (line->redirect_error != NULL){ //para saber un posible error de redirec
		
		printf(" Error de redireccion : %s\n", line->redirect_error);
	}
}

void UnEjecucion(tline *line){  
   
	int pid = fork();
	int status;
	
	if(pid < 0){ 	//Error  
	
		fprintf(stderr, "Error del fork().\n%s\n", strerror(errno));
		
		exit(1);
	}

	if(pid == 0){ 	// Proceso Hijo  y redirecciones tanto entrada como salida:
		
		if (!line->background){
			 //Si recibe la señal SIGQUIT o SIGINT
			signal(SIGQUIT,SIG_DFL);
			signal(SIGINT,SIG_DFL);
		}
		RedireccionEntrada(line);
		RedireccionSalida(line);

		execvp(line->commands[0].filename, line->commands[0].argv); //se ejecutaria el mandato
			
		printf("Error al ejecutar el comando: %s\n", strerror(errno)); //Error execvp			
	}
			
	else{ 	//Padre (process) 
		if (!line->background){
			waitpid (pid,&status,0);
			if(WIFEXITED(status) != 0)
				
				if(WEXITSTATUS(status) != 0)
					printf("No se ha ejecutado el comando correcamente\n");
		}
	}
}

void DosEjecucion(tline *line){
	
	
	int status;
	int p[2]; //tuberia
	pipe(p); //creamos la tuberia 
	pid_t pid1, pid2; //Proceso principal, proceso 1º, proceso 2º

	pid1 = fork(); // Se Crea un hijo que indicara que poner dentro del pipe
		
	
	if (pid1 == 0){ // 1º Hijo escribe
		if (!line->background){
			 //Si recibe la señal SIGQUIT o SIGINT
			signal(SIGQUIT,SIG_DFL);
			signal(SIGINT,SIG_DFL);
		}
		close(p[0]); //salida del pipe se cierra
		
		dup2(p[1],1); // se redirec la salida estandar aal pipe

		RedireccionEntrada(line); //Gestion de redirecciones. Solo puede ser de Entrada

		execvp(line->commands[0].filename, line->commands[0].argv); //Ejecucion del mandato
		fprintf(stderr,"Error al ejecutar el comando%s\n", strerror(errno));
		exit(1);	
	}

	pid2 = fork();//Creamos el hijo encargado de leer
		
	if (pid2 == 0){  	// El 2º Hijo lee
		if (!line->background){
			 //Si recibe la señal SIGQUIT o SIGINT
			signal(SIGQUIT,SIG_DFL);
			signal(SIGINT,SIG_DFL);
		}
		close(p[1]); 	// la entrada del pie se cierra
		
		dup2(p[0],0);	// Redirec Entrada standar al pipe

		RedireccionSalida(line); //Gestion de redirecciones. Solo puede ser de Salida
			
		execvp(line->commands[1].filename, line->commands[1].argv); //Ejecucion del mandato
		fprintf(stderr,"Error al ejecutar el comando%s\n", strerror(errno));
		exit(1);	
	}
	
	else{ //Padre wait a terminen los procesos y cierra el pipe.
		
	
		
		close(p[0]);
		close(p[1]);
		if (!line->background){
			waitpid (pid1,&status,0);
			if(WIFEXITED(status) != 0)
			
				if(WEXITSTATUS(status) != 0)
					printf("No se ejecuto el comando de forma adecuada\n");
					
			waitpid (pid2,&status,0);
			if(WIFEXITED(status) != 0)
			
				if(WEXITSTATUS(status) != 0)
					printf("No se ejecuto el comando de forma adecuada\n");
		}
	}
}

void nEjecucion(tline *line){
	
	
	int y; // para el for del final
	int  i;
	int p[2];
	int pNuevo[2];
	pid_t pids[128];
	
	
	pipe(p);
	pid_t pid;

	for (i = 0; i<line->ncommands; i++ ) {
	  if (i > 0 && i<line->ncommands-1) { 
		pipe(pNuevo);
	  }
	  pid = fork(); //Creamos los hijos
	  pids[i]=pid;
	  if(pid<0){
		fprintf(stderr,"Error al crear el proceso\n");
		exit(1);
	  }

	  if (pid == 0){ //Estamos en algun hijo.
		  if (!line->background){
			 //Si recibe la señal SIGQUIT o SIGINT
			signal(SIGQUIT,SIG_DFL);
			signal(SIGINT,SIG_DFL);
		}
		  
		if(i==0){ 	// mandato 1º
		       RedireccionEntrada(line); //miramos si hay redireccion de entrada:
			   close(p[0]); 
			   dup2(p[1],1); 
			   close(p[1]);

	  			execvp(line->commands[i].filename, line->commands[i].argv);
	  			fprintf(stderr,"Error al ejecutar el comando%s\n", strerror(errno));
				
				exit(1);
		}

		 else if(i > 0 && i<line->ncommands-1){ 	// mandato intermedio
				dup2(p[0],0); 
				
				close(p[0]); 
				close(p[1]);
				close(pNuevo[0]); 
				
				dup2(pNuevo[1],1);
				
				close(pNuevo[1]); 

	  			execvp(line->commands[i].filename, line->commands[i].argv); //Ejecutamos.
	  			
				fprintf(stderr,"Error al ejecutar el comando%s\n", strerror(errno));
				exit(1);
	  			

		 }

		 else if(i == line->ncommands-1){ // mando ultimo

				dup2(p[0],0); //Redireccionamos la entrada estandar a la salida del pipe;
				
				RedireccionSalida(line);

				close(p[0]);
				close(p[1]);

	  			execvp(line->commands[i].filename, line->commands[i].argv);
	  			printf("Error al ejecutar el comando%s\n", strerror(errno));
				exit(1);
		 }
	   } //terminan el hijo
	   else{ // Padre

			if (i != 0){  // En caso de no es el 1º mandato  cerraremos el pipe principal
				close(p[0]); 
				close(p[1]);
			}
			if (i > 0 && i<line->ncommands-1){ //En caso de no ser el ultimo lo copiaremos y concatenamos al siguiente.
				
				p[0] = pNuevo[0];
				p[1] = pNuevo[1];
			}
		}
	}

	
		
		
		
		
	
	close(p[0]);
	close(p[1]);		// CERRAMOS TODOS LO PIPES ABIERTOS
	close(pNuevo[0]);
	close(pNuevo[1]);
	
	if(!line->background){
			for (y = 0; y<line->ncommands; y++ ){
				waitpid(pids[i],NULL,0);
			}
				
		}

	
			 
}
