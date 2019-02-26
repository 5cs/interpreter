#include <stdio.h>	// sprintf()
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "parse.h"

#define EXPECT(c, ch)		do { assert((*c->code)==(ch)); c->code++; } while(0)
#define ISDIGIT(ch) 		((ch)>='0' && (ch)<='9')
#define ISDIGIT1TO9(ch)		((ch)>='1' && (ch)<='9')
#define ISVALIDSYMBOL(ch)	(((ch)>='a' && (ch)<='z') || ((ch)>='A' && ((ch)<='Z')) || (ch)=='-' || (ch)=='_')

#ifndef LISP_PARSE_INIT_STACK_SIZE
#define LISP_PARSE_INIT_STACK_SIZE 1024
#endif
#define PUTC(c, ch)			do { *(char*)lisp_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTV(c, v)			do { *(lisp_value*)lisp_context_push(c, sizeof(lisp_value)) = (v); } while(0)

typedef struct lisp_context lisp_context;
struct lisp_context {
  const char* code;
  char* stack;
  size_t top, size;
};

static void lisp_context_init(lisp_context* c, const char * code) {
  assert(c != NULL);
  c->code = code;
  c->stack = NULL;
  c->top = 0;
  c->size = 0;
}

static void* lisp_context_push(lisp_context* c, size_t size) {
  void* ret;
  assert(size > 0);
  if(c->top + size >= c->size) {
    if(c->size == 0)
      c->size = LISP_PARSE_INIT_STACK_SIZE;
    while(c->top + size >= c->size)
      c->size += c->size >> 1;
    c->stack = (char*)realloc(c->stack, c->size);
  }
  ret = c->stack + c->top;
  c->top += size;
  return ret;
}

static void* lisp_context_pop(lisp_context* c, size_t size) {
  assert(c->top >= size);
  return c->stack + (c->top -= size);
}

static void lisp_parse_whitespace(lisp_context* c) {
  const char* p = c->code;
  while(*p && (*p==' ' || *p=='\r' || *p=='\t' || *p=='\n'))
    p++;
  c->code = p;
}

static int lisp_parse_operator(lisp_context* c, lisp_value* v, char ch, int type) {
  EXPECT(c, ch);
  v->type = type;
  return LISP_PARSE_OK;
}

static int lisp_parse_literal(lisp_context* c, lisp_value* v, const char* literal, int type) {
  size_t i;
  EXPECT(c, literal[0]);
  for(i = 0; literal[i+1]; i++) {
    if(c->code[i] != literal[i+1])
      return LISP_PARSE_INVALID_VALUE;
  }
  c->code += i;
  v->type = type;
  return LISP_PARSE_OK;
}

static int lisp_parse_list_op(lisp_context* c, lisp_value* v) {
  EXPECT(c, 'c');
  size_t i;
  char* s = NULL;
  int type;
  switch(*c->code) {
    case 'a': s = "ar"; type = LISP_CAR; break;
    case 'd': s = "dr"; type = LISP_CDR; break;
    case 'o': s = "ons"; type = LISP_CONS; break;	// TODO: add list support
    default : return LISP_PARSE_INVALID_VALUE;
  }
  for(i = 0; s[i]; i++) {
    if(c->code[i] != s[i])
      return LISP_PARSE_INVALID_VALUE;
  }
  v->type = type;
  c->code += i;
  return LISP_PARSE_OK;
}

static int lisp_parse_number(lisp_context* c, lisp_value* v) {
  const char* p = c->code;
  if (*p == '-') p++;
  if (*p == '0') p++;
  else {
    if (!ISDIGIT1TO9(*p)) return LISP_PARSE_INVALID_VALUE;
    for (p++; ISDIGIT(*p); p++);
  }
  if (*p == '.') {
    p++;
    if (!ISDIGIT(*p)) return LISP_PARSE_INVALID_VALUE;
    for (p++; ISDIGIT(*p); p++);
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    if (*p == '+' || *p == '-') p++;
    if (!ISDIGIT(*p)) return LISP_PARSE_INVALID_VALUE;
    for (p++; ISDIGIT(*p); p++);
  }
  errno = 0;
  v->u.n = strtod(c->code, NULL);
  if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
    return LISP_PARSE_NUMBER_TOO_BIG;
  v->type = LISP_NUMBER;
  c->code = p;
  return LISP_PARSE_OK;
}


static int lisp_parse_string(lisp_context* c, lisp_value* v) {
  assert(ISVALIDSYMBOL(*c->code));
  size_t size = 0;
  const char* p = c->code;
  while(*p != ' ' && *p != ')')
    PUTC(c, *p++);
  size = p - c->code;
  v->u.sym.size = size;
  v->type = LISP_SYMBOL;
  memcpy((v->u.sym.s = (char*)malloc(size+1)), (char*)lisp_context_pop(c, size), size);
  c->code = p;
  return LISP_PARSE_OK;
}

static int lisp_parse_value(lisp_context* c, lisp_value* v);

static int lisp_parse_list(lisp_context* c, lisp_value* v) {
  size_t size;
  int ret;
  lisp_value e;
  EXPECT(c, '(');

  lisp_parse_whitespace(c);
  if(*c->code == ')') {
    c->code++;
    v->type = LISP_LIST;
    v->u.a.size = 0;
    v->u.a.e = NULL;
    return LISP_PARSE_OK;
  }

  if(*c->code == '\0') {
    lisp_context_pop(c, c->top);
    return LISP_PARSE_MISS_CLOSE_PRAN;
  }

  size = 0;
  for(;;) {
    lisp_value_init(&e);
    if((ret = lisp_parse_value(c, &e)) != LISP_PARSE_OK) {
      lisp_context_pop(c, c->top);
      break;
    }
    PUTV(c, e);
    size++;
    lisp_parse_whitespace(c);
    if(*c->code == ')') {
      c->code++;
      lisp_parse_whitespace(c);
      v->type = LISP_LIST;
      v->u.a.size = size;
      size *= sizeof(lisp_value);
      memcpy((v->u.a.e = (lisp_value*)malloc(size)), (lisp_value*)lisp_context_pop(c, size), size);
      ret = LISP_PARSE_OK;
      break;
    }
    if(*c->code == '\0') {
      lisp_context_pop(c, c->top);
      ret = LISP_PARSE_MISS_CLOSE_PRAN;
      break;
    }
  }

  return ret;
}

// recursive root
static int lisp_parse_value(lisp_context* c, lisp_value* v) {
  int ret, a = 0;
  assert(c != NULL && v != NULL);
  switch(*c->code) {
    case '+': return lisp_parse_operator(c, v, '+', LISP_PLUS);
    case '-':
              if(*(c->code+1) == ' ') 	return lisp_parse_operator(c, v, '-', LISP_MINUS);
              else                 		return lisp_parse_number(c, v);
    case '*': return lisp_parse_operator(c, v, '*', LISP_MULTIPLY);
    case '/': return lisp_parse_operator(c, v, '/', LISP_DIVIDE);
    case '<': return lisp_parse_operator(c, v, '<', LISP_LT);
    case '>': return lisp_parse_operator(c, v, '>', LISP_BT);
    case '=': return lisp_parse_operator(c, v, '=', LISP_EQ);
    case '(': return lisp_parse_list(c, v);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': return lisp_parse_number(c, v);
    case 'd': if((ret = lisp_parse_literal(c, v, "define", LISP_DEFINE)) == LISP_PARSE_OK) return ret; a = 1; break;
    case 'l': if((ret = lisp_parse_literal(c, v, "lambda", LISP_LAMBDA)) == LISP_PARSE_OK) return ret; a = 1; break;
    case 'i': if((ret = lisp_parse_literal(c, v, "if", LISP_IF)) == LISP_PARSE_OK) return ret; a = 1; break;
    case 'n': 
                if(*(c->code+1) == 'o')
                { if((ret = lisp_parse_literal(c, v, "not", LISP_NOT)) == LISP_PARSE_OK) return ret; a = 1; break; }
                else
                { if((ret = lisp_parse_literal(c, v, "null?", LISP_NULL$)) == LISP_PARSE_OK) return ret; a = 1; break; }
    case 'q': if((ret = lisp_parse_literal(c, v, "quote", LISP_QUOTE)) == LISP_PARSE_OK) return ret; a = 1; break;
    case 'c': if((ret = lisp_parse_list_op(c, v)) == LISP_PARSE_OK) return ret; break;
    default : ;	// TODO: add procedure definition supports
  }
  c->code -= a;
  return lisp_parse_string(c, v);
}

int lisp_parse(lisp_value* v, const char* code) {
  int ret;
  lisp_context c;
  lisp_context_init(&c, code);
  lisp_parse_whitespace(&c);
  if((ret = lisp_parse_value(&c, v)) == LISP_PARSE_OK) {
    lisp_parse_whitespace(&c);
    if(*c.code != '\0') {
      v->type = LISP_NULL;
      return LISP_PARSE_ROOT_NOT_SINGULAR;
    }
  }
  assert(c.top == 0);
  return ret;
}

void lisp_value_free(lisp_value* v) {
  assert(v != NULL);
  size_t i;
  switch(v->type) {
    case LISP_LIST:
      for(i = 0; i < lisp_get_list_size(v); i++) {
        lisp_value_free(&v->u.a.e[i]);
      }
      free(v->u.a.e);
      break;
    case LISP_SYMBOL:
      free(v->u.sym.s);
      break;
    default: ;
  }
  v->type = LISP_NULL;
}

int lisp_get_type(const lisp_value* v) {
  assert(v != NULL);
  return v->type;
}

double lisp_get_number(const lisp_value* v) {
  assert(v != NULL && v->type == LISP_NUMBER);
  return v->u.n;
}

size_t lisp_get_list_size(const lisp_value* v) {
  assert(v != NULL && v->type == LISP_LIST);
  return v->u.a.size;
}

lisp_value* lisp_get_list_element(const lisp_value* v, size_t index) {
  assert(v != NULL && v->type == LISP_LIST);
  assert(index < v->u.a.size);
  return &v->u.a.e[index];
}

char* lisp_get_string(const lisp_value* v) {
  assert(v != NULL && v->type == LISP_SYMBOL);
  return v->u.sym.s;
}

size_t lisp_get_string_length(const lisp_value* v) {
  assert(v != NULL && v->type == LISP_SYMBOL);
  return v->u.sym.size;
}

static void lisp_stringfy_number(lisp_context* c, const lisp_value* v) {
  assert(v != NULL && c != NULL && v->type == LISP_NUMBER);
  size_t i = 0;
  char buf[32];
  sprintf(buf, "%.0lf", v->u.n);
  while(buf[i])
    PUTC(c, buf[i++]);
}

static void lisp_stringfy_value(lisp_context* c, const lisp_value* v) {
  size_t i;
  switch(lisp_get_type(v)) {
    case LISP_PLUS:        PUTC(c, '+'); break;
    case LISP_MINUS:	PUTC(c, '-'); break;
    case LISP_MULTIPLY: PUTC(c, '*'); break;
    case LISP_DIVIDE: 	PUTC(c, '/'); break;
    case LISP_LT:         PUTC(c, '<'); break;
    case LISP_BT:         PUTC(c, '>'); break;
    case LISP_EQ:         PUTC(c, '='); break;
    case LISP_TRUE: 	PUTC(c, '1'); break;
    case LISP_FALSE: 	PUTC(c, '0'); break;
    case LISP_NUMBER: 	lisp_stringfy_number(c, v); break;
    case LISP_DEFINE: 	memcpy((char*)lisp_context_push(c, 6), "define", 6); break;
    case LISP_LAMBDA: 	memcpy((char*)lisp_context_push(c, 6), "lambda", 6); break;
    case LISP_IF:         memcpy((char*)lisp_context_push(c, 2), "if", 	 2); break;
    case LISP_NOT:         memcpy((char*)lisp_context_push(c, 3), "not", 	 3); break;
    case LISP_CAR:         memcpy((char*)lisp_context_push(c, 3), "car", 	 3); break;
    case LISP_CDR:         memcpy((char*)lisp_context_push(c, 3), "cdr", 	 3); break;
    case LISP_QUOTE:	memcpy((char*)lisp_context_push(c, 5), "quote",  5); break;
    case LISP_NULL$:	memcpy((char*)lisp_context_push(c, 5), "null?",  5); break;
    case LISP_SYMBOL:	memcpy((char*)lisp_context_push(c, v->u.sym.size), v->u.sym.s, v->u.sym.size); break;

    case LISP_LIST:
                      PUTC(c, '(');
                      for(i = 0; i < lisp_get_list_size(v); i++) {
                        lisp_stringfy_value(c, &v->u.a.e[i]);
                        PUTC(c, ' ');
                      }
                      if(c->stack[c->top-1] == ' ')
                        lisp_context_pop(c, 1);
                      PUTC(c, ')');
                      break;
  }
}

char* lisp_stringfy(const lisp_value* v) {
  lisp_context c;
  lisp_context_init(&c, NULL);    // unintialized context c makes me upset. remember this!
  lisp_stringfy_value(&c, v);
  PUTC(&c, '\0');
  return c.stack;
}
