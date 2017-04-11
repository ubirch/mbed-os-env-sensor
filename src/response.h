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

#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

//! @brief Process response and return key and signature
char *process_response(char *response, uc_ed25519_pub_pkcs8 *key, unsigned char *signature);

//! @brief JSMN helper function to print the current token for debugging
void print_token(const char *prefix, const char *response, jsmntok_t *token);

//! @brief JSMN helper function to compare token values
int jsoneq(const char *json, jsmntok_t *token, const char *key);

#ifdef __cplusplus
}
#endif

#endif // _RESPONSE_H_
