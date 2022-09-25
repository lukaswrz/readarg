#pragma once

#include <stdio.h>

#define READOPT_ALLOC_STRINGS(...) ((char *[]){__VA_ARGS__, NULL})

enum readopt_error
{
    READOPT_ERROR_SUCCESS,
    READOPT_ERROR_NOVAL,
    READOPT_ERROR_NOTREQ,
    READOPT_ERROR_NOTOPT,
    READOPT_ERROR_RANGEOPT,
    READOPT_ERROR_RANGEOPER
};

enum readopt_form
{
    READOPT_FORM_SHORT,
    READOPT_FORM_LONG
};

enum readopt_format
{
    READOPT_FORMAT_PLAIN,
    READOPT_FORMAT_MDOC,
};

struct readopt_view_strings
{
    const char **strings;
    size_t len;
};

struct readopt_bounds
{
    /* The upper value will be ignored if inf is non-zero. */
    size_t val[2];
    int inf;
};

struct readopt_oper
{
    char *name;
    struct readopt_bounds bounds;
    struct readopt_view_strings val;
};

struct readopt_opt
{
    /* Two null-terminated arrays of either long or short option names. */
    char **names[2];

    struct
    {
        int req;

        /* oper.name is the name of the value itself (not the option). */
        struct readopt_oper oper;
    } cont;
};

struct readopt_parser
{
    struct readopt_opt *opts;
    struct readopt_oper *opers;
    struct readopt_view_strings args;
    struct
    {
        int pending;
        const char *grppos;
        struct
        {
            struct readopt_opt *opt;
            /* Reference to the current argument being parsed. */
            const char **arg;
            /* Reference to the last element of the option/operand value view. */
            const char **eoval;
            /* Intermediate operands which have not yet been assigned. */
            struct readopt_view_strings ioper;
        } curr;
    } state;
    enum readopt_error error;
};

/* Iteratively parse the arguments. */
int readopt_parse(struct readopt_parser *rp);
/* args should always exclude the first element. */
void readopt_parser_init(struct readopt_parser *rp, struct readopt_opt *opts, struct readopt_oper *opers, struct readopt_view_strings args);
/* Check whether the argument is a valid option (can be used to check for the end of an array of options). */
int readopt_validate_opt(struct readopt_opt *opt);
/* Check whether the argument is a valid operand (can be used to check for the end of an array of operands). */
int readopt_validate_oper(struct readopt_oper *oper);
/* Check whether the operand's values are within the defined limits. */
int readopt_validate_within(struct readopt_oper *oper);
/* Get the upper limit. */
size_t readopt_select_upper(struct readopt_bounds bounds);
/* Get the lower limit. This does not always return the minimum. */
size_t readopt_select_lower(struct readopt_bounds bounds);
