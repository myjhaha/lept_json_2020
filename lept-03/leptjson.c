#include "leptjson.h"

#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

#include <math.h>
#include <errno.h>

#include <stdio.h>  /* test */

#define EXPECT(c, ch)   do{assert(*c->json == (ch));c->json++;}while(0)

#define IS_DIGIT(ch)      ((ch) >= '0' && (ch) <= '9')
#define IS_DIGIT_1_9(ch)  ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char *json;

}lept_context;

static void lept_parse_whitespace(lept_context *c) {
   const char *p = c->json;
   while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'){
       p++;
   }
   c->json = p;
}

static int lept_parse_literal(lept_context *c, lept_value *v, const char *literal, lept_type type) {
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
static int lept_parse_number(lept_context *c, lept_value *v) {
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

static int lept_parse_value(lept_context *c, lept_value *v) {
    switch( *c->json ) {
        case 'n'  : return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't'  : return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f'  : return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\0' : return LEPT_PARSE_EXPECT_VALUE;
        default :   return lept_parse_number(c, v);
    }
}

int lept_parse(lept_value *v, const char *json) {
    int ret = 0;
    lept_context c;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ( (ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK ) {
        lept_parse_whitespace(&c);
        if ( *(c.json) != '\0') {
            v->type = LEPT_NULL; 
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value *v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value *v) {
    assert(v!= NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

