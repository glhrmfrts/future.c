/**
 * future.c - Future type for C.
 * 
 * Usage:
 * 
 * typedef ftr_of(int) int_ftr_t;
 * 
 * int run_async_task(void* arg) {
 *   int_ftr_t* fut = arg;
 *   sleep(5);
 *   ftr_error_t err = ftr_complete(fut, 42);
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
 *   ftr_error_t err = ftr_get(&fut, 10.0 * 1000.0, &result);
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
#include <time.h>
#include <threads.h>
#include <stdatomic.h>

typedef enum ftr_error {
    ftr_success,
    ftr_timedout,
    ftr_invalid,
    ftr_nomem,
} ftr_error_t;

struct ftr_header {
    atomic_bool is_valid;
    atomic_bool is_set;
    size_t value_size;
};

#define ftr_of(ValueType) struct { struct ftr_header header; ValueType value; }

#define ftr_new(FT) ((FT*)ftr_new_(sizeof(FT)))

#define ftr_init(FT, fh) (ftr_init_((struct ftr_header*)(fh), sizeof(FT)))

#define ftr_get(fh, timeout_ms, dest) (ftr_get_((struct ftr_header*)(fh), (timeout_ms), (void*)(dest)))

#define ftr_complete(fh, value) (ftr_complete_((struct ftr_header*)(fh), (void*)(value)))

#define ftr_destroy(fh) (ftr_destroy_((struct ftr_header*)(fh)))

#define ftr_delete(fh) (ftr_delete_((struct ftr_header*)(fh)))

// utility function: difference between timespecs in microseconds
double ftr_usdiff_(struct timespec s, struct timespec e);

double ftr_msdiff_(struct timespec s, struct timespec e);

void ftr_init_(struct ftr_header* fh, size_t size);

struct ftr_header* ftr_new_(size_t size);

void ftr_destroy_(struct ftr_header* fh);

void ftr_delete_(struct ftr_header* fh);

ftr_error_t ftr_wait_(struct ftr_header* fh, double timeout_ms);

ftr_error_t ftr_get_(struct ftr_header* fh, double timeout_ms, void* dest);

ftr_error_t ftr_complete_(struct ftr_header* fh, void* value);

const char* ftr_errorstr(ftr_error_t err);

#ifdef ftr_implementation

// utility function: difference between timespecs in microseconds
double ftr_usdiff_(struct timespec s, struct timespec e) {
    double sdiff = difftime(e.tv_sec, s.tv_sec);
    long nsdiff = e.tv_nsec - s.tv_nsec;
    if(nsdiff < 0) return 1000000*(sdiff-1) + (1000000000L+nsdiff)/1000.0;
    else return 1000000*(sdiff) + nsdiff/1000.0;
}

double ftr_msdiff_(struct timespec s, struct timespec e) {
    return ftr_usdiff_(s, e) / 1000.0;
}

void ftr_init_(struct ftr_header* fh, size_t size) {
    fh->is_valid = true;
    fh->value_size = size - sizeof(struct ftr_header);
}

struct ftr_header* ftr_new_(size_t size) {
    struct ftr_header* fh = calloc(1, size);
    ftr_init_(fh, size);
    return fh;
}

void ftr_destroy_(struct ftr_header* fh) {
    fh->is_valid = false;
}

void ftr_delete_(struct ftr_header* fh) {
    fh->is_valid = false;
    free(fh);
}

ftr_error_t ftr_wait_(struct ftr_header* fh, double timeout_ms) {
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);

    while (!fh->is_set) {
        thrd_yield();
        timespec_get(&end, TIME_UTC);

        if (ftr_msdiff_(start, end) > timeout_ms) {
            return ftr_timedout;
        }
    }

    return ftr_success;
}

ftr_error_t ftr_get_(struct ftr_header* fh, double timeout_ms, void* dest) {
    ftr_error_t err = ftr_wait_(fh, timeout_ms);
    if (err != ftr_success) {
        return err;
    }

    void* src = (void*)(fh + 1);
    memcpy(dest, src, fh->value_size);

    return ftr_success;
}

ftr_error_t ftr_complete_(struct ftr_header* fh, void* value) {
    if (!fh->is_valid) { return ftr_invalid; }

    void* dest = (void*)(fh + 1);
    memcpy(dest, value, fh->value_size);
    fh->is_set = true;

    return ftr_success;
}

const char* ftr_errorstr(ftr_error_t err) {
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

bool ftr_runtest() {
    return true;
}

#endif