#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#include <readopt.h>

int
main(int argc, char **argv)
{
	if (!*argv)
		return EXIT_FAILURE;

	struct readopt_opt opts[] = {
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("e", "x"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("expr", "expression")
			},
			.cont = {
				.req = 1,
				.oper.bounds.val = {1, 4}
			}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("c"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("config")
			},
			.cont = {
				.req = 1,
				.oper = {
					.name = "file",
					.bounds.val = {2},
				}
			}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("i"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("uri")
			},
			.cont = {
				.req = 1,
				.oper.bounds.inf = 1
			}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("b"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("backup", "backup-file")
			},
			.cont = {
				.req = 1,
				.oper.bounds.inf = 1
			}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("v"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("verbose")
			},
			.cont.oper.bounds.val = {3}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("s"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("sort")
			},
			.cont.oper.bounds.inf = 1
		},
		{
			.names = {
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("help")
			},
			.cont.oper.bounds.val = {1}
		},
		{
			.names = {
				[READOPT_FORM_SHORT] = READOPT_ALLOC_STRINGS("V"),
				[READOPT_FORM_LONG] = READOPT_ALLOC_STRINGS("version")
			},
			.cont.oper.bounds.val = {1}
		},
		{0}
	};

	struct readopt_oper opers[] = {
		{
			.name = "pattern",
			.bounds.inf = 1
		},
		{
			.name = "file",
			.bounds = {
				.val = {1},
				.inf = 1
			}
		},
		{
			.name = "name",
			.bounds = {
				.val = {1},
				.inf = 1
			}
		},
		{0}
	};

	struct readopt_parser rp = readopt_parser_create(
		opts,
		opers,
		(struct readopt_view_strings){
			.strings = argv + 1,
			.len = argc - 1
		}
	);

	struct readopt_answer answer = readopt_parse_all(&rp);

	fputs("status: ", stderr);
	readopt_put_status(answer.status, &(struct readopt_write_context){
		.dest.stream = stderr
	});
	fputc('\n', stderr);
	if (answer.status != READOPT_STATUS_SUCCESS) {
		return EXIT_FAILURE;
	}

	printf("opt:\n");
	{
		struct readopt_opt *curr = rp.opts;
		for (size_t i = 0; readopt_validate_opt(curr + i); i++) {
			for (size_t j = 0; j < sizeof curr[i].names / sizeof *curr[i].names; j++) {
				if (curr[i].names[j]) {
					for (size_t k = 0; curr[i].names[j][k]; k++) {
						printf("%s ", curr[i].names[j][k]);
					}
				}
			}
			printf("{ [%zu] ", curr[i].cont.oper.val.len);
			if (curr[i].cont.req) {
				struct readopt_view_strings val = curr[i].cont.oper.val;
				for (size_t j = 0; j < val.len; j++) {
					printf("%s ", val.strings[j]);
				}
			}
			printf("}\n");
		}
	}

	printf("oper:\n");
	{
		struct readopt_oper *curr = rp.opers;
		for (size_t i = 0; readopt_validate_oper(curr + i); i++) {
			printf("%s { [%zu] ", curr[i].name, curr[i].val.len);
			for (size_t j = 0; j < curr[i].val.len; j++) {
				printf("%s ", curr[i].val.strings[j]);
			}
			printf("}\n");
		}
	}

	printf("usage (plain) (stream):\n");
	readopt_put_usage(&rp, &(struct readopt_format_context){
		.fmt = READOPT_FORMAT_PLAIN,
		.wr = &(struct readopt_write_context){
			.dest.stream = stdout,
		}
	});
	printf("\nusage (mdoc) (stream):\n");
	readopt_put_usage(&rp, &(struct readopt_format_context){
		.fmt = READOPT_FORMAT_MDOC,
		.wr = &(struct readopt_write_context){
			.dest.stream = stdout,
		}
	});

	size_t sz = 0;
	char *buf = NULL;
	/* alternatively, use fmemopen and increase the buffer size via the callback */
	FILE *stream = open_memstream(&buf, &sz);
	readopt_put_usage(&rp, &(struct readopt_format_context){
		.fmt = READOPT_FORMAT_MDOC,
		.wr = &(struct readopt_write_context){
			.dest = {
				.stream = stream,
				.size = SIZE_MAX
			}
		}
	});
	fflush(stream);
	printf("usage (mdoc) (buffer):\n%s", buf);
	fclose(stream);
	free(buf);

	return 0;
}
