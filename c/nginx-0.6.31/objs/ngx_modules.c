
#include <ngx_config.h>
#include <ngx_core.h>



extern ngx_module_t  ngx_core_module;
extern ngx_module_t  ngx_errlog_module;
extern ngx_module_t  ngx_conf_module;
extern ngx_module_t  ngx_events_module;
extern ngx_module_t  ngx_event_core_module;
extern ngx_module_t  ngx_epoll_module;
extern ngx_module_t  ngx_http_module;
extern ngx_module_t  ngx_http_core_module;
extern ngx_module_t  ngx_http_log_module;
extern ngx_module_t  ngx_http_upstream_module;
extern ngx_module_t  ngx_http_static_module;
extern ngx_module_t  ngx_http_index_module;
extern ngx_module_t  ngx_http_rewrite_module;
extern ngx_module_t  ngx_http_proxy_module;
extern ngx_module_t  ngx_http_upstream_hash_module;
extern ngx_module_t  ngx_http_empty_module;
extern ngx_module_t  ngx_http_repl_module;
extern ngx_module_t  ngx_http_rv_module;
extern ngx_module_t  ngx_http_vwrite_module;
extern ngx_module_t  ngx_http_rand_module;
extern ngx_module_t  ngx_http_time_module;
extern ngx_module_t  ngx_http_list_module;
extern ngx_module_t  ngx_http_memcached_more_module;
extern ngx_module_t  ngx_http_write_filter_module;
extern ngx_module_t  ngx_http_header_filter_module;
extern ngx_module_t  ngx_http_chunked_filter_module;
extern ngx_module_t  ngx_http_range_header_filter_module;
extern ngx_module_t  ngx_http_postpone_filter_module;
extern ngx_module_t  ngx_http_ssi_filter_module;
extern ngx_module_t  ngx_http_headers_filter_module;
extern ngx_module_t  ngx_http_copy_filter_module;
extern ngx_module_t  ngx_http_range_body_filter_module;
extern ngx_module_t  ngx_http_not_modified_filter_module;

ngx_module_t *ngx_modules[] = {
    &ngx_core_module,
    &ngx_errlog_module,
    &ngx_conf_module,
    &ngx_events_module,
    &ngx_event_core_module,
    &ngx_epoll_module,
    &ngx_http_module,
    &ngx_http_core_module,
    &ngx_http_log_module,
    &ngx_http_upstream_module,
    &ngx_http_static_module,
    &ngx_http_index_module,
    &ngx_http_rewrite_module,
    &ngx_http_proxy_module,
    &ngx_http_upstream_hash_module,
    &ngx_http_empty_module,
    &ngx_http_repl_module,
    &ngx_http_rv_module,
    &ngx_http_vwrite_module,
    &ngx_http_rand_module,
    &ngx_http_time_module,
    &ngx_http_list_module,
    &ngx_http_memcached_more_module,
    &ngx_http_write_filter_module,
    &ngx_http_header_filter_module,
    &ngx_http_chunked_filter_module,
    &ngx_http_range_header_filter_module,
    &ngx_http_postpone_filter_module,
    &ngx_http_ssi_filter_module,
    &ngx_http_headers_filter_module,
    &ngx_http_copy_filter_module,
    &ngx_http_range_body_filter_module,
    &ngx_http_not_modified_filter_module,
    NULL
};

