/*!
 * @file
 * @brief crypto driver.
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

#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/ed25519.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/sha512.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SHA512_HASH_SIZE      SHA512_DIGEST_SIZE  //!< SHA512 hash size in bytes
#define ED25519_KEYPAIR_SIZE  64                  //!< Size of the ED25519 keypair
#define ED25519_PUB_KEY_SIZE  32                  //!< Size of the ED25519 public key

typedef ed25519_key uc_ed25519_key;               //!< ED25519 key
typedef RsaKey uc_rsa_key;                        //!< RSA key

//! PKCS#8 encoded public ED25519 key
typedef struct {
    uint8_t header[15];                   //!< ASN.1 header
    uint8_t key[ED25519_PUB_KEY_SIZE];    //!< the actual ED25519 key
}  __attribute__((__packed__)) uc_ed25519_pub_pkcs8;

//! Random Number Generator instance
extern WC_RNG uc_random;

/*!
 * @brief Initialize the crypto unit, hardware and random number generator.
 * @return true if already initialized and initialization was successful
 */
int uc_init();

/*!
 * @brief Encode a byte array in Base64 encoding.
 * @param in the byte array input
 * @param inlen the length of the input array
 * @return the base64 encoded string (malloc(), 0 terminated)
 */
char *uc_base64_encode(const unsigned char *in, size_t inlen);

/*!
 * @brief Decode a Base64 encoded character string into a byte array.
 * @param in the encoded character string
 * @param inlen the length of the input string
 * @param out the decoded byte array (must be preallocated)
 * @param outlen the length of the decoded byte array
 * @return true if the operation was successful, false if not
 */
int uc_base64_decode(const char *in, size_t inlen, unsigned char *out, size_t *outlen);

/*!
 * @brief Create an SHA512 hash from the given message.
 * @param in the message to hash
 * @param inlen the size of the message
 * @param hash the hash to store the SHA512 digest in (must be preallocated to 64 bytes)
 * @return cStatus_Success for succes or cStatus_Failure if it fails
 */
int uc_sha512(const unsigned char *in, size_t inlen, unsigned char *hash);

/*!
 * @brief Create a Base64 encoded SHA512 hash from the given message.
 * @param in the input data to hash
 * @param inlen the length of the input data
 * @return the Base64 encoded string (malloc(), 0 terminated)
 */
char *uc_sha512_encoded(const unsigned char *in, size_t inlen);

/*!
 * @brief Create new ECC key.
 * @param key pointer to the structure to store the key in
 */
int uc_ecc_create_key(uc_ed25519_key *key);

/*!
 * @brief Import ECC key from array ([32b priv, 32b pub], like raw bytes of key).
 * @param key where to store the key
 * @param in the data array containing the complete private key
 * @param inlen length of the input array
 */
int uc_import_ecc_key(uc_ed25519_key *key, const unsigned char *in, size_t inlen);

/*!
 * @brief Import ECC public key from array.
 * @param key the key where to store the public part in
 * @param in the data array containing the public key
 * @param inlen length of the input array
 */
int uc_import_ecc_pub_key(uc_ed25519_key *key, const unsigned char *in, size_t inlen);

/*!
 * @brief Import ECC public key from encoded PKCS#8 structure.
 * @param key the key where to store the imported public key
 * @param pkcs8 the PKCS#8 encoded public key
 */
int uc_import_ecc_pub_key_encoded(uc_ed25519_key *key, uc_ed25519_pub_pkcs8 *pkcs8);

/*!
 * @brief Export ECC public key.
 * @param key the key to export the public key from
 * @param pkcs8 the PKCS#8 structure where the public key will be stored
 * @return true if the export is successful
 */
int uc_ecc_export_pub(ed25519_key *key, uc_ed25519_pub_pkcs8 *pkcs8);

/*!
 * @brief Export ECC public key Base64 encoded.
 * @param key the public key to export
 * @return a Base64 encoded string
 */
char *uc_ecc_export_pub_encoded(ed25519_key *key);

/*!
 * @brief Sign message using specified ECC key.
 * @param key the key to sign with
 * @param in the input data to sign
 * @param inlen the size of the input
 * @param signature the byte array to store the signature in, must be preallocated
 * @return true if signature has been created, false if not
 */
int uc_ecc_sign(uc_ed25519_key *key, const unsigned char *in, size_t inlen, unsigned char *signature);

/*!
 * @brief Sign message using specified ECC key and create Base64 encoded result.
 * @param key the key to sign with
 * @param in the byte array to sign
 * @param inlen the size of the input
 * @return the Base64 encoded signature
 */
char *uc_ecc_sign_encoded(uc_ed25519_key *key, const unsigned char *in, size_t inlen);

/*!
 * @brief Verify signature of a message using the public key and the signature.
 * @param key the public key to verify against
 * @param in the byte array to verify
 * @param inlen the length of the byte array to verify
 * @param signature the signature to verify
 * @param siglen the length of the signature
 * @return true if message and signature match
 */
int uc_ecc_verify(uc_ed25519_key *key, const unsigned char *in, size_t inlen, const unsigned char *signature, size_t siglen);



#ifdef __cplusplus
}
#endif
