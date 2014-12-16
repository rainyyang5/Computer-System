/* 
 * Group Member 1: Tianqi Wen (tianqiw)
 * Group Member 2: Mengyu Yang (mengyuy)
 * 
 * This lab:
 * 1. Implementing a simple sequential web proxy
 * 2. Dealing with multiple concurrent requests
 * 3. Implementing a cache with LRU eviction policy using linked list
 *
 */ 

#include <stdio.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE (1 << 20)
#define MAX_OBJECT_SIZE 102400

#define DEFAULT_PORT 80

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate";
static const char *conn_hdr = "Connection: close";
static const char *proxy_conn_hdr = "Proxy-Connection: close";

/* inline helper functions */

/* 
 * return 1 if is not standard headers
 * return 0 if it's any one of them
 */
inline static int isUnknownHdr(char *buf){
    return (strncmp(buf, "User-Agent", 10) &&
        strncmp(buf, "Accept:", 7) &&
        strncmp(buf, "Accept-Encoding", 15) &&
        strncmp(buf, "Connection", 10) &&
        strncmp(buf, "Proxy-Connection", 16));
}

/* construct a header for the request to sever with client header info */
inline static void requestHdr(rio_t *rio_ptr, 
    char *request_buf, char *host, char *filename){

    char get_cmd[MAXLINE], host_hdr[MAXLINE], std_hdr[MAXLINE], append_hdr[MAXLINE];
    char buf[MAXLINE];

    /* construct get request header */
    sprintf(get_cmd, "GET %s HTTP/1.0\r\n", filename);

    /* construct headers for the host and suffix part */
    strcpy(host_hdr, "");

    while (Rio_readlineb(rio_ptr, buf, MAXLINE) > 0) {
        if (!strcmp(buf, "\r\n")) {
            break;
        }
        else if (!strncmp(buf, "Host:", 5)) {
            strcpy(host_hdr, buf);
        }
        else if (isUnknownHdr(buf)) {
            strcat(append_hdr, buf);
        }
    }

    /* if no host info in client header */
    if (!strlen(host_hdr)) {
        sprintf(host_hdr, "Host: %s\r\n", host);
    }

    /* construct standard headers */
    sprintf(std_hdr, "%s%s%s\r\n%s\r\n%s\r\n\r\n",
        user_agent_hdr,
        accept_hdr,
        accept_encoding_hdr,
        conn_hdr,
        proxy_conn_hdr);

    /* construct header for server */
    sprintf(request_buf, "%s%s%s%s",
        get_cmd,
        host_hdr,
        std_hdr,
        append_hdr);

    return;
}

/* Global pointer to cache base */
cache *cache_ptr;

/* helper function delaration */
int parse_uri(char *uri, char *host, int *port, char *suffix);
void *doit(void *vargp);
void printerror(int fd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);


/* ----------------- main routine of web proxy ----------------- */
int main(int argc, char *argv[]) {
    int listenfd, *connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t pid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* init cache */
    cache_ptr = cache_init();

    /* listen to port */
    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    clientlen = sizeof(clientaddr);

    while (1) {
        connfd = malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        /* create and start a new thread */
        Pthread_create(&pid, NULL, doit, (void*)connfd);
    }

    return 0;
}

/*
 * handle HTTP request/response transaction of a thread
 * clinet-----(request)----->server
 *       <------(data)-------
 */
void *doit(void *connfd) {
    int fd = *(int *)connfd;
    int fd_server;
    /* detach thread */
    Pthread_detach(pthread_self());

    rio_t rio;
    char buf[MAXLINE], object_buf[MAX_OBJECT_SIZE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    /* uri info */
    char host[MAXLINE];
    int port;
    char filename[MAXLINE];

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    /* request method is not GET */
    if (strcmp(method, "GET")) {
        printerror(fd, method, "501", "Not Implemented",
            "tianqiw's proxy does not implement this method");
        Close(fd);
        return NULL;
    }

    /* request method is GET
     * look for the object in cache */
    cache_block *block = cache_match(cache_ptr, uri);

    if (block != NULL) {
        /* cache hit */
        Rio_writen(fd, block->object, block->object_size);
    }
    else {
        /* cache miss */
        parse_uri(uri, host, &port, filename);

        /* construct the request header */
        char request_buf[MAXLINE];
        requestHdr(&rio, request_buf, host, filename);

        /* send request to server */
        if ((fd_server = open_clientfd_r(host, port)) < 0) {
            /* server connection error */
            char longmsg[MAXBUF];
            sprintf(longmsg, "Cannot open connection to server at <%s, %d>", host, port);
            printerror(fd, "Connection Failed", "404", "Not Found", longmsg);
            Close(fd);
            return NULL;
        }

        /* reset rio for server use */
        memset(&rio, 0, sizeof(rio_t));
        Rio_readinitb(&rio, fd_server);
        Rio_writen(fd_server, request_buf, strlen(request_buf));

        /* get data from server and send to client */
        size_t object_size = 0;
        size_t buflen;
        int is_exceed = 0;

        while ((buflen = Rio_readlineb(&rio, buf, MAXLINE))) {
            Rio_writen(fd, buf, buflen);

            /* size of the buffer exceeds the max object size
             * discard the buffer */
            if ((object_size + buflen) > MAX_OBJECT_SIZE) {
                is_exceed = 1;
            }
            else {
                memcpy(object_buf + object_size, buf, buflen);
                object_size += buflen;
            }
        }

        /* if not exceed the max object size, insert to cache */
        if (!is_exceed) {
            cache_insert(cache_ptr, uri, object_buf, object_size);
        }

        /* clear the buffer */
        Close(fd_server);
    }

    Close(fd);
    return NULL;
}

/* 
 * get host, port, filename from the uri
 * http://<host>:<port><filename>
 * if port is not provided in the uri, use DEFAULT_PORT instead
 */
int parse_uri(char *uri, char *host, int *port, char *filename){

    if (strncasecmp(uri, "http://", 7)) {
        /* wrong uri format */
        host[0] = '\0';
        return -1;
    }

    char buf[MAXLINE];
    strcpy(buf, uri);
    *port = DEFAULT_PORT;

    /* ptr point to the location after http:// */
    char *ptr = buf + 7;

    /* retrieve hostname */
    while (*ptr != '/' && *ptr != ':') {
        *host = *ptr;
        ptr++;
        host++;
    }
    *host = '\0';

    /* retrieve port number */
    if (*ptr == ':') {
        *ptr = '\0';
        ptr++;
        sscanf(ptr, "%d%s", port, ptr);
    }

    /* retrieve filename */
    strcpy(filename, ptr);

    return 0;
}

/* print error message using HTTP response */
void printerror(int fd, char *cause, char *errnum,
    char *shortmsg, char *longmsg){

    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
