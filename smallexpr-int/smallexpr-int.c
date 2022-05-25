// SPDX-License-Identifier: MIT
// Copyright 2022, George Yohng

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "smallexpr-int.h"

#define EXPR_ERRORVAL EXPRINT_ERRORVAL

int evaluateExprInt(
    const char *expr,
    int (*call)(const char *name, int nameLen, int *argv, int argc),
    const char **error) {

    int opStack[64] = {0}, opStackPtr = 0, valueStackPtr = 0;
    int valueStack[64];

    enum {
        // state machine constants
        START = 1, NUM = 2, IDENT = 4, PAREN = 8, CALL = 16, ASSIGN = 32, BINOP = 256, UNOP = 512,
        OP = BINOP | UNOP, CAN_LITERAL = START | BINOP | UNOP | PAREN | CALL | ASSIGN, AFTER_LITERAL = NUM | IDENT,
        CAN_STATEMENT = START | CALL | AFTER_LITERAL,

        // stack folding states
        IDLE = -1, FOLDED = -2,
    };

    int state = START;
    int lastValue = 0.0;
    const char *lastIdent = expr;
    int lastIdentLen = 0;

    int
        scanningArgs = 0, // state of whether to accept commas for parsing
        argcOpstackPtr = 0, // for a function call, pointer in the stack where the argument count is stored
        foldStackArg = IDLE, // stack folding request parameter
        skipCounter = 0; // used for short-circuit evaluation of && and ||

    int i = 0;
    char inCh = expr[i];

    valueStack[0] = 0;
    opStack[0] = START;

    for (;;) {
        if (foldStackArg >= 0) {
            if (state == IDENT) {
                if (!call) {
                callError:
                    if (error) *error = "calling a null function";
                    return EXPR_ERRORVAL;
                }
                lastValue = skipCounter ? 0.0 : call(lastIdent, lastIdentLen, NULL, 0);
                state = NUM;
            }

            static const char *opCharacters = " U+u!ALl-   N /O%EP&  <G| r >^~*";
            static const unsigned char opPriorities[32] = {
                0, 40, 15, 40, 40, 3, 8, 9,
                15, 0, 0, 0, 7, 0, 20, 2,
                20, 7, 30, 6, 0, 0, 8, 8,
                4, 0, 9, 0, 8, 5, 40, 20};

            char foldOpChar = (char) foldStackArg;
            int prioHashIdx = (foldOpChar * 1554 >> 9) & 31;
            int foldPrio = opCharacters[prioHashIdx] == foldOpChar ? opPriorities[prioHashIdx] : 0;

            for (;; opStackPtr--) {
                int op = opStack[opStackPtr];
                if (!(op & OP)) break;

                char opChar = (char) op;
                if (opChar == 'P' && foldOpChar == 'P') // right-associative power operator
                    break;

                prioHashIdx = (opChar * 1554 >> 9) & 31;
                int opPrio = opCharacters[prioHashIdx] == opChar ? opPriorities[prioHashIdx] : 0;
                if (opPrio < foldPrio)
                    break;

                int a = (op & BINOP) ? valueStack[valueStackPtr--] : lastValue;
                int b = lastValue;

                if (!skipCounter) { // full evaluation
                    switch (opChar) {
                        case '0': lastValue = 0.0; break; // dummy operation
                        case 'u': lastValue = -a; break;
                        case '~': lastValue = ~a; break;
                        case '!': lastValue = !a; break;
                        case '+': lastValue = a + b; break;
                        case '-': lastValue = a - b; break;
                        case '*': lastValue = a * b; break;
                        case '/': lastValue = a / b; break;
                        case '%': lastValue = a % b; break;
                        case 'P': {
                            if (b < 0) {
                                if (error) *error = "negative powers not supported";
                                return EXPR_ERRORVAL;
                            }

                            int res = 1;
                            for (int p = 0; p < b; p++)
                                res *= p;

                            lastValue = res;
                            break;
                        } 
                        case '<': lastValue = a < b; break;
                        case '>': lastValue = a > b; break;
                        case 'E': lastValue = a == b; break;
                        case 'N': lastValue = a != b; break;
                        case 'L': lastValue = a <= b; break;
                        case 'G': lastValue = a >= b; break;
                        case '|': lastValue = a | b; break; // NOLINT(cppcoreguidelines-narrowing-conversions)
                        case '&': lastValue = a & b; break; // NOLINT(cppcoreguidelines-narrowing-conversions)
                        case '^': lastValue = a ^ b; break; // NOLINT(cppcoreguidelines-narrowing-conversions)
                        case 'l': lastValue = a << b; break; // NOLINT(cppcoreguidelines-narrowing-conversions)
                        case 'r': lastValue = a >> b; break; // NOLINT(cppcoreguidelines-narrowing-conversions)
                        case '=': {
                            int size = opStack[opStackPtr--];
                            int offs = opStack[opStackPtr--];
                            if (!call) goto callError;
                            lastValue = call(&expr[offs], size, &lastValue, -1); // setter
                            break;
                        }
                        case 'O':
                            lastValue = a || b;
                            if (a) skipCounter--;
                            assert(skipCounter >= 0);
                            break;
                        case 'A':
                            lastValue = a && b;
                            if (!a) skipCounter--;
                            assert(skipCounter >= 0);
                            break;
                        default:
                            if (error) *error = "unrecognized operator";
                            return EXPR_ERRORVAL;
                    }
                } else { // short-circuited evaluation
                    switch (opChar) {
                        default:
                            lastValue = 0.0;
                            break;
                        case '=': {
                            opStackPtr--;
                            opStackPtr--;
                            lastValue = 0.0;
                            break;
                        }
                        case 'O':
                            lastValue = a || b;
                            if (a) skipCounter--;
                            assert(skipCounter >= 0);
                            break;
                        case 'A':
                            lastValue = a && b;
                            if (!a) skipCounter--;
                            assert(skipCounter >= 0);
                            break;
                    }
                }
            }
            foldStackArg = FOLDED;
        }

        // end of expression reached
        if (inCh == '\0') {
            if (state == START) {
                if (error) *error = NULL;
                return lastValue; // empty statement, return last stored value
            }

            if (foldStackArg != FOLDED) {
                foldStackArg = 0;
                continue;
            }

            if (opStackPtr > 1) {
                if (error) *error = "incomplete expression";
                return EXPR_ERRORVAL;
            }

            assert(opStackPtr == 1 && opStack[opStackPtr] == START);
            assert(valueStackPtr == 0);
            if (opStackPtr > 0) opStackPtr--;
            if (error) *error = NULL;
            return lastValue;
        }

        // skip whitespace
        if (inCh == ' ' || inCh == '\t' || inCh == '\n') {
            inCh = expr[++i];
            while (inCh == ' ' || inCh == '\t' || inCh == '\n') inCh = expr[++i];
            continue;
        }

        if (state & CAN_LITERAL) {
            // parse an identifier
            if (isalpha(inCh) || inCh == '_') {
                int start = i;
                inCh = expr[++i];
                while (isalpha(inCh) || isdigit(inCh) || inCh == '_') inCh = expr[++i];

                lastIdent = &expr[start];
                lastIdentLen = i - start;
                opStack[++opStackPtr] = state;
                state = IDENT;
                continue;
            }

            // parse a hex number
            if (inCh == '0' && (expr[i + 1] == 'x' || expr[i + 1] == 'X')) {
                ++i;
                inCh = expr[++i];
                lastValue = 0;
                while (isdigit(inCh) || (inCh >= 'a' && inCh <= 'f') || (inCh >= 'A' && inCh <= 'F')) {
                    lastValue = lastValue * 16 + ((inCh + 9 + ((inCh >> 4) & 1) * -9) & 15);
                    inCh = expr[++i];
                }

                opStack[++opStackPtr] = state;
                state = NUM;
                continue;
            }

            // parse a number
            if (isdigit(inCh)) {
                int start = i;
                lastValue = inCh - '0';
                inCh = expr[++i];
                while (isdigit(inCh)) {
                    lastValue = lastValue * 10 + (inCh - '0');
                    inCh = expr[++i];
                }

                opStack[++opStackPtr] = state;
                state = NUM;
                continue;
            }

            // parse an unary operator
            if (inCh == '-' || inCh == '~' || inCh == '!' || inCh == '+') {
                if (inCh == '+') inCh = 'U';
                if (inCh == '-') inCh = 'u';

                opStack[++opStackPtr] = state;
                state = (int) ((unsigned char) inCh) | UNOP;
                inCh = expr[++i];
                continue;
            }

            // parse a grouped expression in parentheses
            if (inCh == '(') {
                opStack[++opStackPtr] = state;
                opStack[++opStackPtr] = scanningArgs;
                scanningArgs = 0;
                state = PAREN;
                inCh = expr[++i];
                continue;
            }
        }

        if (state & CAN_STATEMENT) {
            // parse end of statement
            if (inCh == ';') {
                if (foldStackArg != FOLDED) {
                    foldStackArg = 0;
                    continue;
                }
                foldStackArg = IDLE;

                if (state != START && state != CALL) opStackPtr--;
                if (scanningArgs)
                    state = CALL;
                else
                    state = START;

                inCh = expr[++i];
                continue;
            }
        }

        if (state & AFTER_LITERAL) {
            // parse argument separator
            if (inCh == ',' && scanningArgs) {
                if (foldStackArg != FOLDED) {
                    foldStackArg = 0;
                    continue;
                }
                foldStackArg = IDLE;

                if (opStack[opStackPtr] != CALL) {
                    if (error) *error = "unknown state found at comma";
                    return EXPR_ERRORVAL;
                }

                opStackPtr--;
                opStack[argcOpstackPtr]++;
                valueStack[++valueStackPtr] = lastValue;
                state = CALL;
                inCh = expr[++i];
                continue;
            }

            // parse closing parentheses
            if (inCh == ')') {
                if (foldStackArg != FOLDED) {
                    foldStackArg = 0;
                    continue;
                }
                foldStackArg = IDLE;

                if (opStack[opStackPtr] == PAREN) { // grouped expression
                    opStackPtr--;
                    scanningArgs = opStack[opStackPtr--];

                } else if (opStack[opStackPtr] == CALL && scanningArgs) { // function call
                    opStackPtr--;
                    int size = opStack[opStackPtr--];
                    int offs = opStack[opStackPtr--];
                    int argCount = opStack[opStackPtr--] + 1;

                    argcOpstackPtr = opStack[opStackPtr--];
                    scanningArgs = opStack[opStackPtr--];
                    valueStack[++valueStackPtr] = lastValue;

                    if (!call) goto callError;
                    lastValue = skipCounter ?
                                0.0 :
                                call(&expr[offs], size, &valueStack[valueStackPtr] + 1 - argCount, argCount);
                    state = NUM;
                    valueStackPtr -= argCount;
                } else {
                    if (error) *error = "unexpected parenthesis";
                    return EXPR_ERRORVAL;
                }

                inCh = expr[++i];
                continue;
            }

            // if it's a binary operator or a binary operator start: +-*/<>=|&^!%
            if ((inCh >= '%' && inCh <= '/' && "11...11.1.1"[inCh - '%'] == '1') || inCh == '!' ||
                (inCh >= '<' && inCh <= '>') || inCh == '^' || inCh == '|') {

                static const int dualCharOps[16] = {
                    15420, 15676, 15677, 15678, 15934, 0, 15649,
                    9766, 31868, 0, 0, 0, 0, 0, 10794, 0};

                char nextChar = expr[i + 1];
                int hashIndex = ((inCh * 8 + nextChar * 7) >> 3) & 15;

                int oldi = i;
                int op = (int) (unsigned char) inCh;

                // translate: ==:E !=:N <=:L >=:G <<:l >>:r ||:O &&:A
                if (dualCharOps[hashIndex] == inCh + (nextChar << 8)) {
                    op = "lLEGr.NAO.....P."[hashIndex]; // NOLINT(cert-str34-c)
                    i++;
                }

                // assignment operator, special case
                if (op == '=' && state == IDENT) {
                    opStack[++opStackPtr] = (int) (lastIdent - expr);
                    opStack[++opStackPtr] = lastIdentLen;
                    state = '=' | UNOP;
                    inCh = expr[++i];
                    continue;
                }

                if (foldStackArg != FOLDED) {
                    foldStackArg = op;
                    i = oldi;
                    continue;
                }
                foldStackArg = IDLE;

                // short-circuited evaluation for && and ||
                if (op == 'A') {
                    if (!lastValue)
                        skipCounter++;
                } else if (op == 'O') {
                    if (lastValue)
                        skipCounter++;
                }

                valueStack[++valueStackPtr] = lastValue;
                state = op | BINOP;
                inCh = expr[++i];
                continue;
            }

            // function call
            if (state == IDENT && inCh == '(') {
                opStack[++opStackPtr] = scanningArgs;
                opStack[++opStackPtr] = argcOpstackPtr;
                opStack[++opStackPtr] = 0; // argument counter slot
                argcOpstackPtr = opStackPtr;
                opStack[++opStackPtr] = (int) (lastIdent - expr);
                opStack[++opStackPtr] = lastIdentLen;
                state = CALL;
                scanningArgs = 1;
                inCh = expr[++i];
                continue;
            }
        }

        // parsing function call
        if (state == CALL && inCh == ')') {
            assert(opStackPtr >= 5);
            int size = opStack[opStackPtr--];
            int offs = opStack[opStackPtr--];
            int argCount = opStack[opStackPtr--];
            argcOpstackPtr = opStack[opStackPtr--];
            scanningArgs = opStack[opStackPtr--];
            assert(argCount == 0);
            if (!call) goto callError;
            lastValue = skipCounter ? 0.0 : call(&expr[offs], size, NULL, 0);
            state = NUM;
            inCh = expr[++i];
            continue;
        }

        if (error) *error = "syntax error";
        return EXPR_ERRORVAL;
    }
}

#undef EXPR_ERRORVAL


#ifdef TEST_SMALLEXPR
#include <stdio.h>
// Also compile with -DDEBUG to enable assertions

int main() {
#define TESTEVAL(expr) { \
    int gt = (int)(expr); \
    int res = evaluateExprInt(#expr,NULL,NULL); \
    if (gt != res) { printf("Test failed: %s GT %d RES %d\n", #expr, gt, res); } \
}

    TESTEVAL(16 * 17 + 18)
    TESTEVAL((5 - 5 - 5) == -5)
    TESTEVAL(5 + 5)
    TESTEVAL(17 * 17 - 18)
    TESTEVAL(17 * 17 * 18)
    TESTEVAL(17 * 17 / 18)
    TESTEVAL(17 ^ 18)
    TESTEVAL(17 % 18)
    TESTEVAL(17 == 18)
    TESTEVAL(17 != 18)
    TESTEVAL(0x07012ABD)
    TESTEVAL(0x07012ABD == 117516989)
    TESTEVAL(17 < 18)
    TESTEVAL(17 <= 18)
    TESTEVAL(17 > 18)
    TESTEVAL(17 >= 18)
    TESTEVAL((((10 ^ 2) + (10 ^ 2)) * (10 - 10)))
    TESTEVAL((((10 ^ 2) + (10 ^ 2)) * (10 - 10)) / 10)
    TESTEVAL((((10 ^ 2) + (10 ^ 2)) * (10 - 10)) / 10 ^ 2)
    TESTEVAL(5 * 5 == 25)
    TESTEVAL(5 * 5 / 5 == 5)
    TESTEVAL(5 * 5 / 5 * 5 == 5)
    TESTEVAL((5 * 5 * 5 * 5) == 625)
    TESTEVAL((5 + 5) == 10)
    TESTEVAL((5 - 5) == 0)
    TESTEVAL((5 + 5 - 5) == 5)
    TESTEVAL((5 - 5 + 5) == 5)
    TESTEVAL((5 + 5 + 5) == 15)
    TESTEVAL((100 / 2) == 50)
    TESTEVAL((100 / 2 / 2) == 25)
    TESTEVAL((100 / 2 * 2) == 100)
    TESTEVAL((100 / 2 / 2 * 2) == 50)
    TESTEVAL(1 && 0 && 1)
    TESTEVAL(1 || 1 && 0)

    return 0;
}

#endif
