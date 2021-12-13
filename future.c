/**
 * future.c - Future type for C.
 * Written by Guilherme Nemeth. Public Domain.
 * 
 * Usage:
 * 
 * typedef ftr_of(int) int_ftr_t;
 * 
 * int run_async_task(void* arg) {
 *   int_ftr_t* fut = arg;
 *   sleep(5);
 *   int err = ftr_complete(fut, 42);
 *   if (err) {
 *     fprintf(stderr, "error completing future: %s", ftr_errorstr(err));
 *   }
 * }
 * 
 * void main() {
 *   int_ftr_t fut = {0};
 *   ftr_init(int_ftr_t, &fut);
 * 
 *   thrd_create(&thread, run_async_task, &fut);
 * 
 *   int result;
 *   int err = ftr_get(&fut, 10.0 * 1000.0, &result);
 *   if (err) {
 *      fprintf(stderr, "error completing future: %s", ftr_errorstr(err));
 *      return;
 *   }
 * 
 *   assert(result == 42);
 *  
 *   ftr_destroy(&fut);
 * }
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <threads.h>
#include <stdatomic.h>

#ifndef define_error
#define define_error(e) e
#endif

enum {
    ftr_success,
    define_error(ftr_timedout),
    define_error(ftr_invalid),
    define_error(ftr_nomem),
    define_error(ftr_destsize),
    define_error(ftr_error),
};

struct ftr_header {
    cnd_t cvar;
    mtx_t mtx;
    atomic_bool is_valid;
    bool is_set;
    size_t value_size;
    size_t in_offs;
    size_t out_offs;
};

#define ftr_of(ValueType) struct { struct ftr_header header; ValueType in_value; ValueType out_value; }

#define ftr_new(FT) ((FT*)ftr_new_(sizeof(FT), sizeof(((FT*)0)->out_value), offsetof(FT, in_value), offsetof(FT, out_value)))

#define ftr_init(FT, future) (ftr_init_((struct ftr_header*)(future), sizeof(((FT*)0)->out_value)), offsetof(FT, in_value), offsetof(FT, out_value))

#define ftr_wait(future, timeout_ms) (ftr_wait_((struct ftr_header*)(future), (timeout_ms)))

#define ftr_get(future, timeout_ms, dest) (ftr_get_((struct ftr_header*)(future), (timeout_ms), (void*)(dest), sizeof(*(dest)), sizeof((*(dest))=future->out_value)))

#define ftr_complete(future, val) ((future)->in_value=(val), ftr_complete_((struct ftr_header*)(future)))

#define ftr_destroy(future) (ftr_destroy_((struct ftr_header*)(future)))

#define ftr_delete(future) (ftr_delete_((struct ftr_header*)(future)))

int ftr_init_(struct ftr_header* fh, size_t vsize, size_t in_offs, size_t out_offs);

struct ftr_header* ftr_new_(size_t wholesize, size_t vsize, size_t in_offs, size_t out_offs);

void ftr_destroy_(struct ftr_header* fh);

void ftr_delete_(struct ftr_header* fh);

int ftr_wait_(struct ftr_header* fh, int32_t timeout_ms);

int ftr_get_(struct ftr_header* fh, int32_t timeout_ms, void* dest, size_t dest_size, size_t check);

int ftr_complete_(struct ftr_header* fh);

const char* ftr_errorstr(int err);

#ifdef ftr_implementation

int ftr_init_(struct ftr_header* fh, size_t vsize, size_t in_offs, size_t out_offs) {
    fh->is_valid = true;
    fh->is_set = false;
    fh->value_size = vsize;
    fh->in_offs = in_offs;
    fh->out_offs = out_offs;
    if (cnd_init(&fh->cvar)) {
        return ftr_error;
    }
    if (mtx_init(&fh->mtx, mtx_plain)) {
        return ftr_error;
    }
    return ftr_success;
}

struct ftr_header* ftr_new_(size_t wholesize, size_t vsize, size_t in_offs, size_t out_offs) {
    struct ftr_header* fh = calloc(1, wholesize);
    int err = ftr_init_(fh, vsize, in_offs, out_offs);
    if (err) {
        fprintf(stderr, "error initializing future: %s", ftr_errorstr(err));
        return NULL;
    }
    return fh;
}

void ftr_destroy_(struct ftr_header* fh) {
    fh->is_valid = false;
    cnd_destroy(&fh->cvar);
    mtx_destroy(&fh->mtx);
}

void ftr_delete_(struct ftr_header* fh) {
    ftr_destroy_(fh);
    free(fh);
}

int ftr_wait_(struct ftr_header* fh, int32_t timeout_ms) {
    mtx_lock(&fh->mtx);
    if (fh->is_set) {
        mtx_unlock(&fh->mtx);
        return ftr_success;
    }
    if (timeout_ms == 0) {
        mtx_unlock(&fh->mtx);
        return ftr_timedout;
    }

    struct timespec start, end;
    timespec_get(&start, TIME_UTC);

    end.tv_sec = start.tv_sec + (timeout_ms / 1000);
    end.tv_nsec = (timeout_ms % 1000) * 1000000;

    int err = cnd_timedwait(&fh->cvar, &fh->mtx, &end);
    if (err == thrd_timedout) {
        mtx_unlock(&fh->mtx);
        return ftr_timedout;
    } else if (err) {
        mtx_unlock(&fh->mtx);
        return ftr_error;
    }

    mtx_unlock(&fh->mtx);
    return ftr_success;
}

int ftr_get_(struct ftr_header* fh, int32_t timeout_ms, void* dest, size_t dest_size, size_t check) {
    (void)check;

    if (!fh->is_valid) {
        return ftr_invalid;
    }

    if (dest_size != fh->value_size) {
        return ftr_destsize;
    }

    int err = ftr_wait_(fh, timeout_ms);
    if (err != ftr_success) {
        return err;
    }

    void* src = (void*)((uint8_t*)(fh) + fh->out_offs); // Out value
    memcpy(dest, src, fh->value_size);

    fh->is_valid = false;
    return ftr_success;
}

int ftr_complete_(struct ftr_header* fh) {
    if ((!fh->is_valid) || fh->is_set) { return ftr_invalid; }
    
    mtx_lock(&fh->mtx);
    
    void* src = (void*)((uint8_t*)(fh) + fh->in_offs); // In value
    void* dest = (void*)((uint8_t*)(fh) + fh->out_offs); // Out value
    memcpy(dest, src, fh->value_size);
    fh->is_set = true;

    cnd_signal(&fh->cvar);
    mtx_unlock(&fh->mtx);

    return ftr_success;
}

const char* ftr_errorstr(int err) {
    switch (err) {
    case ftr_success:   return "ftr_success";
    case ftr_invalid:   return "ftr_invalid: Invalid future object";
    case ftr_timedout:  return "ftr_timedout: Operation timed out";
    case ftr_nomem:     return "ftr_nomem: No memory available";
    case ftr_destsize:  return "ftr_destsize: The destination size is different from the value size";
    default:            return "ftr_errorstr: Unknown error";
    }
}

#endif

#ifdef ftr_unittest

#include <assert.h>

typedef ftr_of(int) int_future_t;

int run_async_task(void* arg) {
    int_future_t* fut = arg;
    thrd_sleep(&(struct timespec){.tv_sec=2}, NULL);
    int err = ftr_complete(fut, 42);
    if (err) {
        fprintf(stderr, "error completing future: %s\n", ftr_errorstr(err));
        return 1;
    }
    return 0;
}

int run_async_task_veryslow(void* arg) {
    int_future_t* fut = arg;
    thrd_sleep(&(struct timespec){.tv_sec=5}, NULL);
    int err = ftr_complete(fut, 42);
    if (err) {
        fprintf(stderr, "error completing future: %s\n", ftr_errorstr(err));
        return 1;
    }
    return 0;
}

typedef ftr_of(int16_t) int16_future_t;

bool ftr_test_valuesize() {
    printf("ftr_test_valuesize\n");

    int16_future_t* fut = ftr_new(int16_future_t);
    printf("fut->header.value_size = %d, sizeof = %d\n", fut->header.value_size, sizeof(int16_t));
    assert(fut->header.value_size == sizeof(int16_t));
    ftr_delete(fut);
    return true;
}

bool ftr_test_samethread() {
    printf("ftr_test_samethread\n");
    
    int_future_t* fut = ftr_new(int_future_t);
    assert(fut);
    assert(fut->in_value == 0 || !"value is zero initialized");
    assert(fut->out_value == 0 || !"value is zero initialized");
    assert(fut->header.is_valid || !"is valid");

    ftr_complete(fut, 42);

    int result;
    int err = ftr_get(fut, 10*1000, &result); if (err) {
        fprintf(stderr, "error getting future: %s\n", ftr_errorstr(err));
        ftr_delete(fut);
        return false;
    }

    assert(result == 42);

    return true;
}

bool ftr_test_twice() {
    printf("ftr_test_twice\n");

    int_future_t* fut = ftr_new(int_future_t);
    ftr_complete(fut, 42);

    int result;
    int err = ftr_get(fut, 10*1000, &result); if (err) {
        fprintf(stderr, "error getting future: %s\n", ftr_errorstr(err));
        ftr_delete(fut);
        return false;
    }

    err = ftr_complete(fut, 100);
    assert(err == ftr_invalid);

    err = ftr_get(fut, 10*1000, &result);
    assert(err == ftr_invalid);

    ftr_delete(fut);
    return true;
}

bool ftr_test_success() {
    printf("ftr_test_success\n");

    int_future_t* fut = ftr_new(int_future_t);

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task, fut);
   
    int result;
    int err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_success);
    if (err) {
        fprintf(stderr, "error waiting future: %s\n", ftr_errorstr(err));
        thrd_join(thread, NULL);
        ftr_delete(fut);
        return false;
    }
 
    assert(result == 42);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return true;
}

bool ftr_test_timedout() {
    printf("ftr_test_timedout\n");

    int_future_t* fut = ftr_new(int_future_t);

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task_veryslow, fut);
   
    int result;
    int err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_timedout);
    if (err) {
        fprintf(stderr, "error waiting future: %s\n", ftr_errorstr(err));
        thrd_join(thread, NULL);
        ftr_delete(fut);
        return true;
    }
 
    assert(result == 42);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return false;
}

bool ftr_test_tryagain() {
    printf("ftr_test_tryagain\n");

    int_future_t* fut = ftr_new(int_future_t);

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task_veryslow, fut);
   
    int result = 0;
    int err;
    
    err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_timedout);
    assert(result == 0);

    thrd_sleep(&(struct timespec){.tv_sec=2}, NULL);

    err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_success);
    assert(result == 42);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return true;
}

bool ftr_test_wait() {
    printf("ftr_test_wait\n");

    int_future_t* fut = ftr_new(int_future_t);

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task_veryslow, fut);
   
    int result = 0;
    int err = ftr_timedout;

    while (err == ftr_timedout) {
        err = ftr_wait(fut, 0);
        printf("ftr_test_wait: %s\n", ftr_errorstr(err));

        if (err != ftr_timedout && err != ftr_success) {
            thrd_join(thread, NULL);
            ftr_delete(fut);
            return false;
        }

        thrd_sleep(&(struct timespec){.tv_sec=1}, NULL);
    }

    err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_success);
    assert(result == 42);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return true;
}

struct data {
    char name[64];
    int x;
    int y;
};

typedef ftr_of(struct data) data_future_t;

int run_async_task_struct(void* arg) {
    data_future_t* fut = arg;
    thrd_sleep(&(struct timespec){.tv_sec=2}, NULL);

    struct data mydata = { "foobar", 200, 400 };
    int err = ftr_complete(fut, mydata);
    if (err) {
        fprintf(stderr, "error completing future: %s\n", ftr_errorstr(err));
        return 1;
    }
    return 0;
}

bool ftr_test_struct() {
    printf("ftr_test_struct\n");

    data_future_t* fut = ftr_new(data_future_t);

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task_struct, fut);
   
    struct data result;
    int err = ftr_get(fut, 4 * 1000, &result);
    assert(err == ftr_success);
    if (err) {
        fprintf(stderr, "error waiting future: %s\n", ftr_errorstr(err));
        thrd_join(thread, NULL);
        ftr_delete(fut);
        return false;
    }
 
    assert(!strcmp(result.name, "foobar"));
    assert(result.x == 200);
    assert(result.y == 400);

    thrd_join(thread, NULL);
    ftr_delete(fut);
    return true;
}

bool ftr_runtest() {
    return (
        ftr_test_valuesize() &&
        ftr_test_twice() &&
        ftr_test_samethread() &&
        ftr_test_success() &&
        ftr_test_timedout() &&
        ftr_test_tryagain() &&
        ftr_test_wait() &&
        ftr_test_struct()
    );
}

#endif