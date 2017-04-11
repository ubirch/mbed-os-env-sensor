/**
 * Response handling.
 *
 * @author Matthias L. Jugel
 * @date 2016-06-14
 *
 * Copyright 2016 ubirch GmbH (https://ubirch.com)
 *
 * == LICENSE ==
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
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../crypto/crypto.h"
#include "../jsmn/jsmn.h"
#include "sensor.h"

#define PRINTF printf
#define PUTCHAR putchar

extern int error_flag;

//! @brief JSMN helper function to print the current token for debugging
void print_token(const char *prefix, const char *response, jsmntok_t *token) {
  const size_t token_size = (const size_t) (token->end - token->start);
  PRINTF("%s ", prefix);
  for (int i = 0; i < token_size; i++) putchar(*(response + token->start + i));
  PRINTF("\r\n");
}

//! @brief JSMN helper function to compare token values
int jsoneq(const char *json, jsmntok_t *token, const char *key) {
  if (token->type == JSMN_STRING &&
      strlen(key) == (size_t)(token->end - token->start) &&
      strncmp(json + token->start, key, (size_t)(token->end - token->start)) == 0) {
    return 0;
  }
  return -1;
}

/*!
 * Process the JSON response from the backend. It should contain configuration
 * parameters that need to be set. The response must be signed and will be
 * checked for signature match and protocol version.
 *
 * @param response the request response, will be freed here
 * @param key where the key from the response will be stored
 * @param signature the extracted payload signature
 * @return the payload string
 */
char *process_response(char *response, uc_ed25519_pub_pkcs8 *key, unsigned char *signature) {
  char *payload = NULL;

  jsmntok_t *token;
  jsmn_parser parser;
  jsmn_init(&parser);

  // identify the number of tokens in our response, we expect 13
  const int token_count = jsmn_parse(&parser, response, strlen(response), NULL, 0);
  // TODO check token count and return if too many
  token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

  // reset parser, parse and store tokens
  jsmn_init(&parser);
  const int parsed_token_count = jsmn_parse(&parser, response, strlen(response), token, (unsigned int) token_count);
  if (parsed_token_count == token_count && token[0].type == JSMN_OBJECT) {
    int index = 0;
    while (++index < token_count) {
      if (jsoneq(response, &token[index], P_VERSION) == 0 && token[index + 1].type == JSMN_STRING) {
        index++;
        if (strncmp(response + token[index].start, PROTOCOL_VERSION_MIN, 3) != 0) {
          print_token("protocol version mismatch:", response, &token[index]);

          // do not continue if the version does not match, free already copied payload or sig
          error_flag |= E_PROTOCOL_FAIL;
          break;
        }
      } else if (jsoneq(response, &token[index], P_KEY) == 0 && token[index + 1].type == JSMN_STRING) {
        index++;
        print_token("key:", response, &token[index]);


        // extract signature and decode it
        size_t key_length = sizeof(uc_ed25519_pub_pkcs8);
        memset(key, 0, key_length);
        if (!uc_base64_decode(response + token[index].start, (size_t)(token[index].end - token[index].start),
                              (unsigned char *) key, &key_length)) {
          PRINTF("ERROR decoding key.\r\n");
        }
      } else if (jsoneq(response, &token[index], P_SIGNATURE) == 0 && token[index + 1].type == JSMN_STRING) {
        index++;
        print_token("signature:", response, &token[index]);

        // extract signature and decode it
        memset(signature, 0, SHA512_HASH_SIZE);
        size_t hash_length = SHA512_HASH_SIZE;
        if (!uc_base64_decode(response + token[index].start,
                              (size_t)(token[index].end - token[index].start),
                              signature, &hash_length)) {
          PRINTF("ERROR decoding hash digest.\r\n");
        }
      } else if (jsoneq(response, &token[index], P_PAYLOAD) == 0 && token[index + 1].type == JSMN_OBJECT) {
        index++;
        print_token("payload:", response, &token[index]);

        // extract payload from json
        size_t payload_length = (uint8_t)(token[index].end - token[index].start);
        payload = strndup(response + token[index].start, payload_length);
        payload[payload_length] = '\0';

        index += 2 * token[index].size;
      } else {
        // simply ignore unknown keys
        print_token("unknown key:", response, &token[index]);
        index++;
      }
    }
  } else {
    error_flag |= E_JSON_FAILED;
  }

  // free used heap (token)
  free(token);

  return payload;
}
