#include <stdio.h>
#include "smallexpr.h"

int main() {
    printf("Result: %lf\n", evaluateExpr("2**10 - 24", NULL, NULL));
    return 0;
}
