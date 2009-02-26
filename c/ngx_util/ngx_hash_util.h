#ifndef __ngx_hash_util__
#define __ngx_hash_util__


#define HASH_MAX_SIZE 32
#define HASH_BUCKET_SIZE 32


typedef struct hash_elt_s hash_elt_t;
struct hash_elt_s {
  ngx_uint_t hash;
  ngx_str_t name;

#define HASH_VAL_UNSET -1

  intptr_t val;
  hash_elt_t *next;
};

typedef struct {
  ngx_pool_t *pool;
  ngx_int_t size;
  hash_elt_t *buckets;
}
hash_t;


#define HASH_ISUNSET(v) (HASH_VAL_UNSET == (intptr_t)(v))

  static hash_t *
hash_create(ngx_pool_t *pool, ngx_int_t size)
{
  ngx_int_t i;
  hash_t *h = ngx_pcalloc(pool, sizeof(hash_t));
  if (NULL == h) {
    return NULL;
  }

  h->pool = pool;
  h->size = size;

  h->buckets = ngx_pcalloc(pool, sizeof(hash_elt_t) * size);
  if (NULL == h->buckets) {
    return NULL;
  }

  for (i= 0; i < h->size; ++i){
    h->buckets[i].val = HASH_VAL_UNSET;
  }

  return h;
}

  static ngx_int_t
hash_add(hash_t *hash, hash_elt_t *elt)
{
  ngx_int_t i;
  hash_elt_t *b;


  i = elt->hash % hash->size;
  /* fprintf(stderr, "hash:%d\n", i); */

  b = &hash->buckets[i];
  
  if (HASH_VAL_UNSET == b->val) {
    *b = *elt;
    /* fprintf(stderr, "add as first:%.*s\n", strp(elt->name)); */
    return NGX_OK;
  }

  for (;b->next;b = b->next);

  b->next = elt;
  elt->next = NULL;
  /* fprintf(stderr, "add as tail:%.*s\n", strp(b->next->name)); */

  return NGX_OK;
}

  static ngx_int_t
hash_padd(ngx_pool_t *p, hash_t *hash, ngx_str_t *name, uintptr_t val)
{
  hash_elt_t *elt;

  elt = ngx_pcalloc(p, sizeof(hash_elt_t));
  if (NULL == elt) {
    return NGX_ERROR;
  }

  elt->name = *name;
  elt->val = val;

  elt->hash = ngx_hash_key(name->data, name->len);
  /* fprintf(stderr, "%.*s, %d\n", strpp(name), elt->hash); */

  return hash_add(hash, elt);
}

  static intptr_t
hash_find(hash_t *hash, ngx_str_t *name)
{
  ngx_uint_t h;
  hash_elt_t *elt;


  h = ngx_hash_key(name->data, name->len);
  elt = &hash->buckets[h % hash->size];
  
  if (HASH_VAL_UNSET == elt->val) {
    return HASH_VAL_UNSET;
  }

  /* if (NULL == elt->next) { */
    /* return elt->val; */
  /* } */

  for(;elt && !ngx_str_eq(name, &elt->name);elt = elt->next);

  if (NULL == elt) {
    return HASH_VAL_UNSET;
  }

  return elt->val;
}


#endif /* __ngx_hash_util__ */
