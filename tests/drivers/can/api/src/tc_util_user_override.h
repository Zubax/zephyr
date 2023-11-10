/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TC_UTIL_USER_OVERRIDE_H__
#define __TC_UTIL_USER_OVERRIDE_H__

#ifdef __cplusplus
extern "C"
{
#endif
    int semihost_log(const char* fmt, ...);
#ifdef __cplusplus
};
#endif

// custom PRINT_DATA (replacing serial output with semihost writes)
#define PRINT_DATA semihost_log
#define PRINT PRINT_DATA
// This is for overriden messages of asserts, vprintk is used for that
#define vprintk PRINT_DATA

#endif /* __TC_UTIL_USER_OVERRIDE_H__ */
