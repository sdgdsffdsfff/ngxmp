#ifndef __ngx_hash_util__
#define __ngx_hash_util__


#define HASH_MAX_SIZE 32
#define HASH_BUCKET_SIZE 32


typedef struct hash_elt_s hash_elt_t;
struct hash_elt_s {
  ngx_uint_t hash;
  ngx_str_t name;

#define HASH_VAL_UNSET 0
  uintptr_t val;
  hash_elt_t *next;
};

typedef struct {
  ngx_pool_t *pool;
  ngx_int_t size;
  hash_elt_t *buckets;
}
hash_t;

  static hash_t *
hash_create(ngx_pool_t *pool, ngx_int_t size)
{
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

  return h;
}

  static ngx_int_t
hash_add(hash_t *hash, hash_elt_t *elt)
{
  ngx_int_t i;
  hash_elt_t *b;


  i = elt->hash % hash->size;

  b = &hash->buckets[i];
  if (HASH_VAL_UNSET == b->val) {
    *b = *elt;
  }

  for (;b->next;b = b->next);

  b->next = elt;
  elt->next = NULL;

  return NGX_OK;
}

#endif /* __ngx_hash_util__ */
