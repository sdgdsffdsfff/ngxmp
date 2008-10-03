#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include "mc_protocal.h"

u_char _buf[4096];

#define loge(fmt...) do {        \
  u_char *p = _buf;                 \
  u_char *l = p + 4096;             \
  p = ngx_snprintf(p, l - p, fmt);\
  p = ngx_snprintf(p, l - p, "\n"); \
  *p = 0;                           \
  fprintf(stderr, (char*)_buf);     \
} while(0)                                 

u_char * ngx_cdecl
ngx_snprintf(u_char *buf, size_t max, const char *fmt, ...)
{
    u_char   *p;
    va_list   args;

    va_start(args, fmt);
    p = ngx_vsnprintf(buf, max, fmt, args);
    va_end(args);

    return p;
}

u_char *
ngx_vsnprintf(u_char *buf, size_t max, const char *fmt, va_list args)
{
    u_char                *p, zero, *last, temp[NGX_INT64_LEN + 1];
                                    /*
                                     * really we need temp[NGX_INT64_LEN] only,
                                     * but icc issues the warning
                                     */
    int                    d;
    size_t                 len, slen;
    uint32_t               ui32;
    int64_t                i64;
    uint64_t               ui64;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hexadecimal, max_width;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;
    static u_char          hex[] = "0123456789abcdef";
    static u_char          HEX[] = "0123456789ABCDEF";

    if (max == 0) {
        return buf;
    }

    last = buf + max;

    while (*fmt && buf < last) {

        /*
         * "buf < last" means that we could copy at least one character:
         * the plain character, "%%", "%c", and minus without the checking
         */

        if (*fmt == '%') {

            i64 = 0;
            ui64 = 0;

            zero = (u_char) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hexadecimal = 0;
            max_width = 0;
            slen = (size_t) -1;

            p = temp + NGX_INT64_LEN;

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + *fmt++ - '0';
            }


            for ( ;; ) {
                switch (*fmt) {

                case 'u':
                    sign = 0;
                    fmt++;
                    continue;

                case 'm':
                    max_width = 1;
                    fmt++;
                    continue;

                case 'X':
                    hexadecimal = 2;
                    sign = 0;
                    fmt++;
                    continue;

                case 'x':
                    hexadecimal = 1;
                    sign = 0;
                    fmt++;
                    continue;

                case '*':
                    slen = va_arg(args, size_t);
                    fmt++;
                    continue;

                default:
                    break;
                }

                break;
            }


            switch (*fmt) {

            case 'V':
                v = va_arg(args, ngx_str_t *);

                len = v->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);

                buf = ngx_cpymem(buf, v->data, len);
                fmt++;

                continue;

            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);

                len = vv->len;
                len = (buf + len < last) ? len : (size_t) (last - buf);

                buf = ngx_cpymem(buf, vv->data, len);
                fmt++;

                continue;

            case 's':
                p = va_arg(args, u_char *);

                if (slen == (size_t) -1) {
                    while (*p && buf < last) {
                        *buf++ = *p++;
                    }

                } else {
                    len = (buf + slen < last) ? slen : (size_t) (last - buf);

                    buf = ngx_cpymem(buf, p, len);
                }

                fmt++;

                continue;

            case 'O':
                i64 = (int64_t) va_arg(args, off_t);
                sign = 1;
                break;

            case 'P':
                i64 = (int64_t) va_arg(args, ngx_pid_t);
                sign = 1;
                break;

            case 'T':
                i64 = (int64_t) va_arg(args, time_t);
                sign = 1;
                break;

            case 'M':
                ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                if ((ngx_msec_int_t) ms == -1) {
                    sign = 1;
                    i64 = -1;
                } else {
                    sign = 0;
                    ui64 = (uint64_t) ms;
                }
                break;

            case 'z':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ssize_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;

            case 'i':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_uint_t);
                }

                if (max_width) {
                    width = NGX_INT_T_LEN;
                }

                break;

            case 'd':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_int);
                }
                break;

            case 'l':
                if (sign) {
                    i64 = (int64_t) va_arg(args, long);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;

            case 'D':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int32_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;

            case 'L':
                if (sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;

            case 'A':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ngx_atomic_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_atomic_uint_t);
                }

                if (max_width) {
                    width = NGX_ATOMIC_T_LEN;
                }

                break;

#if !(NGX_WIN32)
            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;
#endif

            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hexadecimal = 2;
                sign = 0;
                zero = '0';
                width = NGX_PTR_SIZE * 2;
                break;

            case 'c':
                d = va_arg(args, int);
                *buf++ = (u_char) (d & 0xff);
                fmt++;

                continue;

            case 'Z':
                *buf++ = '\0';
                fmt++;

                continue;

            case 'N':
#if (NGX_WIN32)
                *buf++ = CR;
#endif
                *buf++ = LF;
                fmt++;

                continue;

            case '%':
                *buf++ = '%';
                fmt++;

                continue;

            default:
                *buf++ = *fmt++;

                continue;
            }

            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;

                } else {
                    ui64 = (uint64_t) i64;
                }
            }

            if (hexadecimal == 1) {
                do {

                    /* the "(uint32_t)" cast disables the BCC's warning */
                    *--p = hex[(uint32_t) (ui64 & 0xf)];

                } while (ui64 >>= 4);

            } else if (hexadecimal == 2) {
                do {

                    /* the "(uint32_t)" cast disables the BCC's warning */
                    *--p = HEX[(uint32_t) (ui64 & 0xf)];

                } while (ui64 >>= 4);

            } else if (ui64 <= NGX_MAX_UINT32_VALUE) {

                /*
                 * To divide 64-bit number and to find the remainder
                 * on the x86 platform gcc and icc call the libc functions
                 * [u]divdi3() and [u]moddi3(), they call another function
                 * in its turn.  On FreeBSD it is the qdivrem() function,
                 * its source code is about 170 lines of the code.
                 * The glibc counterpart is about 150 lines of the code.
                 *
                 * For 32-bit numbers and some divisors gcc and icc use
                 * the inlined multiplication and shifts.  For example,
                 * unsigned "i32 / 10" is compiled to
                 *
                 *     (i32 * 0xCCCCCCCD) >> 35
                 */

                ui32 = (uint32_t) ui64;

                do {
                    *--p = (u_char) (ui32 % 10 + '0');
                } while (ui32 /= 10);

            } else {
                do {
                    *--p = (u_char) (ui64 % 10 + '0');
                } while (ui64 /= 10);
            }

            len = (temp + NGX_INT64_LEN) - p;

            while (len++ < width && buf < last) {
                *buf++ = zero;
            }

            len = (temp + NGX_INT64_LEN) - p;
            if (buf + len > last) {
                len = last - buf;
            }

            buf = ngx_cpymem(buf, p, len);

            fmt++;

        } else {
            *buf++ = *fmt++;
        }
    }

    return buf;
}

int main(int argv, char **args)
{

  mc_data data, *d = &data;

  ngx_buf_t buf;
  ngx_chain_t cl;
  ngx_str_t rget = ngx_string("get abc\r\n");
  ngx_str_t rgetv = ngx_string("VALUE abc 0 1 5\r\n12345\r\nEND\r\n");

  ngx_int_t rc;



  cl.buf = &buf;
  cl.next = NULL;


  
  rc = mc_parse_init(NULL, &d, mc_get_req, MC_T_GET);

  loge("%V", &rget);
  loge("init rc=%d", rc);

  buf.pos = rget.data;
  buf.last = rget.data + rget.len;

  rc = mc_parse(d, &cl);
  loge("parse rc=%d", rc);
  loge("key:%V", &d->key);


  rc = mc_parse_init(NULL, &d, mc_get_v_req, MC_T_VALUE);
  loge("%V", &rgetv);
  loge("init rc=%d", rc);

  buf.pos = rgetv.data;
  buf.last = rgetv.data + rgetv.len;

  rc = mc_parse(d, &cl);
  loge("parse rc=%d", rc);
  loge("key:%V", &d->key);
  loge("val:%V", &d->val);
  loge("len:%V", &d->len);
  loge("flag:%V", &d->flag);
  loge("exp:%V", &d->exp);
  
  return 0;
}
