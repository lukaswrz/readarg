#include <string.h>
#include <assert.h>

#include "readopt.h"

static enum readopt_status parse_arg(struct readopt_parser *rp, char *arg);

static enum readopt_status parse_opt(struct readopt_parser *rp, enum readopt_form form, char **pos);

static struct readopt_opt *match_opt(struct readopt_parser *rp, enum readopt_form form, char **needle);
static struct readopt_opt *match_finish(struct readopt_parser *rp, char **needle, char *cmp, struct readopt_opt *opt);

static enum readopt_status update_opt(struct readopt_parser *rp, char *attach, struct readopt_opt *opt);
static enum readopt_status update_oper(struct readopt_parser *rp, struct readopt_view_strings val);

static enum readopt_status assign_opers(struct readopt_parser *rp);

static enum readopt_status add_val(struct readopt_parser *rp, struct readopt_oper *oper, char *str, int end);

static char *skip_incl(const char *inner, char *outer);

static enum readopt_status occ_opt(struct readopt_parser *rp, struct readopt_opt *opt);

/* permutes the argument list to store a value for an option or operand */
static void permute_val(struct readopt_parser *rp, struct readopt_view_strings *target, char *val, int end);
static void incr_between(char **start, char **stop, struct readopt_view_strings *curr, struct readopt_view_strings *exclude);
static void permute_rest(char **target, struct readopt_view_strings start);

static void format_usage_opts(struct readopt_format_context *ctx, struct readopt_opt *opts);
static void format_usage_opers(struct readopt_format_context *ctx, struct readopt_oper *opers);

struct readopt_answer
readopt_parse_all(struct readopt_parser *rp)
{
	struct readopt_answer answer;

	do
		answer = readopt_parse(rp);
	while (!answer.end && answer.status == READOPT_STATUS_SUCCESS);

	return answer;
}

struct readopt_answer
readopt_parse(struct readopt_parser *rp)
{
	/* check whether the current offset is at the end of argv */
	size_t off = rp->state.curr.arg - rp->args.strings;
	if (off >= rp->args.len) {
		if (rp->state.pending)
			/* the last specified option required an argument, but has been ignored */
			return (struct readopt_answer){1, READOPT_STATUS_NOVAL};

		for (size_t i = 0; readopt_validate_opt(rp->opts + i); i++)
			if (!readopt_validate_within(&rp->opts[i].cont.oper))
				return (struct readopt_answer){1, READOPT_STATUS_RANGEOPT};

		return (struct readopt_answer){1, assign_opers(rp)};
	}

	if (rp->state.pending) {
		enum readopt_status status = add_val(rp, &rp->state.curr.opt->cont.oper, *rp->state.curr.arg, 0);
		++rp->state.curr.arg;
		return (struct readopt_answer){0, status};
	}

	enum readopt_status status = parse_arg(rp, *rp->state.curr.arg);

	/* if grouped options are still being parsed, they should not be discarded */
	if (!rp->state.grppos)
		++rp->state.curr.arg;

	return (struct readopt_answer){0, status};
}

static enum readopt_status
parse_arg(struct readopt_parser *rp, char *arg)
{
	/* parse the next option in the grouped option string, which automatically advances it */
	if (rp->state.grppos) {
		enum readopt_status s = parse_opt(rp, READOPT_FORM_SHORT, &rp->state.grppos);
		if (!*rp->state.grppos) {
			rp->state.grppos = NULL;
		}
		return s;
	}

	char *pos = arg;

	switch (*pos) {
	case '-':
		++pos;
		switch (*pos) {
		case '-':
			++pos;
			switch (*pos) {
				size_t off;
			case '\0':
				/* "--" denotes the end of options */
				off = rp->args.len - (rp->state.curr.arg - rp->args.strings);
				assert(off);
				if (off == 1) {
					/* no operands after the "--" */
					return READOPT_STATUS_SUCCESS;
				} else {
					enum readopt_status s = update_oper(rp, (struct readopt_view_strings){
						.len = off - 1,
						.strings = rp->state.curr.arg + 1
					});
					rp->state.curr.arg = rp->args.strings + rp->args.len - 1;

					return s;
				}
			default:
				return parse_opt(rp, READOPT_FORM_LONG, &pos);
			}
		case '\0':
			return update_oper(rp, (struct readopt_view_strings){.len = 1, .strings = (char *[]){arg}});
		default:
			return parse_opt(rp, READOPT_FORM_SHORT, &pos);
		}
	default:
		return update_oper(rp, (struct readopt_view_strings){.len = 1, .strings = (char *[]){arg}});
	}
}

static enum readopt_status
parse_opt(struct readopt_parser *rp, enum readopt_form form, char **pos)
{
	struct readopt_opt *match;
	assert(form == READOPT_FORM_SHORT || form == READOPT_FORM_LONG);

	if (form == READOPT_FORM_SHORT) {
		match = match_opt(rp, form, pos);
		if (match) {
			char *strpos = *pos;

			if (!match->cont.req && *strpos) {
				rp->state.grppos = strpos;
				return update_opt(rp, NULL, match);
			}

			return update_opt(rp, *strpos ? strpos : NULL, match);
		} else {
			return READOPT_STATUS_NOTOPT;
		}
	} else {
		/* match and advance pos to the end of the match */
		match = match_opt(rp, form, pos);

		if (match) {
			switch (**pos) {
			case '\0':
				return update_opt(rp, NULL, match);
			case '=':
				++(*pos);
				return update_opt(rp, *pos, match);
			default:
				return READOPT_STATUS_NOTOPT;
			}
		} else {
			return READOPT_STATUS_NOTOPT;
		}
	}
}

static struct readopt_opt *
match_opt(struct readopt_parser *rp, enum readopt_form form, char **needle)
{
	/* structure representing the last, inexact match */
	struct {
		/* the current advanced string */
		char *adv;
		/* current option */
		struct readopt_opt *opt;
	} loose = {0};

	struct readopt_opt *haystack = rp->opts;
	for (size_t i = 0; readopt_validate_opt(haystack + i); i++) {
		/* iterate through all names (null-terminated) of the current option with the correct form */
		char **names = haystack[i].names[form];

		if (!names)
			/* ignore the option as it does not have names in the required form */
			continue;

		char *cmp = loose.adv;

		for (size_t j = 0; names[j]; j++) {
			char *name = names[j];
			cmp = skip_incl(name, *needle);

			if (!cmp)
				continue;

			if (!*cmp)
				/* a guaranteed match */
				return match_finish(rp, needle, cmp, haystack + i);
			else if ((cmp - *needle) > (loose.adv - *needle))
				/* maybe a match */
				loose.adv = cmp, loose.opt = haystack + i;
		}
	}

	return match_finish(rp, needle, loose.adv, loose.opt);
}

static struct readopt_opt *
match_finish(struct readopt_parser *rp, char **needle, char *adv, struct readopt_opt *opt)
{
	if (adv)
		*needle = adv;

	if (opt)
		rp->state.curr.opt = opt;

	return opt;
}

static enum readopt_status
update_opt(struct readopt_parser *rp, char *attach, struct readopt_opt *opt)
{
	if (opt->cont.req) {
		if (attach) {
			/* --opt=value, --opt=, -ovalue */
			struct readopt_oper *curr = &rp->state.curr.opt->cont.oper;
			return occ_opt(rp, opt) == READOPT_STATUS_SUCCESS ? add_val(rp, curr, attach, 0) : READOPT_STATUS_SUCCESS;
		} else {
			/* --opt value, -o value */
			rp->state.pending = 1;
			return occ_opt(rp, opt);
		}
	} else {
		enum readopt_status s = occ_opt(rp, opt);
		return attach ? READOPT_STATUS_NOTREQ : s;
	}
}

static enum readopt_status
update_oper(struct readopt_parser *rp, struct readopt_view_strings val)
{
	assert(val.len && val.strings);

	if (val.len == 1) {
		++rp->state.curr.ioper.len;
		permute_val(rp, &rp->state.curr.ioper, val.strings[0], 1);
	} else {
		permute_rest(rp->state.curr.eoval, val);
		rp->state.curr.ioper.len += val.len;
	}

	return READOPT_STATUS_SUCCESS;
}

static enum readopt_status
assign_opers(struct readopt_parser *rp)
{
	size_t count = rp->state.curr.ioper.len;

	size_t nlower = 0;
	size_t nupper = 0;
	for (size_t i = 0; readopt_validate_oper(rp->opers + i); i++) {
		nlower += readopt_select_lower(rp->opers[i].bounds);
		nupper += readopt_select_upper(rp->opers[i].bounds);
	}

	if (count < nlower)
		return READOPT_STATUS_RANGEOPER;

	struct {
		size_t extra;
		size_t req;
	} rest = {
		count - nlower,
		nlower
	};

	for (size_t i = 0; readopt_validate_oper(rp->opers + i); i++) {
		if (count == 0 || !rp->opers[i].val.strings) {
			size_t off = count - (rest.extra + rest.req);
			rp->opers[i].val.strings = rp->state.curr.ioper.strings + off;
		}

		size_t lower = readopt_select_lower(rp->opers[i].bounds);
		size_t upper = readopt_select_upper(rp->opers[i].bounds);
		int inf = rp->opers[i].bounds.inf;

		size_t add;

		/* add required elements */
		add = rest.req > lower ? lower : rest.req;
		rp->opers[i].val.len += add, rest.req -= add;

		/* add optional elements */
		add = inf ? rest.extra : rest.extra > upper ? upper : rest.extra;
		rp->opers[i].val.len += add, rest.extra -= add;
	}

	return rest.extra || rest.req ? READOPT_STATUS_RANGEOPER : READOPT_STATUS_SUCCESS;
}

static enum readopt_status
add_val(struct readopt_parser *rp, struct readopt_oper *oper, char *string, int end)
{
	rp->state.pending = 0;

	if (!readopt_validate_within(oper))
		return READOPT_STATUS_RANGEOPT;

	permute_val(rp, &oper->val, string, end);
	return READOPT_STATUS_SUCCESS;
}

static char *
skip_incl(const char *inner, char *outer)
{
	while (*inner == *outer) {
		if (!*inner)
			return outer;
		++inner, ++outer;
	}
	return !*inner ? outer : NULL;
}

static enum readopt_status
occ_opt(struct readopt_parser *rp, struct readopt_opt *opt)
{
	assert(opt);
	rp->state.curr.opt = opt;
	++rp->state.curr.opt->cont.oper.val.len;
	return READOPT_STATUS_SUCCESS;
}

static void
permute_val(struct readopt_parser *rp, struct readopt_view_strings *target, char *val, int end)
{
	if (!target->strings)
		/* fallback position when no value has been set yet */
		target->strings = rp->state.curr.eoval - (end ? 0 : rp->state.curr.ioper.len);

	char **pos = target->strings + (target->len - 1);

	assert(rp->state.curr.arg >= rp->state.curr.eoval);

	memmove(pos + 1, pos, (rp->state.curr.eoval - pos) * sizeof *pos);

	*pos = val;
	++rp->state.curr.eoval;

	char **start = pos, **stop = rp->state.curr.eoval;

	/* increment all value pointers in the operands and options which are between start and stop, inclusive */
	for (size_t i = 0; readopt_validate_opt(rp->opts + i); i++)
		incr_between(start, stop, &rp->opts[i].cont.oper.val, target);

	incr_between(start, stop, &rp->state.curr.ioper, target);
}

static void
incr_between(char **start, char **stop, struct readopt_view_strings *curr, struct readopt_view_strings *exclude)
{
	if (curr->strings >= start && curr->strings <= stop && curr != exclude)
		++curr->strings;
}

static void
permute_rest(char **target, struct readopt_view_strings start)
{
	memmove(target, start.strings, start.len * sizeof *start.strings);
}

void
readopt_parser_init(struct readopt_parser *rp, struct readopt_opt *opts, struct readopt_oper *opers, struct readopt_view_strings args)
{
	*rp = (struct readopt_parser){
		.args = args,
		.opts = opts,
		.opers = opers,
		.state.curr = {
			.arg = args.strings,
			.eoval = args.strings
		}
	};
}

struct readopt_parser
readopt_parser_create(struct readopt_opt *opts, struct readopt_oper *opers, struct readopt_view_strings args)
{
	struct readopt_parser rp = {0};
	readopt_parser_init(&rp, opts, opers, args);
	return rp;
}

int
readopt_validate_opt(struct readopt_opt *opt)
{
	assert(opt);
	return opt->names[0] || opt->names[1];
}

int
readopt_validate_oper(struct readopt_oper *oper)
{
	assert(oper);
	return !!oper->name;
}

int
readopt_validate_within(struct readopt_oper *oper)
{
	size_t occ = oper->val.len;
	size_t upper = readopt_select_upper(oper->bounds);
	size_t lower = readopt_select_lower(oper->bounds);
	return occ >= lower && (occ <= upper || oper->bounds.inf);
}

size_t
readopt_select_upper(struct readopt_bounds bounds)
{
	return bounds.val[0] > bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

size_t
readopt_select_lower(struct readopt_bounds bounds)
{
	return bounds.inf ? readopt_select_upper(bounds) : bounds.val[0] < bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

char *
readopt_keyval(char *s)
{
	while (*s != '=') ++s;
	*s = '\0';
	return ++s;
}

int
readopt_put_status(enum readopt_status status, struct readopt_write_context *ctx)
{
	const char *s;
	switch (status) {
	case READOPT_STATUS_SUCCESS:   s = "Success"; break;
	case READOPT_STATUS_NOVAL:     s = "Option did not receive its required value"; break;
	case READOPT_STATUS_NOTREQ:    s = "No value required for option"; break;
	case READOPT_STATUS_NOTOPT:    s = "Specified option does not exist"; break;
	case READOPT_STATUS_RANGEOPT:  s = "Option(s) are not within the defined limits"; break;
	case READOPT_STATUS_RANGEOPER: s = "Operand(s) are not within the defined limits"; break;
	default:                       return 0;
	}

	readopt_write_string(ctx, s);
	return 1;
}

void
readopt_put_usage(struct readopt_parser *rp, struct readopt_format_context *ctx)
{
	assert(ctx->fmt == READOPT_FORMAT_MDOC || ctx->fmt == READOPT_FORMAT_PLAIN);
	ctx->esc.needles = "-";
	ctx->esc.pre = '\\';
	format_usage_opts(ctx, rp->opts);
	format_usage_opers(ctx, rp->opers);
}

static void
format_usage_opts(struct readopt_format_context *ctx, struct readopt_opt *opts)
{
	int nxvalid = readopt_validate_opt(opts);
	for (size_t i = 0; nxvalid; i++) {
		nxvalid = readopt_validate_opt(opts + i + 1);
		size_t lower = readopt_select_lower(opts[i].cont.oper.bounds);
		size_t upper = readopt_select_upper(opts[i].cont.oper.bounds);
		int inf = opts[i].cont.oper.bounds.inf;
		size_t nforms = sizeof opts[i].names / sizeof *opts[i].names;

		for (size_t j = 0; j < (upper ? upper : !!inf); j++) {
			readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, '.');
			if (j >= lower) {
				readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, "Op ");
				readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, '[');
			}

			for (size_t k = 0; k < nforms; k++) {
				int grp = 0;
				if (opts[i].names[k]) {
					for (size_t l = 0; opts[i].names[k][l]; l++) {
						if (!grp) {
							readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, "Fl \\&");

							if (k == READOPT_FORM_SHORT) {
								readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, '-');
							}

							if (k == READOPT_FORM_LONG) {
								readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, "\\-");
								readopt_cond_write_string(READOPT_FORMAT_PLAIN, ctx, "--");
							}
						}

						readopt_cond_write_esc_string(READOPT_FORMAT_MDOC, ctx, opts[i].names[k][l]);
						readopt_cond_write_string(READOPT_FORMAT_PLAIN, ctx, opts[i].names[k][l]);

						if (k == READOPT_FORM_SHORT) {
							grp = 1;
							if (!opts[i].names[k][l + 1]) {
								readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, ' ');
								readopt_write_string(ctx->wr, ", ");
							}
							continue;
						} else if (k + 1 < nforms || opts[i].names[k][l + 1]) {
							readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, ' ');
							readopt_write_string(ctx->wr, ", ");
						} else {
							if (opts[i].cont.req){
								readopt_write_char(ctx->wr, ' ');
								readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, "Ar ");
								if (opts[i].cont.oper.name) {
									readopt_cond_write_esc_string(READOPT_FORMAT_MDOC, ctx, opts[i].cont.oper.name);
									readopt_cond_write_string(READOPT_FORMAT_PLAIN, ctx, opts[i].cont.oper.name);
								} else {
									readopt_write_string(ctx->wr, "value");
								}

								if (inf) {
									readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, ' ');
									readopt_write_string(ctx->wr, "...");
								}
							}
						}
					}
				}
			}

			if (j >= lower) {
				readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, ']');
			}

			readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, '\n');
			readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, ' ');
		}
	}
}

static void
format_usage_opers(struct readopt_format_context *ctx, struct readopt_oper *opers)
{
	int nxvalid = readopt_validate_oper(opers);
	for (size_t i = 0; nxvalid; i++) {
		nxvalid = readopt_validate_oper(opers + i + 1);
		size_t lower = readopt_select_lower(opers[i].bounds);
		size_t upper = readopt_select_upper(opers[i].bounds);
		int inf = opers[i].bounds.inf;

		for (size_t j = 0; j < lower; j++) {
			readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, ".Ar \\&");

			readopt_cond_write_esc_string(READOPT_FORMAT_MDOC, ctx, opers[i].name);
			readopt_cond_write_string(READOPT_FORMAT_PLAIN, ctx, opers[i].name);

			if (inf && j + 1 == lower) {
				readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, ' ');
				readopt_write_string(ctx->wr, "...");
			}

			if (nxvalid) {
				readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, ' ');
			}

			readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, '\n');
		}

		size_t amt = upper ? upper : inf ? lower + 1 : 0;
		for (size_t j = lower; j < amt; j++) {
			readopt_cond_write_string(READOPT_FORMAT_MDOC, ctx, ".Op Ar \\&");
			readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, '[');

			readopt_cond_write_esc_string(READOPT_FORMAT_MDOC, ctx, opers[i].name);
			readopt_cond_write_string(READOPT_FORMAT_PLAIN, ctx, opers[i].name);

			if (inf && j + 1 == amt) {
				readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, ' ');
				readopt_write_string(ctx->wr, "...");
			}

			readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, ']');

			if (nxvalid) {
				readopt_cond_write_char(READOPT_FORMAT_PLAIN, ctx, ' ');
			}

			readopt_cond_write_char(READOPT_FORMAT_MDOC, ctx, '\n');
		}
	}
}

void
readopt_cond_write_esc_string(enum readopt_format desired, struct readopt_format_context *ctx, const char *string)
{
	if (ctx->fmt == desired) readopt_write_esc_string(ctx, string);
}

void
readopt_cond_write_string(enum readopt_format desired, struct readopt_format_context *ctx, const char *string)
{
	if (ctx->fmt == desired) readopt_write_string(ctx->wr, string);
}

void
readopt_cond_write_char(enum readopt_format desired, struct readopt_format_context *ctx, char ch)
{
	if (ctx->fmt == desired) readopt_write_char(ctx->wr, ch);
}

void
readopt_write_esc_string(struct readopt_format_context *ctx, const char *string)
{
	char *s = (char *)string;
	for (; *s; ++s) {
		if (strchr(ctx->esc.needles, *s) && *s) {
			readopt_write_char(ctx->wr, ctx->esc.pre);
		}
		readopt_write_char(ctx->wr, *s);
	}
}

void
readopt_write_string(struct readopt_write_context *ctx, const char *string)
{
	ctx->src.len = strlen(string);
	ctx->src.payload = string;
	readopt_write_stream(ctx);
}

void
readopt_write_char(struct readopt_write_context *ctx, char ch)
{
	ctx->src.len = sizeof (ch);
	ctx->src.payload = &ch;
	readopt_write_stream(ctx);
}

void
readopt_write_stream(struct readopt_write_context *ctx)
{
	while (ctx->callback && (ctx->src.len > ctx->dest.size || !ctx->dest.stream))
		if (!ctx->callback(ctx))
			return;

	ctx->written += fwrite(ctx->src.payload, ctx->src.len, 1, ctx->dest.stream);
}
