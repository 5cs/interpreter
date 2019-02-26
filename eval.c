#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "parse.h"
#include "eval.h"

#ifndef LISP_EVAL_INIT_STACK_SIZE
#define LISP_EVAL_INIT_STACK_SIZE 1024
#endif
#ifndef LISP_EVAL_ENV_STACK_SIZE
#define LISP_EVAL_ENV_STACK_SIZE 1024
#endif

typedef struct eval_context eval_context;
struct eval_context {
  char* stack;
  size_t size, top;
};
eval_context eval_stack;
eval_context eval_tmp_variables;

#define PUTV(v) 	do { *(lisp_value*)eval_context_push(&eval_stack, sizeof(lisp_value)) = (v); } while(0)
#define LINKTO(v)	do { *(lisp_value**)eval_context_push(&eval_tmp_variables, sizeof(lisp_value*)) = (v); } while(0)

static void eval_context_init() {
  eval_stack.stack = NULL;
  eval_stack.size = 0;
  eval_stack.top = 0;
}

static void* eval_context_push(eval_context* c, size_t size) {
  assert(c != NULL);
  void* ret;
  if(c->top + size >= c->size) {
    if(c->size == 0)
      c->size = LISP_EVAL_INIT_STACK_SIZE;
    while(c->top + size >= c->size)
      c->size += c->size >> 1;
    c->stack = (char*)realloc(c->stack, c->size);
  }
  ret = c->stack + c->top;
  c->top += size;
  return ret;
}

static void* eval_context_pop(eval_context* c, size_t size) {
  assert(c != NULL && c->size >= size);
  return c->stack + (c->top -= size);
}

void env_init(env_t* p, env_t* e) {
  e->prev = p;
  e->next = NULL;
  e->s.p = NULL;
  e->s.size = 0;
  e->s.top = 0;
}

// TODO: free allocated temp environmental values
void env_free(env_t* e) {
  for(; e != NULL; e = e->next)
    free(e->s.p);
}

static void lisp_free_tmp_variable(eval_context* tmp_stack) {
  size_t i, size = tmp_stack->top / sizeof(lisp_value*);
  lisp_value** p = (lisp_value**)tmp_stack->stack;
  char* s;
  if(lisp_get_type(p[size-1]) == LISP_LIST) size--;
  printf("# of variable: %zu\n", size);
  for(i = 0; i < size; i++) {
    s = lisp_stringfy(p[i]);
    printf("# %zu value is: %s\n", i, s);
    free(s);
    lisp_value_free(p[i]);
    free(p[i]);
  }
}

static void* lisp_env_push(env_t* e, size_t size) {
  assert(e != NULL);
  void* ret;
  if(e->s.top + size >= e->s.size) {
    if(e->s.size == 0)
      e->s.size = LISP_EVAL_ENV_STACK_SIZE;
    while(e->s.top + size >= e->s.size)
      e->s.size += e->s.size >> 1;
    e->s.p = (lisp_value_pair*)realloc(e->s.p, e->s.size);
  }
  ret = e->s.p + e->s.top/sizeof(lisp_value_pair);    // e->s.p's type matters. WTF.
  e->s.top += size;
  return ret;
}

static void* lisp_env_pop(env_t* e, size_t size) {
  assert(e != NULL && e->s.size >= size);
  return e->s.p + (e->s.top -= size)/sizeof(lisp_value_pair);
}

void lisp_env_print(env_t* e) {
  size_t i;
  char *symbol, *value;
  for(i = 0; i < e->s.top/sizeof(lisp_value_pair); i++) {
    printf("#%zu ", i);
    if(e->s.p[i].symbol != NULL) symbol = lisp_stringfy(e->s.p[i].symbol);
    if(e->s.p[i].value != NULL) value = lisp_stringfy(e->s.p[i].value);
    printf("symbol: %s => value: %s\n", symbol, value);
    free(symbol); free(value);
  }
}

void lisp_print_value(lisp_value* v) {
  char* s = lisp_stringfy(v);
  printf("%s\n", s);
  free(s);
}

static int lisp_is_lambda_or_quote(lisp_value* v) {
  assert(lisp_get_type(v) == LISP_LIST);
  lisp_value* p = lisp_get_list_element(v, 0);
  return lisp_get_type(p) == LISP_LAMBDA || lisp_get_type(p) == LISP_QUOTE;
}

static int lisp_copy_list(lisp_value* dst, lisp_value* src, size_t size) {
  assert(lisp_get_type(src) == LISP_LIST);
  size_t i;
  lisp_value* p;
  dst->type = LISP_LIST;
  dst->u.a.size = size;
  dst->u.a.e = (lisp_value*)malloc(size * sizeof(lisp_value));
  for(i = 0; i < size; i++) {
    p = lisp_get_list_element(src, i);
    if(lisp_get_type(p) == LISP_LIST) {
      lisp_copy_list(&(dst->u.a.e[i]), p, lisp_get_list_size(p));
    }
    else dst->u.a.e[i] = *p;	// perform deep copy.
  }
  return LISP_EVAL_OK;
}

lisp_value car0(lisp_value c) {
  assert(lisp_get_type(&c) == LISP_LIST);
  return c.u.a.e[0];
}

lisp_value cdr0(lisp_value c) {
  assert(lisp_get_type(&c) == LISP_LIST && lisp_get_list_size(&c)>=1);
  lisp_value v;
  v.u.a.e = c.u.a.e + 1;
  v.u.a.size = c.u.a.size - 1;
  v.type = v.u.a.size != 0 ? LISP_LIST : LISP_NIL;
  return v;
}

static int lisp_eval_value(lisp_value v, env_t* e);
static int lisp_eval_symbol(lisp_value v, env_t* e);

static int lisp_eval_number(lisp_value v) {
  assert(v.type == LISP_NUMBER);
  PUTV(v);
  return LISP_EVAL_OK;
}

static int lisp_eval_bin_op(lisp_value v, int type, env_t* e) {
  lisp_value* oprans;
  int ret;
  size_t i, count = lisp_get_list_size(&v) - 1;
  lisp_value dummy;
  double tmp = 0;
  for(dummy = cdr0(v); lisp_get_type(&dummy) != LISP_NIL; dummy = cdr0(dummy)) {    // TODO: change to iter form
    if((ret = lisp_eval_value(car0(dummy), e)) != LISP_EVAL_OK)
      return ret;
  }
  oprans = (lisp_value*)eval_context_pop(&eval_stack, count*sizeof(lisp_value));
  tmp = lisp_get_number(oprans);
  switch(type) {
    case LISP_PLUS:
      for(i = 1; i < count; i++)
        tmp += lisp_get_number(&oprans[i]);
      break;
    case LISP_MINUS:
      for(i = 1; i < count; i++)
        tmp -= lisp_get_number(&oprans[i]);
      break;
    case LISP_MULTIPLY:
      for(i = 1; i < count; i++)
        tmp *= lisp_get_number(&oprans[i]);
      break;
    case LISP_DIVIDE:
      for(i = 1; i < count; i++)
        tmp /= lisp_get_number(&oprans[i]);
      break;
  }
  dummy.type = LISP_NUMBER;
  dummy.u.n = tmp;
  PUTV(dummy);
  return LISP_EVAL_OK;
}

static int lisp_eval_logic_op(lisp_value v, int type, env_t* e) {
  lisp_value* oprans;
  int ret;
  lisp_value dummy;
  for(dummy = cdr0(v); lisp_get_type(&dummy) != LISP_NIL; dummy = cdr0(dummy)) {
    if((ret = lisp_eval_value(car0(dummy), e)) != LISP_EVAL_OK)
      return ret;
  }
  oprans = (lisp_value*)eval_context_pop(&eval_stack, 2*sizeof(lisp_value));
  switch(type) {
    case LISP_BT: dummy.type = oprans[0].u.n > oprans[1].u.n ? LISP_TRUE : LISP_FALSE; break;	// type check..
    case LISP_LT: dummy.type = oprans[0].u.n < oprans[1].u.n ? LISP_TRUE : LISP_FALSE; break;
    case LISP_EQ: dummy.type = oprans[0].u.n == oprans[1].u.n ? LISP_TRUE : LISP_FALSE; break;
  }
  PUTV(dummy);
  return LISP_EVAL_OK;
}

static int lisp_eval_if(lisp_value v, env_t* e) {
  lisp_value* oprans;
  int ret;
  if((ret = lisp_eval_value(*(lisp_value*)lisp_get_list_element(&v, 1), e)) != LISP_EVAL_OK)
    return ret;
  oprans = (lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
  if(lisp_get_type(oprans) == LISP_TRUE)
    return lisp_eval_value(*(lisp_value*)lisp_get_list_element(&v, 2), e);
  else
    return lisp_eval_value(*(lisp_value*)lisp_get_list_element(&v, 3), e);
}

static int lisp_eval_not(lisp_value v, env_t* e) {
  lisp_value* oprans;
  int ret;
  if((ret = lisp_eval_value(*(lisp_value*)lisp_get_list_element(&v, 1), e)) != LISP_EVAL_OK)
    return ret;
  oprans = (lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
  if(lisp_get_type(oprans) == LISP_TRUE)
    oprans->type = LISP_FALSE;
  else
    oprans->type = LISP_TRUE;
  PUTV(*oprans);
  return LISP_EVAL_OK;
}

static int lisp_eval_car(lisp_value v, env_t* e);
static int lisp_eval_cdr(lisp_value v, env_t* e);
static int lisp_eval_cons(lisp_value v, env_t* e);

// TODO: keep track tmp list memory.
static int lisp_eval_car(lisp_value v, env_t* e) {
  lisp_value* lst, *p;
  lisp_value n;
  int ret, type;
  p = lisp_get_list_element(&v, 1);
  if(lisp_get_type(p) == LISP_SYMBOL) {
    if((ret = lisp_eval_symbol(*p, e)) != LISP_EVAL_OK) return ret;
    n = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
    lst = &n;
  }
  else {
    type = lisp_get_type(lisp_get_list_element(p, 0));
    if(type != LISP_QUOTE) {	// (car (car (quote ((1 2) 3))))
      n = *(lisp_value*)lisp_get_list_element(&v, 1);
      switch(type) {
        case LISP_CAR 	:	if((ret = lisp_eval_car(n, e)) != LISP_EVAL_OK) return ret; break;
        case LISP_CDR 	:	if((ret = lisp_eval_cdr(n, e)) != LISP_EVAL_OK) return ret; break;
        case LISP_CONS	:	if((ret = lisp_eval_cons(n, e)) != LISP_EVAL_OK) return ret; break;
        default			:	return LISP_LISP_OP_ILLEAGE;
      }
      n = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
      lst = &n;
    }
    else lst = lisp_get_list_element(&v, 1);	// lst point to quoted list.
  }

  assert(lisp_get_type(lst) == LISP_LIST && lisp_get_list_size(lst) > 0);
  p = lisp_get_list_element(lisp_get_list_element(lst, 1), 0);
  if(lisp_get_type(p) == LISP_LIST) {    // return quoted list : (car (quote ((1) 2))) => (quote (1))
    lisp_value* res = (lisp_value*)malloc(sizeof(lisp_value));
    res->type = LISP_LIST;
    res->u.a.size = 2;
    // TODO: memory leak
    res->u.a.e = (lisp_value*)malloc(2*sizeof(lisp_value));
    LINKTO(res);
    res->u.a.e[0].type = LISP_QUOTE;
    lisp_copy_list(&(res->u.a.e[1]), p, lisp_get_list_size(p));
    PUTV(*res);
  }
  else PUTV(*p);
  return LISP_EVAL_OK;
}

static int lisp_eval_cdr(lisp_value v, env_t* e) {
  lisp_value *lst, *p, *res;
  lisp_value n, dummy;
  int ret, type;
  p = lisp_get_list_element(&v, 1);
  if(lisp_get_type(p) == LISP_SYMBOL) {    // (cdr lst)
    if((ret = lisp_eval_symbol(*p, e)) != LISP_EVAL_OK) return ret;
    n = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
    lst = &n;
  }
  else {    // (cdr (quote ()))
    type = lisp_get_type(lisp_get_list_element(p, 0));
    if(type != LISP_QUOTE) {
      n = *(lisp_value*)lisp_get_list_element(&v, 1);
      switch(type) {
        case LISP_CAR 	:	if((ret = lisp_eval_car(n, e)) != LISP_EVAL_OK) return ret; break;
        case LISP_CDR 	:	if((ret = lisp_eval_cdr(n, e)) != LISP_EVAL_OK) return ret; break;
        case LISP_CONS 	: 	if((ret = lisp_eval_cons(n, e)) != LISP_EVAL_OK) return ret; break;
        default			:	return LISP_LISP_OP_ILLEAGE;
      }
      n = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
      lst = &n;
    }
    else lst = lisp_get_list_element(&v, 1);
  }

  lst = lisp_get_list_element(lst, 1);

  size_t size = lisp_get_list_size(lst);
  assert(size > 0);
  res = (lisp_value*)malloc(sizeof(lisp_value));
  res->type = LISP_LIST;
  res->u.a.size = size;
  if(size == 1) {
    res->u.a.size = 2;
    res->u.a.e = (lisp_value*)malloc(2 * sizeof(lisp_value));	// (cdr (quote (1)))	=> (quote ())
    LINKTO(res);
    res->u.a.e[0].type = LISP_QUOTE;
    res->u.a.e[1].type = LISP_LIST;
    res->u.a.e[1].u.a.size = 0;
    res->u.a.e[1].u.a.e = NULL;
  } else {
    res->u.a.e = (lisp_value*)malloc(size * sizeof(lisp_value));	// (cdr (quote (1 2)))	=> (quote (2))
    LINKTO(res);
    res->u.a.e[0].type = LISP_QUOTE;
    dummy.type = LISP_LIST;
    dummy.u.a.size = size - 1;
    dummy.u.a.e = lisp_get_list_element(lst, 1);
    lisp_copy_list(&(res->u.a.e[1]), &dummy, size-1);
  }
  PUTV(*res);
  return LISP_EVAL_OK;
}

static int lisp_eval_cons(lisp_value v, env_t* e) {

}

// (null? symbol)
// (null? (quote ())) => LISP_TRUE
// (null? (quote (1))) => LISP_FALSE
static int lisp_eval_is_null(lisp_value v, env_t* e) {
  lisp_value* p = lisp_get_list_element(&v, 1);
  lisp_value dummy;
  int ret;
  switch(lisp_get_type(p)) {
    // find symbol value from env_t. <= (null? lst)
    case LISP_SYMBOL:
      if((ret = lisp_eval_symbol(*p, e)) != LISP_EVAL_OK)
        return ret;
      p = eval_context_pop(&eval_stack, sizeof(lisp_value));
      break;
      // eval (car (cdr etc.))         <= (null? (cdr (quote (1))))
    case LISP_LIST  :
      if(lisp_get_type(lisp_get_list_element(p, 0)) != LISP_QUOTE) {
        if((ret = lisp_eval_value(*p, e)) != LISP_EVAL_OK)
          return ret;
        p = eval_context_pop(&eval_stack, sizeof(lisp_value));
        break;
      }
  }
  // (null? (quote ()))
  p = lisp_get_list_element(p, 1);
  dummy.type = lisp_get_list_size(p) == 0 ? LISP_TRUE : LISP_FALSE;
  PUTV(dummy);
  return LISP_EVAL_OK;
}

static int lisp_extend_eval_env(env_t* e, lisp_value*s, lisp_value* args);

static int lisp_cmp_symbol(lisp_value* v, lisp_value *e) {
  assert(v != NULL && e != NULL);
  return (lisp_get_string_length(v) != lisp_get_string_length(e)) || memcmp(v->u.sym.s, e->u.sym.s, v->u.sym.size);
}

static int lisp_eval_symbol(lisp_value v, env_t* e) {
  assert((v.type == LISP_SYMBOL || v.type == LISP_LIST) && e != NULL);
  int ret;
  size_t i, num_of_parameter;
  lisp_value body, parameters, args, dummy;

  if(v.type == LISP_SYMBOL) dummy = v;
  else dummy = *(lisp_value*)lisp_get_list_element(&v, 0);

  // look symbol-value pair backwards
  for(i = e->s.top/sizeof(lisp_value_pair) - 1; i >= 0 ; i--) {
    if(lisp_cmp_symbol(&dummy, e->s.p[i].symbol) == 0) {
      switch(lisp_get_type(e->s.p[i].value)) {	// according to symbol value's type, doing correspondent operations
        case LISP_NUMBER: PUTV(*(e->s.p[i].value)); return LISP_EVAL_OK;
        case LISP_LIST 	:
                          // if type of v is list, means it is symbol application, otherwise lambda calculus.
                          if(lisp_get_type(&v) == LISP_LIST) {
                            body = *(lisp_value*)lisp_get_list_element(e->s.p[i].value, 2);
                            parameters = *(lisp_value*)lisp_get_list_element(e->s.p[i].value, 1);
                            args = cdr0(v);
                            // printf("application: "); lisp_print_value(&v);
                            // printf("lambda: "); lisp_print_value(e->s.p[i].value);
                            // printf("args: "); lisp_print_value(&args);
                            num_of_parameter = lisp_get_list_size(&args);
                            if((ret = lisp_extend_eval_env(e, &parameters, &args)) != LISP_EVAL_ENV_EXTENED_OK)
                              return ret;
                            if((ret = lisp_eval_value(body, e)) != LISP_EVAL_OK)
                              return ret;
                            lisp_env_pop(e, num_of_parameter*sizeof(lisp_value_pair));
                          } else {
                            PUTV(*(e->s.p[i].value));
                          }
                          return LISP_EVAL_OK;
        case LISP_SYMBOL: dummy = *(e->s.p[i].value);	// found next
      }
    }
  }
  return LISP_EVAL_VARIABLE_NOT_FOUND;
}

// to support recurisive calls, using strict value evaluation.
static int lisp_extend_eval_env(env_t* e, lisp_value* s, lisp_value* args) {
  assert(e != NULL && lisp_get_list_size(s) == lisp_get_list_size(args));
  size_t i, count = lisp_get_list_size(s);
  int ret;
  lisp_value_pair* p = (lisp_value_pair*)lisp_env_push(e, count*sizeof(lisp_value_pair));
  for(i = 0; i < count; i++) {
    if(lisp_get_type(lisp_get_list_element(args, i)) == LISP_LIST && !lisp_is_lambda_or_quote(lisp_get_list_element(args, i))) {
      e->s.top -= count * sizeof(lisp_value_pair);
      if((ret = lisp_eval_value(*(lisp_value*)lisp_get_list_element(args, i), e)) != LISP_EVAL_OK)
        return ret;
      // keep track of the malloced memory using linked list.
      p[i].value = (lisp_value*)malloc(sizeof(lisp_value));
      LINKTO(p[i].value);
      *(p[i].value) = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
      e->s.top += count * sizeof(lisp_value_pair);
    }
    else p[i].value = lisp_get_list_element(args, i);

    p[i].symbol = lisp_get_list_element(s, i);
  }
  return LISP_EVAL_ENV_EXTENED_OK;
}

// lambda expression from stack
static int lisp_eval_lambda(lisp_value args, env_t *e) {
  int ret;
  size_t num_of_parameter;
  lisp_value lambda = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));    // pop lambda expression from stack
  lisp_value parameters = *(lisp_value*)lisp_get_list_element(&lambda, 1);        // parameter symbols
  lisp_value body = *(lisp_value*)lisp_get_list_element(&lambda, 2);                // body
  num_of_parameter = lisp_get_list_size(&args);    // book keeping the number of parameters
  if((ret = lisp_extend_eval_env(e, &parameters, &args)) != LISP_EVAL_ENV_EXTENED_OK)
    return ret;
  if((ret = lisp_eval_value(body, e)) != LISP_EVAL_OK)
    return ret;
  lisp_env_pop(e, num_of_parameter*sizeof(lisp_value_pair));
  return LISP_EVAL_OK;                            // (x 1) x := (lambda (y) y)
}

static int lisp_eval_get_lambda(lisp_value v, env_t* e) {
  int ret;
  if(lisp_get_type(lisp_get_list_element(lisp_get_list_element(&v, 0), 0)) == LISP_LAMBDA) {
    PUTV(*(lisp_value*)(lisp_get_list_element(&v, 0)));
  }
  else {    // todo lambda calculus, push final result(lambda expression) to eval_stack.
    if((ret = lisp_eval_value(car0(v), e)) != LISP_EVAL_OK)
      return ret;
  }
  return LISP_EVAL_OK;
}

static int lisp_eval_define(lisp_value v, env_t* e) {
  assert(lisp_get_list_size(&v) == 3);    // typical : (define id (lambda (x) x))
  lisp_value_pair* p = (lisp_value_pair*)lisp_env_push(e, sizeof(lisp_value_pair));
  p[0].symbol = lisp_get_list_element(&v, 1);
  p[0].value = lisp_get_list_element(&v, 2);
  return LISP_EVAL_OK;
}

static int lisp_eval_list(lisp_value v, env_t* e) {
  int ret;
  lisp_value dummy = car0(v);
  switch(lisp_get_type(&dummy)) {
    case LISP_PLUS        : 	return lisp_eval_bin_op(v, LISP_PLUS, e);
    case LISP_MINUS        : 	return lisp_eval_bin_op(v, LISP_MINUS, e);
    case LISP_MULTIPLY	: 	return lisp_eval_bin_op(v, LISP_MULTIPLY, e);
    case LISP_DIVIDE	: 	return lisp_eval_bin_op(v, LISP_DIVIDE, e);
    case LISP_BT         :	return lisp_eval_logic_op(v, LISP_BT, e);
    case LISP_LT         :	return lisp_eval_logic_op(v, LISP_LT, e);
    case LISP_EQ         : 	return lisp_eval_logic_op(v, LISP_EQ, e);
    case LISP_IF        :	return lisp_eval_if(v, e);
    case LISP_NOT        :	return lisp_eval_not(v, e);
    case LISP_CAR        :	return lisp_eval_car(v, e);
    case LISP_CDR         :	return lisp_eval_cdr(v, e);
    case LISP_NULL$        :	return lisp_eval_is_null(v, e);
    case LISP_SYMBOL 	:	return lisp_eval_symbol(v, e);
    case LISP_DEFINE 	:	return lisp_eval_define(v, e);	// (define id (lambda (x) x))
    case LISP_LAMBDA 	: 	PUTV(v); return LISP_EVAL_OK;	// put lambda expression to the stack.
                          // (((lambda (x) x) (lambda (y) y)) 1) => return lambda first.
    case LISP_LIST         :	if((ret = lisp_eval_get_lambda(v, e)) != LISP_EVAL_OK) return ret;
    default                :
                                dummy = cdr0(v);						// args
                                return lisp_eval_lambda(dummy, e);		// ((lambda (x y) (- y (- x x))) 2 1), push and pop
  }
}

static int lisp_eval_value(lisp_value v, env_t* e) {
  switch(lisp_get_type(&v)) {
    case LISP_NUMBER 	: return lisp_eval_number(v);
    case LISP_LIST         : return lisp_eval_list(v, e);
    case LISP_SYMBOL 	: return lisp_eval_symbol(v, e);
    default : return LISP_EVAL_INVALID_VALUE;
  }
}

int lisp_eval(lisp_value* v, lisp_value* result, env_t* e) {
  int ret;
  eval_context_init();
  memset(&eval_tmp_variables, 0, sizeof(eval_context));
  if((ret = lisp_eval_value(*v, e)) != LISP_EVAL_OK) {
    free(eval_stack.stack);
    return ret;
  }
  if(lisp_get_type(v) != LISP_LIST || lisp_get_type(lisp_get_list_element(v, 0)) != LISP_DEFINE)
    *result = *(lisp_value*)eval_context_pop(&eval_stack, sizeof(lisp_value));
  else result->type = LISP_NIL;
  assert(eval_stack.top == 0);
  free(eval_stack.stack);

  // final result at top, remove it from tmp variable list.
  if(eval_tmp_variables.top != 0) {
    eval_tmp_variables.top -= sizeof(lisp_value*);
    if(eval_tmp_variables.top != 0)
      lisp_free_tmp_variable(&eval_tmp_variables);
  }
  return ret;
}
