#include <stdio.h>
#include <stdlib.h>
#include "multilevel_queue_private.h"

#define LEVEL 4

typedef struct item {
    struct item* prev;
    struct item* next;
    queue_t queue;
    int data;
} item;

int
print_item(void* item1, void* item2) {
    printf("%d ",((item*)item2)->data);
    return 0;
}

int
main() {
    int i,j;
    item num[100];
    item *it;
    multilevel_queue_t mq = multilevel_queue_new(LEVEL);

    for (j=0; j<LEVEL; ++j) {
        printf("Level %d:\n",j);

        printf("Append 0 to 29:\n");
        for (i = 0; i < 30; ++i) {
            num[i].data = i;
            if (-1 == multilevel_queue_enqueue(mq, i % LEVEL, num+i))
                printf("multilevel_queue_enqueue failed on i = %d\n",i);
        }

        printf("Inspect each queue:\n");
        for (i = 0; i < LEVEL; ++i) {
            queue_iterate(mq->q[i], print_item, NULL);
            printf("\n");
        }

        printf("Dequeue 0 to 29 from level %d:\n",j);
        for (i = 0; i < 30; ++i) {
            if (-1 == multilevel_queue_dequeue(mq, j, (void**) &it))
                printf("multilevel_queue_dequeue failed on i = %d\n",i);
            if (NULL == it)
                printf("Dequeue fails on %d\n", i);
            else
                printf("%d ", it->data);
        }
        printf("\n");

        printf("Inspect each queue:\n");
        for (i = 0; i < LEVEL; ++i) {
            queue_iterate(mq->q[i], print_item, NULL);
            printf("\n");
        }
    }

    if (-1 == multilevel_queue_free(mq))
        printf("multilevel_queue_free failed.\n");
    else
        printf("multilevel_queue_free succeeded.\n");

    return 0;
}
