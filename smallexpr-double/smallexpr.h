// SPDX-License-Identifier: MIT
// Copyright 2022, George Yohng

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

double evaluateExpr(
    const char *expr,
    double (*call)(const char *name, int nameLen, double *argv, int argc),
    const char **error);

#ifdef __cplusplus
}
#endif
