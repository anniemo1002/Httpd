/* code taken and adapted from http://www.ecst.csuchico.edu/~beej/guide/net/ */
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "httpd.h"
int main(int argc, char* argv[]){
   int port;
   int server_fd;
   if (argc!=2){
      fprintf(stderr, "Usage: ./simple_net port_number\n");
      exit(-1);
   }
   port = atoi(argv[1]);
   if (port<1024 || port > 65535){
      fprintf(stderr, "Invalid port number");
      exit(1);
   }
   setup(SIGCHLD);
   server_fd = create_service(port,20);
   while(1){
      int client_fd;
      client_fd = accept_connection(server_fd);
      if (client_fd<0){
         fprintf(stderr, "can't accept connection");
         exit(-1);
      }
      handle_request(client_fd);
      close(client_fd);
   }
}
void handle_action(int signo){
   if (signo == SIGCHLD){
      wait(NULL);
   }
   return;
}
void setup(int signo){
   struct sigaction action;
   if (sigemptyset(&action.sa_mask)==-1){
      perror("");
      exit(-1);
   }
   action.sa_flags = 0;
   action.sa_handler = handle_action;
   if (sigaction(signo, &action, NULL)==-1){
      perror("");
      exit(-1);
   }
}
void handle_request(int client_fd){
   pid_t pid;
   char* line;
   char** req;
   int i = 0;
   int res;
   int output_fd;
   char* tok;
   char* path= NULL;
   struct stat buf;
   size_t size;
   char filename[25]; 
   char* Err500 = "HTTP/1.0 500 Internal Error\n";
   char* Err400 = "HTTP/1.0 400 Bad Request\n";
   char* Err403 = "HTTP/1.0 403 Permission Denied\n";
   char* Err404 = "HTTP/1.0 404 Not Found\n";
   if ((pid = fork())<0){
      write(client_fd, Err500, strlen(Err500));
      return;
   }
   if (pid>0) return;  /*Parent*/
   if ((line = readline(client_fd) )==NULL || strstr(line,"..")!=NULL){      /*Child*/
      write(client_fd,Err400, strlen(Err400));
      close(client_fd);
      exit(-1);
   }
   tok = line;
   req = checked_malloc(4*sizeof(char*));
   while ((req[i++]=str_sep(&tok," "))!=NULL ){
      if (i>=4){  /*make sure 3 strings */
         write(client_fd,Err400, strlen(Err400));
         free(line);
         free(req);
         close(client_fd);
         exit(-1);   
      }
   }
   if ((strcmp(req[0],"GET")!=0 || strcmp(req[0],"HEAD")!=0) && req[1][0]!='/'){
      write(client_fd,Err400, strlen(Err400));
      free(line);
      free(req);
      close(client_fd);
      exit(-1);
   }
   if (strncmp(req[1],"/cgi-like/", 10 )==0){ /*cgi-like*/
      if (chdir("cgi-like")<0){
         write(client_fd,Err404, strlen(Err404));
         free(line);
         free(req);
         close(client_fd);
         exit(-1);
      }
      sprintf(filename, "%d", getpid());
      output_fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
      res = exec((req[1]+strlen("/cgi-like/")),output_fd, filename);  /*execute the cmd*/
      chdir("..");
      if (res<0 || fstat(output_fd,&buf)<0){
         if (res<0) write(client_fd,Err403, strlen(Err403)); /*fork error*/
         else write(client_fd,Err500, strlen(Err500));   /*exec error*/
         free(line);
         free(req);
         close(client_fd);
         exit(-1);
      }
   }
   else{                                                      /*not cgi-like, just get information from a file*/
      path = checked_malloc(sizeof(char)*(strlen(req[1])+2));
      sprintf(path, ".%s", req[1]);
      req[1] = path;
      if (stat(req[1],&buf)<0){
         write(client_fd,Err403, strlen(Err403));
         free(line);
         free(req);
         free(path);
         close(client_fd);
         exit(-1);
      }
   }
   size = buf.st_size;
   write_header(client_fd, size);
   if (strcmp(req[0],"HEAD")==0){  /*Head request, exit*/
      free(line);
      close(output_fd);
      free(req);
      close(client_fd);
      if (path!=NULL) free(path);
      exit(0);
   }
   if (strncmp(req[1],"/cgi-like/", 10 )!=0 && (output_fd = open(req[1],O_RDONLY))<0){
      write(client_fd,Err404, strlen(Err404));
      close(client_fd);
      free(line);
      free(req);
      if (path!=NULL) free(path);
      exit(-1);
   }
   write_content(client_fd, output_fd, size);   /*GET request*/
   free(req);
   free(line);
   if (path!=NULL) free(path);
   else remove(filename);
   close(output_fd);
   close(client_fd);
   exit(0);
}
int exec(char* cmd, int output_fd, char* filename){
   char** arg;
   int i = 1;
   int size = 10;
   int pid;
   arg = checked_malloc(10*sizeof(char*));
   arg[0] = str_sep(&cmd, "?");
   while ((arg[i++] = str_sep(&cmd, "&"))!=NULL){
      if (i>=size-1){
         arg = checked_realloc(arg, sizeof(char*)*(size+10));
         size = size+10;
      }
   }
   if ((pid = fork())){
      if (pid<0){   /*error*/
         return -1;
      }
      wait(NULL);  /*parent*/
      free(arg);
      return 0;
   }
   else{           /*child for exec*/
      dup2(output_fd,1);
      dup2(output_fd,2);
      close(output_fd);
      if (execv(arg[0],arg)<0)
         remove(filename);
         exit(-1);
   }
}
void write_content(int client_fd, int file, size_t size){
   char* buf;
   lseek(file, SEEK_SET, 0);
   buf = checked_malloc(size);
   read(file,buf, size);
   write(client_fd, buf,size);
   free(buf);
   return;
}
void write_header(int client_fd, size_t size){
   char* header = checked_malloc(100);
   sprintf(header, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", (int)size);
   write(client_fd,header, strlen(header) );
   free(header);
   return;
}
void* checked_malloc(size_t size){
   void* p;
   if ((p = malloc(size))==NULL){
      perror("");
      exit(-1);
   }
   return p;
}
void * checked_realloc(void* p, size_t size){
   void* temp;
   if ((temp = realloc(p, size))==NULL){
      perror("");
      exit(-1);
   }
   return temp;
}
int create_service(unsigned short port, int queue_size)
{
   int fd;  /* listen on sock_fd, new connection on new_fd */
   struct sockaddr_in local_addr;    /* my address information */
   int yes=1;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      return -1;
   }

   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
   {
      return -1;
   }
        
   local_addr.sin_family = AF_INET;         /* host byte order */
   local_addr.sin_port = htons(port);       /* short, network byte order */
   local_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
   memset(&(local_addr.sin_zero), '\0', 8); /* zero the rest of the struct */

   if (bind(fd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) == -1)
   {
      return -1;
   }

   if (listen(fd, queue_size) == -1)
   {
      return -1;
   }

   return fd;
}

int accept_connection(int fd)
{
   int new_fd;
   struct sockaddr_in remote_addr;
   socklen_t size = sizeof(struct sockaddr_in);

   errno = EINTR;
   while (errno == EINTR)
   {
      if ((new_fd = accept(fd, (struct sockaddr*)&remote_addr, &size)) == -1
         && errno != EINTR)
      {
         return -1;
      }
      else if (new_fd != -1)
      {
         break;
      }
   }

   return new_fd;
}
char* str_sep(char** stringp, char* delim){
   char* tok;
   if (!stringp || ! *stringp) return NULL;
   tok = *stringp;
   while (**stringp!='\0'){
      if (**stringp==*delim){
         **stringp = '\0';
         (*stringp)++;
         return tok;
      }
      (*stringp)++;
   }
   *stringp = NULL;
   return tok;
}
char* readline(int fd){
   char c;
   int length;
   int i = 0;
   char * line;
   read(fd, &c, 1);
   if (c ==EOF){
      return NULL;
   }
   length = 20;
   line = (char*)malloc(20*sizeof(c));
   while (c!= '\n'&& c!=EOF){
      if (i+1>=length){
         line = (char*)realloc(line, length*2);
         length = length*2;
      }
      line[i++]= c;
      read(fd, &c, 1);
   }
   line[i]='\0';
   return line;
}
