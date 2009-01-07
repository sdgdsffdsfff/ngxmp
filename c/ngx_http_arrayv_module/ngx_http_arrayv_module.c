/**
 * Copyright 2008 Sina Corporation. All rights reserved.
 *
 *  arrayv module
 *  array variable, a variable contains some variables as an array.
 *
 * commands:
 *
 * push_array
 *           push_array $array $elts;
 *           this cmd means, you add a new element to the array using push(OP_HEAD), $array is the array u want to add,
 *           and $elts is the variable to be added.
 *pop_array
 *           pop_array  $out $array;
 *            this cmd means delete a element from an array and put the delete element's value to the $out.
 *ushift_array
 *             ushift_array $array $elts;
 *              the same as the push_array, but the add function using OP_TAIL。
 *shift_array
 *              shift_array $out $array;
 *              the same as the pop_array, but the delete OP using OP_TAIL.
 *print_array
 *              should take 3 args, it like array to string.
 *              print_array $out $array format_string;
 *              use this cmd, the elts of the array will print to $out as the format the format_string defined.
 *              such as "\{{(\"$:$\"),?}\}". '{','$','(' and '?' are define to be format special use.so ,if your format contais
 *              these chars, please use '\' to diff.
 *              "\{{(\"$:$\"),?}\}" means use '{' as head '}' as tail ,2 elts as one new element, and ','to between them. '"'\
 *              to as the lhead and ltail.
 *              so if the array has the elts: a,b,c,d,e,f,g
 *              it will be: {"a:b","c:d","e:f"},'g'will be cut.
 *str_to_array
 *                should also take 3 args. put a string to an array use delete string.
 *                str_to_array $array string del_uchar;
 *                eg.  str_to_array $array "$a$b" ":";
 *                it means use ':'to del the string ,and each new string as a new element add to $array. "$a$b" also can
 *                be just a string, such as str_to_array $array "aaa:ccc:d:ef".
 *                this add op usd OP_HEAD.
 *
 *  Authors:
 *      Chaimvy <zhangyu4@staff.sina.com.cn>
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include<ngx_http_script.h>

#include"debug.h"
#include"ngx_util.h"

/*push & pop use this op*/
#define OP_HEAD 1
/*shift & unshift use this op*/
#define OP_TAIL 2
/*in get handler function ,list means print the array in a string*/
#define OP_LIST 3
/*str_to_array*/
#define OP_STR 4

#define OP_DEFAULT -1

/**
 * ngx_http_script_var_get_handler_code_t for array_var
 */
typedef struct {
  ngx_http_script_code_pt code;
  ngx_http_get_variable_pt handler;
  uintptr_t data;
} 
ngx_http_script_var_get_handler_code_t;

/* this struct was defined in rewrite module, arrayv module use it */
typedef struct
{
  ngx_array_t *codes; /* uintptr_t */

  ngx_uint_t captures;
  ngx_uint_t stack_size;

  ngx_flag_t log;
  ngx_flag_t uninitialized_variable_warn;
} ngx_http_rewrite_loc_conf_t;

/* to store the array variables  the conf defined */
typedef struct
{
  ngx_array_t *avs; /* ngx_http_variable_t*,  array_variable*/
} ngx_http_av_main_conf_t;

/*use two arrays */
typedef struct
{
  ngx_array_t *arr;
  ngx_int_t size;
  ngx_int_t start;
  ngx_int_t end;
} ngx_http_av_elt;

typedef struct
{

  ngx_http_av_elt *pop;/*array variable, each value is a variable's value, pop one*/
  ngx_http_av_elt *sft;/*shift one*/

} ngx_http_av_vctx_t;

/*this struct used in print array funtion*/
typedef struct
{
  ngx_int_t step;
  ngx_str_t head;
  ngx_str_t tail;
  ngx_str_t lhead;
  ngx_str_t ltail;
  ngx_str_t ldel;
  ngx_array_t *dels;/*dels*/
} ngx_http_av_joinx_t;

/*used in vcode, as the variables's data*/
typedef struct
{
  ngx_int_t op;/*operation flag*/
  ngx_int_t index;/*av index*/
  ngx_http_av_joinx_t *joinx;/*print, 11.13*/
  u_char del;
} ngx_http_av_op_t;

/*
 * get handler for array_v
 */
       void       ngx_http_script_var_get_handler_code(ngx_http_script_engine_t *e);
static void      *ngx_http_create_main_conf           (ngx_conf_t *cf);
static char      *ngx_http_init_main_conf             (ngx_conf_t *cf, void *conf);
static char      *ngx_http_av_array_var_command       (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_push_var_command           (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_pop_var_command            (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_shift_var_command          (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_unshift_var_command        (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_print_var_command          (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_str_to_var_command         (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t  ngx_http_av_get_handler             (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static void       ngx_http_av_set_handler             (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static char      *ngx_http_av_in_command              (ngx_conf_t *cf, void *conf, ngx_int_t op);
static char      *ngx_http_av_out_command             (ngx_conf_t *cf, void *conf, ngx_int_t op);
static ngx_int_t  ngx_http_rewrite_var                (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static char      *ngx_http_rewrite_value              (ngx_conf_t *cf, ngx_http_rewrite_loc_conf_t *lcf, ngx_str_t *value);
static void       ngx_http_av_copy_vvt                (ngx_http_request_t *r, ngx_http_variable_value_t *to, ngx_http_variable_value_t *from);

static ngx_http_av_joinx_t * ngx_http_av_readstr(ngx_conf_t *cf, ngx_str_t * str);

static ngx_command_t ngx_http_av_commands[] =
{

  { ngx_string("array_var"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,/*@todo, define and push*/
    ngx_http_av_array_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("pop_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
    ngx_http_pop_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("push_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
    ngx_http_push_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("shift_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
    ngx_http_shift_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("unshift_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
    ngx_http_unshift_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("print_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_2MORE,
    ngx_http_print_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  { ngx_string("str_to_array"),
    NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_2MORE,
    ngx_http_str_to_var_command, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},

  ngx_null_command };

static ngx_http_module_t ngx_http_av_module_ctx =
{ 
  NULL,                                         /* preconfiguration */
  NULL,                                         /* postconfiguration */
  ngx_http_create_main_conf,                    /* create main configuration */
  ngx_http_init_main_conf,                      /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};

extern ngx_module_t ngx_http_rewrite_module;

ngx_module_t ngx_http_av_module =
{ 
  NGX_MODULE_V1,
  &ngx_http_av_module_ctx,                      /* module context */
  ngx_http_av_commands,                         /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  NULL,                                         /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  NULL,                                         /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};

/*the cmd to define a array variable*/
  static char *
ngx_http_av_array_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  fSTEP;
  ngx_http_core_main_conf_t *cmcf;
  ngx_http_core_loc_conf_t *clcf;
  ngx_http_av_main_conf_t *mcf;

  ngx_str_t *argvalues;
  ngx_str_t valname;
  ngx_http_variable_t *v;
  ngx_int_t vi;

  ngx_http_variable_t **ip;
  ngx_uint_t i;
  char *res;
  ngx_http_av_op_t *opindex;/*add vvt*/

  if (cf->args->nelts != 2)
  {
    return "arguments number must  be  1";
  }

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_av_module);

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

  argvalues = cf->args->elts;

  res = ngx_http_get_var_name_str(&argvalues[1], &valname);
  if (NULL != res)
  {
    logc("get var name err! @ array_var directive function. ");
    return res;
  }

  for (i = 0; i < mcf->avs->nelts; ++i)
  {
    ip = mcf->avs->elts;
    v = ip[i];
    if (0 == ngx_strncmp(v->name.data, valname.data, valname.len))
    {
      return "found duplicated AV name";
    }
  }

  v = ngx_http_add_variable(cf, &valname, NGX_HTTP_VAR_CHANGEABLE);
  vi = ngx_http_get_variable_index(cf, &valname);
  /*logc("%s 's index is %d",valname.data,vi);*/

  opindex = ngx_pcalloc(cf->pool, sizeof(ngx_http_av_op_t));
  v->get_handler = ngx_http_av_get_handler;
  v->set_handler = ngx_http_av_set_handler;
  opindex->index = vi;
  logc("opindex->index is %d",opindex->index);
  v->data = (uintptr_t) opindex;
  /*logc("v->data->index is %d",((ngx_http_av_op_t*)v->data)->index);*/

  /*logc("AV[%V] ctx:[%p],in array_var cmd function", &v->name, v->data);*/


  ip = ngx_array_push(mcf->avs);
  if (NULL == ip)
  {
    return "pushing array_variable to main conf failed";
  }
  *ip = v;

  return NGX_CONF_OK;
}

  static char*
ngx_http_pop_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{

  if (NGX_CONF_OK == (ngx_http_av_out_command(cf, conf, OP_HEAD)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;

}

  static char*
ngx_http_shift_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  if (NGX_CONF_OK == (ngx_http_av_out_command(cf, conf, OP_TAIL)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;
}

  static char*
ngx_http_push_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  if (NGX_CONF_OK == (ngx_http_av_in_command(cf, conf, OP_HEAD)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;
}
  static char*
ngx_http_unshift_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{

  if (NGX_CONF_OK == (ngx_http_av_in_command(cf, conf, OP_TAIL)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;

}

  static char*
ngx_http_print_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{

  if (NGX_CONF_OK == (ngx_http_av_out_command(cf, conf, OP_LIST)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;

}

  static char*
ngx_http_str_to_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{

  if (NGX_CONF_OK == (ngx_http_av_in_command(cf, conf, OP_STR)))
    return NGX_CONF_OK;
  else
    return NGX_CONF_ERROR;

}

  static ngx_int_t
ngx_http_av_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{

  ngx_http_variable_value_t *vv_value;
  ngx_http_variable_value_t **arg;/*the one get*/
  ngx_http_av_main_conf_t *mcf;
  ngx_http_av_vctx_t *ctx;

  ngx_int_t rc;
  ngx_int_t vvindex;
  ngx_http_av_op_t *opindex;

  /*add for joinx*/
  ngx_http_variable_value_t *item;
  ngx_int_t arr_size;
  ngx_array_t *temparr;

  ngx_str_t out;
  ngx_str_t *pusharg;
  ngx_str_t vstr;/*varialbe_value_t to str*/

  ngx_int_t curr;
  ngx_int_t loop;
  char *buf = NULL;

  /*if use set null or empty ,should define the vars
   *
   ngx_int_t emptynum = 0;/*n%step!=0*/
  ngx_str_t nullstr = {1," "};/*or ngx_null_string.*/
  */

      mcf = ngx_http_get_module_main_conf(r, ngx_http_av_module);
  if (NULL == mcf)
    logr("ngx_http_av_get_handler function, null mcf......");

  /* modify to vvt*/
  opindex = (ngx_http_av_op_t*)data;
  vvindex = opindex->index;

  vv_value = &r->variables[vvindex];

  /*modify end*/

  logr("\n get handler ::::r->variables[vvindex].len is :%d",r->variables[vvindex].len);

  if (0==vv_value->len && vv_value->data == NULL && 0==vv_value->valid&&0==vv_value->not_found)
  {

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_av_vctx_t));
    logr("coming in get handler initializing......");
    if (NULL == ctx)
    {
      logr( "av set handler err: create variable context failed");
    }

    ctx->pop = ngx_pcalloc(r->pool, sizeof(ngx_http_av_elt));
    ctx->sft = ngx_pcalloc(r->pool, sizeof(ngx_http_av_elt));
    ctx->pop->arr = ngx_array_create(r->pool, 20, sizeof(ngx_http_variable_value_t*));
    ctx->sft->arr= ngx_array_create(r->pool, 20, sizeof(ngx_http_variable_value_t*));
    if (NULL == ctx->pop||NULL == ctx->sft)
    {
      return NGX_ERROR;
    }

    ctx->pop->size = ctx->sft->size = 0;
    ctx->pop->start = ctx->sft->start = -1;
    ctx->pop->end = ctx->sft->end = -1;

    logr("get handler initializing......");

    vv_value->data = (u_char*) ctx;
    vv_value->len = sizeof(ngx_http_av_vctx_t);
    vv_value->valid =1;
    vv_value->not_found = 0;

  }

  ctx = (ngx_http_av_vctx_t*)r->variables[vvindex].data;

  if (NULL == ctx)
  {
    logr("ctx is null,,,,, get handler.....");
  }

  item = ngx_pcalloc(r->pool,sizeof(ngx_http_variable_value_t));

  if (opindex->op ==OP_HEAD)
  {

    /*array pop empty ,pop array_push*/
    if(ctx->pop->size == 0)
    {

      if(ctx->sft->size == 0)
      {/*array_push empty*/
        *v = ngx_http_variable_null_value;
        return NGX_OK;
      }
      else if (ctx->sft->size == 1)
      {
        arg = ctx->sft->arr->elts;

        ngx_http_av_copy_vvt(r, v, arg[ctx->sft->start]);
        ctx->sft->start--;/*when size is 1*/
        ctx->sft->end--;
        ctx->sft->size = 0;
        ctx->sft->arr->nelts--;/*-- when size is 1*/
        logr("pop get handler:::ctx->sft->size is %d ",ctx->sft->size);
        return NGX_OK;
      }
      else if (ctx->sft->size> 1)
      {
        arg = ctx->sft->arr->elts;

        ngx_http_av_copy_vvt(r, v, arg[ctx->sft->start]);
        ctx->sft->start++;
        ctx->sft->size--;
        logr("pop get handler:::ctx->sft->size is %d ",ctx->sft->size);
        return NGX_OK;
      }
      return NGX_ERROR;
    }

    else if (ctx->pop->size> 0)
    {
      arg = ctx->pop->arr->elts;

      ngx_http_av_copy_vvt(r, v, arg[ctx->pop->end]);
      if(ctx->pop->size==1)
      {
        ctx->pop->start--;
        ctx->pop->end--;
      }
      else
      {
        ctx->pop->end--;
      }
      ctx->pop->size--;
      ctx->pop->arr->nelts--;
      return NGX_OK;
      logr("pop get handler:::ctx->pop->size is %d ",ctx->pop->size);
    }
    return NGX_ERROR;
  }

  else if (opindex->op == OP_TAIL)
  {
    /*array shift empty ,unshift array_pop*/
    if(ctx->sft->size == 0)
    {

      if(ctx->pop->size == 0)
      {/*array_push empty*/
        *v = ngx_http_variable_null_value;
        return NGX_OK;
      }
      else if (ctx->pop->size == 1)
      {
        arg = ctx->pop->arr->elts;

        ngx_http_av_copy_vvt(r, v, arg[ctx->pop->start]);
        ctx->pop->start--;/*when size is 1*/
        ctx->pop->end--;
        ctx->pop->size = 0;
        ctx->pop->arr->nelts--;/*-- when size is 1*/
        logr("shift::sft->size is 0,ctx->pop->size is %d ",ctx->pop->size);
        return NGX_OK;
      }
      else if (ctx->pop->size> 1)
      {
        arg = ctx->pop->arr->elts;
        ngx_http_av_copy_vvt(r, v, arg[ctx->pop->start]);
        ctx->pop->start++;
        ctx->pop->size--;
        logr("shift::sft->size is 0, ctx->pop->size is %d ",ctx->pop->size);
        return NGX_OK;
      }
      return NGX_ERROR;
    }
    else if (ctx->sft->size> 0)
    {
      arg = ctx->sft->arr->elts;
      ngx_http_av_copy_vvt(r, v, arg[ctx->sft->end]);
      if(ctx->sft->size==1)
      {
        ctx->sft->start--;
        ctx->sft->end--;
      }
      else
      {
        ctx->sft->end--;
      }
      ctx->sft->size--;
      ctx->sft->arr->nelts--;
      logr("shift::,:::ctx->sft->size is %d ",ctx->sft->size);
      return NGX_OK;
    }
    return NGX_ERROR;
  }

  else if (opindex->op == OP_LIST)
  {

    logr("\n\nin get handler, OP_LIST,");
    arr_size = ctx->sft->size + ctx->pop->size;
    logr("ctx->sft->size is %d ",ctx->sft->size);
    logr("ctx->pop->size is %d ",ctx->pop->size);
    if (0 == arr_size)
    {
      logr("array_v's array size is 0, can't get elements.");
      return NGX_ERROR;
    }

    temparr = ngx_array_create(r->pool,10,sizeof(ngx_str_t));

    if (ctx->sft->size> 0)
    {
      loop = 0;
      curr = ctx->sft->end;
      arg = ctx->sft->arr->elts;
      for (loop=0;loop<ctx->sft->size;loop++)
      {
        ngx_http_av_copy_vvt(r, item, arg[curr]);
        buf = ngx_palloc(r->pool, sizeof(char) * ((item->len) + 1));
        ngx_memcpy(buf,item->data,item->len);
        vstr.data = (u_char*)buf;
        vstr.len = item->len;
        pusharg = ngx_array_push(temparr);
        *pusharg = vstr;
        curr--;
      }
      logr("ctx->sft->size is %d ",ctx->sft->size);
    }

    if (ctx->pop->size> 0)
    {

      logr("ctx->pop->size is %d ",ctx->pop->size);
      loop = 0;
      curr = ctx->pop->start;
      arg = ctx->pop->arr->elts;
      for (loop=0;loop<ctx->pop->size;loop++)
      {
        ngx_http_av_copy_vvt(r, item, arg[curr]);

        buf = ngx_palloc(r->pool, sizeof(char) * ((item->len) + 1));
        ngx_memcpy(buf,item->data,item->len);
        vstr.data = (u_char*)buf;
        vstr.len = item->len;
        pusharg = ngx_array_push(temparr);
        *pusharg = vstr;
        curr++;

      }
    }

    if(temparr->nelts%(opindex->joinx->step) != 0)
    {
      /*set as empty null or ""string

        emptynum = opindex->joinx->step - (temparr->nelts%(opindex->joinx->step));
        for(loop=0;loop<emptynum;loop++)
        {

        pusharg = ngx_array_push(temparr);
       *pusharg = nullstr;
       logr("\n\n\nwrite the empty one with null\n\n\n");
       }
       */
      /*cut the nelts%step ones*/

      temparr->nelts -= temparr->nelts%(opindex->joinx->step);
    }

    rc = array_join_x(r->pool, &out, temparr, opindex->joinx->step,
        &opindex->joinx->head, &opindex->joinx->tail, /*&head change to head   chaimvy*/
        &opindex->joinx->lhead, &opindex->joinx->ltail, &opindex->joinx->ldel,
        opindex->joinx->dels);

    if (NGX_OK != rc)
    {
      logr("\nprint array, get handler, rc is not ngx_ok\n");
      *v= ngx_http_variable_null_value;
      return rc;
    }

    v->data = out.data;
    v->len = out.len;
    v->valid = 1;
    return NGX_OK;
  }

  return NGX_OK;
}

  static void
ngx_http_av_set_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
  /**
   * TODO error handling
   */
  /*fSTEP;*/


  ngx_http_variable_value_t *vv_value;
  ngx_http_variable_value_t **arg;
  ngx_http_variable_value_t *vcopy;
  ngx_http_av_main_conf_t *mcf;
  ngx_http_av_vctx_t *ctx;
  ngx_int_t vvindex;
  ngx_http_av_op_t *opindex;/*add vvt*/

  /*for str_to array*/
  ngx_array_t *str_array;/*str_to_array arg,*/
  ngx_int_t arraysize = 20;/*default size as ten.*/
  ngx_uint_t str_loop = 0;
  ngx_http_variable_value_t *str_vcopy;
  ngx_str_t *item;

  mcf = ngx_http_get_module_main_conf(r, ngx_http_av_module);

  opindex = (ngx_http_av_op_t*) data;
  vvindex = opindex->index;

  logr("vvindex is %d",vvindex);

  vv_value = &r->variables[vvindex];

  if (NULL == vv_value)
    logr("set handler vv_value not exist");

  logr("set handler ::::r->variables[vvindex].len:%d",vv_value->len);

  if(0==vv_value->len
      && vv_value->data == NULL
      && 0==vv_value->valid
      &&0==vv_value->not_found)
  {

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_av_vctx_t));
    logr("coming in set handler initializing......");
    if (NULL == ctx)
    {
      logr( "av set handler err: create variable context failed");
    }

    ctx->pop = ngx_pcalloc(r->pool, sizeof(ngx_http_av_elt));
    ctx->sft = ngx_pcalloc(r->pool, sizeof(ngx_http_av_elt));
    ctx->pop->arr = ngx_array_create(r->pool, 20, sizeof(ngx_http_variable_value_t*));
    ctx->sft->arr= ngx_array_create(r->pool, 20, sizeof(ngx_http_variable_value_t*));
    if (NULL == ctx->pop->arr || NULL == ctx->sft->arr)
    {
      logr("pop or sft arr palloc err...., in set handler.....");
    }
    if (NULL == ctx->pop||NULL == ctx->sft)
    {
      logr( "av set handler err: NULL == ctx->pop||NULL == ctx->sft");
    }

    ctx->pop->size = ctx->sft->size = 0;
    ctx->pop->start = ctx->sft->start = -1;
    ctx->pop->end = ctx->sft->end = -1;
    /*ctx->op = OP_DEFAULT;*/
    logr("set handler initializing......");

    vv_value->data = (u_char*) ctx;
    vv_value->len = sizeof(ngx_http_av_vctx_t);
    vv_value->valid =1;
    vv_value->not_found = 0;
  }

  ctx = (ngx_http_av_vctx_t*)vv_value->data;

  if (NULL == ctx)
    logr("ctx is null in set handler...");

  if (NULL == ctx->pop||NULL == ctx->sft)
  {
    logr("set handler err: pop array or sft array is NULL");
  }

  vcopy = (ngx_http_variable_value_t*)ngx_pcalloc(r->pool,sizeof(ngx_http_variable_value_t));

  ngx_http_av_copy_vvt(r,vcopy, v);

  if (opindex->op ==OP_HEAD)
  {
    logr("set handler :::: begin to set.....,begin push array");
    logr("ctx->pop->size ::%d",ctx->pop->size);

    if(ctx->pop->size==0&&(ctx->sft->size>0)&&ctx->sft->start>0)
    {
      arg=ctx->pop->arr->elts;
      arg[--ctx->pop->start]=vcopy;
      ctx->sft->size++;
      logr("ctx->sft->arr[%d] is %s",ctx->sft->start,vcopy->data);
    }

    else
    {
      arg = ngx_array_push(ctx->pop->arr);
      logr("set handler :::: begin to set.....,after push array");
      if (NULL == arg)
      {
        logr("pushing array_variable to main conf failed");

      }

      *arg = vcopy;

      logr("before push size %d",ctx->pop->size);
      if(ctx->pop->size == 0)
      {

        ctx->pop->start += 1;
        ctx->pop->end += 1;
        ctx->pop->size = 1;
      }
      else if (ctx->pop->size>0)
      {
        ctx->pop->end += 1;
        ctx->pop->size += 1;
      }
      logr("after push size %d",ctx->pop->size);
      arg=ctx->pop->arr->elts;
      v=arg[ctx->pop->end];
      logr("ctx->pop->arr[%d] is %s",ctx->pop->end,v->data);

    }
  }

  else if (opindex->op == OP_TAIL)
  {

    if(ctx->sft->size==0&&(ctx->pop->size>0)&&ctx->pop->start>0)
    {
      arg=ctx->pop->arr->elts;
      arg[--ctx->pop->start]=vcopy;
      ctx->pop->size++;
      logr("ctx->pop->arr[%d] is %s",ctx->pop->start,vcopy->data);

    }
    else
    {

      arg=ngx_array_push(ctx->sft->arr);
      if (NULL == arg)
      {
        logr("pushing array_variable to main conf failed");
        return;
      }
      *arg = vcopy;
      logr("before unshift sft size %d",ctx->sft->size);
      if(ctx->sft->size == 0)
      {

        ctx->sft->start += 1;
        ctx->sft->end += 1;
        ctx->sft->size = 1;
      }
      else if (ctx->sft->size>0)
      {
        ctx->sft->end += 1;
        ctx->sft->size += 1;
      }
      logr("after unshift sft size %d",ctx->sft->size);
      arg=ctx->sft->arr->elts;
      v=arg[ctx->sft->end];
      logr("ctx->sft->arr[%d] is %s",ctx->sft->end,v->data);
    }
  }

  else if (opindex->op == OP_STR)
  {

    if(NGX_OK != str_to_array(r->pool,v->data,v->len,opindex->del,&str_array,arraysize))
    {
      logr("av set hangdler,str_to_array function err.");
      return;
    }
    item=str_array->elts;
    logr("\n\n\nstr_array 's size is %d \n and opindex->del is %c\n\n",str_array->nelts,opindex->del);

    for(str_loop=0;str_loop<str_array->nelts;str_loop++)
    {
      str_vcopy = (ngx_http_variable_value_t*)ngx_pcalloc(r->pool,sizeof(ngx_http_variable_value_t));/*a new vvt everytime*/
      str_vcopy->data = item[str_loop].data;
      str_vcopy->len = item[str_loop].len;
      str_vcopy->valid = 1;
      str_vcopy->not_found = 0;

      logr("str_vcopy->data ::%s",str_vcopy->data);

      if(ctx->pop->size==0&&(ctx->sft->size>0)&&ctx->sft->start>0)
      {
        arg=ctx->pop->arr->elts;
        arg[--ctx->pop->start]=str_vcopy;
        ctx->sft->size++;
        logr("ctx->sft->arr[%d] is %s",ctx->sft->start,vcopy->data);
      }

      else
      {
        arg = ngx_array_push(ctx->pop->arr);
        logr("set handler :::: begin to set.....,after push array");
        if (NULL == arg)
        {
          logr("pushing array_variable to main conf failed");

        }

        *arg = str_vcopy;

        logr("before push size %d",ctx->pop->size);
        if(ctx->pop->size == 0)
        {

          ctx->pop->start += 1;
          ctx->pop->end += 1;
          ctx->pop->size = 1;
        }
        else if (ctx->pop->size>0)
        {
          ctx->pop->end += 1;
          ctx->pop->size += 1;
        }
        logr("after push size %d",ctx->pop->size);
        arg=ctx->pop->arr->elts;
        v=arg[ctx->pop->end];
        logr("ctx->pop->arr[%d] is %s",ctx->pop->end,v->data);

      }

    }

  }

}

  static void *
ngx_http_create_main_conf(ngx_conf_t *cf)
{
  /*fSTEP;*/

  ngx_http_av_main_conf_t * mcf = ngx_pcalloc(cf->pool,
      sizeof(ngx_http_av_main_conf_t));
  memset(mcf, 0, sizeof(ngx_http_av_main_conf_t));

  mcf->avs = ngx_array_create(cf->pool, 20, sizeof(ngx_http_variable_t*));
  if (NULL == mcf->avs)
  {
    return NULL;
  }

  /* logc("create main config done"); */
  return mcf;
}

  static char*
ngx_http_init_main_conf(ngx_conf_t *cf, void *conf)
{
  return NULL;
}

  static char*
ngx_http_av_in_command(ngx_conf_t *cf, void *conf, ngx_int_t opflag)
{
  ngx_http_rewrite_loc_conf_t *lcf;

  ngx_http_av_op_t *opindex;

  ngx_int_t index;
  ngx_str_t *value;
  ngx_http_variable_t *v;
  ngx_http_script_var_code_t *vcode;
  ngx_http_script_var_handler_code_t *vhcode;

  value = cf->args->elts;

  lcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_rewrite_module);/*modified from rewrite module*/
  if (NULL == lcf)
    logc("lcf null......!");
  if (value[1].data[0] != '$')
  {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid variable name \"%V\"",
        &value[1]);
    return NGX_CONF_ERROR;
  }

  value[1].len--;
  value[1].data++;

  v = ngx_http_add_variable(cf, &value[1], NGX_HTTP_VAR_CHANGEABLE);

  if (v == NULL)
  {
    logc("in cmd...., v err....");
    return NGX_CONF_ERROR;
  }
  opindex = ngx_palloc(cf->pool, sizeof(ngx_http_av_op_t));
  opindex->index = ((ngx_http_av_op_t*) v->data)->index;
  opindex->op = opflag;

  /*for str_to_array*/
  if (cf->args->nelts > 3)
  {
    if (cf->args->nelts != 4)
    {
      logc("err,str_to_array arg num is %d, not 3",cf->args->nelts-1);
      return NGX_CONF_ERROR;
    }

    opindex->del = value[3].data[0];/*just get one char...*/

    logc("::in cmd:: opindex->del is %c ", opindex->del);

  }
  /*  str_to_array ends*/

  index = ngx_http_get_variable_index(cf, &value[1]);
  if (index == NGX_ERROR)
  {
    logc("in cmd...., index err....");
    return NGX_CONF_ERROR;
  }

  if (v->get_handler == NULL && ngx_strncasecmp(value[1].data,
        (u_char *) "http_", 5) != 0 && ngx_strncasecmp(value[1].data,
          (u_char *) "sent_http_", 10) != 0 && ngx_strncasecmp(value[1].data,
            (u_char *) "upstream_http_", 14) != 0)
  {
    v->get_handler = ngx_http_rewrite_var;/*可改动*/
    v->data = index;
  }

  if (ngx_http_rewrite_value(cf, lcf, &value[2]) != NGX_CONF_OK)
  {
    return NGX_CONF_ERROR;
  }

  if (v->set_handler)
  {
    vhcode = ngx_http_script_start_code(cf->pool, &lcf->codes,
        sizeof(ngx_http_script_var_handler_code_t));
    if (vhcode == NULL)
    {
      return NGX_CONF_ERROR;
    }

    vhcode->code = ngx_http_script_var_set_handler_code;
    vhcode->handler = v->set_handler;
    vhcode->data = (uintptr_t) opindex;
    /*logc("in cmd...., set handler code.");*/

    return NGX_CONF_OK;
  }
  /*ngx_http_script_start_code*/
  vcode = ngx_http_script_start_code(cf->pool, &lcf->codes,
      sizeof(ngx_http_script_var_code_t));
  if (vcode == NULL)
  {
    return NGX_CONF_ERROR;
  }

  vcode->code = ngx_http_script_set_var_code;
  vcode->index = (uintptr_t) index;
  return NGX_CONF_OK;
}

  static char*
ngx_http_av_out_command(ngx_conf_t *cf, void *conf, ngx_int_t opflag)
{
  ngx_http_rewrite_loc_conf_t            *rlcf;

  ngx_int_t                               index;
  ngx_int_t                               arrayindex;
  ngx_str_t                              *value;
  ngx_http_variable_t                    *vout;          /*pop $vout $varray; */
  ngx_http_variable_t                    *varray;

  ngx_http_script_var_code_t             *vcode;
  ngx_http_script_var_handler_code_t     *vhcode;
  ngx_http_script_var_get_handler_code_t *varrayhcode;
  ngx_http_av_op_t                       *opindex;

  value = cf->args->elts;

  rlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_rewrite_module);/*modified from rewrite module*/
  if (value[1].data[0] != '$' || value[2].data[0] != '$')
  {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid variable name \"%V\"",
        &value[1]);
    return NGX_CONF_ERROR;
  }

  value[1].len--;
  value[1].data++;

  value[2].len--;
  value[2].data++;

  vout = ngx_http_add_variable(cf, &value[1], NGX_HTTP_VAR_CHANGEABLE);
  varray = ngx_http_add_variable(cf, &value[2], NGX_HTTP_VAR_CHANGEABLE);

  opindex = ngx_palloc(cf->pool, sizeof(ngx_http_av_op_t));
  opindex->index = ((ngx_http_av_op_t*) varray->data)->index;
  opindex->op = opflag;

  if (cf->args->nelts > 3)
  {
    if (cf->args->nelts != 4)
    {
      logc("err,print_array arg num is %d, not 3",cf->args->nelts-1);
      return NGX_CONF_ERROR;
    }

    opindex->joinx = ngx_http_av_readstr(cf, &value[3]);
    if (NULL == opindex->joinx)
      logc("::out cmd:: opindex->joinx is NULL");
    /*logc("joinx.step is %d",opindex->joinx->step);*/
    /* logc("joinx.head.data is %s and len is %d ",opindex->joinx->head.data,opindex->joinx->head.len);*/

  }

  /*logc("vout is %s and varray is %s",value[1].data,value[2].data);*/

  if (vout == NULL || varray == NULL)
  {
    return NGX_CONF_ERROR;
  }

  index = ngx_http_get_variable_index(cf, &value[1]);
  if (index == NGX_ERROR)
  {

    logc("vout index  is NGX_ERROR");
    return NGX_CONF_ERROR;
  }

  arrayindex = ngx_http_get_variable_index(cf, &value[2]);
  if (NGX_ERROR == arrayindex)
  {

    logc("arrayindex is %d",arrayindex);
    return NGX_CONF_ERROR;
  }

  if (vout->get_handler == NULL && ngx_strncasecmp(value[1].data,
        (u_char *) "http_", 5) != 0 && ngx_strncasecmp(value[1].data,
          (u_char *) "sent_http_", 10) != 0 && ngx_strncasecmp(value[1].data,
            (u_char *) "upstream_http_", 14) != 0)
  {
    vout->get_handler = ngx_http_rewrite_var;
    vout->data = index;
    logc("vout.gethandler is null ,and vout is not special var.");
  }

  if (varray->get_handler == NULL && ngx_strncasecmp(value[1].data,
        (u_char *) "http_", 5) != 0 && ngx_strncasecmp(value[1].data,
          (u_char *) "sent_http_", 10) != 0 && ngx_strncasecmp(value[1].data,
            (u_char *) "upstream_http_", 14) != 0)
  {
    varray->get_handler = ngx_http_rewrite_var;
    varray->data = arrayindex;
  }
  else if (varray->get_handler)
  {

    varrayhcode = ngx_http_script_start_code(cf->pool, &rlcf->codes,
        sizeof(ngx_http_script_var_get_handler_code_t));
    if (varrayhcode == NULL)
    {
      /*logc("varrayhcode is NULL in av_out cmd....!!");*/
      return NGX_CONF_ERROR;
    }

    varrayhcode->code = ngx_http_script_var_get_handler_code;
    varrayhcode->handler = varray->get_handler;
    varrayhcode->data = (uintptr_t) opindex;
    /*logc("av_out cmd array get handler code....");*/
  }

  if (vout->set_handler)
  {
    vhcode = ngx_http_script_start_code(cf->pool, &rlcf->codes,
        sizeof(ngx_http_script_var_handler_code_t));
    if (vhcode == NULL)
    {
      return NGX_CONF_ERROR;
    }

    vhcode->code = ngx_http_script_var_set_handler_code;
    vhcode->handler = vout->set_handler;
    vhcode->data = vout->data;
    /*logc("av_out cmd vout set handler code....!!!");*/
    return NGX_CONF_OK;
  }

  vcode = ngx_http_script_start_code(cf->pool, &rlcf->codes,
      sizeof(ngx_http_script_var_code_t));
  if (vcode == NULL)
  {
    return NGX_CONF_ERROR;
  }

  vcode->code = ngx_http_script_set_var_code;
  vcode->index = (uintptr_t) index;

  return NGX_CONF_OK;
}

  static ngx_int_t
ngx_http_rewrite_var(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
  ngx_http_variable_t *var;
  ngx_http_core_main_conf_t *cmcf;
  ngx_http_rewrite_loc_conf_t *rlcf;

  rlcf = ngx_http_get_module_loc_conf(r, ngx_http_rewrite_module);

  if (rlcf->uninitialized_variable_warn == 0)
  {
    *v = ngx_http_variable_null_value;
    return NGX_OK;
  }

  cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

  var = cmcf->variables.elts;

  /*
   * the ngx_http_rewrite_module sets variables directly in r->variables,
   * and they should be handled by ngx_http_get_indexed_variable(),
   * so the handler is called only if the variable is not initialized
   */

  ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
      "using uninitialized \"%V\" variable", &var[data].name);

  *v = ngx_http_variable_null_value;

  return NGX_OK;
}

  static char *
ngx_http_rewrite_value(ngx_conf_t *cf, ngx_http_rewrite_loc_conf_t *lcf,
    ngx_str_t *value)
{
  ngx_int_t n;
  ngx_http_script_compile_t sc;
  ngx_http_script_value_code_t *val;
  ngx_http_script_complex_value_code_t *complex;

  n = ngx_http_script_variables_count(value);/* '$',查看变量数*/

  logc("variable cout %d\n",n);

  if (n == 0)
  {
    val = ngx_http_script_start_code(cf->pool, &lcf->codes,
        sizeof(ngx_http_script_value_code_t));
    if (val == NULL)
    {
      return NGX_CONF_ERROR;
    }

    n = ngx_atoi(value->data, value->len);

    if (n == NGX_ERROR)
    {
      n = 0;
    }

    val->code = ngx_http_script_value_code;
    val->value = (uintptr_t) n;
    val->text_len = (uintptr_t) value->len;
    val->text_data = (uintptr_t) value->data;

    return NGX_CONF_OK;
  }

  complex = ngx_http_script_start_code(cf->pool, &lcf->codes,
      sizeof(ngx_http_script_complex_value_code_t));
  if (complex == NULL)
  {
    return NGX_CONF_ERROR;
  }

  complex->code = ngx_http_script_complex_value_code;
  complex->lengths = NULL;

  ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

  sc.cf = cf;
  sc.source = value;
  sc.lengths = &complex->lengths;
  sc.values = &lcf->codes;
  sc.variables = n;
  sc.complete_lengths = 1;

  if (ngx_http_script_compile(&sc) != NGX_OK)
  {
    return NGX_CONF_ERROR;
  }

  return NGX_CONF_OK;
}

  static void
ngx_http_av_copy_vvt(ngx_http_request_t *r, ngx_http_variable_value_t *to,
    ngx_http_variable_value_t *from)
{
  char *buf = NULL;
  buf = ngx_palloc(r->pool, sizeof(char) * ((from->len) + 1));
  ngx_memcpy(buf,from->data,from->len);
  to->data = (u_char*) buf;
  to->len = from->len;
  to->valid = from->valid;
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "from data %s",from->data);
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "to data %s",to->data);
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "from len is %d", from->len);
}

  static ngx_http_av_joinx_t *
ngx_http_av_readstr(ngx_conf_t *cf, ngx_str_t * str)
{
  ngx_http_av_joinx_t *joinx;
  ngx_str_t temp;
  char tok;
  size_t i = 0;
  ngx_int_t plen = 0;
  ngx_int_t nelts = 0;/*for '$'*/
  ngx_str_t nullstr =
      ngx_null_string;
  ngx_str_t ldeltemp;
  ngx_str_t *d;
  char *ldelbuf;

  joinx = ngx_pcalloc(cf->pool, sizeof(ngx_http_av_joinx_t));

  temp.data = ngx_pcalloc(cf->pool, str->len + 1);

  joinx->dels = ngx_array_create(cf->pool, 2, sizeof(ngx_str_t));

  tok = str->data[i];
  if (tok == '{')
    joinx->head = nullstr;
  else
  {
    do
    {

      logc("tok is %c",tok);
      if (tok == '\\')
      {

        i++;
        tok = str->data[i];

      }
      temp.data[plen] = tok;

      plen++;

      i++;
      tok = str->data[i];

    }
    while (tok != '{' && i < str->len);

  }
  logc("after head tok is %c",tok);

  joinx->head.data = ngx_palloc(cf->pool, plen + 1);
  ngx_memcpy(joinx->head.data,temp.data,plen);/*head ok*/
  joinx->head.len = plen;

  plen = 0;

  i++;
  tok = str->data[i];

  logc("tok should be ( is %c",tok);

  if (tok != '(')
  {
    logc("the elts format wrong....");
    return NULL;
  }

  i++;
  tok = str->data[i];
  logc("tok is %c",tok);

  if (tok == '$')
    joinx->lhead = nullstr;
  else
  {

    do
    {
      /* logc("tok is %c",tok);*/
      if (tok == '\\')
      {
        i++;
        tok = str->data[i];

      }
      temp.data[plen] = tok;
      plen++;
      i++;
      tok = str->data[i];

    }
    while (tok != '$' && i < str->len);

    joinx->lhead.data = ngx_palloc(cf->pool, plen + 1);
    ngx_memcpy(joinx->lhead.data,temp.data,plen);/*head ok*/
    joinx->lhead.len = plen;

  }

  logc("tok after lhead shoud be $ is %c",tok);
  plen = 0;
  i++;
  nelts++;/*$ ++*/
  tok = str->data[i]; /*pass '$'*/

  logc("tok is %c",tok);

  /*to find the dels and ltail.
   *loop to find ldels and ltail, maybe null(empty)
   *
   */

  if (tok == ')')
  {
    /*ltail & ldel empty*/
    /*set empty*/
    joinx->ltail = nullstr;
    d = ngx_array_push(joinx->dels);
    *d = nullstr;
  }

  while (tok != ')')
  {
    plen = 0;
    while (tok != '$' && tok != ')' && i < str->len)
    {
      /*logc("tok is %c",tok);*/

      if (tok == '\\')
      {

        i++;
        tok = str->data[i];

      }
      temp.data[plen] = tok;
      plen++;
      i++;
      tok = str->data[i];

    }

    if (tok == '$')
    {

      nelts++;/*number of'$' */
      logc("nelts is %d", nelts);

      d = ngx_array_push(joinx->dels);
      if (plen == 0)
      {
        *d = nullstr;
      }
      else
      {
        ldelbuf = ngx_pcalloc(cf->pool, plen + 1);
        ngx_memcpy(ldelbuf,temp.data,plen);
        ldeltemp.data = (u_char*) ldelbuf;
        ldeltemp.len = plen;
        *d = ldeltemp;
      }

      i++;
      tok = str->data[i];

    }
    else if (')' == tok)
    {
      /* ltail*/
      if (plen == 0)
      {
        joinx->ltail = nullstr;
      }
      else
      {
        joinx->ltail.data = ngx_palloc(cf->pool, plen + 1);
        ngx_memcpy(joinx->ltail.data,temp.data,plen);
        joinx->ltail.len = plen;
      }

      break;

    }

  }

  /* logc("after check $ elts......");*/

  plen = 0;
  i++;
  tok = str->data[i]; /*pass ')'*/

  /*del....*/

  if ('?' == tok)
  {

    joinx->ldel = nullstr;

  }

  while (tok != '?' && i < str->len)
  {
    if (tok == '\\')
    {
      i++;
      tok = str->data[i];
    }
    temp.data[plen] = tok;
    plen++;
    i++;
    tok = str->data[i];

  }
  joinx->ldel.data = ngx_palloc(cf->pool, plen + 1);
  ngx_memcpy(joinx->ldel.data,temp.data,plen);
  joinx->ldel.len = plen;

  plen = 0;
  i++;
  tok = str->data[i]; /*pass '?'*/
  if ('}' != tok)
  {
    logc("the } format wrong....");
    return NULL;
  }

  /*tail*/
  i++;
  tok = str->data[i];

  if ('\0' == tok)
  {
    joinx->tail = nullstr;
    joinx->step = nelts;
    return joinx; /*set step*/
  }
  while ('\0' != tok)
  {
    if (tok == '\\')
    {
      i++;
      tok = str->data[i];
    }
    temp.data[plen] = tok;
    plen++;
    i++;
    tok = str->data[i];

  }
  joinx->tail.data = ngx_palloc(cf->pool, plen + 1);/*tail malloc*/

  ngx_memcpy(joinx->tail.data,temp.data,plen);
  joinx->tail.len = plen;
  joinx->step = nelts; /*set step*/

  return joinx;

}

/*added for array_var*/
  void
ngx_http_script_var_get_handler_code(ngx_http_script_engine_t *e)
{
  ngx_http_script_var_get_handler_code_t *code;

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
      "\n\n http script get var handler \n\n");

  code = (ngx_http_script_var_get_handler_code_t *) e->ip;

  e->ip += sizeof(ngx_http_script_var_get_handler_code_t);

  if(NGX_OK == code->handler(e->request, e->sp, code->data))
    fprintf(stderr,"call get handler ok........\n\n");
  else fprintf(stderr,"call get hander err.....\n\n");
  /*fprintf(stderr,"\nscript get handler e->sp address is %X \n",(e->sp));*/
  e->sp ++;/**/


}

