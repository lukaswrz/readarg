#pragma once

#include <stdio.h>

#define READARG_STRINGS(...) ((char *[]){__VA_ARGS__, NULL})

enum readarg_error
{
    READARG_ERROR_SUCCESS,
    READARG_ERROR_NOVAL,
    READARG_ERROR_NOTREQ,
    READARG_ERROR_NOTOPT,
    READARG_ERROR_RANGEOPT,
    READARG_ERROR_RANGEOPER
};

enum readarg_form
{
    READARG_FORM_SHORT,
    READARG_FORM_LONG
};

enum readarg_format
{
    READARG_FORMAT_PLAIN,
    READARG_FORMAT_MDOC,
};

struct readarg_view_strings
{
    const char **strings;
    size_t len;
};

struct readarg_bounds
{
    /* The upper value will be ignored if inf is non-zero. */
    size_t val[2];
    int inf;
};

struct readarg_oper
{
    char *name;
    struct readarg_bounds bounds;
    struct readarg_view_strings val;
};

struct readarg_opt
{
    /* Two null-terminated arrays of either long or short option names. */
    char **names[2];

    struct
    {
        int req;

        /* oper.name is the name of the value itself (not the option). */
        struct readarg_oper oper;
    } cont;
};

struct readarg_parser
{
    struct readarg_opt *opts;
    struct readarg_oper *opers;
    struct readarg_view_strings args;
    struct
    {
        int pending;
        const char *grppos;
        struct
        {
            struct readarg_opt *opt;
            /* Reference to the current argument being parsed. */
            const char **arg;
            /* Reference to the last element of the option/operand value view. */
            const char **eoval;
            /* Intermediate operands which have not yet been assigned. */
            struct readarg_view_strings ioper;
        } curr;
    } state;
    enum readarg_error error;
};

/* Iteratively parse the arguments. */
int readarg_parse(struct readarg_parser *rp);
/* args should always exclude the first element. */
void readarg_parser_init(struct readarg_parser *rp, struct readarg_opt *opts, struct readarg_oper *opers, struct readarg_view_strings args);
/* Check whether the argument is a valid option (can be used to check for the end of an array of options). */
int readarg_validate_opt(struct readarg_opt *opt);
/* Check whether the argument is a valid operand (can be used to check for the end of an array of operands). */
int readarg_validate_oper(struct readarg_oper *oper);
/* Check whether the operand's values are within the defined limits. */
int readarg_validate_within(struct readarg_oper *oper);
/* Get the upper limit. */
size_t readarg_select_upper(struct readarg_bounds bounds);
/* Get the lower limit. This does not always return the minimum. */
size_t readarg_select_lower(struct readarg_bounds bounds);

#ifdef READARG_IMPLEMENTATION

#include <assert.h>
#include <string.h>

static void readarg_parse_arg(struct readarg_parser *rp, const char *arg);

static void readarg_parse_opt(struct readarg_parser *rp, enum readarg_form form, const char **pos);

static struct readarg_opt *readarg_match_opt(struct readarg_parser *rp, enum readarg_form form, const char **needle);
static struct readarg_opt *readarg_match_finish(struct readarg_parser *rp, const char **needle, const char *adv, struct readarg_opt *opt);

static void readarg_update_opt(struct readarg_parser *rp, const char *attach, struct readarg_opt *opt);
static void readarg_update_oper(struct readarg_parser *rp, struct readarg_view_strings val);

static void readarg_assign_opers(struct readarg_parser *rp);

static void readarg_add_val(struct readarg_parser *rp, struct readarg_oper *oper, const char *string, int end);

static const char *readarg_skip_incl(const char *outer, const char *inner);

static void readarg_occ_opt(struct readarg_parser *rp, struct readarg_opt *opt);

static void readarg_permute_val(struct readarg_parser *rp, struct readarg_view_strings *target, const char *val, int end);
static void readarg_incr_between(const char **start, const char **stop, struct readarg_view_strings *curr, struct readarg_view_strings *exclude);
static void readarg_permute_rest(const char **target, struct readarg_view_strings start);

int readarg_parse(struct readarg_parser *rp)
{
    /* Check whether the current offset is at the end of argv. */
    size_t off = rp->state.curr.arg - rp->args.strings;
    if (off >= rp->args.len)
    {
        if (rp->state.pending)
        {
            /* The last specified option required an argument, but no argument has been provided. */
            rp->error = READARG_ERROR_NOVAL;
            return 0;
        }

        for (size_t i = 0; readarg_validate_opt(rp->opts + i); i++)
        {
            if (!readarg_validate_within(&rp->opts[i].cont.oper))
            {
                rp->error = READARG_ERROR_RANGEOPT;
                return 0;
            }
        }

        readarg_assign_opers(rp);
        return 0;
    }

    if (rp->state.pending)
    {
        readarg_add_val(rp, &rp->state.curr.opt->cont.oper, *rp->state.curr.arg, 0);
        ++rp->state.curr.arg;
        return !rp->error;
    }

    readarg_parse_arg(rp, *rp->state.curr.arg);

    /* If grouped options are still being parsed, they should not be discarded. */
    if (!rp->state.grppos)
        ++rp->state.curr.arg;

    return !rp->error;
}

static void readarg_parse_arg(struct readarg_parser *rp, const char *arg)
{
    /* Parse the next option in the grouped option string, which automatically advances it. */
    if (rp->state.grppos)
    {
        readarg_parse_opt(rp, READARG_FORM_SHORT, &rp->state.grppos);
        if (!*rp->state.grppos)
        {
            rp->state.grppos = NULL;
        }
        return;
    }

    const char *pos = arg;

    switch (*pos)
    {
    case '-':
        ++pos;
        switch (*pos)
        {
        case '-':
            ++pos;
            switch (*pos)
            {
                size_t off;
            case '\0':
                /* "--" denotes the end of options. */
                off = rp->args.len - (rp->state.curr.arg - rp->args.strings);
                assert(off);
                if (off == 1)
                {
                    /* No operands after the "--". */
                    return;
                }

                readarg_update_oper(rp, (struct readarg_view_strings){
                                    .len = off - 1,
                                    .strings = rp->state.curr.arg + 1});
                rp->state.curr.arg = rp->args.strings + rp->args.len - 1;

                return;
            default:
                readarg_parse_opt(rp, READARG_FORM_LONG, &pos);
                return;
            }
        case '\0':
            readarg_update_oper(rp, (struct readarg_view_strings){.len = 1, .strings = (const char *[]){arg}});
            return;
        default:
            readarg_parse_opt(rp, READARG_FORM_SHORT, &pos);
            return;
        }
    default:
        readarg_update_oper(rp, (struct readarg_view_strings){.len = 1, .strings = (const char *[]){arg}});
        return;
    }
}

static void readarg_parse_opt(struct readarg_parser *rp, enum readarg_form form, const char **pos)
{
    struct readarg_opt *match;
    assert(form == READARG_FORM_SHORT || form == READARG_FORM_LONG);

    if (form == READARG_FORM_SHORT)
    {
        match = readarg_match_opt(rp, form, pos);
        if (match)
        {
            const char *strpos = *pos;

            if (!match->cont.req && *strpos)
            {
                rp->state.grppos = strpos;
                readarg_update_opt(rp, NULL, match);
            }
            else
            {
                readarg_update_opt(rp, *strpos ? strpos : NULL, match);
            }
        }
        else
        {
            rp->error = READARG_ERROR_NOTOPT;
        }
    }
    else
    {
        /* Match and advance pos to the end of the match. */
        match = readarg_match_opt(rp, form, pos);

        if (match)
        {
            switch (**pos)
            {
            case '\0':
                readarg_update_opt(rp, NULL, match);
                break;
            case '=':
                ++(*pos);
                readarg_update_opt(rp, *pos, match);
                break;
            default:
                rp->error = READARG_ERROR_NOTOPT;
                break;
            }
        }
        else
        {
            rp->error = READARG_ERROR_NOTOPT;
        }
    }
}

static struct readarg_opt *readarg_match_opt(struct readarg_parser *rp, enum readarg_form form, const char **needle)
{
    /* This represents the last inexact match. */
    struct
    {
        /* The current advanced string. */
        const char *adv;
        /* The current option. */
        struct readarg_opt *opt;
    } loose = {0};

    struct readarg_opt *haystack = rp->opts;
    for (size_t i = 0; readarg_validate_opt(haystack + i); i++)
    {
        /* Iterate through all short or long names of the current option. */
        char **names = haystack[i].names[form];

        if (!names)
            /* Ignore the option as it does not have names in the required form. */
            continue;

        const char *cmp = loose.adv;

        for (size_t j = 0; names[j]; j++)
        {
            char *name = names[j];
            cmp = readarg_skip_incl(*needle, name);

            if (!cmp)
                continue;

            if (!*cmp)
                /* A guaranteed match. */
                return readarg_match_finish(rp, needle, cmp, haystack + i);
            else if ((cmp - *needle) > (loose.adv - *needle))
                /* Maybe a match, maybe not. */
                loose.adv = cmp, loose.opt = haystack + i;
        }
    }

    return readarg_match_finish(rp, needle, loose.adv, loose.opt);
}

static struct readarg_opt *readarg_match_finish(struct readarg_parser *rp, const char **needle, const char *adv, struct readarg_opt *opt)
{
    if (adv)
        *needle = adv;

    if (opt)
        rp->state.curr.opt = opt;

    return opt;
}

static void readarg_update_opt(struct readarg_parser *rp, const char *attach, struct readarg_opt *opt)
{
    if (opt->cont.req)
    {
        if (attach)
        {
            /* --opt=value, --opt=, -ovalue */
            struct readarg_oper *curr = &rp->state.curr.opt->cont.oper;
            readarg_occ_opt(rp, opt);
            readarg_add_val(rp, curr, attach, 0);
        }
        else
        {
            /* --opt value, -o value */
            rp->state.pending = 1;
            readarg_occ_opt(rp, opt);
        }
    }
    else
    {
        readarg_occ_opt(rp, opt);
        if (attach)
            rp->error = READARG_ERROR_NOTREQ;
    }
}

static void readarg_update_oper(struct readarg_parser *rp, struct readarg_view_strings val)
{
    assert(val.len && val.strings);

    if (val.len == 1)
    {
        ++rp->state.curr.ioper.len;
        readarg_permute_val(rp, &rp->state.curr.ioper, val.strings[0], 1);
    }
    else
    {
        readarg_permute_rest(rp->state.curr.eoval, val);
        rp->state.curr.ioper.len += val.len;
    }
}

static void readarg_assign_opers(struct readarg_parser *rp)
{
    size_t count = rp->state.curr.ioper.len;

    size_t nlower = 0;
    size_t nupper = 0;
    for (size_t i = 0; readarg_validate_oper(rp->opers + i); i++)
    {
        nlower += readarg_select_lower(rp->opers[i].bounds);
        nupper += readarg_select_upper(rp->opers[i].bounds);
    }

    if (count < nlower)
    {
        rp->error = READARG_ERROR_RANGEOPER;
        return;
    }

    struct
    {
        size_t extra;
        size_t req;
    } rest = {
        count - nlower,
        nlower};

    for (size_t i = 0; readarg_validate_oper(rp->opers + i); i++)
    {
        if (count == 0 || !rp->opers[i].val.strings)
        {
            size_t off = count - (rest.extra + rest.req);
            rp->opers[i].val.strings = rp->state.curr.ioper.strings + off;
        }

        size_t lower = readarg_select_lower(rp->opers[i].bounds);
        size_t upper = readarg_select_upper(rp->opers[i].bounds);
        int inf = rp->opers[i].bounds.inf;

        size_t add;

        /* Add required elements. */
        add = rest.req > lower ? lower : rest.req;
        rp->opers[i].val.len += add, rest.req -= add;

        /* Add optional elements. */
        add = inf ? rest.extra : rest.extra > upper ? upper
                                                    : rest.extra;
        rp->opers[i].val.len += add, rest.extra -= add;
    }

    if (rest.extra || rest.req)
        rp->error = READARG_ERROR_RANGEOPER;
}

static void readarg_add_val(struct readarg_parser *rp, struct readarg_oper *oper, const char *string, int end)
{
    rp->state.pending = 0;

    if (!readarg_validate_within(oper))
        rp->error = READARG_ERROR_RANGEOPT;
    else
        readarg_permute_val(rp, &oper->val, string, end);
}

static const char *readarg_skip_incl(const char *outer, const char *inner)
{
    while (*inner == *outer)
    {
        if (!*inner)
            return outer;
        ++inner, ++outer;
    }
    return !*inner ? outer : NULL;
}

static void readarg_occ_opt(struct readarg_parser *rp, struct readarg_opt *opt)
{
    assert(opt);
    rp->state.curr.opt = opt;
    ++rp->state.curr.opt->cont.oper.val.len;
}

static void readarg_permute_val(struct readarg_parser *rp, struct readarg_view_strings *target, const char *val, int end)
{
    if (!target->strings)
        /* Fallback position when no value has yet been set. */
        target->strings = rp->state.curr.eoval - (end ? 0 : rp->state.curr.ioper.len);

    const char **pos = target->strings + (target->len - 1);

    assert(rp->state.curr.arg >= rp->state.curr.eoval);

    memmove(pos + 1, pos, (rp->state.curr.eoval - pos) * sizeof *pos);

    *pos = val;
    ++rp->state.curr.eoval;

    const char **start = pos, **stop = rp->state.curr.eoval;

    /* Increment all value pointers in the options which are between start and stop (inclusive). */
    for (size_t i = 0; readarg_validate_opt(rp->opts + i); i++)
        readarg_incr_between(start, stop, &rp->opts[i].cont.oper.val, target);

    readarg_incr_between(start, stop, &rp->state.curr.ioper, target);
}

static void readarg_incr_between(const char **start, const char **stop, struct readarg_view_strings *curr, struct readarg_view_strings *exclude)
{
    if (curr->strings >= start && curr->strings <= stop && curr != exclude)
        ++curr->strings;
}

static void readarg_permute_rest(const char **target, struct readarg_view_strings start)
{
    memmove(target, start.strings, start.len * sizeof *start.strings);
}

void readarg_parser_init(struct readarg_parser *rp, struct readarg_opt *opts, struct readarg_oper *opers, struct readarg_view_strings args)
{
    *rp = (struct readarg_parser){
        .args = args,
        .opts = opts,
        .opers = opers,
        .state.curr = {
            .arg = args.strings,
            .eoval = args.strings}};
}

int readarg_validate_opt(struct readarg_opt *opt)
{
    assert(opt);
    return opt->names[0] || opt->names[1];
}

int readarg_validate_oper(struct readarg_oper *oper)
{
    assert(oper);
    return !!oper->name;
}

int readarg_validate_within(struct readarg_oper *oper)
{
    size_t occ = oper->val.len;
    size_t upper = readarg_select_upper(oper->bounds);
    size_t lower = readarg_select_lower(oper->bounds);
    return occ >= lower && (occ <= upper || oper->bounds.inf);
}

size_t readarg_select_upper(struct readarg_bounds bounds)
{
    return bounds.val[0] > bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

size_t readarg_select_lower(struct readarg_bounds bounds)
{
    return bounds.inf ? readarg_select_upper(bounds) : bounds.val[0] < bounds.val[1] ? bounds.val[0]
                                                                                     : bounds.val[1];
}

#endif
