// SPDX-License-Identifier: MIT
// Copyright 2022, George Yohng

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define EXPRINT_ERRORVAL ((int)0x80000000)

int evaluateExprInt(
    const char *expr,
    int (*call)(const char *name, int nameLen, int *argv, int argc),
    const char **error);

#ifdef __cplusplus
}
#endif
