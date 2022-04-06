#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#define MAGIC_LRU_NUM 999

/* Maximum array dimension */
#define MAXN 256

#define MAXSIZE 30

/* Globals set on the command line */

/*cache line*/
typedef struct cache_line {
    int valid;      // valid bit
    int tag;        // tag
    int count;      // LRU counter
} Line;

typedef struct cache_set {
    Line* lines;    // ever lines in a set
} Set; 

typedef struct {
    int S;          // Number of sets
    int E;          // Number of lines per set
    Set* sets;      // Every sets in a cache
} Cache;

typedef struct {
    int s;          // Number of set index bits
    int E;          // Number of lines per set
    int b;          // Number of block bits
    int S;          // Number of sets
    int B;          // Block size
    int vebose;     // Verbose flag
    char t[100];    // Trace file
} Parameters;

void usage();                                                         //usage - Print usage info
Parameters get_opt(int argc,char** argv);                             //get input parameters
void init_cache(int S, int E, int B, Cache* cache);
int getTag(int address, int s, int b);
int getSetIdx(int address, int s, int b);                             //initialize cache


void loadData(Cache *cache,int address,int size,int setBits,int tagBits,int vebose,int *hit_count,int *miss_count,int *eviction_count);                                                        
void storeData(Cache *cache,int address,int size,int setBits,int tagBits,int vebose,int *hit_count,int *miss_count,int *eviction_count);                                                          
void modifyData(Cache *cache,int address,int size,int setBits,int tagBits,int vebose,int *hit_count,int *miss_count,int *eviction_count); 

/* 
 * main - Main routine
 */
int main(int argc, char* argv[])
{
    /* Locals set on the command line */
    Parameters parameters;
    int hit_count = 0, miss_count = 0, eviction_count = 0;
    char opt;           // Operation: denotes the type of memory access
    //char input[MAXSIZE]; /* Save string into line*/

    /*get operation and parameters from shell command*/
    parameters = get_opt(argc,argv);
    printf("Test strncpy\n");
    printf("%s",parameters.t);

    /*initialize*/
    Cache cache;
    init_cache(parameters.S,parameters.E,parameters.B,&cache);

    /*open trace file*/
    FILE *tracefile = fopen(parameters.t, "r");
    if (!tracefile) {
        printf("File open fail");
        exit(1);
    }

    //int clock = 0;

    int address, size;
    while (fscanf(tracefile,"%c %x,%d",&opt,&address,&size) != EOF) {
        if (parameters.vebose == 1) printf("%c %x,%d ",opt,address,size);
        if (opt=='I') continue;   /*ignore all instruction cache access*/
        //int access = 0;     //cache access number
        int tagBits = getTag(address,parameters.s,parameters.b);
        int setBits = getSetIdx(address,parameters.s,parameters.b);
        if (opt=='M') {
            modifyData(&cache,address,size,setBits,tagBits,parameters.vebose,&hit_count,&miss_count,&eviction_count);
        } else if (opt=='L') {
            loadData(&cache,address,size,setBits,tagBits,parameters.vebose,&hit_count,&miss_count,&eviction_count);
        } else if (opt=='S') {
            storeData(&cache,address,size,setBits,tagBits,parameters.vebose,&hit_count,&miss_count,&eviction_count);
        }
        if(parameters.vebose == 1) printf("\n");
    }
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

void usage() {
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("Options:\n");
    printf("  -h             Optional help flag that prints usage info.\n");
    printf("  -v             Optional verbose flag that displays trace info\n");
    printf("  -s <s>         Number of set index bits (S = 1 << s is the number of sets)\n");
    printf("  -E <E>         Associativity (number of lines per set)\n");
    printf("  -b <b>         Number of block bits (B = 1 << b is the block size)\n");
    printf("  -t <tracefile> Name of the valgrind trace to replay\n");          
}

Parameters get_opt(int argc, char** argv) {
    Parameters parameters;
    char c;
    while ((c = getopt(argc,argv,"hvs:E:b:t:")) != -1) {
        switch(c) {
        case 's':
            parameters.s = atoi(optarg);
            parameters.S = 1 << parameters.s;
            break;
        case 'E':
            parameters.E = atoi(optarg);
            break;
        case 'b':
            parameters.b = atoi(optarg);
            parameters.B = 1 << parameters.b;
            break;
        case 't':
            memset(parameters.t, '\0', sizeof(parameters.t));
            strncpy(parameters.t,optarg,strlen(optarg));
            break;
        case 'v':
            parameters.vebose = 1;
            break;
        case 'h':
            usage();
            exit(0);
        default:
            usage();
            exit(1);
        }
    }
    if (parameters.s < 0) {
        printf("invalid sets number");
        exit(0);
    } else if (parameters.E < 0) {
        printf("invalid lines number");
        exit(0);
    } else if (parameters.b < 0) {
        printf("invalid block bits number");
        exit(0);
    }
    return parameters;
}

void init_cache(int S, int E, int B, Cache* cache) {
    cache->S = S;       // Number of sets
    cache->E = E;       // Number of lines per set
    cache->sets = (Set*)malloc(S * sizeof(Set));
    for(int i = 0; i < S; i++) {
        cache->sets[i].lines = (Line*)malloc(E * sizeof(Line));
        for (int j = 0; j < E; j++) {
            cache->sets[i].lines[j].valid = 0;      // valid bit
            cache->sets[i].lines[j].tag = 0;        // tag
            cache->sets[i].lines[j].count = 0;      // LRU counter
        }
    }
    return;
}

/*get tag from address*/
int getTag(int address, int s, int b) {
    return address >> (s+b);
}

/*get set index from address*/
int getSetIdx(int address, int s, int b) {
    address = address >> b;
    int offset = (1<<s)-1;
    return address &offset;
}

/*Find the Least Renct Use Line in a set as a sacrifice line */
int findLRULineIdx(Cache *cache, int setBits) {
    int index = 0, minLru = MAGIC_LRU_NUM;
    Set current_set = cache->sets[setBits];
    for (int i = 0; i < cache->E; i++) {
        if (current_set.lines[i].count < minLru) {
            index = i;
            minLru = current_set.lines[i].count;
        }
    }
    return index;
}

/*Update LRU, hit is the largest MAGIC_LRU_NUM, other LRUs are reduced by one*/
void updateLRUcount(Cache *cache, int setBits, int hitIdx) {
    cache->sets[setBits].lines[hitIdx].count = MAGIC_LRU_NUM;
    for (int i = 0; i < hitIdx; i++) {
        cache->sets[setBits].lines[i].count--;
    }
    for (int i = hitIdx+1; i < cache->E; i++) {
        cache->sets[setBits].lines[i].count--;
    }
}

/*hit or not*/
int isHit(Cache *cache, int setBits, int tagBits) {
    Set current_set = cache->sets[setBits];
    for (int i = 0; i < cache->E; i++) {
        if (current_set.lines[i].valid == 1 && current_set.lines[i].tag == tagBits) {
            updateLRUcount(cache, setBits, i);
            return 1;
        }
    }
    return 0;
}

int updateCache(Cache* cache, int setBits, int tagBits) {
    /*If is not full just store in cache and return 0*/
    Set current_set = cache->sets[setBits];
    for (int i = 0; i < cache->E; i++) {
        if (current_set.lines[i].valid == 0) {
            cache->sets[setBits].lines[i].valid = 1;
            cache->sets[setBits].lines[i].tag = tagBits;
            updateLRUcount(cache,setBits,i);
            return 0;
        }
    }
    /*If is full evict 1 line and return 1*/
    int evictionIdex = findLRULineIdx(cache, setBits);
    cache->sets[setBits].lines[evictionIdex].tag = tagBits;
    updateLRUcount(cache,setBits,evictionIdex);
    return 1;
}

void loadData(Cache *cache,int address,int size,int setBits,int tagBits ,int vebose, int *hit_count, int *miss_count, int *eviction_count) {
    if (isHit(cache, setBits, tagBits) == 1) {
        hit_count++;
        if (vebose==1) printf("hit ");
    } else {
        miss_count++;
        if (vebose==1) printf("miss ");
        if (updateCache(cache, setBits, tagBits) == 1) {
            eviction_count++;
            if (vebose==1) printf("eviction ");
        }
    }
}

void storeData(Cache *cache,int address,int size,int setBits,int tagBits,int vebose,int *hit_count,int *miss_count,int *eviction_count) {
    loadData(cache,address,size,setBits,tagBits,vebose,hit_count,miss_count,eviction_count);
}

void modifyData(Cache *cache,int address,int size,int setBits,int tagBits,int vebose,int *hit_count,int *miss_count,int *eviction_count){
    loadData(cache,address,size,setBits,tagBits,vebose,hit_count,miss_count,eviction_count);
    storeData(cache,address,size,setBits,tagBits,vebose,hit_count,miss_count,eviction_count);
}