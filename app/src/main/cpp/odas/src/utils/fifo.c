#include <utils/fifo.h>

fifo_obj * fifo_construct_zero(const unsigned int nMaxElements) {

    fifo_obj * obj;

    obj = (fifo_obj *) malloc(sizeof(fifo_obj));

    obj->array = (void **) malloc(sizeof(void *) * nMaxElements);
    memset(obj->array, 0x00, sizeof(void *) * nMaxElements);

    obj->nElements = 0;
    obj->nMaxElements = nMaxElements;

    pthread_mutex_init(&(obj->use), NULL);
    pthread_cond_init(&(obj->cond_push), NULL);
    pthread_cond_init(&(obj->cond_pop), NULL);

    return obj;

}

void fifo_destroy(fifo_obj * obj) {

    pthread_cond_destroy(&(obj->cond_push));
    pthread_cond_destroy(&(obj->cond_pop));
    pthread_mutex_destroy(&(obj->use));
    free((void *) obj->array);
    free((void *) obj);

}

void fifo_push(fifo_obj * obj, void * element) {

    pthread_mutex_lock(&(obj->use));

    while (obj->nElements == obj->nMaxElements) {
        pthread_cond_wait(&(obj->cond_push), &(obj->use));
    }

    obj->array[obj->nElements] = element;
    obj->nElements++;

    pthread_cond_signal(&(obj->cond_pop));
    pthread_mutex_unlock(&(obj->use));

}

void * fifo_pop(fifo_obj * obj) {

    void * rtnPtr;

    pthread_mutex_lock(&(obj->use));

    while (obj->nElements == 0) {
        pthread_cond_wait(&(obj->cond_pop), &(obj->use));
    }

    rtnPtr = obj->array[0];
    memmove(&(obj->array[0]), &(obj->array[1]), (obj->nElements-1) * sizeof(void *));
    obj->nElements--;
    memset(&(obj->array[obj->nElements]), 0x00, (obj->nMaxElements-obj->nElements) * sizeof(void *));

    pthread_cond_signal(&(obj->cond_push));
    pthread_mutex_unlock(&(obj->use));

    return rtnPtr;

}

int fifo_nElements(fifo_obj * obj) {

    int nElements;

    pthread_mutex_lock(&(obj->use));
    nElements = obj->nElements;
    pthread_mutex_unlock(&(obj->use));

    return nElements;

}

void fifo_printf(const fifo_obj * obj) {

    unsigned int iElement;

    for (iElement = 0; iElement < obj->nMaxElements; iElement++) {

        printf("(%03u): %p\n",iElement,obj->array[iElement]);

    }

}
