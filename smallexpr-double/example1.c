#include <stdio.h>
#include "smallexpr.h"

char buf[256];
int main() {
    printf("Enter an expression to evaluate:\n");
    if (!fgets(buf, sizeof(buf), stdin))
        return 0;
    
    const char *err = NULL;
    double result = evaluateExpr(buf, NULL, &err);
    
    if (err)
        printf("Error: %s\n", err);
    else
        printf("Result: %lf\n", result);
    
    return 0;
}
