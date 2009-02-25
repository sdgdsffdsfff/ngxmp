#ifndef __ngx_str_util__
#define __ngx_str_util__


#define ngx_str_buf(s,size)                                                     \
    u_char ___buf_of_##s[size];                                                 \
    ngx_str_t s = {size, ___buf_of_##s};

#define str_buf(s, size) ngx_str_buf(s, size)

#define ngx_str_pbuf(s, p, size)                                                \
    ngx_str_t (s) = {(size), NULL};                                               \
    (s).data = ngx_palloc((p), (size));


/**
 * alloc string in pool, return the data pointer.
 * return NULL for failure
 */
#define str_palloc(s, pool, size) ({                                            \
  (s)->data = ngx_palloc((pool), (size));                                       \
  (s)->len = (size);                                                            \
  __return((s)->data);                                                          \
})


#define str_shift(s, size) do {                                                 \
    if ((s)->len >= (size)) {                                                   \
      (s)->data += (size);                                                      \
      (s)->len -= (size);                                                       \
    }                                                                           \
    else {                                                                      \
      (s)->data = NULL;                                                         \
      (s)->len = 0;                                                             \
    }                                                                           \
  } while(0)


#define strpr(s)  s.data, s.len
#define strp(s)  s.len, s.data
#define strpp(s) s->len, s->data

#define str_ref_vv(s,v) do{(s)->data = (v)->data; (s)->len = (v)->len;}while(0)
#define str_ref_buf(s,b) do{(s)->data = (b)->pos; (s)->len = (b)->last - (b)->pos;}while(0)
#define str_ref_str(s,t) do{(s)->data = (t)->data; (s)->len = (t)->len;}while(0)
#define str_ref_char(s, cc) ({(s)->data = (u_char*)(cc); (s)->len = sizeof((cc)) - 1;})
#define str_ref(s, cc) str_ref_char(s, cc)

#define ngx_str_eq(a, b) ((a)->len == (b)->len && 0 == ngx_strncmp((a)->data, (b)->data, (a)->len))
#define ngx_str_pre(a, b) ((a)->len <= (b)->len && 0 == ngx_strncmp((a)->data, (b)->data, (a)->len))



/**
 * string switch case
 *
 * USAGE:
 * 
 *   ngx_str_t m = ngx_string("m1");
 *   str_switch(&m) {
 *     str_case("m1");
 *     str_case("m") {
 *       printf("got it");
 *       break;
 *     }
 * 
 *     str_default() {
 *       printf("got none");
 *     }
 *   }
 */

#define str_switch(s)  \
  { ngx_str_t *str_ref_internal=(s), str_ref_remainder = *(s); do

#define str_case(q)                                                             \
  if (NULL == str_ref_internal                                                  \
      || (                                                                      \
          ( str_ref_internal->len == (sizeof(q) - 1)                            \
            && 0 == memcmp(q, str_ref_internal->data, sizeof(q) - 1)            \
          )                                                                     \
          ? (str_ref_internal = NULL, 1)                                        \
          : (0)                                                                 \
      )                                                                         \
  )

#define str_case_pre(q)                                                         \
  if (NULL == str_ref_internal                                                  \
      || (                                                                      \
          ( str_ref_internal->len >= (sizeof(q) - 1)                            \
            && 0 == memcmp(q, str_ref_internal->data, sizeof(q) - 1)            \
          )                                                                     \
          ? (str_ref_internal = NULL,                                           \
            str_ref_remainder.data += sizeof(q) - 1,                            \
            str_ref_remainder.len -= sizeof(q) - 1,                             \
            1)                                                                  \
          : (0)                                                                 \
      )                                                                         \
  )

#define str_case_remainder() (&str_ref_remainder)

#define str_default() } while(0); if (str_ref_internal)



#endif /* __ngx_str_util__ */
