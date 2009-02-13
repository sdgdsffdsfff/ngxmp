/* FUNC_DEC_START */
/* FUNC_DEC_END */

#ifndef __ngx_util__
#	define __ngx_util__

#define __return(s) (s)



#define UUID_LEN 37


#ifndef min
#       define min(x,y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#       define max(x,y) ((x) > (y) ? (x) : (y))
#endif


#define LOG_PREFIX ":::"
/* request log */
#define logr(fmt ...)  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, LOG_PREFIX fmt)
#define logre(fmt ...) ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, LOG_PREFIX fmt)

/* conf log */
#define logc(fmt ...)  ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, LOG_PREFIX fmt)
#define logce(fmt ...) ngx_conf_log_error(NGX_LOG_ERR, cf, 0, LOG_PREFIX fmt)


/* cycle log */
#define logy(fmt ...)  ngx_log_error(NGX_LOG_DEBUG, cycle->log, 0, LOG_PREFIX fmt)
#define logye(fmt ...) ngx_log_error(NGX_LOG_ERR, cycle->log, 0, LOG_PREFIX fmt)

#define logmc(fmt ...)  fprintf(stderr, LOG_PREFIX fmt)
#define logmce(fmt ...) fprintf(stderr, LOG_PREFIX fmt)

#define by_offset(p,type,off) (type*)(&((char*)(p))[(off)])

/* without current : c is index */
/* TODO fix other reference */
/* TODO skip */
#define next_tok(d,l,del,c) while((c) < (l) && (del) != (d)[(c)]) {++(c);}

/**
 * char esc
 */
#define next_tok_esc(d,l,del,esc)                                               \
({                                                                              \
    char *c = (d);                                                              \
    char *e = (d) + (l);                                                        \
    while(c < e && (del) != *c) {                                               \
      if ((esc) == *c) {                                                        \
        ++c;                                                                    \
      }                                                                         \
      ++c;                                                                      \
    }                                                                           \
    __return(min(c, e));                                                        \
})

#define bufprintf(n,buf,fmt...)                                                 \
  do{                                                                           \
    (n) = sprintf((char*)(buf)->last,fmt);                                      \
    if ((buf)->last + (n) + 1 /* 0 */ > (buf)->end) {(n) = -1;};                \
    (buf)->last += (n);                                                         \
    *(buf)->last = 0;                                                           \
  }while(0)

#define ngx_array_get(a,n) ((void*)&((char*)((a)->elts))[n * (a)->size])


/**
 * some_type *e;
 * ngx_array_t *a;
 * ngx_array_foreach(a, e) {
 *    
 * }
 */
#define ngx_array_foreach(a, e) for (                                           \
   (e) = (a)->elts;                                                             \
   (e) < (a)->elts + (a)->nelts * (a)->size;                                    \
   (e) = (void*)((u_char*)(e) + (a)->size))

#define ngx_array_foreachi(a, e, i) for (                                       \
   (e) = (a)->elts, (i) = 0;                                                    \
   (i) < (a)->nelts;                                                            \
   ++(i), (e) = ngx_array_get(a, (i)))

#define ngx_array_find(a, e, cond, job)                                         \
    ngx_array_foreach(a, e) {                                                   \
      if (cond) {                                                               \
        job;                                                                    \
        break;                                                                  \
      }                                                                         \
    }

#define ngx_array_findi(a, e, i, cond, job)                                     \
    ngx_array_foreachi(a, e, i) {                                               \
      if (cond) {                                                               \
        job;                                                                    \
        break;                                                                  \
      }                                   








#define get_vv_reterr(v, r, idx) do {                                           \
  (v) = ngx_http_get_flushed_variable((r), (idx));                              \
  if (NULL == (v) || !(v)->valid || (v)->not_found) {                           \
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, (r)->connection->log, 0,                  \
        "::: Cant find variable "#v" at index[%d]", (idx));                     \
    return NGX_ERROR;                                                           \
  }                                                                             \
} while(0)

#define get_vv_ornull(v, r, idx) do {                                           \
  (v) = ngx_http_get_flushed_variable((r), (idx));                              \
  if (NULL == (v)) {                                                            \
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, (r)->connection->log, 0,                  \
        "::: no variable "#v" at index[%d]", (idx));                            \
    return NGX_ERROR;                                                           \
  }                                                                             \
  else {                                                                        \
    if ((v)->not_found || !(v)->valid) {                                        \
      (v)->data = NULL;                                                         \
      (v)->len = 0;                                                             \
      (v)->not_found = 0;                                                       \
      (v)->valid = 1;                                                           \
    }                                                                           \
  }                                                                             \
} while(0)


/* uri unescape */

#define is_ltr(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define to_lower(c) (c|0x20)
#define is_num(c) ((c) >= '0' && (c) <= '9')
#define is_af(c) (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define is_x(c) (is_num(c) || is_af(c))

#define x_to_c(c) (is_af(c) ? (to_lower(c) - 'a' + 10) : ((c) - '0'))


/**
 * unescape from 'from' of length 'len' & write to 'to',
 * 'to' will be modified to point to the last char unescaped
 */
#define uri_unescape(to, from, len)                                             \
    do {                                                                        \
                                                                                \
      ngx_uint_t i;                                                             \
      ngx_uint_t j;                                                             \
      u_char c;                                                                 \
      for (i= 0, j=0; i < (len); ++i){                                          \
        if ((from)[i] == '+')  {                                                \
          (to)[j++] = ' ';                                                      \
        }                                                                       \
                                                                                \
        else if ((from)[i] == '%' && i < (len) - 2) {                           \
          if (is_x((from)[i+1])                                                 \
              && is_x((from)[i+2])) {                                           \
            c = x_to_c((from)[i+1]) << 4;                                       \
            c |= x_to_c((from)[i+2]);                                           \
            if (0 && c == '<') {                                                     \
              (to)[j++] = '&';                                                  \
              (to)[j++] = 'l';                                                  \
              (to)[j++] = 't';                                                  \
              (to)[j++] = ';';                                                  \
            }                                                                   \
            else if (0 && c == '>') {                                                \
              (to)[j++] = '&';                                                  \
              (to)[j++] = 'g';                                                  \
              (to)[j++] = 't';                                                  \
              (to)[j++] = ';';                                                  \
            }                                                                   \
            else {                                                              \
              (to)[j++] = c;                                                    \
            }                                                                   \
                                                                                \
            i += 2;                                                             \
          }                                                                     \
          else {                                                                \
            (to)[j++] = i;                                                      \
          }                                                                     \
        }                                                                       \
        else {                                                                  \
          (to)[j++] = (from)[i];                                                \
        }                                                                       \
      }                                                                         \
      (to) = &(to)[j];                                                          \
    } while(0)




#define uri_unescape_p(pool, to, tolen, from, len)                              \
  do {                                                                          \
     u_char *buf = ngx_palloc((pool), (len));                                   \
      if (NULL == buf) {                                                        \
        (to) = NULL;                                                            \
        (tolen) = 0;                                                            \
        break;                                                                  \
      }                                                                         \
      u_char *o = buf;                                                          \
      uri_unescape(o, (from), (len));                                           \
      (to) = buf;                                                               \
      (tolen) = o - buf;                                                        \
  } while (0)

#define variable_isvalid(v) ((v) && (v)->valid && !(v)->not_found)
#define variable_isok(v) (variable_isvalid(v) && (v)->len)

#define buf_len(b) ((b)->last - (b)->pos)

#include "ngx_str_util.h"
#include "ngx_hash_util.h"
#include "ngx_debug_util.h"
#include "ngx_memcached_util.h"

#ifndef __UTIL_WITHOUT_FUNC__



  static char *
ngx_http_get_var_name_str(ngx_str_t *v, ngx_str_t *n)
{
  if ('$' != v->data[0]) {
    fprintf(stderr, "%.*s", strpp(v));
    return (char*)"found invalid variable name";
  }
  n->data = v->data + 1;
  n->len  = v->len - 1;

  return NULL;
}

  static u_char*
append_vv(u_char *start, u_char *end, ngx_http_variable_value_t *vv)
{
  if (NULL == vv){
    return start;
  }
  if (end - start >= vv->len) {
    memcpy(start, vv->data, vv->len);
    return start + vv->len;
  }
  return NULL;
}

  static u_char*
append_str(u_char *start, u_char *end, ngx_str_t *s)
{
  if (NULL == s){
    return start;
  }

  if (end >= start + s->len) {
    memcpy(start, s->data, s->len);
    return start + s->len;
  }
  return NULL;
}


  static ngx_int_t
ngx_buf_append_str(ngx_buf_t *buf, ngx_str_t *str)
{
  u_char * nb = append_str(buf->last, buf->end, str);
  if (NULL == nb) {
    return NGX_ERROR;
  }

  buf->last = nb;
  return NGX_OK;
}


  static ngx_int_t
str_to_array_0(u_char *data, ngx_int_t len, u_char del, ngx_array_t **arr)
{
  ngx_int_t i = 0;

  u_char *s;
  u_char *e;

  ngx_str_t *str;


  if (NULL == *arr){
    return NGX_ERROR;
  }

  s = data;
  for (i = 0;i < len;) {

    next_tok(data, len, del, i);
    e = &data[i];

    str = (ngx_str_t*)ngx_array_push(*arr);
    if (NULL == str){ return NGX_ERROR; }

    str->data = s;
    str->len = e-s;

    s = &data[i+1];
  }

  return NGX_OK;
}

  static ngx_int_t
str_to_array(ngx_pool_t *p, u_char *data, ngx_int_t len, u_char del,
  ngx_array_t **arr, ngx_int_t n)
{

  *arr = ngx_array_create(p, n, sizeof(ngx_str_t));

  if (NULL == *arr){
    return NGX_ERROR;
  }

  return str_to_array_0(data, len, del, arr);

  /* s = data; */
  /* for (i = 0;i < len;) { */

    /* next_tok(data, len, del, i); */
    /* e = &data[i]; */

    /* str = ngx_array_push(*arr); */
    /* if (NULL == str){ return NGX_ERROR; } */

    /* str->data = s; */
    /* str->len = e-s; */

    /* s = &data[i+1]; */
  /* } */

  /* return NGX_OK; */
}


#define MK_cp(n) ((char*)n)
#define AJ_item(arr,off,i) (&MK_cp((arr)->elts)[(i) * (arr)->size + (off)])

  static ngx_int_t
array_join(ngx_pool_t *p,
    ngx_str_t *re,
    ngx_array_t *arr,
    ngx_int_t item_offset,
    ngx_str_t *head,
    ngx_str_t *tail,

    ngx_str_t *pre,
    ngx_str_t *suf,
    ngx_str_t *del)
{

  ngx_int_t  len;
  ngx_int_t  n;
  ngx_int_t  i;
  ngx_str_t *item;

  u_char    *c;
  u_char    *e;

  len = 0;
  n = arr->nelts;
  for (i= 0; i < n; ++i){
    item = (ngx_str_t*)AJ_item(arr, item_offset, i);
    len += item->len;
  }

  if (NULL != head) len += head->len;
  if (NULL != tail) len += tail->len;
  if (NULL != pre)  len += pre->len * n;
  if (NULL != suf)  len += suf->len * n;
  if (NULL != del)  len += del->len * ((n > 1)? (n - 1) : 0);

  if (0 >= len) {
    re->data = (u_char*)"";
    re->len = 0;
    return NGX_OK;
  }

  re->data = (u_char*)ngx_palloc(p, len + 1);
  if (NULL == re->data){
    return NGX_ERROR;
  }


  c = re->data;
  e = &re->data[len];
  c = append_str(c, e, head);

  for (i= 0; i < n-1; ++i){

    item = (ngx_str_t*)AJ_item(arr, item_offset, i);

    c = append_str(c, e, pre);
    c = append_str(c, e, item);
    c = append_str(c, e, suf);
    c = append_str(c, e, del);
  }

  item = (ngx_str_t*)AJ_item(arr, item_offset, i);
  c = append_str(c, e, pre);
  c = append_str(c, e, item);
  c = append_str(c, e, suf);

  c = append_str(c, e, tail);

  re->len = c - re->data;
  *c = 0;

  return NGX_OK;
}

  static ngx_int_t
array_join_x(
    ngx_pool_t  *p,

    ngx_str_t   *re,
    ngx_array_t *arr,
    ngx_int_t    step,

    ngx_str_t   *head,
    ngx_str_t   *tail,

    ngx_str_t   *lhead,
    ngx_str_t   *ltail,
    ngx_str_t   *ldel,

    ngx_array_t *dels   /* ngx_str_t */)
{

  ngx_int_t  len;
  ngx_int_t  n = 0;
  ngx_int_t  ln;
  ngx_int_t  i;
  ngx_int_t  j;
  ngx_str_t *item;

  u_char    *c;
  u_char    *e;

  ngx_str_t *del = NULL;
  ngx_int_t  ndel = 0;


  if (0 >= step) step = 1;

  if (0 != n % step) return NGX_ERROR;

  if (NULL != dels) {
    del = (ngx_str_t*)dels->elts;
    ndel = dels->nelts;
  }



  len = 0;
  n = arr->nelts;
  for (i= 0; i < n; ++i){
    item = (ngx_str_t*)arr->elts;
    item = &item[i];
    len += item->len;
  }

  if (NULL != head) len += head->len;
  if (NULL != tail) len += tail->len;
  if (NULL != lhead)  len += lhead->len * (n / step + 1);
  if (NULL != ltail)  len += ltail->len * (n / step + 1);
  if (NULL != ldel)  len += ldel->len * (n / step + 1);


  if (NULL != del) {
    for (i= 0; i < step; ++i){
      len += del[i % ndel].len * (n / step + 1);
    }
  }

  re->data = (u_char*)ngx_palloc(p, len+1);
  if (NULL == re->data){
    return NGX_ERROR;
  }

  if (0 == n) { ln = 0; }
  else  { ln = (n-1) / step + 1; }

  item = (ngx_str_t*)arr->elts;
  item = &item[(ln - 1) * step];
  if (NULL == item->data && 0 == item->len) {
    --ln;
  }

  c = re->data;
  e = &re->data[len];
  c = append_str(c, e, head);

  for (i= 0; i < ln; ++i){

    item = (ngx_str_t*)arr->elts;
    item = &item[i * step];
    if (NULL == item->data && 0 == item->len) {continue;}

    c = append_str(c, e, lhead);

    for (j= 0; j < step; ++j){

      item = (ngx_str_t*)arr->elts;
      item = &item[i * step + j];

      c = append_str(c, e, item);

      if (step - 1 != j && NULL != del) {
        c = append_str(c, e, &del[j % ndel]);
      }
    }


    c = append_str(c, e, ltail);

    if (ln - 1 != i)
      c = append_str(c, e, ldel);
  }


  c = append_str(c, e, tail);




  re->len = c - re->data;
  *c = 0;

  return NGX_OK;
}

#ifdef _UUID_UUID_H
  static ngx_int_t
ngx_uuid(ngx_str_t *u)
{
  uuid_t uuid;
  if (NULL == u || u->len < UUID_LEN) {
    return NGX_ERROR;
  }
  uuid_generate(uuid);
  uuid_unparse(uuid, (char*)u->data);
  return NGX_OK;
}
#endif


  static ngx_int_t
ngx_htmlencoding(ngx_pool_t *pool, ngx_str_t *s)
{
  ngx_uint_t i;
  ngx_uint_t j;
  ngx_uint_t n;
  ngx_uint_t len;
  u_char *d;
  for (i= 0; i < s->len; ++i){
    if (s->data[i] == '<' || s->data[i] == '>') {
      ++n;
    }
  }

  len = s->len + n * 3;
  d = ngx_palloc(pool, len);
  if (NULL == d) {
    return NGX_ERROR;
  }

  for (i = 0, j = 0; i < s->len; ++i){
    if (s->data[i] == '<') {
      d[j++] = '&';
      d[j++] = 'l';
      d[j++] = 't';
      d[j++] = ';';
    }
    else if (s->data[i] == '>') {
      d[j++] = '&';
      d[j++] = 'g';
      d[j++] = 't';
      d[j++] = ';';
    }
    else {
      d[j++] = s->data[i];
    }
  }

  s->data = d;
  s->len = j;

  return NGX_OK;
}

#endif /* __UTIL_WITHOUT_FUNC__ */

#endif
