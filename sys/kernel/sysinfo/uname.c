#include "nwos_internal.h"

void uname_r(void) {
    print(OS_VERSION_STR "\n");
}

void uname(void) {
    print("NwOS\n");
}

void uname_p(void) {
    print("x86_64 CPU\n");
}

void uname_o(void) {
    print("NwOS\n");
}

void uname_a(void) {
    print(OS_NAME " | " OS_VERSION_STR " | x86_64 CPU\n");
}