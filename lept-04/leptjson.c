#include "leptjson.h"

#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL malloc() realloc(), free(), stdtod() */
#include <string.h>  /* memcpy() */

#include <math.h>    /* HUGE_VAL */
#include <errno.h>   /* errno, ERANGE */

#include <stdio.h>  /* test */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)   do{assert(*c->json == (ch));c->json++;}while(0)

#define IS_DIGIT(ch)      ((ch) >= '0' && (ch) <= '9')
#define IS_DIGIT_1_9(ch)  ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
    void * ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        }
        while(c->top + size >= c->size) {
            c->size += c->size >> 1; /* c->size *= 1.5 */
        }
        c->stack = (char*) realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert( c->top >= size );
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
   const char *p = c->json;
   while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'){
       p++;
   }
   c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i+1] ; i++) {
        if ( c->json[i] != literal[i+1] ) {
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;

}

static int lept_parse_number(lept_context* c, lept_value* v) {
    /* do something */
    const char *p = c->json;
    int state = 1;

    while(state != 0) {
        switch(state) {
            case 1:
                if (*p == '-'){
                    state = 3; p++;
                }else if (*p == '0'){
                    state = 2; p++;
                }else if ( IS_DIGIT_1_9(*p)) {
                    state = 4; p++;
                }else {
                    return LEPT_PARSE_INVALID_VALUE;
                }
                break;
            case 2:
                if ( *p == '.') {
                    state = 5; p++;
                }else if (*p == 'e' || *p == 'E'){
                    state = 6; p++;
                }else {
                    state = 0;
                    break; /* end */
                }
                break;
            case 3:
                if (*p == '0') {
                    state = 2; p++;
                }else if ( IS_DIGIT_1_9(*p) ) {
                    state = 4; p++;
                }else {
                    return LEPT_PARSE_INVALID_VALUE; 
                }
                break;
            case 4:
                if (*p == '.') {
                    state = 5; p++;
                }else if ( IS_DIGIT(*p) ) {
                    state = 4; p++;
                }else if ( *p == 'e' || *p == 'E') {
                    state = 6; p++;
                }else {
                    state = 0;
                    break; /* end */
                }
                break;
            case 5:
                if ( IS_DIGIT(*p) ) {
                    state = 7; p++;
                }else {
                    return LEPT_PARSE_INVALID_VALUE;
                }
                break;
            case 6:
                if ( IS_DIGIT(*p) ) {
                    state = 9; p++;
                }else if ( *p == '+' || *p == '-') {
                    state = 8; p++;
                }else {
                    return LEPT_PARSE_INVALID_VALUE;
                }
                break;
            case 7:
                if ( IS_DIGIT(*p) ) {
                    state = 7; p++;
                }else if ( *p == 'e' || *p == 'E') {
                    state = 6; p++;
                }else {
                    state = 0;
                    break; /* end */
                }
                break;
            case 8:
                if ( IS_DIGIT(*p) ) {
                    state = 9; p++;
                }else {
                    return LEPT_PARSE_INVALID_VALUE;
                }
                break;
            case 9:
                if ( IS_DIGIT(*p) ) {
                    state = 9; p++;
                }else {
                    state = 0;
                    break; /* end */
                }
                break;
            default: 
                /* fuck off !! */
                break;
        }
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n ==-HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

static const char* lept_parse_hex4(const char* p, unsigned int* u) {
    char ch;
    short i = 0;
    *u = 0;
    for(i = 0; i < 4; i++ ){
        ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9'){
            *u |= (ch - '0');
        }else if (ch >= 'A' && ch <= 'F') {
            *u |= (ch - 'A' + 10);
        }else if (ch >= 'a' && ch <= 'f') {
            *u |= (ch - 'a' + 10);
        }else{
            return NULL;
        }
    }
    return p;
}

static void lept_encode_utf8(lept_context* c, unsigned int u) {
    if (u <= 0x007F) {
        PUTC(c, u & 0xFF);
    }else if (u <= 0x07FF ) {
        PUTC(c, 0xC0 | ( (u >>  6) & 0x1F ) );
        PUTC(c, 0x80 | (  u        & 0X3F ) );
    }else if (u <= 0xFFFF ) {
        PUTC(c, 0xE0 | ( (u >> 12) & 0x0F ) );
        PUTC(c, 0x80 | ( (u >>  6) & 0x3F ) );
        PUTC(c, 0x80 | ( (u      ) & 0x3F ) );
    }else {
        assert( u <= 0x10FFFF );
        PUTC(c, 0xF0 | ( (u >> 18) & 0x07 ) );
        PUTC(c, 0x80 | ( (u >> 12) & 0x3F ) );
        PUTC(c, 0x80 | ( (u >>  6) & 0x3F ) );
        PUTC(c, 0x80 | ( (u      ) & 0x3F ) );
    }
}

static int lept_parse_string(lept_context* c, lept_value* v) {
    unsigned int u, u2;
    size_t len;
    size_t head = c->top;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char *) lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0': 
                c->top = head;   /* resume */
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            case '\\':
                switch (*p ++) {
                    case '\x22': 
                        PUTC(c, '\x22'); break;   /* " */
                    case '\x5C':
                        PUTC(c, '\x5C'); break;   /* \ */    
                    case '\x2F':
                        PUTC(c, '\x2F'); break;   /* / */
                    case '\x62':
                        PUTC(c, '\x08'); break;   /* b */
                    case '\x66':
                        PUTC(c, '\x0C'); break;   /* f */
                    case '\x6E': 
                        PUTC(c, '\x0A'); break;   /* n */
                    case '\x72':
                        PUTC(c, '\x0D'); break;   /* r */
                    case '\x74':
                        PUTC(c, '\x09'); break;   /* t */
                    case '\x75':    
                        /* \uXXXX */  
                        if ( !(p = lept_parse_hex4(p, &u)) ) {
                            c->top = head; 
                            return LEPT_PARSE_INVALID_UNICODE_HEX;
                        }
                        if ( u >= 0xD800 && u <= 0xDBFF) {
                            if ( *p++ != '\\') {
                                c->top = head; 
                                return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            }
                            if ( *p++ != 'u') {
                                c->top = head; 
                                return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            }
                            if ( !(p = lept_parse_hex4(p, &u2)) ) {
                                c->top = head;
                                return LEPT_PARSE_INVALID_UNICODE_HEX; 
                            }
                            if (u2 < 0xDC00 || u2 > 0xDFFF) {
                                c->top = head;
                                return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            }
                            u =  (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            default:
                if ((unsigned char)ch < 0x20) {
                    c->top = head;
                    return LEPT_PARSE_INVALID_STRING_CHAR;
                }
                PUTC(c, ch);
                break;
        }
    }
}

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch( *c->json ) {
        case 'n'  : return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't'  : return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f'  : return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\0' : return LEPT_PARSE_EXPECT_VALUE;
        case '\"' : return lept_parse_string(c, v);
        default :   return lept_parse_number(c, v);
    }
}

int lept_parse(lept_value *v, const char *json) {
    int ret = 0;
    lept_context c;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ( (ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK ) {
        lept_parse_whitespace(&c);
        if ( *(c.json) != '\0') {
            v->type = LEPT_NULL; 
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    }
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

/* boolean */
int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}
void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = ( (b) ? LEPT_TRUE : LEPT_FALSE );
}

/* number */
double lept_get_number(const lept_value *v) {
    assert(v!= NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}
void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

/* string */
const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}
size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}
void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL ||  len == 0));
    lept_free(v);
    v->u.s.s = (char*) malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

