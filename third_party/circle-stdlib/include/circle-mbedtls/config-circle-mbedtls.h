/**
 * BMX configuration overlay for Mbed TLS 3.6 LTS on Circle.
 *
 * SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef BMX_CIRCLE_MBEDTLS_CONFIG_H
#define BMX_CIRCLE_MBEDTLS_CONFIG_H


#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT


#define MBEDTLS_HAVE_TIME_DATE

#include <mbedtls/mbedtls_config.h>


#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_TIMING_C


#undef MBEDTLS_AESCE_C

#endif
