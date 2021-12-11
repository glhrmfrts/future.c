# future.c

Single file, header-only library implementing a **future**-type for C.

## Usage

```c
typedef ftr_of(int) int_future_t;

int run_async_task(void* arg) {
    int_future_t* fut = arg;
    thrd_sleep(&(struct timespec){.tv_sec=8}, NULL);
    int err = ftr_complete(fut, 42);
    if (err) {
        fprintf(stderr, "error completing future: %s\n", ftr_errorstr(err));
        return 1;
    }
    return 0;
}

void f() {
    int_future_t* fut = ftr_new(int_future_t);

    thrd_t thread = {0};
    thrd_create(&thread, run_async_task, fut);

    int result;
    int err = ftr_get(fut, 10 * 1000, &result);
    if (err) {
        fprintf(stderr, "error waiting future: %s\n", ftr_errorstr(err));
        thrd_join(thread, NULL);
        ftr_delete(fut);
        return;
    }

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return;
}
```

# LICENSE

Public domain.