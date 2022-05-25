# smallexpr

## Introduction

This is a self-contained expression evaluator. Two versions are provided:
 - a double version
 - an integer-only version
 
Both versions of the evaluator are lean, do not use dynamic memory and do not rely on external number conversion routines, thus are suitable for embedded use.

The evaluator is capable of evaluating the following expression types:
 - addition/subtraction
 - multiplication/division/modulo
 - true/false logical comparisons (with short-circuited evaluation)
 - bitwise logical operators and shifts
 - parentheses

When handed a call handler function, it can be made to support additionally: 
 - variables
 - function calls


## API

The function API is:

```C
double evaluateExpr(
    const char *expr,
    double (*call)(const char *name, int nameLen, double *argv, int argc),
    const char **error);

int evaluateInt(
    const char *expr,
    int (*call)(const char *name, int nameLen, int *argv, int argc),
    const char **error);
```

### Return value:

The function returns the result of the expression evaluation. If an error occurs, the function will return NaN for the double version and EXPRINT_ERRORVAL (-2147483648) for the integer version. You can differentiate the error values from the result of the calculation by passing and checking `error` against NULL (success).

### Parameters:
 - `expr` - an expression to evaluate
 - `call` - a handler function to call when identifiers or function calls are found in the expression, passing NULL is acceptable.
 - `error` - returned error text, NULL is written to `*error` on success; if no error handling is necessary, pass NULL.

### Call Handler Function

```C
// for evaluateExprInt
int (*call)(const char *name, int nameLen, int *argv, int argc)

// for evaluateExpr
double (*call)(const char *name, int nameLen, double *argv, int argc)
```

The function takes the identifier name and its length. The name is not null-terminated because it points directly to a part of the expression string passed to the evaluate function.

The actual call parameters are passed via `argv` and argc.

Variable getters (identifiers referenced without parentheses) are handled as function calls with zero arguments.

Variable setters (e.g. `"varName = 5;"` passed as an expression) are handled through a special case, where `argc` is set to -1, and `argv` contains a single element.

If you use modern C++ and have `string_view`, then a comfortable way of checking the identifier name could be done in the following way:

```C
double callHandler(const char *name, int nameLen, double *argv, int argc) {
    std::string_view ident(name, nameLen);
    if (ident == "pow" && argc == 2) return pow(argv[0], argv[1]);
    if (ident == "sin" && argc == 1) return sin(argv[0]);
    ...
    ...
    ...
}
```

## Example Use
Here's an example use:

### Example 1:
```C
#include <stdio.h>
#include "smallexpr.h"

int main() {
    printf("Result: %lf\n", evaluateExpr("2**10 - 24", NULL, NULL));
    return 0;
}
```

### Example 2:
```C
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
```

### Example 3:

```C
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
```

Output:
```
10 17 
888 
-1000 
Result: 27
```

## Notes

The function currently does not check for internal stack overflow, the size of which is currently hardcoded.
