/* test_queue.c

   Spawn a single thread.
*/

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

typedef struct item {
    struct item* prev;
    struct item* next;
    queue_t queue;
    int data;
} item;

int print_item(void* item1, void* item2) {
    printf("%d ",((item*)item2)->data);
    return 0;
}

void
main(void) {
    int i;
    item num[100];
    item* it;
    queue_t q = queue_new();

    printf("Append 1 to 10:\n");
    for (i = 1; i <= 10; ++i) {
        num[i-1].data = i;
        if (queue_append(q, num+i-1))
            printf("queue_append failed on i = %d",i);
    }
    i = queue_iterate(q, print_item, NULL);
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("Return value: %d\n\n",i);

    printf("Prepend 11 to 20:\n");
    for (i = 11; i <= 20; ++i) {
        num[i-1].data = i;
        if (queue_prepend(q, num+i-1))
            printf("queue_prepend failed on i = %d",i);
    }
    i = queue_iterate(q, print_item, NULL);
    printf("\n");
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("Return value: %d\n\n",i);

    printf("First 20 elements in array:\n");
    for (i = 0; i < 20; ++i) {
        printf("%d ",num[i].data);
    }
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("\n");

    printf("Delete from the queue:\n");
    it = num + 15;
    printf("Data on the node: %d\n",it->data);
    i = queue_delete(q, (void**)&it);
    printf("Return value: %d\n",i);
    i = queue_iterate(q, print_item, NULL);
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("Return value: %d\n\n",i);

    printf("Delete from the queue:\n");
    it = num + 19;
    printf("Data on the node: %d\n",it->data);
    i = queue_delete(q, (void**)&it);
    printf("Return value: %d\n",i);
    i = queue_iterate(q, print_item, NULL);
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("Return value: %d\n\n",i);

    printf("Delete from the queue:\n");
    it = num + 9;
    printf("Data on the node: %d\n",it->data);
    i = queue_delete(q, (void**)&it);
    printf("Return value: %d\n",i);
    i = queue_iterate(q, print_item, NULL);
    printf("\n");
    printf("Length of the queue: %d\n",queue_length(q));
    printf("Return value: %d\n\n",i);

    printf("Dequeue every element:\n");
    while (0 == queue_dequeue(q, (void**)&it) && it != NULL)
        printf("%d ",it->data);
    queue_free (q);

    printf("\n");
    exit(0);
}
