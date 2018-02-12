#ifndef SIMPLE_NET_H
#define SIMPLE_NET_H

#include <sys/socket.h>
void handle_action(int signo);
void setup(int signo);
void handle_request(int client_fd);
void write_content(int client_fd, int file, size_t size);
void write_header(int client_fd, size_t size);
char* str_sep(char** stringp, char* delim);
int create_service(unsigned short port, int queue_size);
int accept_connection(int fd);

int exec(char* cmd, int output_fd, char* filename);
void * checked_malloc(size_t size);
void * checked_realloc(void* p, size_t size);
char* readline(int fd);
#endif
