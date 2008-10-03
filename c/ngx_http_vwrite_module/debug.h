#define LOG_TO_STDERR 1


/* #define IS_FATAL 1 */
/* #define IS_ERROR 1 */
/* #define IS_DEBUG 1 */
/* #define IS_STEP  1 */
/* #define IS_TRACE 1 */



#undef IS_TEST_MODE
#if IS_TRACE || IS_STEP || IS_DEBUG
#	define IS_TEST_MODE 1
#endif


/* #define IS_PRINT  1 */

#ifdef LOG_TO_STDERR
#   define _SN_LOG_TO(FMT ...)  {fprintf(stderr, FMT);}
#else
#   define _SN_LOG_TO(FMT ...)  {printf(FMT);}
#endif

/* #define LOG_FORMAT(type, ARG ...) _SN_LOG_TO("%10s %5u %20.20s\n ", __FILE__, __LINE__, __FUNCTION__) _SN_LOG_TO(type) _SN_LOG_TO(ARG) _SN_LOG_TO("\n") */
/* #define LOG_FORMAT(type, ARG ...) _SN_LOG_TO("%5u %20.20s ", __LINE__, __FUNCTION__) _SN_LOG_TO(type) _SN_LOG_TO(ARG) _SN_LOG_TO("\n") */
#define LOG_FORMAT(type, ARG ...)  _SN_LOG_TO(type) _SN_LOG_TO(ARG) _SN_LOG_TO("\n")



#ifdef IS_FATAL
#   define FATAL(ARG ...) LOG_FORMAT("FATAL     : ", ARG)
#else
#   define FATAL(ARG ...) 
#endif

#ifdef IS_ERROR
#   define ERROR(ARG ...) LOG_FORMAT("ERROR!!!! : ", ARG)
    /* static char _error_buf[4096] = {0}; */
#    define nsERROR(fmt, ns) {snprintf(_error_buf, (ns)->len + 1, "%s", (ns)->data); ERROR(fmt, _error_buf)}
#else
#   define ERROR(ARG ...) 
#   define nsERROR(fmt, ns)
#endif

#ifdef IS_DEBUG
#   define DEBUG(ARG ...) LOG_FORMAT("---debug  : ", ARG)
#else
#   define DEBUG(ARG ...) 
#endif

#ifdef IS_STEP
#   define STEP(ARG ...)  LOG_FORMAT(">>>step   : ", ARG)
#else
#   define STEP(ARG ...) 
#endif

#ifdef IS_TRACE
#   define TRACE(ARG ...) LOG_FORMAT("            ", ARG)
    /* static char _trace_buf[4096] = {0}; */
    /**
     * short cuts
     */
#    define sTRACE(n) TRACE(#n"= %20.20s", (n))
#    define dTRACE(n) TRACE(#n"= %d",      (n))
#    define lTRACE(n) TRACE(#n"= %ld",     (n))
#    define nsTRACE(fmt, ns) {snprintf(_trace_buf, (ns)->len + 1, "%s", (ns)->data); TRACE(fmt, _trace_buf)}
#else
#   define TRACE(ARG ...) 
#   define sTRACE(n)
#   define dTRACE(n)
#   define lTRACE(n)
#   define nsTRACE(fmt, ns)
#endif



#ifdef IS_PRINT
#   define PRINT(coll, ARG ...)   _SN_LOG_TO("%20s %6u %25s ", __FILE__, __LINE__, __FUNCTION__)\
                                  _SN_LOG_TO("PRINT : ") _SN_LOG_TO(ARG) _SN_LOG_TO("\n")\
                                  coll_print(coll);
#else
#   define PRINT(ARG ...) 
#endif






#define fSTEP STEP("------------------------------------------------------------ %s", __FUNCTION__)
 

/* for nginx */
/* trace ngx_table_elt_t */
#define TR_TBL(k) TRACE(#k" : key=%10.10s", (k)->key.data); TRACE("value=%10.10s", (k)->value.data);


#ifdef IS_DEBUG
#   define RUN_DEBUG(code) code
#else
#   define RUN_DEBUG(code)
#endif

