#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include <pcre.h>

#include "ngx_sdb.h"


#define log(x...) do {fprintf(stderr, x); fprintf(stderr, "\n");} while(0)

int main(int argv, char **args){
  char *str = "&AttributeName.0=Color&AttributeName.1=Size";
  char *ptn = rg_AttributeName;

  int slen = strlen(str);
  /* int ptnlen = strlen(ptn); */

  pcre *re;

  int v[30];
  const char *err;
  int erroffset;




  re = pcre_compile(
      ptn, 
      0, 
      &err, 
      &erroffset, 
      NULL);

  log("re=%p", re);

  int rc;
  rc = pcre_exec(
      re, 
      NULL, 
      str, 
      slen, 
      0, 
      0, 
      v, 
      30
      );

  
  log("all:%d", v[0]);
  int i;
  char *s;
  /* char *e; */
  int  l;
  for (i= 0; i < rc; ++i){
    l = v[2*i+1] - v[2*i];
    s = str + v[2*i];
    log("%2d: %.*s\n", i, l, s);
  }
  return 0;
}
