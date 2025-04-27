//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "tests/common/client/http.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/lib/buf.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/macro/check.h"
#include "tests/common/client/curl_connect.h"
#include "tests/common/constant.h"

static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb,
                                  void *data) {
  TEN_ASSERT(data, "Invalid argument.");

  size_t real_size = size * nmemb;

  ten_buf_t *buf = data;
  ten_buf_reserve(buf, real_size + 1);
  ten_buf_push(buf, ptr, real_size);

  ((char *)(buf->data))[buf->content_size] = 0;

  return real_size;
}

void ten_test_http_client_init() {
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  TEN_ASSERT(res == CURLE_OK, "Failed to init curl.");
}

void ten_test_http_client_deinit() { curl_global_cleanup(); }

void ten_test_http_client_get(const char *url, ten_string_t *result) {
  TEN_ASSERT(url && result, "Invalid argument.");

  CURL *pCurl = NULL;
  pCurl = curl_easy_init();
  TEN_ASSERT(pCurl != NULL, "Failed to init CURL.");

  // There maybe a timeout issue (ex: operation timed out after 3000
  // milliseconds with 0 bytes received). When the curl client does not receive
  // the response until timeout, it will try to send the request again. But ten
  // runtime can not handle the same request in some cases. Ex: sends a
  // 'connect_to' cmd with the same uri twice. We increase the timeout for now.
  //
  // TODO(Liu): the curl connection (#0) will be closed before retry, then the
  // libws client is being closed, and try to close the TEN protocol and remote.
  // Then a new connection (#1) is established, and sends the same request.
  // There will be a timing issue, the TEN engine may receive the second request
  // before the connection closing event.
  curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(pCurl, CURLOPT_HEADER, 0L);
  curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  // Do _NOT_ use http proxy in the environment variables.
  curl_easy_setopt(pCurl, CURLOPT_PROXY, "");

  ten_buf_t buf;
  ten_buf_init_with_owned_data(&buf, 0);
  curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &buf);

  curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(pCurl, CURLOPT_URL, url);

  struct curl_slist *pList = NULL;
  curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pList);

  bool is_connected = ten_test_curl_connect_with_retry(
      pCurl, CURL_CONNECT_MAX_RETRY_TIMES, CURL_CONNECT_DELAY_IN_MS);
  if (!is_connected) {
    ten_string_set_formatted(result, "Connection refused to server(%s).", url);
    TEN_ASSERT(0, "Should not happen.");
    goto done;
  }

  long res_code = 0;
  CURLcode res = curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &res_code);

  if (res == CURLE_OK) {
    ten_string_set_formatted(result, "%s", buf.data);
  }

done:
  ten_buf_deinit(&buf);

  curl_slist_free_all(pList);
  curl_easy_cleanup(pCurl);
}

void ten_test_http_client_post(const char *url, const char *body,
                               ten_string_t *result) {
  TEN_ASSERT(url && body, "Invalid argument.");

  CURL *pCurl = NULL;
  pCurl = curl_easy_init();
  TEN_ASSERT(pCurl != NULL, "Failed to init CURL.");

  curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(pCurl, CURLOPT_HEADER, 0L);
  curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  // Do _NOT_ use http proxy in the environment variables.
  curl_easy_setopt(pCurl, CURLOPT_PROXY, "");

  ten_buf_t buf;
  ten_buf_init_with_owned_data(&buf, 0);
  curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &buf);

  curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(pCurl, CURLOPT_URL, url);

  curl_easy_setopt(pCurl, CURLOPT_POST, 1L);
  curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, strlen(body));

  struct curl_slist *pList = NULL;
  pList = curl_slist_append(pList, "Content-Type:application/json");
  curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pList);

  bool is_connected = ten_test_curl_connect_with_retry(
      pCurl, CURL_CONNECT_MAX_RETRY_TIMES, CURL_CONNECT_DELAY_IN_MS);
  if (!is_connected) {
    if (result) {
      ten_string_set_formatted(result, "Connection refused to server(%s).",
                               url);
      TEN_ASSERT(0, "Should not happen.");
    }

    goto done;
  }

  long res_code = 0;
  CURLcode res = curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &res_code);

  if (res == CURLE_OK && result) {
    ten_string_set_formatted(result, "%s", buf.data);
  }

done:
  ten_buf_deinit(&buf);

  curl_slist_free_all(pList);
  curl_easy_cleanup(pCurl);
}
