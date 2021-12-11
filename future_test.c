#define ftr_implementation
#define ftr_unittest

#include "future.c"

int main(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    if (ftr_runtest()) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}