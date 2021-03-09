#pragma once

#include <stdio.h>

#define READOPT_ALLOC_STRINGS(...) ((char *[]){__VA_ARGS__, NULL})

enum readopt_error {
	READOPT_ERROR_SUCCESS,
	READOPT_ERROR_NOVAL,
	READOPT_ERROR_NOTREQ,
	READOPT_ERROR_NOTOPT,
	READOPT_ERROR_RANGEOPT,
	READOPT_ERROR_RANGEOPER
};

enum readopt_form {
	READOPT_FORM_SHORT,
	READOPT_FORM_LONG
};

enum readopt_format {
	READOPT_FORMAT_PLAIN,
	READOPT_FORMAT_MDOC,
};

struct readopt_view_strings {
	const char **strings;
	size_t len;
};

struct readopt_bounds {
	/* upper val will be ignored if inf is non-zero */
	size_t val[2];
	int inf;
};

struct readopt_oper {
	char *name;
	struct readopt_bounds bounds;
	struct readopt_view_strings val;
};

struct readopt_opt {
	/* two null-terminated arrays of either long or short option names */
	char **names[2];

	struct {
		int req;

		/* oper.name is the name of the value itself (not the option), such as "file" in "--config=file" */
		struct readopt_oper oper;
	} cont;
};

struct readopt_parser {
	struct readopt_opt *opts;
	struct readopt_oper *opers;
	struct readopt_view_strings args;
	struct {
		int pending;
		const char *grppos;
		struct {
			struct readopt_opt *opt;
			/* reference to the current argument being parsed */
			const char **arg;
			/* reference to the last element of the option/operand value view */
			const char **eoval;
			/* intermediate operands which have not yet been assigned */
			struct readopt_view_strings ioper;
		} curr;
	} state;
	enum readopt_error error;
};

struct readopt_write_context {
	size_t written;
	void *additional;

	struct {
		FILE *stream;
		size_t size;
	} dest;

	struct {
		const char *payload;
		size_t len;
	} src;

	/* returning 0 means that the current string will not be written */
	int (*callback)(struct readopt_write_context *);
};

struct readopt_format_context {
	enum readopt_format fmt;

	struct {
		const char *needles;
		char pre;
	} esc;

	struct readopt_write_context *wr;
};

/* iteratively parse the arguments */
int readopt_parse(struct readopt_parser *rp);
/* args should always exclude the first element, like this: {.strings = argv + 1, .len = argc - 1} */
void readopt_parser_init(struct readopt_parser *rp, struct readopt_opt *opts, struct readopt_oper *opers, struct readopt_view_strings args);
/* check whether the argument is a valid option (can be used to check for the end of an array of options) */
int readopt_validate_opt(struct readopt_opt *opt);
/* check whether the argument is a valid operand (can be used to check for the end of an array of operands) */
int readopt_validate_oper(struct readopt_oper *oper);
/* check whether the operand's values are within the defined limits */
int readopt_validate_within(struct readopt_oper *oper);
/* get the upper limit */
size_t readopt_select_upper(struct readopt_bounds bounds);
/* get the lower limit (this does not always return the minimum, if e.g. .val is {0, 1} and inf != 0, then 1 will be considered the lower limit as well as the upper limit) */
size_t readopt_select_lower(struct readopt_bounds bounds);
/* pass a string like "thing=value" and get "value" back */
char *readopt_keyval(char *s);

/* write the passed error as a string via ctx */
int readopt_put_error(enum readopt_error error, struct readopt_write_context *ctx);
/* write the usage string, either as plaintext or mdoc format */
void readopt_put_usage(struct readopt_parser *rp, struct readopt_format_context *ctx);

void readopt_write_stream(struct readopt_write_context *ctx);
void readopt_write_string(struct readopt_write_context *ctx, const char *string);
void readopt_write_char(struct readopt_write_context *ctx, char ch);
void readopt_cond_write_string(enum readopt_format desired, struct readopt_format_context *ctx, const char *string);
void readopt_cond_write_char(enum readopt_format desired, struct readopt_format_context *ctx, char ch);
void readopt_write_esc_string(struct readopt_format_context *ctx, const char *string);
void readopt_cond_write_esc_string(enum readopt_format desired, struct readopt_format_context *ctx, const char *string);
