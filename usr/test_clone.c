#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int child_func(void* arg) {
    char* buffer = (char*)arg;
    printf("Child sees buffer = \"%s\"\n", buffer);
    strcpy(buffer, "hello from child");
    return 0;
}

int main(int argc, char** argv) {
    // Allocate stack for child task.
    const int STACK_SIZE = 65536;
    char* stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        exit(1);
    }

    // When called with the command-line argument "vm", set the CLONE_VM flag on.
    unsigned long flags = 0;
    if (argc > 1 && !strcmp(argv[1], "vm")) {
        flags |= CLONE_VM;
    }

    char buffer[100];
    strcpy(buffer, "hello from parent");
    if (clone(child_func, stack + STACK_SIZE, flags | SIGCHLD, buffer) == -1) {
        perror("clone");
        exit(1);
    }

    int status;
    if (wait(&status) == -1) {
        perror("wait");
        exit(1);
    }

    printf("Child exited with status %d. buffer = \"%s\"\n", status, buffer);
    return 0;
}
