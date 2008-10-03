#define NGX_CONFIGURE "--add-module=../nginx_upstream_hash-0.2 --add-module=../ngx_http_empty_module --add-module=../ngx_http_repl_module --add-module=../ngx_http_rv_module --add-module=../ngx_http_vwrite_module --add-module=../ngx_http_rand_module --add-module=../ngx_http_time_module --add-module=../ngx_http_list_module --add-module=../ngx_http_memcached_more_module --with-cc-opt=-g3 --with-ld-opt=-g3 --with-debug --prefix=/data1/ngmp.nginx --without-http_access_module --without-http_auth_basic_module --without-http_autoindex_module --without-http_browser_module --without-http_charset_module --without-http_empty_gif_module --without-http_fastcgi_module --without-http_geo_module --without-http_gzip_module --without-http_limit_zone_module --without-http_map_module --without-http_referer_module --without-http_upstream_ip_hash_module --without-http_userid_module --without-http_memcached_module --without-mail_imap_module --without-mail_pop3_module --without-mail_smtp_module --without-poll_module --without-select_module --sbin-path=sbin/ngmp.nginx"

#ifndef NGX_DEBUG
#define NGX_DEBUG  1
#endif


#ifndef NGX_COMPILER
#define NGX_COMPILER  "gcc 4.2.4 (Ubuntu 4.2.4-1ubuntu1)"
#endif


#ifndef NGX_HAVE_GCC_VARIADIC_MACROS
#define NGX_HAVE_GCC_VARIADIC_MACROS  1
#endif


#ifndef NGX_HAVE_C99_VARIADIC_MACROS
#define NGX_HAVE_C99_VARIADIC_MACROS  1
#endif


#ifndef NGX_HAVE_EPOLL
#define NGX_HAVE_EPOLL  1
#endif


#ifndef NGX_HAVE_CLEAR_EVENT
#define NGX_HAVE_CLEAR_EVENT  1
#endif


#ifndef NGX_HAVE_SENDFILE
#define NGX_HAVE_SENDFILE  1
#endif


#ifndef NGX_HAVE_SENDFILE64
#define NGX_HAVE_SENDFILE64  1
#endif


#ifndef NGX_HAVE_PR_SET_DUMPABLE
#define NGX_HAVE_PR_SET_DUMPABLE  1
#endif


#ifndef NGX_HAVE_SCHED_SETAFFINITY
#define NGX_HAVE_SCHED_SETAFFINITY  1
#endif


#ifndef NGX_HAVE_NONALIGNED
#define NGX_HAVE_NONALIGNED  1
#endif


#ifndef NGX_CPU_CACHE_LINE
#define NGX_CPU_CACHE_LINE  32
#endif


#define NGX_KQUEUE_UDATA_T  (void *)


#ifndef NGX_HTTP_SSI
#define NGX_HTTP_SSI  1
#endif


#ifndef NGX_HTTP_REWRITE
#define NGX_HTTP_REWRITE  1
#endif


#ifndef NGX_HTTP_PROXY
#define NGX_HTTP_PROXY  1
#endif


#ifndef NGX_PCRE
#define NGX_PCRE  1
#endif


#ifndef NGX_HAVE_UNIX_DOMAIN
#define NGX_HAVE_UNIX_DOMAIN  1
#endif


#ifndef NGX_PTR_SIZE
#define NGX_PTR_SIZE  4
#endif


#ifndef NGX_SIG_ATOMIC_T_SIZE
#define NGX_SIG_ATOMIC_T_SIZE  4
#endif


#ifndef NGX_HAVE_LITTLE_ENDIAN
#define NGX_HAVE_LITTLE_ENDIAN  1
#endif


#ifndef NGX_MAX_SIZE_T_VALUE
#define NGX_MAX_SIZE_T_VALUE  2147483647L
#endif


#ifndef NGX_SIZE_T_LEN
#define NGX_SIZE_T_LEN  (sizeof("-2147483648") - 1)
#endif


#ifndef NGX_MAX_OFF_T_VALUE
#define NGX_MAX_OFF_T_VALUE  9223372036854775807LL
#endif


#ifndef NGX_OFF_T_LEN
#define NGX_OFF_T_LEN  (sizeof("-9223372036854775808") - 1)
#endif


#ifndef NGX_TIME_T_SIZE
#define NGX_TIME_T_SIZE  4
#endif


#ifndef NGX_TIME_T_LEN
#define NGX_TIME_T_LEN  (sizeof("-2147483648") - 1)
#endif


#ifndef NGX_HAVE_PREAD
#define NGX_HAVE_PREAD  1
#endif


#ifndef NGX_HAVE_PWRITE
#define NGX_HAVE_PWRITE  1
#endif


#ifndef NGX_HAVE_GNU_STRERROR_R
#define NGX_HAVE_GNU_STRERROR_R  1
#endif


#ifndef NGX_HAVE_LOCALTIME_R
#define NGX_HAVE_LOCALTIME_R  1
#endif


#ifndef NGX_HAVE_POSIX_MEMALIGN
#define NGX_HAVE_POSIX_MEMALIGN  1
#endif


#ifndef NGX_HAVE_MEMALIGN
#define NGX_HAVE_MEMALIGN  1
#endif


#ifndef NGX_HAVE_SCHED_YIELD
#define NGX_HAVE_SCHED_YIELD  1
#endif


#ifndef NGX_HAVE_MAP_ANON
#define NGX_HAVE_MAP_ANON  1
#endif


#ifndef NGX_HAVE_MAP_DEVZERO
#define NGX_HAVE_MAP_DEVZERO  1
#endif


#ifndef NGX_HAVE_SYSVSHM
#define NGX_HAVE_SYSVSHM  1
#endif


#ifndef NGX_HAVE_MSGHDR_MSG_CONTROL
#define NGX_HAVE_MSGHDR_MSG_CONTROL  1
#endif


#ifndef NGX_HAVE_FIONBIO
#define NGX_HAVE_FIONBIO  1
#endif


#ifndef NGX_HAVE_GMTOFF
#define NGX_HAVE_GMTOFF  1
#endif


#ifndef NGX_USE_HTTP_FILE_CACHE_UNIQ
#define NGX_USE_HTTP_FILE_CACHE_UNIQ  1
#endif


#ifndef NGX_SUPPRESS_WARN
#define NGX_SUPPRESS_WARN  1
#endif


#ifndef NGX_SMP
#define NGX_SMP  1
#endif


#ifndef NGX_PREFIX
#define NGX_PREFIX  "/data1/ngmp.nginx/"
#endif


#ifndef NGX_SBIN_PATH
#define NGX_SBIN_PATH  "/data1/ngmp.nginx/sbin/ngmp.nginx"
#endif


#ifndef NGX_CONF_PREFIX
#define NGX_CONF_PREFIX  "/data1/ngmp.nginx/conf/"
#endif


#ifndef NGX_CONF_PATH
#define NGX_CONF_PATH  "/data1/ngmp.nginx/conf/nginx.conf"
#endif


#ifndef NGX_PID_PATH
#define NGX_PID_PATH  "/data1/ngmp.nginx/logs/nginx.pid"
#endif


#ifndef NGX_LOCK_PATH
#define NGX_LOCK_PATH  "/data1/ngmp.nginx/logs/nginx.lock"
#endif


#ifndef NGX_ERROR_LOG_PATH
#define NGX_ERROR_LOG_PATH  "/data1/ngmp.nginx/logs/error.log"
#endif


#ifndef NGX_HTTP_LOG_PATH
#define NGX_HTTP_LOG_PATH  "/data1/ngmp.nginx/logs/access.log"
#endif


#ifndef NGX_HTTP_CLIENT_TEMP_PATH
#define NGX_HTTP_CLIENT_TEMP_PATH  "/data1/ngmp.nginx/client_body_temp"
#endif


#ifndef NGX_HTTP_PROXY_TEMP_PATH
#define NGX_HTTP_PROXY_TEMP_PATH  "/data1/ngmp.nginx/proxy_temp"
#endif


#ifndef NGX_HTTP_FASTCGI_TEMP_PATH
#define NGX_HTTP_FASTCGI_TEMP_PATH  "/data1/ngmp.nginx/fastcgi_temp"
#endif


#ifndef NGX_USER
#define NGX_USER  "nobody"
#endif


#ifndef NGX_GROUP
#define NGX_GROUP  "nogroup"
#endif

