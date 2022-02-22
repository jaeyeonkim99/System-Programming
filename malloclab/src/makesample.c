#include <stdlib.h>
#include <stdio.h>

int main(){
    int maxheap;
    scanf("%d", &maxheap);

    int numofpointer;
    scanf("%d", &numofpointer);

    int ops;

    scanf("%d", &ops);

    int weight;
    scanf("%d", &weight);

    printf("#include \"mm.h\"\n");
    printf("#include \"memlib.h\"\n");
    printf("int main(){\n");

    char operation;
    int id;
    int size;

    printf("mem_init();\n");

    while(scanf("%c %d %d",  &operation, &id, &size)!=EOF){;

        if(operation=='a'){
            printf("void *p%d = mm_malloc(%d);\n", id, size);
        }
        else if(operation=='f'){
            printf("mm_free(p%d);\n", id);
        }
        else if(operation=='r'){
            printf("p%d = mm_realloc(%d);\n", id, size);
        }
    }

    printf("}\n");

}