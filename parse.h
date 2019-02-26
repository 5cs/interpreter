#ifndef LEPT_LISP__
#define LEPT_LISP__

#include <stddef.h>
#include <assert.h>

enum value_type {
  LISP_NULL,
  LISP_TRUE,
  LISP_FALSE,
  LISP_NUMBER,
  LISP_PLUS,
  LISP_MINUS,
  LISP_MULTIPLY,
  LISP_DIVIDE,
  LISP_LT,
  LISP_BT,
  LISP_EQ,
  LISP_DEFINE,
  LISP_LAMBDA,
  LISP_CAR,
  LISP_CDR,
  LISP_CONS,
  LISP_QUOTE,
  LISP_LIST,
  LISP_NULL$,
  LISP_IF,
  LISP_NOT,
  LISP_SYMBOL,
  LISP_NIL
};

typedef struct lisp_value lisp_value;

struct lisp_value {
  union {
    struct { lisp_value* e; size_t size; }a;
    struct { char* s; size_t size; }sym;
    double n;
  }u;
  int type;
};

enum parse_state {
  LISP_PARSE_OK,
  LISP_PARSE_INVALID_VALUE,
  LISP_PARSE_INVALID_NUMBER,
  LISP_PARSE_NUMBER_TOO_BIG,
  LISP_PARSE_ROOT_NOT_SINGULAR,
  LISP_PARSE_MISS_CLOSE_PRAN,
  LISP_PARSE_NULL
};

#define lisp_value_init(v) \
  do { \
    (v)->type = LISP_NULL; 	\
    (v)->u.a.e = NULL;	 	\
    (v)->u.a.size = 0;	 	\
  } while(0)

void lisp_value_free(lisp_value* v);

int lisp_parse(lisp_value* v, const char* code);

int lisp_get_type(const lisp_value* v);

double lisp_get_number(const lisp_value* v);

size_t lisp_get_list_size(const lisp_value* v);
lisp_value* lisp_get_list_element(const lisp_value* v, size_t index);

char* lisp_get_string(const lisp_value* v);
size_t lisp_get_string_length(const lisp_value* v);

char* lisp_stringfy(const lisp_value* v);

#endif
