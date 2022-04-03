#include <iostream>
#include <map>

typedef struct {
    int key;
    int value;
    DlinkedNode* prev;
    DlinkedNode* next;
}DlinkedNode;

struct LRUcache {
    std::map<int,Node> cache;
};


typedef struct DlinkedNode Node;
