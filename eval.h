#ifndef LEPT_EVAL__
#define LEPT_EVAL__
#include "parse.h"

enum {
  LISP_EVAL_OK,
  LISP_EVAL_INVALID_VALUE,
  LISP_EVAL_UNKNOWN_BIN_OP,
  LISP_EVAL_ENV_EXTENED_OK,
  LISP_EVAL_VARIABLE_NOT_FOUND,
  LISP_LISP_OP_ILLEAGE
};

typedef struct lisp_value_pair lisp_value_pair;
struct lisp_value_pair {
  lisp_value *symbol, *value;
};

typedef struct env env_t;
struct env {
  env_t *prev, *next;
  struct {
    lisp_value_pair* p;
    size_t top, size;
  }s;
};

env_t global_env;

int lisp_eval(lisp_value* v, lisp_value* result, env_t* e);

#if 1
lisp_value* car(lisp_value* c, lisp_value* v);
lisp_value* cdr(lisp_value* c, lisp_value* v);
#endif

lisp_value car0(lisp_value c);
lisp_value cdr0(lisp_value c);

void env_init(env_t* p, env_t* e);
void env_free(env_t* e);
size_t env_size(env_t* e);
void lisp_env_print(env_t* e);

#endif
