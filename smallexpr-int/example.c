#include <stdio.h>
#include <string.h>
#include "smallexpr-int.h"

static int vars[256];

int callHandler(const char *name, int nameLen, int *argv, int argc) {    
    if (nameLen == 1) {
        if (argc == -1) // special setter syntax, expect 1 argument
            return (vars[name[0]] = argv[0]);
        else if (argc == 0) // getter / call
            return vars[name[0]];
    }

    if (nameLen == 5 && strncmp(name, "print", 5) == 0) {
        for (int i = 0; i < argc; i++)
            printf("%d ", argv[i]);
        printf("\n");
        return 0;
    }

    if (nameLen == 9 && strncmp(name, "return888", 9) == 0) {
        return 888;
    }

    printf("Unknown function call (nameLen=%d): ",nameLen);
    fwrite(name, 1, nameLen, stdout);
    printf(" args: ");
    for (int i = 0; i < argc; i++)
        printf("%d ", argv[i]);
    printf("\n");
    return 0;
}

int main() {
    const char *err = NULL;
    int result = evaluateExprInt(
        "a = 10;"
        "b = 17;"
        "print(a,b);"
        "print(return888());"
        "a > b && print(1000);"
        "a < b && print(-1000);"
        "a + b"
        ,callHandler, &err);
    
    if (err)
        printf("Error: %s\n", err);
    else
        printf("Result: %d\n", result);

    return 0;
}
