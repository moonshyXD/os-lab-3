#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __real_pthread_join(pthread_t, void **);

int __wrap_pthread_create(pthread_t *thread,
                          const pthread_attr_t *attributes,
                          void *(*start)(void *),
                          void *argument) {
    static int calls = 0;
    const char *failure = getenv("LAB3_FAIL_CREATE");
    if (failure != NULL && ++calls == atoi(failure)) {
        return EAGAIN;
    }
    return __real_pthread_create(thread, attributes, start, argument);
}

int __wrap_pthread_join(pthread_t thread, void **result) {
    static int calls = 0;
    const char *failure = getenv("LAB3_FAIL_JOIN");
    if (failure != NULL && ++calls == atoi(failure)) {
        return EINVAL;
    }
    return __real_pthread_join(thread, result);
}
