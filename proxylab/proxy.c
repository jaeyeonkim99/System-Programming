#include <stdio.h>
#include "csapp.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


typedef struct header {
    char header[MAXLINE];

    struct header* next;

} Header;

typedef struct request {
    
    /*request line*/
    char method[8];
    char uri[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    char version[10];

    /*in order to manage headers using linked list*/
    Header* root;
    Header* last;
    int length;

} Request;


typedef struct cache_entry{

    /*key*/
    char host[MAXLINE];
    char port[6];
    char path[MAXLINE];

    /*item*/
    char** response;  // response[13][MAXLINE], 12.5*8192 = 102400
    
    /*other datas*/
    int num_line;    
    int last_line; //length of last line
    int size;

    /*for LRU*/
    clock_t access;

    /*to manange cache as linked list*/
    struct cache_entry* next; 

} Cache_entry;



/*functions*/
void *handle_client(void *vargp);
int parse_request_line(char* buf, Request* request);
int parse_header(char* buf, Request* request, int* hostflag);
void send_response(Request* request, int connfd);
void eviction(Cache_entry* new_entry);

/*global variables*/
/*to manange cache as linked list*/
Cache_entry* cache_root = NULL;
Cache_entry* cache_last = NULL;
int cache_num_item=0;
int cache_size=0;

/*for synchronization*/
pthread_rwlock_t rwlock;

int main(int argc, char **argv)
{
    int listenfd;
    struct sockaddr_in clientaddr;
    int clientlen = sizeof(clientaddr);
    pthread_t tid;

    char* port;

    port = argv[1];
    while((listenfd = Open_listenfd(port))<0){
        printf("Failed to open listen. Reenter appropriate port number\n");
        scanf("%s", port);
    }

    pthread_rwlock_init(&rwlock, NULL);

    while(1){
        int* connfdp= Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        Pthread_create(&tid, NULL, handle_client, connfdp);
    }

    /*Free Cache*/
    Cache_entry* curr = cache_root;
    Cache_entry* next;
    while(curr!=NULL){
        next = curr->next;
        for(int i=0; i<curr->num_line; i++) Free(curr->response[i]);
        Free(curr->response);
        Free(curr);
        curr = next;
    }

    return 0;
}

void *handle_client(void *vargp){
    int connfd= *((int*)vargp);
    Pthread_detach(pthread_self()); 
    Free(vargp);

    rio_t rio;
    char buf1[MAXLINE];
    char header[MAXLINE];
    header[0] ='\0';

    Request* request_to_send = Calloc(1, sizeof(Request));
    request_to_send->method[0] = '\0';
    request_to_send->uri[0] = '\0';
    request_to_send->version[0] = '\0';


    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf1, MAXLINE);

    
    /*parsing the request line*/
    if(parse_request_line(buf1, request_to_send)==-1) goto IFERROR1;

    //printf("method: %s, hostname: %s, path: %s\n", request_to_send->method, request_to_send->hostname, request_to_send->path);

    /*parsing header start. save the header in the form of linked list*/
    /*Attach Required Headers*/
    Header* User_Agent = Malloc(sizeof(Header));
    Header* Connection = Malloc(sizeof(Header));
    Header* Proxy_Connection = Malloc(sizeof(Header));

    strcpy(User_Agent->header, user_agent_hdr);
    strcpy(Connection->header, "Connection: close\r\n");
    strcpy(Proxy_Connection->header, "Proxy-Connection: close\r\n");

    request_to_send->root = User_Agent;
    request_to_send->last = Proxy_Connection;
    request_to_send->length = 3;

    User_Agent->next = Connection;
    Connection->next = Proxy_Connection;
    Proxy_Connection->next = NULL;



    /*To define which host to useL Browers own Host header if exist*/
    int* hostflag;
    int flag = 0;
    hostflag = &flag;


    while(1){
        Rio_readlineb(&rio, header, MAXLINE);
        int result;

        if(strcmp(header, "\r\n")==0) break;
        else result = parse_header(header, request_to_send, hostflag);

        if(result==-1) goto IFERROR2;     
    }

    if(flag==0){
        char *tmp_host_head = "Host: ";
        strcat(tmp_host_head, request_to_send->hostname);
        strcat(tmp_host_head, "\r\n");

        Header* new = Malloc(sizeof(Header));  
        strcpy(new->header, tmp_host_head);      
        new->next = request_to_send->root;
        request_to_send->root = new;
        request_to_send->length++;
    }
    /*parsing the headers is done*/


    /*send the response to client*/
    send_response(request_to_send, connfd);


IFERROR2:
    flag=0;
    //free data structures
    Header* curr = request_to_send->root;
    Header* next = curr;

    while(next!=NULL){
        next = curr->next;
        free(curr);
        curr=next;
    }

  IFERROR1: 
    free(request_to_send);


    Close(connfd);
    return NULL;
}

int parse_request_line(char* buf, Request* request){    

    char* hostname_start;
    char* pathname_start;

    /*parse request line*/
    /*check the format*/
    if(sscanf(buf, "%s %s %s\r\n", request->method, request->uri, request->version)==-1){
        printf("Wrong request line\n");
        return -1;
    };

    /*if(strlen(request->method)==0||strlen(request->uri)==0||strlen(request->version)){
        printf("Wrong request line\n");
        return -1;
    }*/

    /*Want to handle GET request only*/
    if(strcmp(request->method, "GET")!=0){
        printf("Want to handle GET request only\n");
        return -1;
    }

    /*parse URI into hostname and path*/
    if((hostname_start=strstr(request->uri, "http://"))==NULL){
        request->hostname[0]='\0';
        strcpy(request->path, request->uri);
    }
    else{
        hostname_start+=7;
        pathname_start = strstr(hostname_start, "/");

        strcpy(request->path, pathname_start);
        strncpy(request->hostname, hostname_start, (size_t)(pathname_start-hostname_start));
    }

    return 0;
}

int parse_header(char* header, Request* request, int* hostflag){
    char* name_end= strstr(header, ": ");
    if(name_end==NULL||strlen(name_end)==strlen(header)||strlen(name_end)==0){
        printf("Wrong Header Format");
        return -1;
    }    

    /*to avoid repetition of already attached necessary headrs*/
    if(strstr(header, "Connection")!=NULL) return 0;
    if(strstr(header, "Proxy-Connection")!=NULL) return 0;
    if(strstr(header, "User-Agent")!=NULL) return 0;

    /*for HOST header*/
    if(strstr(header, "Host")!=NULL){ 
        
        *hostflag= 1;
        Header* new = Malloc(sizeof(Header));  
        strcpy(new->header, header);
        new->next = request->root;
        request->root = new;
        request->length++;
        return 0;
    }
        
    Header* new = Malloc(sizeof(Header));  
    strcpy(new->header, header);
    request->last->next = new;
    new->next = NULL;
    request->last = new;
    request->length++;
    return 0;
}

void send_response(Request* request, int connfd){

    /*parse hostname of request to host & portnumber*/
    char host[MAXLINE];
    char port[6];
    host[0] ='\0';
    port[0] = '\0';

    char *temp;
    if((temp=strstr(request->hostname, ":"))==NULL){
        strcpy(host, request->hostname);
        strcpy(port, "80");
    }
    else{
        temp++;
        strcpy(port, temp);
        strncpy(host, request->hostname, (size_t)(temp-request->hostname)-1);
    }

    /*search in Cache, and if in Cache, send response from cache*/
    pthread_rwlock_rdlock(&rwlock);
    
    Cache_entry* curr = cache_root;
    while(curr!=NULL){
        if(strcmp(curr->host, host)==0&&strcmp(curr->port, port)==0&&strcmp(curr->path, request->path)==0){
            for(int i=0; i<curr->num_line; i++){
               if(i!=curr->num_line-1) Rio_writen(connfd, curr->response[i], MAXLINE);
               else Rio_writen(connfd, curr->response[i], curr->last_line);
            }            
            curr->access = clock();
            pthread_rwlock_unlock(&rwlock);
            //printf("Got it from Cache\n");
            return;
        }
        else curr = curr->next;
    }
    pthread_rwlock_unlock(&rwlock);


    /*send response from server*/
    /*send modified request to server*/
    char tosend[MAXLINE*request->length/2];
    char response[MAXLINE];
    tosend[0] = '\0';

    strcat(tosend, request->method);
    strcat(tosend, " ");
    strcat(tosend, request->path);
    strcat(tosend, " ");
    strcat(tosend, "HTTP/1.0\r\n");

   
    Header* curr_header = request->root;
    while(curr_header!=NULL){
        strcat(tosend, curr_header->header);
        curr_header = curr_header->next;
    }
    strcat(tosend, "\r\n");

    //printf("%s", tosend);

    /*make connection to server*/
    int clientfd;
    clientfd = Open_clientfd(host, port);

    if(clientfd<0){
        printf("Open_clientfd failed: Failed to connect to server\n");
        return;
    }

    /*write the modified request to server*/
    rio_t rio;
    Rio_writen(clientfd, tosend, strlen(tosend));   

    /*get response from server*/
    /*write it to client*/
    
    Rio_readinitb(&rio, clientfd);

    size_t n=0;
    size_t obj_size = 0;
    size_t numline=0;
    size_t tmp_last = 0;
    char  **tmp_response;
    tmp_response = (char**)Calloc(13, sizeof(char*));

    while((n = Rio_readnb(&rio, response, MAXLINE))>0){        
        Rio_writen(connfd, response, n);

        /*to check if it can go into cache*/
        if(obj_size<MAX_OBJECT_SIZE){
            obj_size+=n;
            if(obj_size<MAX_OBJECT_SIZE){
                tmp_response[numline] = (char*)Malloc(MAXLINE*sizeof(char));
                strcpy(tmp_response[numline], response);
                numline++;
                if(n<MAXLINE) tmp_last = n;
            }
        }
    }

    Close(clientfd);

    /*update cache*/

    /*make new cache entry*/
    Cache_entry* new_entry;

    if(obj_size<MAX_OBJECT_SIZE){
        new_entry = Malloc(sizeof(Cache_entry));
        strcpy(new_entry->host, host);
        strcpy(new_entry->port, port);
        strcpy(new_entry->path, request->path);

        new_entry->num_line = numline;
        new_entry->response = tmp_response;
        new_entry->last_line = tmp_last;
        new_entry->size = obj_size;

        new_entry->access = clock();

        new_entry->next = NULL;

    }
    else{
        for(int i=0; i<numline; i++) free(tmp_response[numline]);
        free(tmp_response);
        return;
    }

    /*evict and insert in cache*/
    pthread_rwlock_wrlock(&rwlock);

    if((cache_size+obj_size)<MAX_CACHE_SIZE){ /*insert*/
        if(cache_root==NULL) cache_root = new_entry;
        if(cache_last==NULL) cache_last = new_entry;
        else{
            cache_last->next = new_entry;
            cache_last = new_entry;
        }
        
        cache_num_item++;
        cache_size+=obj_size;
    }
    else{ /*eviction, policy: modified LRU*/
        eviction(new_entry);
    }

    pthread_rwlock_unlock(&rwlock);

    //printf("Update Cache: cahce_size %d, object size %d\n", cache_size, new_entry->size);

}

void eviction(Cache_entry* new_entry){
    Cache_entry* evict;
    Cache_entry* prev_evict;
    clock_t min=LONG_MAX;

    Cache_entry* curr= cache_root;

    int totalsize = cache_size+new_entry->size;

    /*for root*/
     if((totalsize-curr->size)<MAX_CACHE_SIZE){
            min=curr->access;
            prev_evict = NULL;
            evict = curr;
        }

    /*for other entries*/
    while(curr->next!=NULL){
        if((totalsize-curr->next->size)<MAX_CACHE_SIZE){
            if(curr->next->access<min){
                min=curr->next->access;
                prev_evict = curr;
                evict = curr->next;
            }
        }
        curr = curr->next;
    }

    /*Delete evicted Entries*/
    if(evict==cache_root){
        cache_num_item--;
        cache_size-=evict->size;
        cache_root = cache_root->next;
        Free(evict);        
    }
    else if(evict==cache_last){
        cache_num_item--;
        cache_size-=evict->size;
        cache_last = prev_evict;
        cache_last->next = 0;
        Free(evict);
    }
    else{
        cache_num_item--;
        cache_size-=evict->size;
        prev_evict->next = evict->next;
        Free(evict);
    }

    /*Insert New Entry*/
    cache_last->next = new_entry;
    cache_last = new_entry;
    cache_num_item++;
    cache_size+=new_entry->size;

    return;
}
