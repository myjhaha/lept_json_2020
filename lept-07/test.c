#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leptjson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality){\
            test_pass++;\
	}else{\
            fprintf(stderr, "%s:%d expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    }while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual),  expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_FALSE(actual) EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")

#define EXPECT_EQ_STRING(expect, actual, actlength) \
    EXPECT_EQ_BASE((sizeof(expect) - 1 == actlength) && (memcmp(expect, actual, actlength) == 0), expect, actual, "%s")

#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

#define TEST_NUMBER(expect, json) \
    do { \
        lept_value v; \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v)); \
        EXPECT_EQ_DOUBLE(expect, lept_get_number(&v)); \
    }while(0)

#define TEST_ERROR(error, json) \
    do { \
        lept_value v; \
        v.type = LEPT_FALSE; \
        EXPECT_EQ_INT(error, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v)); \
    }while(0)

#define TEST_STRING(expect, json) \
    do {\
        lept_value v; \
        lept_init(&v); \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_STRING, lept_get_type(&v)); \
        EXPECT_EQ_STRING(expect, lept_get_string(&v), lept_get_string_length(&v)); \
        lept_free(&v); \
   } while(0)

#define TEST_ROUNDTRIP(json) \
    do {\
        lept_value v; \
        char* json2; \
        size_t length; \
        lept_init(&v); \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_STRINGIFY_OK, lept_stringify(&v, &json2, &length)); \
        EXPECT_EQ_STRING(json, json2, length); \
        lept_free(&v); \
        free(json2); \
    } while(0) 
        

static void test_parse_number() {
    TEST_NUMBER(123.0, "123");
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000"); /* must underflow */
}

static void test_parse_null() {
    lept_value v;
    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
}

static void test_parse_true() {
    lept_value v;
    v.type = LEPT_NULL;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "true"));
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(&v));
}

static void test_parse_false() {
    lept_value v;
    v.type = LEPT_NULL;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "false"));
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(&v));
}

static void test_parse_expect_value() {
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, " ");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "  ");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "    ");
}

static void test_parse_invalid_value() {
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "?");

#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nan");
#endif
}

static void test_parse_root_not_singular() {
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "null x");

#if 1
    /* invalid number */
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0123"); /* after zero should be '.' , 'E' , 'e' or nothing */
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x123");
#endif
}

static void test_parse_number_too_big() {
#if 1
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "1E309");
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "-1e309");
#endif
}

/* test access [null, number, string, boolean] */
static void test_access_null() {
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "aa", 1);
    lept_set_null(&v);
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
    lept_free(&v);
}
static void test_access_number() {
    lept_value v;
    lept_init(&v);
    lept_set_number(&v, 0.0);
    EXPECT_EQ_DOUBLE(0.0, lept_get_number(&v));
    lept_free(&v);
}
static void test_access_string() {
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "", 0);
    EXPECT_EQ_STRING("", lept_get_string(&v), lept_get_string_length(&v));
    lept_set_string(&v, " ", 1);
    EXPECT_EQ_STRING(" ", lept_get_string(&v), lept_get_string_length(&v));
    lept_set_string(&v, "Hello", 5);
    EXPECT_EQ_STRING("Hello", lept_get_string(&v), lept_get_string_length(&v));
    lept_free(&v);
}
static void test_access_boolean() {
    lept_value v;
    lept_init(&v);
    lept_set_string(&v, "", 0);
    lept_set_boolean(&v, 1);  /* set true */
    EXPECT_TRUE(lept_get_boolean(&v));
    lept_set_boolean(&v, 0); /* se false */
    EXPECT_FALSE(lept_get_boolean(&v));
    lept_free(&v);
}

static void test_parse_string() {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
#if 1      /* escape character */
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
#endif
    /* test \uXXXX */
    TEST_STRING("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING("\x24", "\"\\u0024\"");         /* Dollar sign U+0024 */
    TEST_STRING("\xC2\xA2", "\"\\u00A2\"");     /* Cents sign U+00A2 */
    TEST_STRING("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* G clef sign U+1D11E */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  /* G clef sign U+1D11E */
}

static void test_parse_invalid_string_escape() {
#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
#endif
}

static void test_parse_invalid_string_char() {
#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
#endif
}

static void test_parse_invalid_unicode_hex() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u00/0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
}

static void test_parse_invalid_unicode_surrogate() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_array() {
    lept_value v;
    lept_value* pt;
    size_t i = 0;
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[  ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(0, lept_get_array_size(&v));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[[ 1,2,3,4,5,6,7,8,9,10,[[[]]]  ],[],[],[],\"abc\",12.0  ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(6, lept_get_array_size(&v));
    pt = lept_get_array_element(&v, 0);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    EXPECT_EQ_SIZE_T(11,lept_get_array_size(pt));
    pt = lept_get_array_element(&v, 1);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    pt = lept_get_array_element(&v, 2);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    pt = lept_get_array_element(&v, 3);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    pt = lept_get_array_element(&v, 4);
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(pt));
    pt = lept_get_array_element(&v, 5);
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(pt));
    EXPECT_EQ_DOUBLE(12.0, lept_get_number(pt));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ null, true, false, \"\", \"ab\\u0000c\", 123.00, [[null, null, null  ]], [0,1,2] ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(8, lept_get_array_size(&v));
    
    pt = lept_get_array_element(&v, 0);
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(pt));
    
    pt = lept_get_array_element(&v, 1);
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(pt));
    
    pt = lept_get_array_element(&v, 2);
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(pt));
    
    pt = lept_get_array_element(&v, 3);
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(pt));
    EXPECT_EQ_STRING("", lept_get_string(pt), lept_get_string_length(pt));
    
    pt = lept_get_array_element(&v, 4);
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(pt));
    EXPECT_EQ_STRING("ab\0c", lept_get_string(pt), lept_get_string_length(pt));
    
    pt = lept_get_array_element(&v, 5);
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(pt)); 
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(pt));

    pt = lept_get_array_element(&v, 6);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    EXPECT_EQ_SIZE_T(1, lept_get_array_size(pt));
    pt = lept_get_array_element(pt, 0);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    EXPECT_EQ_SIZE_T(3, lept_get_array_size(pt));
    for(i = 0; i < 3 ; i++){
        EXPECT_EQ_INT(LEPT_NULL, lept_get_type(lept_get_array_element(pt, i)));       
    }

    pt = lept_get_array_element(&v, 7);
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(pt));
    EXPECT_EQ_SIZE_T(3, lept_get_array_size(pt));
    for(i = 0; i < 3 ; i++){
        EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_array_element(pt, i)));
        EXPECT_EQ_DOUBLE((double)i, lept_get_number(lept_get_array_element(pt, i)));
    }
    lept_free(&v);
}

static void test_parse_object() {
    lept_value v;

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, " { } "));
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(0, lept_get_object_size(&v));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v,
        " { "
            "\"n\" : null , "
            "\"f\" : false , "
            "\"t\" : true , "
            "\"i\" : 123 , "
            "\"s\" : \"abc\", "
            "\"a\" : [ 1, 2, 3 ],"
            "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 } ,   "
            "\"hello\\u0000world\"  :  \"helloworld\""
        " } "
    ));
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(8, lept_get_object_size(&v));
    EXPECT_EQ_STRING("n", lept_get_object_key(&v, 0), lept_get_object_key_length(&v, 0));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(lept_get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", lept_get_object_key(&v, 1), lept_get_object_key_length(&v, 1));
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(lept_get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", lept_get_object_key(&v, 2), lept_get_object_key_length(&v, 2));
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(lept_get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", lept_get_object_key(&v, 3), lept_get_object_key_length(&v, 3));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", lept_get_object_key(&v, 4), lept_get_object_key_length(&v, 4));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", lept_get_string(lept_get_object_value(&v, 4)), lept_get_string_length(lept_get_object_value(&v, 4)));
    
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(lept_get_object_value_by_key(&v, "n", 1)));
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(lept_get_object_value_by_key(&v, "f", 1)));
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(lept_get_object_value_by_key(&v, "t", 1)));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_object_value_by_key(&v, "i", 1)));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_object_value_by_key(&v, "s", 1)));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(lept_get_object_value_by_key(&v, "a", 1)));
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(lept_get_object_value_by_key(&v, "o", 1)));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_object_value_by_key(&v, "hello\x00world", 11)));
    

    lept_free(&v);
}

static void test_parse_invalid_array() {
    lept_value v;

    lept_init(&v);
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[null, false, [null, null] false]");

    lept_init(&v);
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[null, false, [null, null], 1234.0  ");
    
    lept_init(&v);
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[null, false, null, null, null, [null, null   , 1234.0, 1.0, 1.0, 1.0, 1.0  ");
}
static void test_parse_miss_key() {
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{ : 1," );
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{1:1,}");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{\"a\":1,");
}
static void test_parse_miss_colon() {
    TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}
static void test_parse_miss_comma_or_curly_bracket() {
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_object();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();

    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    /* \uxxxx unicode test */
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();

    test_parse_invalid_array();

    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();

}

static void test_stringify_object() {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"a\":\"aa\\u0000\\u0000\"}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
    TEST_ROUNDTRIP("{\"n\":null,\"t\":true,\"f\":false,\"s\":\"sss\"}");
    TEST_ROUNDTRIP("{\"n\":null,\"t\":true,\"f\":false,\"s\":\"sss\",\"a\":[null,false,true,[],{},\"aaa\\u0000aaa\"],\"o\":{\"key\\u0000key\":\"value\\u0000value\"}}" );
}

static void test_stringify_array() {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3],{\"n\":null}]");
}

static void test_stringify_string() {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
    TEST_ROUNDTRIP("\"aaaa\\u0000aaaa\"");
}

static void test_stringify_number() {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
#if 0
    TEST_ROUNDTRIP("1.23e20");
    TEST_ROUNDTRIP("1.23e+20");
    TEST_ROUNDTRIP("1.23e-20");
#endif
} 

static void test_stringify() {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");

    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
    
}

static void test_access() {
    test_access_null();
    test_access_boolean();
    test_access_number();
    test_access_string();
}
int main() {
    /* size_t alen = 1000; */
    test_parse();
    test_access();
    test_stringify();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    /* printf("%s %s \n", "\xE2\x82\xAC", "\xF0\x9D\x84\x9E"); */
    /* printf("size_t = %zu \n", alen); */
    return main_ret;
}

