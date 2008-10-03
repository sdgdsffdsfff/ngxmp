/* FUNC_DEC_START */
/* FUNC_DEC_END */

#ifndef __ngx_util__
#	define __ngx_util__

/* request log */
#	define logr(fmt ...) ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, ":::"fmt)
#	define logre(fmt ...) ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, ":::"fmt)

/* conf log */
#	define logc(fmt ...) ngx_conf_log_error(NGX_LOG_ERR, cf, 0, ":::"fmt)
#	define logce(fmt ...) ngx_conf_log_error(NGX_LOG_ERR, cf, 0, ":::"fmt)

/* cycle log */
#	define logy(fmt ...) ngx_log_debug(NGX_LOG_ERR, cycle->log, 0, ":::"fmt)
#	define logye(fmt ...) ngx_log_debug(NGX_LOG_ERR, cycle->log, 0, ":::"fmt)

#	define logmc(fmt ...) fprintf(stderr, ":::"fmt)
#	define logmce(fmt ...) fprintf(stderr, ":::"fmt)

/* without current : c is index */
#	define next_tok(d,l,del,c) do{++(c);} while((c) < (l) && (del) != (d)[(c)])

  static char *
ngx_http_get_var_name_str(ngx_str_t *v, ngx_str_t *n)
{
  if ('$' != v->data[0]) {
    return "found invalid variable name";
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

    str = ngx_array_push(*arr);
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

  static ngx_int_t
array_join(ngx_pool_t *p,  ngx_str_t *re, ngx_array_t *arr, ngx_str_t *head, ngx_str_t *tail, 
  ngx_str_t *pre, ngx_str_t *suf, ngx_str_t *del)
{

  ngx_int_t len;
  ngx_int_t n;
  ngx_int_t i;
  ngx_str_t *item;

  u_char *c;
  u_char *e;

  len = 0;
  n = arr->nelts;
  for (i= 0; i < n; ++i){
    item = arr->elts;
    item = &item[i];
    len += item->len;
  }

  if (NULL != head) len += head->len;
  if (NULL != tail) len += tail->len;
  if (NULL != pre)  len += pre->len * n;
  if (NULL != suf)  len += suf->len * n;
  if (NULL != del)  len += del->len * (n-1);

  re->data = ngx_palloc(p, len+1);
  if (NULL == re->data){
    return NGX_ERROR;
  }


  c = re->data;
  e = &re->data[len];
  c = append_str(c, e, head);
  for (i= 0; i < n-1; ++i){
    item = arr->elts;
    item = &item[i];
    c = append_str(c, e, pre);
    c = append_str(c, e, item);
    c = append_str(c, e, suf);
    c = append_str(c, e, del);
  }
  item = arr->elts;
  item = &item[i];
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
  ngx_int_t  n;
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
    del = dels->elts;
    ndel = dels->nelts;
  }



  len = 0;
  n = arr->nelts;
  for (i= 0; i < n; ++i){
    item = arr->elts;
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

  re->data = ngx_palloc(p, len+1);
  if (NULL == re->data){
    return NGX_ERROR;
  }

  if (0 == n) { ln = 0; }
  else  { ln = (n-1) / step + 1; }

  item = arr->elts;
  item = &item[(ln - 1) * step];
  if (NULL == item->data && 0 == item->len) {
    --ln;
  }

  c = re->data;
  e = &re->data[len];
  c = append_str(c, e, head);

  for (i= 0; i < ln; ++i){

    item = arr->elts;
    item = &item[i * step];
    if (NULL == item->data && 0 == item->len) {continue;}

    c = append_str(c, e, lhead);

    for (j= 0; j < step; ++j){

      item = arr->elts;
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






#endif
