# future.c

Single file, header-only library implementing a **future**-type for C11.

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

## Docs

##### `ftr_of(Type)`

Macro to create a struct type that will hold a value of `Type`. It's meant to be used as `typedef ftr_of(Type) FutureType`.

##### `FutureType* ftr_new(FutureType)`

Allocate and initialize a future of type `FutureType`, that is, the type you typedef'ed with ftr_of.

##### `int ftr_get(FutureType* future, int32_t timeout_ms, Type* dest)`

Get the value of the `future` and place it in `dest`, or waits up to `timeout_ms` milliseconds if not ready yet. Returns 0 on success or one of the [Error codes](#error-codes).

##### `int ftr_wait(FutureType* future, int32_t timeout_ms)`

Waits up to `timeout_ms` milliseconds for the `future` to be complete. Returns 0 on success or one of the [Error codes](#error-codes).

##### `int ftr_complete(FutureType* future, Type value)`

Completes the `future` with the given `value`. If there's a thread waiting on `ftr_get` that thread is signaled and the value is received there. Returns 0 on success or one of the [Error codes](#error-codes).

##### `void ftr_delete(FutureType* future)`

Release a future object that was created with `ftr_new`.

## Error codes

- `ftr_success` -> Code returned on success.
- `ftr_invalid` -> The future object is in a invalid state.
- `ftr_timedout` -> The operation timed out.
- `ftr_nomem` -> No memory available.
- `ftr_destsize` -> The size of the `dest` parameter in `ftr_get` is not enough to hold the future's value.
- `ftr_error` -> Other unknown error ocurred.

# LICENSE

Public domain.
