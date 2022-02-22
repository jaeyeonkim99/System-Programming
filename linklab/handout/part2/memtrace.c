//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

static size_t allocated_total = 0;
static size_t freed_total = 0;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  unsigned long total_n_allocated = n_calloc + n_malloc+ n_realloc;

  unsigned long avg_allocated = allocated_total/total_n_allocated;

  LOG_STATISTICS(allocated_total, avg_allocated, freed_total);

  if(freed_total!=allocated_total){
    LOG_NONFREED_START();
    item * curr = list;

    while(curr->next!=NULL){
      curr = curr->next;
      if(curr->cnt>0) LOG_BLOCK(curr->ptr, curr->size, curr->cnt);
    }
  }

  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...

void *malloc(size_t size){
  char *error;
  void *ptr;
  void *copy;

 if(!mallocp){
   mallocp = dlsym(RTLD_NEXT, "malloc");
   if((error = dlerror())!=NULL){
      fputs(error, stderr);
      exit(1);
    }
 }

  ptr = mallocp(size);  
  alloc(list, ptr, size);
  LOG_MALLOC(size, ptr);
  n_malloc++;
  allocated_total+=size;
  
  return ptr; 
}

void *calloc(size_t nmemb, size_t size){
  char *error;
  void *ptr;

 if(!callocp){
   callocp = dlsym(RTLD_NEXT, "calloc");
   if((error = dlerror())!=NULL){
      fputs(error, stderr);
      exit(1);
    }
 }

  ptr = callocp(nmemb, size);  
  LOG_CALLOC(nmemb, size, ptr);
  n_calloc++;
  allocated_total+=size*nmemb;
  
  alloc(list, ptr, size*nmemb);

  return ptr;

}


void *realloc(void* ptr, size_t size){
  char *error;
  item* freed;
  item* original;
  item* prev;

  void *new;

 if(!reallocp){
   reallocp = dlsym(RTLD_NEXT, "realloc");
   if((error = dlerror())!=NULL){
      fputs(error, stderr);
      exit(1);
    }
 }

  freed = find(list, ptr);

  //considering no illegal, double free in part2
  if(freed!=NULL&&freed->cnt>0){

    if(freed->size>=size){
      LOG_REALLOC(ptr, size, ptr);
      return ptr;
    }

    new = reallocp(ptr, size);  
    LOG_REALLOC(ptr, size, new);
    n_realloc++;
    allocated_total+=size;
    dealloc(list, ptr);
    freed_total += freed->size;
    alloc(list, new, size);    
  }

  return new;
}


void free(void *ptr){
  char *error;
  item *freed;
  if(!freep){
   freep = dlsym(RTLD_NEXT, "free");
   if((error = dlerror())!=NULL){
      fputs(error, stderr);
      exit(1);
   }
  }
  
  freed = find(list, ptr);
  
  //part2; assume that no illegal, double free for part2
  if(freed!=NULL && freed->cnt>0){
    freed_total+= freed->size;
    dealloc(list, ptr);
    freep(ptr);
    LOG_FREE(ptr);
  
  }
  
}




