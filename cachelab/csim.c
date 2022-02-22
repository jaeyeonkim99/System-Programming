/* Header Comment
*
*  Name: 김재연
*  Login ID: stu67
*
*/

#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char *optarg;

/*struct for cache_line */
typedef struct cache_line{
    char valid;
    unsigned long long tag;
    int LRU_cnt;
} CACHE_LINE;

/*Search if requested memory block is in cacheline, and return 1 and update LRU_cnt if hitted*/
int search(CACHE_LINE* cacheline, unsigned long long asso, int tag, int inst_cnt);

/*Update cacheline when miss happens, and execute eviction if required. Return 1 if eviction happend*/
int update_cache(CACHE_LINE* cacheline, unsigned long long asso, int tag, int inst_cnt);

int main(int argc, char * const * argv)
{
    /*Cache we are simulating*/
    CACHE_LINE **cache;

    /*parameters from the commandline*/
    unsigned long long b_size = 0; //block size
    unsigned long long set = 0; //number of sets
    unsigned long long asso = 0; //associativity
    int verbose_flag = 0; // flag for verbose option
    
    char* f_name= NULL; //trace file name;

    unsigned long long setmask=0;  //mask for setbits
    unsigned long long blockmask=0; //mask for block offset
    int b; //number of block bits
    int s; //number of set bits

    /*variables for parsing & simulating trace file*/
    unsigned long long address;
    char inst[2];
    int size;
    unsigned long long set_num; 
    unsigned long long tag;

    /*for output*/
    int hit_cnt=0; 
    int miss_cnt=0;
    int evict_cnt = 0;

    /*parsing command line argument for csim + check invalid input for each options */
    char opt; 

     while((opt = getopt(argc, argv, "vs:E:b:t:")) != -1){
         switch(opt){
            case 'v':
                verbose_flag = 1;
                break;
            case 's':
                if(atoi(optarg)>32){ 
                    printf("wrong argument for s\n");
                    if(f_name!=NULL) free(f_name);
                    exit(0);
                 } 
                s=atoi(optarg);
                set = 1<<s;
                break;
            case 'E':
                if((unsigned int)atoi(optarg)>(1<<31)){ 
                    printf("wrong argument for E\n");
                    if(f_name!=NULL) free(f_name);
                    exit(0);
                 } 
                asso = atoi(optarg);
                break;
            case 'b':
                if(atoi(optarg)>32){ 
                    printf("wrong argument for b\n");
                    if(f_name!=NULL) free(f_name);
                    exit(0);
                 } 
                b=atoi(optarg);
                b_size = 1<<b;
                break;
            case 't':
                f_name = malloc(100);
                strcpy(f_name, optarg);
                break;
            default:
                //printf("wrong option: '%c'\n", opt);
                if(f_name!=NULL) free(f_name);
                exit(0);
            break;
         }
     }

    /*check invalid command line inputs */
    if(set==0||asso==0||b_size==0||f_name==NULL){
        if(set==0) printf("Insufficient options: no option -s \n");
        else if(asso==0) printf("Insufficient options: no option -E \n");
        else if(b_size==0) printf("Insufficient options: no option -b \n");
        else if(f_name==NULL) printf("Insufficient options: no option -f \n");

        if(f_name!=NULL) free(f_name);
        exit(0);
    }
    else if((b+s)>32){
        printf("Invalid argument for -s or -b\n");
        if(f_name!=NULL) free(f_name);
        exit(0);
    }

    /*open trace file*/
    FILE* fp = fopen(f_name, "rt");
    free(f_name);
    if(fp==NULL){
        printf("Failed to open the tracefile.\n");
        exit(0);
    }

    /*allocate space for cache*/
    cache = (CACHE_LINE**)calloc(set, sizeof(CACHE_LINE*));
    for(int i=0; i<set; i++) cache[i] = (CACHE_LINE*)calloc(asso, sizeof(CACHE_LINE));

    /*make mask for set & block bits*/
    for(int i=b; i<b+s; i++) setmask = setmask|(1<<i);
    for(int i=0; i<b; i++) blockmask = blockmask|(1<<i);

   
    /*to indicate  the order of current instruction, used for LRU*/
    int inst_cnt = 0;

    /*parse & simulate trace file*/
    while((fscanf(fp, "%s %llx, %d", inst, &address, &size))!=EOF){

        if(strcmp(inst, "I")==0) continue;
        //
        inst_cnt++;

       
        if(verbose_flag==1){
            printf("%s %llx,%d", inst, address, size);
        }
        /*parse set_number and tag bit from the address*/
        set_num = (address&setmask)>>b;
        tag = address>>(b+s);

        /*simulate case for each instruction*/
        int j;
        if(strcmp(inst, "L")==0||strcmp(inst, "S")==0) j=1;
        else if(strcmp(inst, "M")==0) j=2; //to repeat the same process 2 time for L instructions

        while(j>0){

            int if_hit=0;
            int if_evict = 0;

            if_hit = search(cache[set_num], asso, tag, inst_cnt);

            if(if_hit==1){ //hit
                hit_cnt++;

                if(verbose_flag==1) printf(" hit");
            } 
            else{ //miss
                miss_cnt++;
                if(verbose_flag==1) printf(" miss");

                if_evict = update_cache(cache[set_num], asso, tag, inst_cnt);
                if(if_evict==1){
                    evict_cnt++;
                    printf(" eviction");
                } 
            }        
            j--;                
        }
       if(verbose_flag==1) printf("\n");
    }

    fclose(fp);
    printSummary(hit_cnt, miss_cnt, evict_cnt);

    /*free cache*/
    for(int i=0; i<set; i++) free(cache[i]);
    free(cache);   
   
    return 0;
}

/*Search if requested memory block is in cacheline, and return 1 and update LRU_cnt if hitted*/
int search(CACHE_LINE* cacheline, unsigned long long asso, int tag, int inst_cnt){
    for(int i =0; i<asso; i++){
        if(cacheline[i].valid==1){
            if(cacheline[i].tag==tag){
                cacheline[i].LRU_cnt = inst_cnt;
                return 1;
            } 
        }
    }
    return 0;
}

/*Update cacheline when miss happens, and execute eviction if required. Return 1 if eviction happend*/
int update_cache(CACHE_LINE* cacheline, unsigned long long asso, int tag, int inst_cnt){
    
    int min_LRU = 2147483647;
    int LRU_index=asso;

    /*search if empty entry exist*/
    for(int i=0; i<asso; i++){
        if(cacheline[i].valid==0){ /*if empty entry exist*/
            cacheline[i].valid=1;
            cacheline[i].tag = tag;
            cacheline[i].LRU_cnt = inst_cnt;

            return 0;
        }

        /*search for block to evict at the same time*/
        if(cacheline[i].valid==1&&cacheline[i].LRU_cnt<min_LRU){
            LRU_index = i;
            min_LRU = cacheline[i].LRU_cnt;
        } 
    }

    /*evict & get new memory block into cache*/
    if(LRU_index<asso){
        cacheline[LRU_index].tag = tag;
        cacheline[LRU_index].LRU_cnt = inst_cnt;
    return 1;
    }
    else return 0;
}
