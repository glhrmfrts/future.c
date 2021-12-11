# future.c

Single file, header-only library implementing a **future**-type for C.

## Usage

```c
typedef ftr_of(int) int_future_t;

void f() {
    int_future_t* fut = ftr_new(int_future_t);
    assert(fut);
    assert(fut->value == 0 || !"value is zero initialized");
    assert(fut->header.is_valid || !"is valid");

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task, fut);

    int result;
    int err = ftr_get(fut, 10.0 * 1000.0, &result);
    assert(err == ftr_success);
    if (err) {
        fprintf(stderr, "error waiting future: %s\n", ftr_errorstr(err));
        thrd_join(thread, NULL);
        ftr_delete(fut);
        return;
    }

    assert(result == 42);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return;
}
```

# LICENSE

Public domain.