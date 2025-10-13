/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/apps/httpd_opts.h"
#include "lwip/def.h"
#include "lwip/apps/fs.h"
#include "fsdata.h"
#include <string.h>
#include <stdbool.h>


#if HTTPD_USE_CUSTOM_FSDATA
#include "fsdata_custom.c"
#else /* HTTPD_USE_CUSTOM_FSDATA */
#include "fsdata.c"
#endif /* HTTPD_USE_CUSTOM_FSDATA */

/*-----------------------------------------------------------------------------------*/

#if LWIP_HTTPD_CUSTOM_FILES
int fs_open_custom(struct fs_file *file, const char *name);
void fs_close_custom(struct fs_file *file);
#if LWIP_HTTPD_FS_ASYNC_READ
u8_t fs_canread_custom(struct fs_file *file);
u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg);
int fs_read_async_custom(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg);
#else /* LWIP_HTTPD_FS_ASYNC_READ */
int fs_read_custom(struct fs_file *file, char *buffer, int count);
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
#endif /* LWIP_HTTPD_CUSTOM_FILES */

/*-----------------------------------------------------------------------------------*/
err_t
fs_open(struct fs_file *file, const char *name)
{
  const struct fsdata_file *f;

  if ((file == NULL) || (name == NULL)) {
     return ERR_ARG;
  }

#if LWIP_HTTPD_CUSTOM_FILES
  if (fs_open_custom(file, name)) {
    file->is_custom_file = 1;
    return ERR_OK;
  }
  file->is_custom_file = 0;
#endif /* LWIP_HTTPD_CUSTOM_FILES */

  for (f = FS_ROOT; f != NULL; f = f->next) {
    if (!strcmp(name, (const char *)f->name)) {
      file->data = (const char *)f->data;
      file->len = f->len;
      file->index = f->len;
      file->pextension = NULL;
      file->flags = f->flags;
#if HTTPD_PRECALCULATED_CHECKSUM
      file->chksum_count = f->chksum_count;
      file->chksum = f->chksum;
#endif /* HTTPD_PRECALCULATED_CHECKSUM */
#if LWIP_HTTPD_FILE_STATE
      file->state = fs_state_init(file, name);
#endif /* #if LWIP_HTTPD_FILE_STATE */
      return ERR_OK;
    }
  }
  /* file not found */
  return ERR_VAL;
}

/*-----------------------------------------------------------------------------------*/
void
fs_close(struct fs_file *file)
{
#if LWIP_HTTPD_CUSTOM_FILES
  if (file->is_custom_file) {
    fs_close_custom(file);
  }
#endif /* LWIP_HTTPD_CUSTOM_FILES */
#if LWIP_HTTPD_FILE_STATE
  fs_state_free(file, file->state);
#endif /* #if LWIP_HTTPD_FILE_STATE */
  LWIP_UNUSED_ARG(file);
}
/*-----------------------------------------------------------------------------------*/
#if LWIP_HTTPD_DYNAMIC_FILE_READ
#if LWIP_HTTPD_FS_ASYNC_READ
int
fs_read_async(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg)
#else /* LWIP_HTTPD_FS_ASYNC_READ */
int
fs_read(struct fs_file *file, char *buffer, int count)
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
{
  int read;
  if(file->index == file->len) {
    return FS_READ_EOF;
  }
#if LWIP_HTTPD_FS_ASYNC_READ
  LWIP_UNUSED_ARG(callback_fn);
  LWIP_UNUSED_ARG(callback_arg);
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
#if LWIP_HTTPD_CUSTOM_FILES
  if (file->is_custom_file) {
#if LWIP_HTTPD_FS_ASYNC_READ
    return fs_read_async_custom(file, buffer, count, callback_fn, callback_arg);
#else /* LWIP_HTTPD_FS_ASYNC_READ */
    return fs_read_custom(file, buffer, count);
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
  }
#endif /* LWIP_HTTPD_CUSTOM_FILES */

  read = file->len - file->index;
  if(read > count) {
    read = count;
  }

  MEMCPY(buffer, (file->data + file->index), read);
  file->index += read;

  return(read);
}
#endif /* LWIP_HTTPD_DYNAMIC_FILE_READ */
/*-----------------------------------------------------------------------------------*/
#if LWIP_HTTPD_FS_ASYNC_READ
int
fs_is_file_ready(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg)
{
  if (file != NULL) {
#if LWIP_HTTPD_FS_ASYNC_READ
#if LWIP_HTTPD_CUSTOM_FILES
    if (!fs_canread_custom(file)) {
      if (fs_wait_read_custom(file, callback_fn, callback_arg)) {
        return 0;
      }
    }
#else /* LWIP_HTTPD_CUSTOM_FILES */
    LWIP_UNUSED_ARG(callback_fn);
    LWIP_UNUSED_ARG(callback_arg);
#endif /* LWIP_HTTPD_CUSTOM_FILES */
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
  }
  return 1;
}
#endif /* LWIP_HTTPD_FS_ASYNC_READ */
/*-----------------------------------------------------------------------------------*/
int
fs_bytes_left(struct fs_file *file)
{
  return file->len - file->index;
}

#if LWIP_HTTPD_CUSTOM_FILES
/* ---- Авторизация: если логин/пароль верный, перенаправляем на index.html ---- */
#include "credentials.h"
#include <string.h>
extern volatile uint8_t g_is_authenticated; // флаг из main.c (устаревший общий флаг)
extern volatile uint32_t g_auth_deadline_ms; // срок действия авторизации (общий)
extern uint32_t HAL_GetTick(void);
extern bool Auth_IsCurrentRequestAuthorized(uint32_t now_ms);
extern void Auth_RevokeCurrentSession(void);
extern bool Auth_TakePendingSetCookie(char* out_sid, uint16_t out_len);
extern uint32_t g_auth_ttl_ms;

static char g_setcookie_hdr[256];

int fs_open_custom(struct fs_file *file, const char *name)
{
  if (file == NULL || name == NULL) return 0;

  /* If login just created a session, send Set-Cookie once and redirect to index */
  char sid[33];
  if (Auth_TakePendingSetCookie(sid, sizeof(sid))) {
    unsigned long max_age = (unsigned long)(g_auth_ttl_ms / 1000U);
    int n = snprintf(g_setcookie_hdr, sizeof(g_setcookie_hdr),
                     "HTTP/1.1 302 Found\r\nSet-Cookie: SID=%s; Path=/; HttpOnly; Max-Age=%lu\r\nLocation: /index.html\r\n\r\n",
                     sid, max_age);
    (void)n;
    file->data = g_setcookie_hdr;
    file->len = strlen(file->data);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
  }

  /* Разрешаем страницы логина и ошибки всегда */
  if (!strcmp(name, "/login.html") || !strcmp(name, "/login_failed.html")) {
    return 0; /* обычная отдача */
  }

  /* /login.cgi обрабатывается через CGI handler в main.c */
  if (!strncmp(name, "/login.cgi", 10)) {
    return 0; /* для GET-логина параметры парсит CGI */
  }

  /* Logout */
  if (!strncmp(name, "/logout.cgi", 11)) {
    g_is_authenticated = 0;
    Auth_RevokeCurrentSession();
    file->data = (const char*)"HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
    file->len = strlen(file->data);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
  }

  /* Для остальных HTML-страниц — пропускаем, если авторизован (и не истёк TTL); иначе редирект на login */
  const char *path = (name && name[0]) ? name : "/";
  int is_root = !strcmp(path, "/");
  int is_index_variation = (!strcmp(path, "/index.html") || !strcmp(path, "index.html"));
  int is_html = (strstr(path, ".html") != NULL) || (strstr(path, ".shtml") != NULL);
  if (is_root || is_index_variation || is_html) {
    uint32_t now = HAL_GetTick();
    if (Auth_IsCurrentRequestAuthorized(now)) {
      return 0; /* отдать страницу обычно */
    } else {
      file->data = (const char*)"HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
      file->len = strlen(file->data);
      file->index = file->len;
      file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
      return 1;
    }
  }
  return 0;
}

void fs_close_custom(struct fs_file *file)
{
  LWIP_UNUSED_ARG(file);
}
#endif /* LWIP_HTTPD_CUSTOM_FILES */
