#define ftr_implementation
#define ftr_unittest

#include "future.c"

int main(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    printf("future_test: %d\n", ftr_runtest());
}