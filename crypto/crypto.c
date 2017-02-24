/*
 * ubirch#1 crypto driver.
 *
 * @author Matthias L. Jugel
 * @date 2016-04-09
 *
 * Copyright 2016 ubirch GmbH (https://ubirch.com)
 *
 * ```
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ```
 */

#if !FSL_FEATURE_SOC_LTC_COUNT
#  include <fsl_ltc.h>
#endif

#include <stdio.h>
#include "wolfssl/wolfcrypt/sha512.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/ed25519.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "crypto.h"

#ifdef NDEBUG
#  define UCERROR(p, e)   printf("E: %s: %d\r\n", (p), (e))
#  define UCDUMP(p, b, s) dbg_dump((p), (b), (s))
#else
#  define UCERROR(...)
#  define UCDUMP(...)
#endif


void dbg_dump(const char *prefix, const uint8_t *b, size_t size) {
  for (int i = 0; i < size; i += 16) {
    if (prefix && strlen(prefix) > 0) printf("%s %06x: ", prefix, i);
    for (int j = 0; j < 16; j++) {
      if ((i + j) < size) printf("%02x", b[i + j]); else printf("  ");
      if ((j+1) % 2 == 0) putchar(' ');
    }
    putchar(' ');
    for (int j = 0; j < 16 && (i + j) < size; j++) {
      putchar(b[i + j] >= 0x20 && b[i + j] <= 0x7E ? b[i + j] : '.');
    }
    printf("\r\n");
  }
}


WC_RNG uc_random;

static int initialized = false;

int uc_init() {
  if (initialized) return true;
//  trng_config_t trngConfig;
//  TRNG_GetDefaultConfig(&trngConfig);
//  trngConfig.sampleMode = kTRNG_SampleModeVonNeumann;
//  if (TRNG_Init(TRNG0, &trngConfig) != kStatus_Success) return false;
  if (wc_InitRng(&uc_random)) return false;

#if !FSL_FEATURE_SOC_LTC_COUNT
  PRINTF("- no LTC available\r\n");
#else
  LTC_Init(LTC0);
#endif

  initialized = true;
  return true;
}

// === BASE64 ===

char *uc_base64_encode(const unsigned char *in, size_t inlen) {
  // estimate the length of the encoded digest
  word32 base64_len;
  if (Base64_Encode_NoNl(in, inlen, NULL, &base64_len) != LENGTH_ONLY_E) return NULL;

  // allocate memory for the encoded string + 1
  char *encoded_digest = malloc(base64_len * sizeof(char) + 1);
  const int r = Base64_Encode_NoNl(in, inlen, (byte *) encoded_digest, &base64_len);

  if (r) {
    UCERROR("base64 encode", r);
    return NULL;
  }
  encoded_digest[base64_len] = '\0';

  return encoded_digest;
}

int uc_base64_decode(const char *in, size_t inlen, unsigned char *out, size_t *outlen) {
  // TODO: check updated version of wolfSSL, which expects outlen+1 bytes, but only decodes outlen
  (*outlen) += 1;

  const int r = Base64_Decode((const byte *) in, inlen, out, outlen);
  if (r) {
    UCERROR("base64 decode", r);
    return false;
  }

  return true;
}

// === SHA512 ===

int uc_sha512(const unsigned char *in, size_t inlen, unsigned char *hash) {
  Sha512 sha512;

  wc_InitSha512(&sha512);
  int r = wc_Sha512Update(&sha512, in, inlen);
  if (r) {
    UCERROR("sha512 update", r);
    return false;
  }
  r = wc_Sha512Final(&sha512, hash);
  if (r) {
    UCERROR("sha512 final", r);
    return false;
  }
  UCDUMP("SHA512", hash, SHA512_DIGEST_SIZE);

  return true;
}

char *uc_sha512_encoded(const unsigned char *in, size_t inlen) {
  unsigned char digest[SHA512_DIGEST_SIZE];
  if (!uc_sha512(in, inlen, digest)) return NULL;

  return uc_base64_encode(digest, SHA512_DIGEST_SIZE);
}

// === ED25519 ===

int uc_ecc_create_key(uc_ed25519_key *key) {
  if (!uc_init()) return false;

  wc_ed25519_init(key);
  if (wc_ed25519_make_key(&uc_random, ED25519_KEY_SIZE, key))
    return false;

//  UCDUMP("ECCPRV", key->k, ED25519_PRV_KEY_SIZE);
  UCDUMP("ECCPUB", key->p, ED25519_PUB_KEY_SIZE);

  return true;
}

int uc_import_ecc_key(uc_ed25519_key *key, const unsigned char *in, size_t inlen) {
  if (inlen != ED25519_PRV_KEY_SIZE) return false;

  wc_ed25519_init(key);
  const int status = wc_ed25519_import_private_key(in + 32, ED25519_KEY_SIZE, in, ED25519_PUB_KEY_SIZE, key);
  if (status < 0) {
    UCERROR("import ecc key", status);
    return false;
  }
//  UCDUMP("ECCPRV", key->k, ED25519_PRV_KEY_SIZE);
  UCDUMP("ECCPUB", key->p, ED25519_PUB_KEY_SIZE);

  return true;
}

int uc_import_ecc_pub_key(uc_ed25519_key *key, const unsigned char *in, size_t inlen) {
  if(inlen != ED25519_PUB_KEY_SIZE) return false;

  wc_ed25519_init(key);
  const int status = wc_ed25519_import_public(in, inlen, key);
  if(status < 0) {
    UCERROR("import ecc pub", status);
    return false;
  }
  UCDUMP("ECCPUB", key->p, ED25519_PUB_KEY_SIZE);

  return true;
}

int uc_import_ecc_pub_key_encoded(uc_ed25519_key *key, uc_ed25519_pub_pkcs8 *pkcs8) {
  return uc_import_ecc_pub_key(key, (const unsigned char *) &(pkcs8->key), sizeof(pkcs8->key));
}


int uc_ecc_export_pub(ed25519_key *key, uc_ed25519_pub_pkcs8 *pkcs8) {
  // copy the ASN.1 (PKCS#8) header into the encoded public key array
  memcpy(pkcs8->header, (const unsigned char[]) {
    0x30, 13 + ED25519_PUB_KEY_SIZE, 0x30, 0x08, 0x06, 0x03, 0x2b, 0x65, 0x64, 0x0a, 0x01, 0x01,
    0x03, 1 + ED25519_PUB_KEY_SIZE, 0x00
  }, 15);

  word32 len = ED25519_PUB_KEY_SIZE;
  const int status = wc_ed25519_export_public(key, pkcs8->key, &len);
  if (status < 0 || len != ED25519_PUB_KEY_SIZE) {
    UCERROR("ecc pub export", status);
    return false;
  }
  UCDUMP("ECCPUB", (const unsigned char *) pkcs8, sizeof(uc_ed25519_pub_pkcs8));

  return true;
}

char *uc_ecc_export_pub_encoded(ed25519_key *key) {
  uc_ed25519_pub_pkcs8 pkcs8;
  uc_ecc_export_pub(key, &pkcs8);

  return uc_base64_encode((const unsigned char *) &pkcs8, sizeof(pkcs8));
}


int uc_ecc_sign(uc_ed25519_key *key, const unsigned char *in, size_t inlen, unsigned char *signature) {
  word32 len = ED25519_SIG_SIZE;

  const int status = wc_ed25519_sign_msg(in, inlen, signature, &len, key);
  if (status < 0 || len != ED25519_SIG_SIZE) {
    UCERROR("ecc sign", status);
    return false;
  }
  UCDUMP("ECCSIG", signature, len);

  return true;
}

char *uc_ecc_sign_encoded(uc_ed25519_key *key, const unsigned char *in, size_t inlen) {
  unsigned char signature[ED25519_SIG_SIZE];
  if (!uc_ecc_sign(key, in, inlen, signature)) return NULL;

  return uc_base64_encode(signature, ED25519_SIG_SIZE);
}

int uc_ecc_verify(uc_ed25519_key *key, const unsigned char *in, size_t inlen, const unsigned char *signature, size_t siglen) {
  int verification = 0;
  int status = wc_ed25519_verify_msg((byte *) signature, siglen, in, inlen, &verification, key);
  if(status < 0) {
    UCERROR("ecc verify", status);
    return false;
  }

  return status == 0 && verification;
}





