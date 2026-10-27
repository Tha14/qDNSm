#ifndef QDNSM_MBEDTLS_CONFIG_H
#define QDNSM_MBEDTLS_CONFIG_H

/*
 * Minimal mbedTLS 2.28.10 client for qDNSmNetLib.
 *
 * TLS 1.2 only, ECDHE with AES-GCM, SHA-256/384, SNI, no certificate
 * verification. There is no file system, no socket layer, no timing module,
 * no threading, no PSA, no debug/error strings and no self-tests: the module
 * supplies TCP and entropy itself and is the only thing that ever calls in.
 */

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C

/*
 * Keep libc's calloc/free/exit out of the link. platform.c initialises its
 * function pointers before mbedtls_memory_buffer_alloc_init() replaces them;
 * the defaults would pull in malloc_lv2.o, which in turn pulls errno.o and
 * _Errno, and a PRX cannot carry a TLS variable. The stubs are unreachable
 * after the arena is installed.
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* qDNSmMbedtlsStubCalloc(size_t count, size_t size);
void qDNSmMbedtlsStubFree(void* pointer);
void qDNSmMbedtlsExit(int status);

#ifdef __cplusplus
}
#endif

#define MBEDTLS_PLATFORM_STD_CALLOC qDNSmMbedtlsStubCalloc
#define MBEDTLS_PLATFORM_STD_FREE qDNSmMbedtlsStubFree
#define MBEDTLS_PLATFORM_EXIT_MACRO qDNSmMbedtlsExit

#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_MD_C
#define MBEDTLS_CIPHER_C

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED

#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_RSA_NO_CRT
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

#define MBEDTLS_SSL_CIPHERSUITES \
	MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
	MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
	MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, \
	MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384

#endif
