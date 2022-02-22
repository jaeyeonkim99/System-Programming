#include "mm.h"
#include "memlib.h"

int main(){

    mem_init();
    void *p0, *p1, *p2, *p3, *p4, *p5;

    p0 = mm_malloc(2040);
    p1 = mm_malloc(2040);
    mm_free(p1);
    p2 = mm_malloc(48);
    p3 = mm_malloc(4072);
    mm_free(p3);
    p4 = mm_malloc(4072);
    mm_free(p0);
    mm_free(p2);
    p5 = mm_malloc(4072);
    mm_free(p4);
    mm_free(p5);
}
