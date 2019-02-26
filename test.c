#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "eval.h"

int main_ret, passed, total;

#define NEWLINE printf("----------------\n");

#define EXPECT_EQ_BASE(equlity,expect,actual,format) \
  do { \
    total++; \
    if(equlity) { \
      passed++; \
    } else { \
      main_ret = 1; \
      printf("failed at file %s line %d, expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual); \
    } \
  } while(0)

#define EXPECT_EQ_INT(expect,actual) \
  EXPECT_EQ_BASE((expect)==(actual), expect, actual, "%d")
#define EXPECT_EQ_SIZE_T(expect,actual) \
  EXPECT_EQ_BASE((expect)==(actual), expect, actual, "%zu")
#define EXPECT_EQ_DOUBLE(expect,actual) \
  EXPECT_EQ_BASE((expect)==(actual), expect, actual, "%lf")
#define EXPECT_EQ_STRING(expect,actual,size) \
  EXPECT_EQ_BASE(!(memcmp(expect,actual,size)), expect, actual, "%s")

#define TEST_STRINGFY(expect,lisp_value) \
  do { \
    size_t len = strlen((expect)); \
    char* s = lisp_stringfy((lisp_value)); \
    EXPECT_EQ_BASE(!(memcmp(expect,s,len)), expect, s, "%s"); \
    free(s); \
  }while(0)


static void test_parse_operator() {
  lisp_value v;
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "+"));
  EXPECT_EQ_INT(LISP_PLUS, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "- "));
  EXPECT_EQ_INT(LISP_MINUS, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "*"));
  EXPECT_EQ_INT(LISP_MULTIPLY, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "/"));
  EXPECT_EQ_INT(LISP_DIVIDE, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "define"));
  EXPECT_EQ_INT(LISP_DEFINE, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "lambda"));
  EXPECT_EQ_INT(LISP_LAMBDA, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "car"));
  EXPECT_EQ_INT(LISP_CAR, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "cdr"));
  EXPECT_EQ_INT(LISP_CDR, lisp_get_type(&v));
  v.type = LISP_NULL;
}

static void test_parse_number() {
  lisp_value v;
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "12"));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&v));
  EXPECT_EQ_DOUBLE((double)12, lisp_get_number(&v));
}

static void test_parse_list() {
  lisp_value v;
  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "()"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)0, lisp_get_list_size(&v));
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(+ 1 2)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(+ 1 (* 2 3))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_PLUS, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(lisp_get_list_element(&v, 1)));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(lisp_get_list_element(&v, 1)));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 2)));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(lisp_get_list_element(&v, 2)));
  EXPECT_EQ_INT(LISP_MULTIPLY, lisp_get_type(lisp_get_list_element(lisp_get_list_element(&v, 2), 0)));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(lisp_get_list_element(lisp_get_list_element(&v, 2), 1)));
  EXPECT_EQ_DOUBLE((double)2, lisp_get_number(lisp_get_list_element(lisp_get_list_element(&v, 2), 1)));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(lisp_get_list_element(lisp_get_list_element(&v, 2), 2)));
  EXPECT_EQ_DOUBLE((double)3, lisp_get_number(lisp_get_list_element(lisp_get_list_element(&v, 2), 2)));
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK,
      lisp_parse(&v, "(define fib (lambda (n) (if (not (> n 2))) n (+ (fib (- n 1)) (fib (- n 2)))))"));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  lisp_value_free(&v);
}

static void test_car_and_cdr() {
  char* s;
  lisp_value v, dummy, result;

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(1 2)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  dummy = car0(v);
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&dummy));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&dummy));
  dummy = cdr0(v);
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)1, lisp_get_list_size(&dummy));
  dummy = car0(cdr0(v));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&dummy));
  EXPECT_EQ_DOUBLE((double)2, lisp_get_number(&dummy));
  dummy = cdr0(cdr0(v));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&dummy));
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define fib (lambda (n) (if (< n 2)) n (+ (fib (- n 1)) (fib (- n 2)))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  dummy = car0(v);
  EXPECT_EQ_INT(LISP_DEFINE, lisp_get_type(&dummy));
  dummy = cdr0(v);
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&dummy));
  dummy = car0(cdr0(v));
  EXPECT_EQ_INT(LISP_SYMBOL, lisp_get_type(&dummy));
  EXPECT_EQ_STRING("fib", lisp_get_string(&dummy), lisp_get_string_length(&dummy));
  dummy = cdr0(cdr0(v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)1, lisp_get_list_size(&dummy));
  dummy = car0(cdr0(cdr0(v)));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)5, lisp_get_list_size(&dummy));
  dummy = car0(car0(cdr0(cdr0(v))));
  EXPECT_EQ_INT(LISP_LAMBDA, lisp_get_type(&dummy));
  dummy = cdr0(car0(cdr0(cdr0(v))));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)4, lisp_get_list_size(&dummy));
  dummy = car0(cdr0(car0(cdr0(cdr0(v)))));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)1, lisp_get_list_size(&dummy));
  dummy = car0(car0(cdr0(car0(cdr0(cdr0(v))))));
  EXPECT_EQ_INT(LISP_SYMBOL, lisp_get_type(&dummy));
  EXPECT_EQ_STRING("n", lisp_get_string(&dummy), lisp_get_string_length(&dummy));
  dummy = cdr0(cdr0(car0(cdr0(cdr0(v)))));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&dummy));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&dummy));
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(quote (1 2))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_QUOTE, lisp_get_type(lisp_get_list_element(&v, 0)));
  TEST_STRINGFY("(quote (1 2))", &v);
  lisp_value_free(&v);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (quote (1 2)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (quote ((1) 2)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  TEST_STRINGFY("(quote (1))", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (quote ((1 (2 3)) 4)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  TEST_STRINGFY("(quote (1 (2 3)))", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (car (quote ((1 (2 3)) 4))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  TEST_STRINGFY("1", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(cdr (quote (1 2)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&result));
  TEST_STRINGFY("(quote (2))", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(cdr (cdr (quote (1 2 3))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&result));
  TEST_STRINGFY("(quote (3))", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (cdr (quote (1 2 3))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  TEST_STRINGFY("2", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(car (cdr (quote (1 (2) 3))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&result));
  TEST_STRINGFY("(quote (2))", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(cdr (quote (1)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&result));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&result));
  TEST_STRINGFY("(quote ())", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);


  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(cdr (cdr (quote (1 2))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  TEST_STRINGFY("(quote ())", &result);
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(null? (cdr (cdr (quote (1 2)))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_TRUE, lisp_get_type(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);
}

static void test_stringfy() {
  char* str;
  lisp_value v;
  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(+ 1 2)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(lisp_get_list_element(&v, 1)));
  str = lisp_stringfy(&v);
  printf("%s\n", str);
  free(str);
  lisp_value_free(&v);

  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define fib (lambda (n) (if (< n 2)) n (+ (fib (- n 1)) (fib (- n 2)))))"));
  str = lisp_stringfy(&v);
  printf("%s\n", str);
  EXPECT_EQ_STRING("(define fib (lambda (n) (if (< n 2)) n (+ (fib (- n 1)) (fib (- n 2)))))", str, strlen(str));
  free(str);
  lisp_value_free(&v);
}

static void test_eval() {
  lisp_value v, result;
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "42"));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)42, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(- (+ 1 (* 99 (/ 13 13))) 5 5)"));
  EXPECT_EQ_INT(LISP_MINUS, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)90, lisp_get_number(&result));
  printf("%lf\n", lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(> (+ 1 1) (* 1 (/ 1 1)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_BT, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_TRUE, lisp_get_type(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(if (> 2 1) 1 0)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)4, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_IF, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(if (< 2 1) 1 0)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)4, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_IF, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)0, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(if (not (< 2 1)) 1 0)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)4, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_IF, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, NULL));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x) x) 1)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x y) (- y (+ x 0))) 1 2)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x) x) (- 2 1))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x y) (- y (+ x 0))) (- 2 1) (* 1 2))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x y) (- y (+ x (if (> x y) 1 0)))) 1 2)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x) (x 1)) (lambda (y) y))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x) (x (* 1 1))) (lambda (y) y))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  // return lambda expression
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(((lambda (x) x) (lambda (y) y)) 1)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "((lambda (x) (x (* 1 1))) (lambda (x) x))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_env_print(&global_env);

  // lisp_value_init(&v);
  // lisp_value_init(&result);
  // EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(((lambda (x) (lambda (y) y)) 0) 1)"));    // return lambda
  // EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  // EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  // EXPECT_EQ_INT(LISP_LIST, lisp_get_type(lisp_get_list_element(&v, 0)));
  // EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  // EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  // EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  // lisp_value_free(&v);
  // lisp_value_free(&result);

  NEWLINE;

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define id (lambda (x) x))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  //lisp_value_free(&v);        // global env point to the value `v`, so don't free it when finished the operations on original value `v`.
  lisp_value_free(&result);
  lisp_env_print(&global_env);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(id 1)"));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  NEWLINE;
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define add (lambda (x y) (+ x y)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  //lisp_value_free(&v);        // global env point to the value `v`, so don't free it when finished the operations on original value `v`.
  lisp_value_free(&result);
  lisp_env_print(&global_env);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(add (* 42 0) 1)"));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  NEWLINE;
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define add (lambda (x y) (* x y)))"));    // shadow previous defined symbol
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  //lisp_value_free(&v);        // global env point to the value `v`, so don't free it when finished the operations on original value `v`.
  lisp_value_free(&result);
  lisp_env_print(&global_env);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(add (* 42 0) 1)"));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)0, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  NEWLINE;
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  //lisp_value_free(&v);        // global env point to the value `v`, so don't free it when finished the operations on original value `v`.
  lisp_value_free(&result);
  lisp_env_print(&global_env);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(fact 3)"));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)6, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  NEWLINE;
  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  //lisp_value_free(&v);        // global env point to the value `v`, so don't free it when finished the operations on original value `v`.
  lisp_value_free(&result);
  lisp_env_print(&global_env);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(fib 6)"));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)8, lisp_get_number(&result));
  lisp_value_free(&v);
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(fact (fib 4))"));        // fib seq: 0 1 1 2 3 ... => (fact 3) => 6
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)6, lisp_get_number(&result));                // returned value is 6.
  lisp_value_free(&v);
  lisp_value_free(&result);
}

#if 1
static void test_global_env() {
  lisp_value v, result;

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define id (lambda (x) x))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(id 1)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define add (lambda (z y) (+ z y)))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(add 0 1)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(define fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)3, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NIL, lisp_get_type(&result));
  lisp_value_free(&result);

  lisp_value_init(&v);
  lisp_value_init(&result);
  EXPECT_EQ_INT(LISP_PARSE_OK, lisp_parse(&v, "(fact 0)"));
  EXPECT_EQ_INT(LISP_LIST, lisp_get_type(&v));
  EXPECT_EQ_SIZE_T((size_t)2, lisp_get_list_size(&v));
  EXPECT_EQ_INT(LISP_EVAL_OK, lisp_eval(&v, &result, &global_env));
  EXPECT_EQ_INT(LISP_NUMBER, lisp_get_type(&result));
  EXPECT_EQ_DOUBLE((double)1, lisp_get_number(&result));
  lisp_value_free(&result);
}
#endif

// and now it maybe as defined `symbol` at the program
static void test_invalid_value() {
  lisp_value v;
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_INVALID_VALUE, lisp_parse(&v, "defi"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_INVALID_VALUE, lisp_parse(&v, "lambd"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_INVALID_VALUE, lisp_parse(&v, "ca"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_INVALID_VALUE, lisp_parse(&v, "cd"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
}

static void test_root_not_singular() {
  lisp_value v;
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "definea"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "define a"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "lambdab"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "carr"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "car r"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "cdrr"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
  EXPECT_EQ_INT(LISP_PARSE_ROOT_NOT_SINGULAR, lisp_parse(&v, "cdr r"));
  EXPECT_EQ_INT(LISP_NULL, lisp_get_type(&v));
  v.type = LISP_NULL;
}

static void test_invalid_list() {
  lisp_value v;
  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_MISS_CLOSE_PRAN, lisp_parse(&v, "("));
  lisp_value_free(&v);
  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_MISS_CLOSE_PRAN, lisp_parse(&v, "(+"));
  lisp_value_free(&v);
  lisp_value_init(&v);
  EXPECT_EQ_INT(LISP_PARSE_MISS_CLOSE_PRAN, lisp_parse(&v, "(("));
  lisp_value_free(&v);
}

static void test_parse() {
  test_parse_operator();
  test_parse_number();
  test_parse_list();
  //test_invalid_value();
  test_root_not_singular();
  test_invalid_list();
  test_car_and_cdr();
  test_stringfy();
  test_eval();
  // test_global_env();
}

int main() {
  env_init(NULL, &global_env);
  test_parse();
  printf("passed/total: %d/%d\n", passed, total);
  env_free(&global_env);

  env_init(NULL, &global_env);
  char* code_buffer = (char*)malloc(2048);    // 2KB
  char* s;
  lisp_value v, result;
  printf("> ");
  while(gets(code_buffer)) {
    lisp_value_init(&v);
    lisp_value_init(&result);
    if(lisp_parse(&v, code_buffer) != LISP_PARSE_OK)
      break;
    if(lisp_eval(&v, &result, &global_env) != LISP_EVAL_OK)
      break;
    if(lisp_get_type(&result) != LISP_NIL) {
      s = lisp_stringfy(&result);
      printf("=> %s\n", s);
      free(s);
      lisp_value_free(&v);
    }
    else printf("=> nil\n");
    printf("> ");
  }
  env_free(&global_env);
  free(code_buffer);

  return main_ret;
}

// gcc test.c lisp_interpreter.c
