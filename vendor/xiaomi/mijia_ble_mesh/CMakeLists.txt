# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral)

target_sources(app PRIVATE
  src/main.c
  src/model.c
  src/mible_core.c
  src/mi_mesh_otp.c
# src/mi_potp_0xec4d3ebdd2c1.c
  src/nvm.c
  src/mible_crypto.c
  src/mi_crypto_backend_mbedtls.c
  src/third_party/mbedtls/asn1parse.c
  src/third_party/mbedtls/sha256.c
  src/third_party/mbedtls/sha256_hkdf.c
  src/third_party/mbedtls/md.c
  src/third_party/mbedtls/md_wrap.c
  src/third_party/mbedtls/ccm.c
  src/third_party/mbedtls/base64.c
  src/third_party/micro-ecc/uECC.c
)

add_definitions(-DuECC_VLI_NATIVE_LITTLE_ENDIAN=0)
