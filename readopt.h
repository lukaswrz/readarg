/* vim:set sw=4 ts=4 et: */

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

#ifdef READOPT_IMPLEMENTATION

#include <assert.h>
#include <string.h>

static void parse_arg(struct readopt_parser *rp, const char *arg);

static void parse_opt(struct readopt_parser *rp, enum readopt_form form, const char **pos);

static struct readopt_opt *match_opt(struct readopt_parser *rp, enum readopt_form form, const char **needle);
static struct readopt_opt *match_finish(struct readopt_parser *rp, const char **needle, const char *adv, struct readopt_opt *opt);

static void update_opt(struct readopt_parser *rp, const char *attach, struct readopt_opt *opt);
static void update_oper(struct readopt_parser *rp, struct readopt_view_strings val);

static void assign_opers(struct readopt_parser *rp);

static void add_val(struct readopt_parser *rp, struct readopt_oper *oper, const char *string, int end);

static const char *skip_incl(const char *outer, const char *inner);

static void occ_opt(struct readopt_parser *rp, struct readopt_opt *opt);

static void permute_val(struct readopt_parser *rp, struct readopt_view_strings *target, const char *val, int end);
static void incr_between(const char **start, const char **stop, struct readopt_view_strings *curr, struct readopt_view_strings *exclude);
static void permute_rest(const char **target, struct readopt_view_strings start);

int readopt_parse(struct readopt_parser *rp)
{
    /* Check whether the current offset is at the end of argv. */
    size_t off = rp->state.curr.arg - rp->args.strings;
    if (off >= rp->args.len)
    {
        if (rp->state.pending)
        {
            /* The last specified option required an argument, but no argument has been provided. */
            rp->error = READOPT_ERROR_NOVAL;
            return 0;
        }

        for (size_t i = 0; readopt_validate_opt(rp->opts + i); i++)
        {
            if (!readopt_validate_within(&rp->opts[i].cont.oper))
            {
                rp->error = READOPT_ERROR_RANGEOPT;
                return 0;
            }
        }

        assign_opers(rp);
        return 0;
    }

    if (rp->state.pending)
    {
        add_val(rp, &rp->state.curr.opt->cont.oper, *rp->state.curr.arg, 0);
        ++rp->state.curr.arg;
        return !rp->error;
    }

    parse_arg(rp, *rp->state.curr.arg);

    /* If grouped options are still being parsed, they should not be discarded. */
    if (!rp->state.grppos)
        ++rp->state.curr.arg;

    return !rp->error;
}

static void parse_arg(struct readopt_parser *rp, const char *arg)
{
    /* Parse the next option in the grouped option string, which automatically advances it. */
    if (rp->state.grppos)
    {
        parse_opt(rp, READOPT_FORM_SHORT, &rp->state.grppos);
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

                update_oper(rp, (struct readopt_view_strings){
                                    .len = off - 1,
                                    .strings = rp->state.curr.arg + 1});
                rp->state.curr.arg = rp->args.strings + rp->args.len - 1;

                return;
            default:
                parse_opt(rp, READOPT_FORM_LONG, &pos);
                return;
            }
        case '\0':
            update_oper(rp, (struct readopt_view_strings){.len = 1, .strings = (const char *[]){arg}});
            return;
        default:
            parse_opt(rp, READOPT_FORM_SHORT, &pos);
            return;
        }
    default:
        update_oper(rp, (struct readopt_view_strings){.len = 1, .strings = (const char *[]){arg}});
        return;
    }
}

static void parse_opt(struct readopt_parser *rp, enum readopt_form form, const char **pos)
{
    struct readopt_opt *match;
    assert(form == READOPT_FORM_SHORT || form == READOPT_FORM_LONG);

    if (form == READOPT_FORM_SHORT)
    {
        match = match_opt(rp, form, pos);
        if (match)
        {
            const char *strpos = *pos;

            if (!match->cont.req && *strpos)
            {
                rp->state.grppos = strpos;
                update_opt(rp, NULL, match);
            }
            else
            {
                update_opt(rp, *strpos ? strpos : NULL, match);
            }
        }
        else
        {
            rp->error = READOPT_ERROR_NOTOPT;
        }
    }
    else
    {
        /* Match and advance pos to the end of the match. */
        match = match_opt(rp, form, pos);

        if (match)
        {
            switch (**pos)
            {
            case '\0':
                update_opt(rp, NULL, match);
                break;
            case '=':
                ++(*pos);
                update_opt(rp, *pos, match);
                break;
            default:
                rp->error = READOPT_ERROR_NOTOPT;
                break;
            }
        }
        else
        {
            rp->error = READOPT_ERROR_NOTOPT;
        }
    }
}

static struct readopt_opt *match_opt(struct readopt_parser *rp, enum readopt_form form, const char **needle)
{
    /* This represents the last inexact match. */
    struct
    {
        /* The current advanced string. */
        const char *adv;
        /* The current option. */
        struct readopt_opt *opt;
    } loose = {0};

    struct readopt_opt *haystack = rp->opts;
    for (size_t i = 0; readopt_validate_opt(haystack + i); i++)
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
            cmp = skip_incl(*needle, name);

            if (!cmp)
                continue;

            if (!*cmp)
                /* A guaranteed match. */
                return match_finish(rp, needle, cmp, haystack + i);
            else if ((cmp - *needle) > (loose.adv - *needle))
                /* Maybe a match, maybe not. */
                loose.adv = cmp, loose.opt = haystack + i;
        }
    }

    return match_finish(rp, needle, loose.adv, loose.opt);
}

static struct readopt_opt *match_finish(struct readopt_parser *rp, const char **needle, const char *adv, struct readopt_opt *opt)
{
    if (adv)
        *needle = adv;

    if (opt)
        rp->state.curr.opt = opt;

    return opt;
}

static void update_opt(struct readopt_parser *rp, const char *attach, struct readopt_opt *opt)
{
    if (opt->cont.req)
    {
        if (attach)
        {
            /* --opt=value, --opt=, -ovalue */
            struct readopt_oper *curr = &rp->state.curr.opt->cont.oper;
            occ_opt(rp, opt);
            add_val(rp, curr, attach, 0);
        }
        else
        {
            /* --opt value, -o value */
            rp->state.pending = 1;
            occ_opt(rp, opt);
        }
    }
    else
    {
        occ_opt(rp, opt);
        if (attach)
            rp->error = READOPT_ERROR_NOTREQ;
    }
}

static void update_oper(struct readopt_parser *rp, struct readopt_view_strings val)
{
    assert(val.len && val.strings);

    if (val.len == 1)
    {
        ++rp->state.curr.ioper.len;
        permute_val(rp, &rp->state.curr.ioper, val.strings[0], 1);
    }
    else
    {
        permute_rest(rp->state.curr.eoval, val);
        rp->state.curr.ioper.len += val.len;
    }
}

static void assign_opers(struct readopt_parser *rp)
{
    size_t count = rp->state.curr.ioper.len;

    size_t nlower = 0;
    size_t nupper = 0;
    for (size_t i = 0; readopt_validate_oper(rp->opers + i); i++)
    {
        nlower += readopt_select_lower(rp->opers[i].bounds);
        nupper += readopt_select_upper(rp->opers[i].bounds);
    }

    if (count < nlower)
    {
        rp->error = READOPT_ERROR_RANGEOPER;
        return;
    }

    struct
    {
        size_t extra;
        size_t req;
    } rest = {
        count - nlower,
        nlower};

    for (size_t i = 0; readopt_validate_oper(rp->opers + i); i++)
    {
        if (count == 0 || !rp->opers[i].val.strings)
        {
            size_t off = count - (rest.extra + rest.req);
            rp->opers[i].val.strings = rp->state.curr.ioper.strings + off;
        }

        size_t lower = readopt_select_lower(rp->opers[i].bounds);
        size_t upper = readopt_select_upper(rp->opers[i].bounds);
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
        rp->error = READOPT_ERROR_RANGEOPER;
}

static void add_val(struct readopt_parser *rp, struct readopt_oper *oper, const char *string, int end)
{
    rp->state.pending = 0;

    if (!readopt_validate_within(oper))
        rp->error = READOPT_ERROR_RANGEOPT;
    else
        permute_val(rp, &oper->val, string, end);
}

static const char *skip_incl(const char *outer, const char *inner)
{
    while (*inner == *outer)
    {
        if (!*inner)
            return outer;
        ++inner, ++outer;
    }
    return !*inner ? outer : NULL;
}

static void occ_opt(struct readopt_parser *rp, struct readopt_opt *opt)
{
    assert(opt);
    rp->state.curr.opt = opt;
    ++rp->state.curr.opt->cont.oper.val.len;
}

static void permute_val(struct readopt_parser *rp, struct readopt_view_strings *target, const char *val, int end)
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
    for (size_t i = 0; readopt_validate_opt(rp->opts + i); i++)
        incr_between(start, stop, &rp->opts[i].cont.oper.val, target);

    incr_between(start, stop, &rp->state.curr.ioper, target);
}

static void incr_between(const char **start, const char **stop, struct readopt_view_strings *curr, struct readopt_view_strings *exclude)
{
    if (curr->strings >= start && curr->strings <= stop && curr != exclude)
        ++curr->strings;
}

static void permute_rest(const char **target, struct readopt_view_strings start)
{
    memmove(target, start.strings, start.len * sizeof *start.strings);
}

void readopt_parser_init(struct readopt_parser *rp, struct readopt_opt *opts, struct readopt_oper *opers, struct readopt_view_strings args)
{
    *rp = (struct readopt_parser){
        .args = args,
        .opts = opts,
        .opers = opers,
        .state.curr = {
            .arg = args.strings,
            .eoval = args.strings}};
}

int readopt_validate_opt(struct readopt_opt *opt)
{
    assert(opt);
    return opt->names[0] || opt->names[1];
}

int readopt_validate_oper(struct readopt_oper *oper)
{
    assert(oper);
    return !!oper->name;
}

int readopt_validate_within(struct readopt_oper *oper)
{
    size_t occ = oper->val.len;
    size_t upper = readopt_select_upper(oper->bounds);
    size_t lower = readopt_select_lower(oper->bounds);
    return occ >= lower && (occ <= upper || oper->bounds.inf);
}

size_t readopt_select_upper(struct readopt_bounds bounds)
{
    return bounds.val[0] > bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

size_t readopt_select_lower(struct readopt_bounds bounds)
{
    return bounds.inf ? readopt_select_upper(bounds) : bounds.val[0] < bounds.val[1] ? bounds.val[0]
                                                                                     : bounds.val[1];
}

#endif
