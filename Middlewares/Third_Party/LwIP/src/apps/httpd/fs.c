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
extern volatile uint8_t g_is_authenticated; // флаг из main.c

/* Inline HTML overrides for pages we want to customize without regenerating fsdata.c */
static const char login_html_override[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html; charset=UTF-8\r\n\r\n"
  "<!DOCTYPE html>\n"
  "<html lang=\"ru\">\n"
  "<head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>Вход — Bonch-ATS Monitoring</title>\n"
  "<style>body{font-family:Segoe UI,Tahoma,sans-serif;background:#f5f5f5;color:#333;display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
  ".card{background:#fff;padding:30px;border-radius:12px;box-shadow:rgba(0,0,0,.1) -4px 9px 25px -6px;width:320px;box-sizing:border-box;text-align:center}"
  "h2{color:#ff9900;margin-bottom:20px}.input{width:100%;padding:10px;border-radius:6px;border:0;background:#f5f5f5;color:#333;margin-bottom:14px;font-size:15px;box-sizing:border-box}"
  ".btn{background:#ff9900;color:#fff;padding:10px;width:100%;border:0;border-radius:8px;cursor:pointer;font-weight:700;transition:background .2s}.btn:hover{background:#e68a00}"
  ".status{margin-top:10px;color:#999;min-height:20px;text-align:center}</style></head>\n"
  "<body><div class=\"card\"><h2>BONCH IT</h2>\n"
  "<form action=\"/login.cgi\" method=\"get\">\n"
  "<label for=\"pass\" style=\"display:block;margin:6px 0;text-align:left;font-weight:600\">Пароль</label>\n"
  "<input id=\"pass\" name=\"pass\" type=\"password\" class=\"input\" required>\n"
  "<button type=\"submit\" class=\"btn\">Войти</button>\n"
  "<div id=\"status\" class=\"status\"></div>\n"
  "</form><p style=\"margin-top:10px\">Если возникли вопросы, напишите нам</p>\n"
  "<a href=\"mailto:info@bonch-it.ru\" style=\"color:#e68a00\">info@bonch-it.ru</a>\n"
  "</div></body></html>\n";

static const char settings_html_override[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html; charset=UTF-8\r\n\r\n"
  "<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>Bonch-ATS Monitoring — Настройки</title><link rel=\"stylesheet\" href=\"style.css\"></head><body>\n"
  "<nav><a href=\"index.html\">Главная</a><a href=\"event.html\">Журнал</a><a href=\"settings.html\" class=\"active\">Настройки</a><a href=\"update.html\">Обновление ПО</a></nav>\n"
  "<main><div class=\"header\">Bonch-ATS Monitoring — Настройки</div>\n"
  "<div class=\"tabs\"><div class=\"tab active\" data-tab=\"network\">Сеть</div><div class=\"tab\" data-tab=\"datetime\">Дата и время</div><div class=\"tab\" data-tab=\"snmp\">SNMP</div><div class=\"tab\" data-tab=\"creds\">Пароль</div></div>\n"
  "<div class=\"card tab-content active\" id=\"network\">\n"
  "<form id=\"network-form\" method=\"get\" action=\"set_network.cgi\">\n"
  "<label for=\"ip\">IP-адрес</label><input id=\"ip\" name=\"ip\" class=\"input\" type=\"text\" placeholder=\"192.168.0.100\">\n"
  "<label for=\"mask\">Маска сети</label><input id=\"mask\" name=\"mask\" class=\"input\" type=\"text\" placeholder=\"255.255.255.0\">\n"
  "<label for=\"gateway\">Шлюз (Gateway)</label><input id=\"gateway\" name=\"gateway\" class=\"input\" type=\"text\" placeholder=\"192.168.0.1\">\n"
  "<label><input id=\"dhcp\" name=\"dhcp\" type=\"checkbox\" value=\"on\"> Использовать DHCP</label>\n"
  "<div style=\"margin-top:12px;\"><button type=\"submit\" class=\"btn\">Сохранить</button></div><div id=\"status-network\" class=\"status\"></div></form></div>\n"
  "<div class=\"card tab-content\" id=\"datetime\">\n"
  "<form id=\"datetime-form\" method=\"get\" action=\"\">\n"
  "<label for=\"date\">Дата</label><input id=\"date\" name=\"date\" class=\"input\" type=\"date\">\n"
  "<label for=\"time\">Время</label><input id=\"time\" name=\"time\" class=\"input\" type=\"time\">\n"
  "<div style=\"margin-top:12px;\"><button type=\"submit\" class=\"btn\">Сохранить дату и время</button></div><div id=\"status-datetime\" class=\"status\"></div></form></div>\n"
  "<div class=\"card tab-content\" id=\"snmp\">\n"
  "<form id=\"snmp-form\" method=\"get\" action=\"set_snmp.cgi\">\n"
  "<label for=\"snmp-read\">Пароль (community) на чтение</label><input id=\"snmp-read\" name=\"snmp-read\" class=\"input\" type=\"text\" placeholder=\"public\">\n"
  "<label for=\"snmp-write\">Пароль (community) на запись</label><input id=\"snmp-write\" name=\"snmp-write\" class=\"input\" type=\"text\" placeholder=\"private\">\n"
  "<label for=\"snmp-trap\">Пароль (community) на trap/inform</label><input id=\"snmp-trap\" name=\"snmp-trap\" class=\"input\" type=\"text\" placeholder=\"public\">\n"
  "<div style=\"margin-top:12px;\"><button type=\"submit\" class=\"btn\">Сохранить</button></div><div id=\"status-snmp\" class=\"status\"></div></form></div>\n"
  "<div class=\"card tab-content\" id=\"creds\">\n"
  "<form id=\"creds-form\" method=\"get\" action=\"set_creds.cgi\">\n"
  "<label for=\"login-pass\">Новый пароль (до 8 символов)</label><input id=\"login-pass\" name=\"pass\" class=\"input\" type=\"password\" maxlength=\"8\" placeholder=\"••••••••\">\n"
  "<div style=\"margin-top:12px;\"><button type=\"submit\" class=\"btn\">Сохранить</button></div><div id=\"status-creds\" class=\"status\"></div></form></div>\n"
  "</main>\n"
  "<script>(function(){const tabs=document.querySelectorAll('.tab');const contents=document.querySelectorAll('.tab-content');tabs.forEach(t=>t.addEventListener('click',()=>{tabs.forEach(x=>x.classList.remove('active'));contents.forEach(c=>c.classList.remove('active'));t.classList.add('active');document.getElementById(t.dataset.tab).classList.add('active');}));"
  "const forms=[{id:'network-form',status:'status-network'},{id:'snmp-form',status:'status-snmp'},{id:'creds-form',status:'status-creds'}];forms.forEach(f=>{const form=document.getElementById(f.id);const st=document.getElementById(f.status);form.addEventListener('submit',ev=>{ev.preventDefault();const data=new URLSearchParams(new FormData(form)).toString();location.href=form.action+'?'+data;});});"
  "const datetimeForm=document.getElementById('datetime-form');const statusDatetime=document.getElementById('status-datetime');datetimeForm.addEventListener('submit',ev=>{ev.preventDefault();const fd=new FormData(datetimeForm);const d=fd.get('date');const t=fd.get('time');statusDatetime.textContent='Сохранение...';if(d){new Image().src='set_date.cgi?date='+d;}if(t){new Image().src='set_time.cgi?time='+t;}statusDatetime.textContent='Дата и время сохранены!';setTimeout(()=>location.reload(),1000);});"
  "async function loadSettings(){try{const resp=await fetch('/table.shtml',{cache:'no-store'});if(!resp.ok)throw new Error('HTTP '+resp.status);const html=await resp.text();const ip=html.match(/<!--#netip-->(.*?)<\/td>/s);const mask=html.match(/<!--#netmask-->(.*?)<\/td>/s);const gw=html.match(/<!--#netgw-->(.*?)<\/td>/s);const dhcp=html.match(/<!--#netdhcp-->(.*?)<\/td>/s);if(ip)document.getElementById('ip').placeholder=ip[1].trim();if(mask)document.getElementById('mask').placeholder=mask[1].trim();if(gw)document.getElementById('gateway').placeholder=gw[1].trim();if(dhcp)document.getElementById('dhcp').checked=(dhcp[1].trim()==='on');const d=html.match(/<!--#date-->(.*?)<\/td>/s);const tt=html.match(/<!--#time-->(.*?)<\/td>/s);if(d)document.getElementById('date').value=d[1].trim();if(tt)document.getElementById('time').value=tt[1].trim();}catch(e){console.error(e);}}loadSettings();})();</script>\n"
  "</body></html>\n";

int fs_open_custom(struct fs_file *file, const char *name)
{
  if (file == NULL || name == NULL) return 0;

  /* Отдаём переопределённые версии страниц */
  if (!strcmp(name, "/login.html")) {
    file->data = login_html_override;
    file->len = strlen(login_html_override);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
  }
  if (!strcmp(name, "/settings.html")) {
    file->data = settings_html_override;
    file->len = strlen(settings_html_override);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
  }
  /* Разрешаем страницу ошибки всегда */
  if (!strcmp(name, "/login_failed.html")) {
    return 0; /* обычная отдача */
  }

  /* /login.cgi обрабатывается через CGI handler в main.c */
  if (!strncmp(name, "/login.cgi", 10)) {
    return 0; /* для GET-логина параметры парсит CGI */
  }

  /* Logout */
  if (!strncmp(name, "/logout.cgi", 11)) {
    g_is_authenticated = 0;
    file->data = (const char*)"HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
    file->len = strlen(file->data);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    return 1;
  }

  /* Для остальных страниц — пропускаем, если авторизован; иначе редирект на login */
  if (!strcmp(name, "/") || !strcmp(name, "/index.html") || !strcmp(name, "/settings.html") ||
      !strcmp(name, "/event.html") || !strcmp(name, "/update.html") || strstr(name, ".shtml") != NULL) {
    if (g_is_authenticated) {
      return 0; /* отдать страницу обычно */
    } else {
      file->data = (const char*)"HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
      file->len = strlen(file->data);
      file->index = file->len;
      file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
      return 1;
    }
  }

  /* Разрешаем прямой доступ к CGI-скриптам (в т.ч. /login.cgi) без авторизации
     иначе браузер может уйти в цикл редиректов при GET-логине */
  if (strstr(name, ".cgi") != NULL) {
    return 0;
  }
  return 0;
}

void fs_close_custom(struct fs_file *file)
{
  LWIP_UNUSED_ARG(file);
}
#endif /* LWIP_HTTPD_CUSTOM_FILES */
