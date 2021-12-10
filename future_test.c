#define ftr_implementation
#define ftr_unittest

#include "future.c"

typedef ftr_of(int) int_future_t;

int main(int argc, const char** argv) {
    int_future_t* fut = ftr_new(int_future_t);
    ftr_delete(fut);

    printf("future_test: %d\n", ftr_runtest());
}