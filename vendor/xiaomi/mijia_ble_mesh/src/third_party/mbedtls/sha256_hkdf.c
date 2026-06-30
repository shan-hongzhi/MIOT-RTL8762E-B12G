/*
 *  FIPS-180-2 compliant SHA-256 implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
/*
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "md.h"

#ifndef CEIL_DIV
#define CEIL_DIV(A, B)           (((A) + (B) - 1) / (B))
#endif

#ifndef   MIN
  #define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#endif

unsigned int mbedtls_sha256_hkdf(const unsigned char *key, unsigned int key_len,
                                const unsigned char *salt, unsigned int salt_len,
                                const unsigned char *info, unsigned int info_len,
                                unsigned char *out, unsigned int out_len)
{
    const mbedtls_md_info_t * sha256 = mbedtls_md_info_from_string("SHA256");
    const unsigned char null_salt[32] = {0};
    unsigned char PRK[32];
    unsigned char T_n[32];
    unsigned int loop;
    unsigned int temp_len;

    // Step 1: HKDF-Extract(salt, IKM) -> PRK
    if (salt == NULL)
        mbedtls_md_hmac(sha256, null_salt, 32, key, key_len, PRK);
    else
        mbedtls_md_hmac(sha256, salt, salt_len, key, key_len, PRK);

    // Step 2: HKDF-Expand(PRK, info, L) -> OKM
    //T(0) = empty string (zero length)
    //T(1) = HMAC-Hash(PRK, T(0) | info | 0x01)
    //T(2) = HMAC-Hash(PRK, T(1) | info | 0x02)
    //T(3) = HMAC-Hash(PRK, T(2) | info | 0x03)

    unsigned char temp[32 + info_len + 1];
    memset(temp, 0, 32 + info_len +1);
    loop = CEIL_DIV(out_len, 32);

    for (int i = 0; i < loop ; i++) {
        if (i == 0) {
            temp_len = 0;
        } else {
            memcpy(temp, T_n, 32);
            temp_len = 32;
        }

        memcpy(temp + temp_len, info, info_len);
        temp_len += info_len;

        temp[temp_len] = i + 1;
        temp_len += 1;

        mbedtls_md_hmac(sha256, PRK, 32, temp, temp_len, T_n);

        memcpy(out + 32 * i, T_n, MIN(32, out_len));
        out_len -= 32;
    }

    return 0;
}
