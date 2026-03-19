#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    for (int i = 0; i < 2; i++) {
        fork();
        printf("X");
    }
}