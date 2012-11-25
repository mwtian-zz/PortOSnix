#include <stdlib.h>
#include "queue.h"
#include "queue_wrap.h"

struct queue_wrap_node {
    struct node node;
    void *data;
};

int
queue_wrap_prepend(queue_t q, void* data)
{
    struct queue_wrap_node *node;
    if (NULL == q)
        return -1;
    if ((node = malloc(sizeof(struct queue_wrap_node))) == NULL)
        return -1;
    node->data = data;
    return queue_prepend(q, node);
}

int
queue_wrap_enqueue(queue_t q, void* data)
{
    struct queue_wrap_node *node;
    if (NULL == q)
        return -1;
    if ((node = malloc(sizeof(struct queue_wrap_node))) == NULL)
        return -1;
    node->data = data;
    return queue_append(q, node);
}

int
queue_wrap_dequeue(queue_t q, void** data)
{
    struct queue_wrap_node *node;
    if (NULL == q || NULL == data)
        return -1;
    if (queue_dequeue(q, (void**) &node) == 0) {
        *data = node->data;
        free(node);
        return 0;
    } else {
        *data = NULL;
        return -1;
    }
}
