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
    define_error(ftr_error),
};

struct ftr_header {
    cnd_t cvar;
    mtx_t mtx;
    atomic_bool is_valid;
    atomic_bool is_set;
    size_t value_size;
};

#define ftr_of(ValueType) struct { struct ftr_header header; ValueType value; }

#define ftr_new(FT) ((FT*)ftr_new_(sizeof(FT)))

#define ftr_init(FT, fh) (ftr_init_((struct ftr_header*)(fh), sizeof(FT)))

#define ftr_get(fh, timeout_ms, dest) (ftr_get_((struct ftr_header*)(fh), (timeout_ms), (void*)(dest)))

#define ftr_complete(fh, val) (fh->value=(val), ftr_complete_((struct ftr_header*)(fh)))

#define ftr_destroy(fh) (ftr_destroy_((struct ftr_header*)(fh)))

#define ftr_delete(fh) (ftr_delete_((struct ftr_header*)(fh)))

int ftr_init_(struct ftr_header* fh, size_t size);

struct ftr_header* ftr_new_(size_t size);

void ftr_destroy_(struct ftr_header* fh);

void ftr_delete_(struct ftr_header* fh);

int ftr_wait_(struct ftr_header* fh, int32_t timeout_ms);

int ftr_get_(struct ftr_header* fh, int32_t timeout_ms, void* dest);

int ftr_complete_(struct ftr_header* fh);

const char* ftr_errorstr(int err);

#ifdef ftr_implementation

int ftr_init_(struct ftr_header* fh, size_t size) {
    fh->is_valid = true;
    fh->value_size = size - sizeof(struct ftr_header);
    if (cnd_init(&fh->cvar)) {
        return ftr_error;
    }
    if (mtx_init(&fh->mtx, mtx_plain)) {
        return ftr_error;
    }
    return ftr_success;
}

struct ftr_header* ftr_new_(size_t size) {
    struct ftr_header* fh = calloc(1, size);
    int err = ftr_init_(fh, size);
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
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);

    end.tv_sec = start.tv_sec + (timeout_ms / 1000);
    end.tv_nsec = (timeout_ms % 1000) * 1000000;

    mtx_lock(&fh->mtx);

    int err = cnd_timedwait(&fh->cvar, &fh->mtx, &end);
    if (err == thrd_timedout) {
        mtx_unlock(&fh->mtx);
        return ftr_timedout;
    }

    mtx_unlock(&fh->mtx);
    return ftr_success;
}

int ftr_get_(struct ftr_header* fh, int32_t timeout_ms, void* dest) {
    int err = ftr_wait_(fh, timeout_ms);
    if (err != ftr_success) {
        return err;
    }

    void* src = (void*)(fh + 1);
    memcpy(dest, src, fh->value_size);

    return ftr_success;
}

int ftr_complete_(struct ftr_header* fh) {
    if (!fh->is_valid) { return ftr_invalid; }
    
    cnd_signal(&fh->cvar);

    return ftr_success;
}

const char* ftr_errorstr(int err) {
    switch (err) {
    case ftr_success:   return "ftr_success";
    case ftr_invalid:   return "ftr_invalid";
    case ftr_timedout:  return "ftr_timedout";
    case ftr_nomem:     return "ftr_nomem";
    default:            return "ftr_errorstr: unknown error";
    }
}

#endif

#ifdef ftr_unittest

#include <assert.h>

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

int run_async_task_veryslow(void* arg) {
    int_future_t* fut = arg;
    thrd_sleep(&(struct timespec){.tv_sec=12}, NULL);
    int err = ftr_complete(fut, 42);
    if (err) {
        fprintf(stderr, "error completing future: %s\n", ftr_errorstr(err));
        return 1;
    }
    return 0;
}

bool ftr_runtest_success() {
    int_future_t* fut = ftr_new(int_future_t);
    assert(fut);
    assert(fut->value == 0 || !"value is zero initialized");
    assert(fut->header.is_valid || !"is valid");

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task, fut);
   
    int result;
    int err = ftr_get(fut, 10 * 1000, &result);
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

bool ftr_runtest_error() {
    int_future_t* fut = ftr_new(int_future_t);
    assert(fut);
    assert(fut->value == 0 || !"value is zero initialized");
    assert(fut->header.is_valid || !"is valid");

    thrd_t thread = {0};

    thrd_create(&thread, run_async_task_veryslow, fut);
   
    int result;
    int err = ftr_get(fut, 10 * 1000, &result);
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

bool ftr_runtest() {
    return ftr_runtest_success() && ftr_runtest_error();
}

#endif