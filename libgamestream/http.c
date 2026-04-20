/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "http.h"
#include "errors.h"

#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>

static const char *pCertFile = "./client.pem";
static const char *pKeyFile = "./key.pem";

static bool debug;
static struct curl_blob certBlob, keyBlob;

void http_free_certblobs(void) {
    if (certBlob.data) {
        free((void*)certBlob.data);
        certBlob.data = NULL;
        certBlob.len = 0;
        certBlob.flags = 0;
    }
    if (keyBlob.data) {
        free((void*)keyBlob.data);
        keyBlob.data = NULL;
        keyBlob.len = 0;
        keyBlob.flags = 0;
    }
}


static size_t _write_curl(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  PHTTP_DATA mem = (PHTTP_DATA)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL)
    return 0;

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static size_t _write_curl_binary(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t written = fwrite(contents, size, nmemb, userp);
    return written;
}

CURL* get_curl_handle() {
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    if (certBlob.data != NULL) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT_BLOB, &certBlob);
    }
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
    if (keyBlob.data != NULL) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY_BLOB, &keyBlob);
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);
    return curl;
}

int http_init(const char* keyDirectory, int logLevel) {
  debug = logLevel >= 2;

  // If we already have blobs allocated, free them first so repeated init is safe
  http_free_certblobs();

  char certificateFilePath[4096];
  snprintf(certificateFilePath, sizeof(certificateFilePath), "%s%s", keyDirectory, CERTIFICATE_FILE_NAME);

  char keyFilePath[4096];
  snprintf(keyFilePath, sizeof(keyFilePath), "%s%s", keyDirectory, KEY_FILE_NAME);

  // Read certificate file
  FILE* fp = fopen(certificateFilePath, "rb");
  if (fp == NULL) return 1;
  if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 1; }
  long size = ftell(fp);
  if (size <= 0) { fclose(fp); return 1; }
  rewind(fp);
  void* certificateBuffer = malloc((size_t)size);
  if (certificateBuffer == NULL) { fclose(fp); return 1; }
  size_t read = fread(certificateBuffer, 1, (size_t)size, fp);
  fclose(fp);
  if (read != (size_t)size) { free(certificateBuffer); return 1; }
  certBlob.data = certificateBuffer;
  certBlob.len = read;
  certBlob.flags = CURL_BLOB_COPY;

  // Read key file
  fp = fopen(keyFilePath, "rb");
  if (fp == NULL) {
    http_free_certblobs();
    return 1;
  }
  if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); http_free_certblobs(); return 1; }
  size = ftell(fp);
  if (size <= 0) { fclose(fp); http_free_certblobs(); return 1; }
  rewind(fp);
  void* keyBuffer = malloc((size_t)size);
  if (keyBuffer == NULL) { fclose(fp); http_free_certblobs(); return 1; }
  read = fread(keyBuffer, 1, (size_t)size, fp);
  fclose(fp);
  if (read != (size_t)size) { free(keyBuffer); http_free_certblobs(); return 1; }
  keyBlob.data = keyBuffer;
  keyBlob.len = read;
  keyBlob.flags = CURL_BLOB_COPY;

  return GS_OK;
}

int http_request(CURL* curl, char* url, PHTTP_DATA data) {
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_curl);

  if (debug)
    printf("Request %s\n", url);

  if (data->size > 0) {
    free(data->memory);
    data->memory = malloc(1);
    if(data->memory == NULL)
      return GS_OUT_OF_MEMORY;

    data->size = 0;
  }
  CURLcode res = curl_easy_perform(curl);

  if(res != CURLE_OK) {
    gs_error = curl_easy_strerror(res);
    return GS_FAILED;
  } else if (data->memory == NULL) {
    return GS_OUT_OF_MEMORY;
  }

  if (debug)
    printf("Response:\n%s\n\n", data->memory);

  return GS_OK;
}

int http_request_binary(CURL *curl, char* url, FILE *data) {
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_curl_binary);

    if (debug)
        printf("Request %s\n", url);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        gs_error = curl_easy_strerror(res);
        return GS_FAILED;
    }
    return GS_OK;
}

void http_cleanup(CURL *curl) {
  curl_easy_cleanup(curl);
}

PHTTP_DATA http_create_data() {
  PHTTP_DATA data = malloc(sizeof(HTTP_DATA));
  if (data == NULL)
    return NULL;

  data->memory = malloc(1);
  if(data->memory == NULL) {
    free(data);
    return NULL;
  }
  data->size = 0;

  return data;
}

void http_free_data(PHTTP_DATA data) {
  if (data != NULL) {
    if (data->memory != NULL)
      free(data->memory);

    free(data);
  }
}
