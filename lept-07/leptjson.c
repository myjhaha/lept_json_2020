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
#define PUTS(c, s, len)     memcpy(lept_context_push(c, len), s, len)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

static int lept_parse_value(lept_context* c, lept_value* v);/* forward declaration */

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

static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
    unsigned int u, u2;
    size_t head = c->top;
    const char* p;
    assert(str != NULL && len != NULL);
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = (char *) lept_context_pop(c, *len);
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

static int lept_parse_string(lept_context* c, lept_value* v) {
    int ret;
    char* s;
    size_t len;
    if ( (ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK ) {   
        lept_set_string(v, s, len);
    }
    return ret;
}

static int lept_parse_array(lept_context* c, lept_value* v) {
    size_t size = 0;
    int ret;
    size_t i;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if (*(c->json) == ']') {
        c->json++;
        v->type = LEPT_ARRAY;
        v->u.a.e = NULL;
        v->u.a.size = 0;
        return LEPT_PARSE_OK;
    }
    for(;;) {
        lept_value ev;
        lept_init(&ev);
        if ( (ret = lept_parse_value(c, &ev)) != LEPT_PARSE_OK) {
            break;
        }
        memcpy(lept_context_push(c, sizeof(lept_value)), &ev, sizeof(lept_value) );
        size ++;
        lept_parse_whitespace(c);
        if (*c->json == ']') {
            /* array parse OK */
            v->type = LEPT_ARRAY;
            v->u.a.size = size;
            size *= sizeof(lept_value);
            memcpy((v->u.a.e = malloc(size)), lept_context_pop(c, size), size);
            c->json++;
            return LEPT_PARSE_OK;
        }
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
            continue;
        }
        ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;        
        break;
    }
    for(i = 0; i < size; i++ ) {
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    }
    return ret;
}

static int lept_parse_object(lept_context* c, lept_value* v) {
    size_t size = 0, i;
    lept_member m;
    int ret;
    
    m.k = NULL;
    m.klen = 0;
    lept_init(&m.v);

    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*(c->json) == '}') {
        c->json++;
        v->type = LEPT_OBJECT;
        v->u.o.size = 0;
        v->u.o.m = NULL;
        return LEPT_PARSE_OK; 
    }
    for(;;) {
        char* s;
        m.k = NULL;
        m.klen = 0;
        lept_init(&m.v);
        /* parse key (string)*/
        if (*c->json != '\"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ( (ret = lept_parse_string_raw(c, &s, &(m.klen) ) )  == LEPT_PARSE_OK )  {
            m.k = (char*) malloc( m.klen + 1 );
            memcpy(m.k, s, m.klen);
            m.k[m.klen] = '\0';
        }else {
            /* parse key error */
            break;
        }
        lept_parse_whitespace(c);
        if ( *c->json == ':' ) {
            c->json ++;
        }else {
            free(m.k);
            m.k = NULL;
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        lept_parse_whitespace(c);
        if ( (ret = lept_parse_value(c, &(m.v))) != LEPT_PARSE_OK) { 
            free(m.k);
            m.k = NULL;
            break;
        }
        size ++;
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member) );
        lept_parse_whitespace(c);
        if (*c->json == ',' ) {
            c->json ++;
            lept_parse_whitespace(c);
            continue;
        }else if (*c->json == '}') {
            /* end parse object */
            c->json ++;
            v->type = LEPT_OBJECT;
            v->u.o.size = size;
            size *= sizeof(lept_member);
            v->u.o.m = (lept_member*) malloc(size);
            memcpy(v->u.o.m, lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        }
        ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
        break;
    }
    /* error */
    for(i = 0; i < size; i++) {
        lept_member* pt = lept_context_pop(c, sizeof(lept_member));
        free(pt->k);
        lept_free(&pt->v);        
    }
    return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch( *c->json ) {
        case 'n'  : return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't'  : return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f'  : return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\0' : return LEPT_PARSE_EXPECT_VALUE;
        case '\"' : return lept_parse_string(c, v);
        case '['  : return lept_parse_array(c, v);
        case '{'  : return lept_parse_object(c, v);
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
    size_t i;
    assert(v != NULL);
    if (v->type == LEPT_STRING) {
        free(v->u.s.s);
    }else if (v->type == LEPT_ARRAY) {
        for (i = 0; i < v->u.a.size; i++) {
            lept_free(&v->u.a.e[i]);
        }
        free(v->u.a.e);
    }else if (v->type == LEPT_OBJECT) {
        for( i = 0; i < v->u.o.size; i++ ) {
            lept_free( &(v->u.o.m[i].v) );
            free( v->u.o.m[i].k );
        }
        free(v->u.o.m);
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

/* array */
size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}
lept_value* lept_get_array_element(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index >= 0 && index < v->u.a.size);
    if (index < 0 || index >= v->u.a.size) {
        return NULL;
    }
    return &v->u.a.e[index];
}

/* object */
size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.size;
}
const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index >= 0 && index < v->u.o.size);
    return v->u.o.m[index].k;
}
size_t lept_get_object_key_length (const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index >= 0 && index < v->u.o.size);
    return v->u.o.m[index].klen;
}
lept_value* lept_get_object_value(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index >= 0 && index < v->u.o.size);
    return &(v->u.o.m[index].v);
}
lept_value*  lept_get_object_value_by_key(const lept_value* v, const char* k, size_t klen) {
    size_t size, i;
    assert(v != NULL && v->type == LEPT_OBJECT && k != NULL);
    size = v->u.o.size;
    for(i = 0; i < size; i++) {
        if (klen != v->u.o.m[i].klen) {
            continue;
        }
        if (memcmp(k, v->u.o.m[i].k, klen) == 0) {
            return &(v->u.o.m[i].v);
        }else {
            continue;
        }
    }
    return NULL;
}

static int lept_stringify_string(lept_context* c, const char* str, size_t len) {
    size_t i;
    assert(str != NULL);
    PUTC(c, '\"');
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        switch(ch) {
            case '\"':  PUTS(c, "\\\"", 2);break;
            case '\\':  PUTS(c, "\\\\", 2);break;
            case '\b':  PUTS(c, "\\b" , 2);break;
            case '\f':  PUTS(c, "\\f", 2);break;
            case '\n':  PUTS(c, "\\n" , 2);break;
            case '\r':  PUTS(c, "\\r" , 2);break;
            case '\t':  PUTS(c, "\\t" , 2);break;
            default: 
                if (ch < 0x20) {
                    char buffer[7];
                    sprintf(buffer, "\\u%04x", ch);
                    PUTS(c, buffer, 6);
                }else {
                    PUTC(c, ch);
                }
        }
    }
    PUTC(c, '\"');
    return LEPT_STRINGIFY_OK; 
}

static int lept_stringify_value(lept_context* c, const lept_value* v) {
    size_t i;

    switch (v->type) {
        case LEPT_NULL:  PUTS(c, "null", 4);break;
        case LEPT_FALSE: PUTS(c, "false", 5);break;
        case LEPT_TRUE:  PUTS(c, "true", 4);break;
        case LEPT_NUMBER:
            {
                char* buffer = lept_context_push(c, 32);
                int length = sprintf(buffer, "%.17g", v->u.n);
                c->top -= (32 - length);
            }
            break;
        case LEPT_ARRAY:
            {
                PUTC(c, '[');
                for (i = 0; i < v->u.a.size; i++) {
                    if (i > 0 ) {
                        PUTC(c, ',');
                    }
                    lept_stringify_value(c, &v->u.a.e[i]); 
                }
                PUTC(c, ']');
            }
            break;
        case LEPT_OBJECT: 
            {
                PUTC(c, '{');
                for (i = 0; i < v->u.o.size; i++) {
                    if ( i > 0) {
                        PUTC(c, ',');
                    }
                    lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                    PUTC(c, ':');
                    lept_stringify_value(c, &v->u.o.m[i].v);
                }
                PUTC(c, '}');
            }
            break;
        case LEPT_STRING:
            lept_stringify_string(c, v->u.s.s, v->u.s.len);
            break;
        default:
            /* error */
            break;
    }
    return LEPT_STRINGIFY_OK;
}

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

int lept_stringify(const lept_value* v, char** json, size_t* length) {
    lept_context c;
    int ret;
    assert (v != NULL);
    assert(json != NULL);
    c.stack = (char*) malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    if ((ret = lept_stringify_value(&c, v) ) != LEPT_STRINGIFY_OK) {
        free(c.stack);
        *json = NULL;
        return ret;
    }
    
    if ( length ) {
        *length = c.top;
    }
    PUTC(&c, '\0');
    *json = c.stack;
    return LEPT_STRINGIFY_OK;
}
