#define READARG_IMPLEMENTATION

#include "../readarg.h"

int main(int argc, char **argv)
{
    struct readarg_opt opts[] = {
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("e", "x"),
                [READARG_FORM_LONG] = READARG_STRINGS("expr", "expression"),
            },
            .cont = {
                .req = 1,
                .oper.bounds.val = {
                    1,
                    4,
                },
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("c"),
                [READARG_FORM_LONG] = READARG_STRINGS("config"),
            },
            .cont = {
                .req = 1,
                .oper = {
                    .name = "file",
                    .bounds.val = {
                        2,
                    },
                },
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("i"),
                [READARG_FORM_LONG] = READARG_STRINGS("uri"),
            },
            .cont = {
                .req = 1,
                .oper.bounds.inf = 1,
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("b"),
                [READARG_FORM_LONG] = READARG_STRINGS("backup", "backup-file"),
            },
            .cont = {
                .req = 1,
                .oper.bounds.inf = 1,
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("v"),
                [READARG_FORM_LONG] = READARG_STRINGS("verbose"),
            },
            .cont.oper.bounds.val = {
                3,
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("s"),
                [READARG_FORM_LONG] = READARG_STRINGS("sort"),
            },
            .cont.oper.bounds.inf = 1,
        },
        {
            .names = {
                [READARG_FORM_LONG] = READARG_STRINGS("help"),
            },
            .cont.oper.bounds.val = {
                1,
            },
        },
        {
            .names = {
                [READARG_FORM_SHORT] = READARG_STRINGS("V"),
                [READARG_FORM_LONG] = READARG_STRINGS("version"),
            },
            .cont.oper.bounds.val = {
                1,
            },
        },
        {0},
    };

    struct readarg_oper opers[] = {
        {
            .name = "pattern",
            .bounds.inf = 1,
        },
        {
            .name = "file",
            .bounds = {
                .val = {
                    1,
                },
                .inf = 1,
            },
        },
        {
            .name = "name",
            .bounds = {
                .val = {
                    1,
                },
                .inf = 1,
            },
        },
        {
            0,
        },
    };

    struct readarg_parser rp;
    readarg_parser_init(
        &rp,
        opts,
        opers,
        (struct readarg_view_strings){
            .strings = (const char **)argv + 1,
            .len = argc - 1});

    while (readarg_parse(&rp))
        ;

    fprintf(stderr, "error: %d\n", rp.error);
    if (rp.error != READARG_ERROR_SUCCESS)
    {
        return 1;
    }

    printf("opt:\n");
    {
        struct readarg_opt *curr = rp.opts;
        for (size_t i = 0; readarg_validate_opt(curr + i); i++)
        {
            for (size_t j = 0; j < sizeof curr[i].names / sizeof *curr[i].names; j++)
            {
                if (curr[i].names[j])
                {
                    for (size_t k = 0; curr[i].names[j][k]; k++)
                    {
                        printf("%s ", curr[i].names[j][k]);
                    }
                }
            }
            printf("{ [%zu] ", curr[i].cont.oper.val.len);
            if (curr[i].cont.req)
            {
                struct readarg_view_strings val = curr[i].cont.oper.val;
                for (size_t j = 0; j < val.len; j++)
                {
                    printf("%s ", val.strings[j]);
                }
            }
            printf("}\n");
        }
    }

    printf("oper:\n");
    {
        struct readarg_oper *curr = rp.opers;
        for (size_t i = 0; readarg_validate_oper(curr + i); i++)
        {
            printf("%s { [%zu] ", curr[i].name, curr[i].val.len);
            for (size_t j = 0; j < curr[i].val.len; j++)
            {
                printf("%s ", curr[i].val.strings[j]);
            }
            printf("}\n");
        }
    }

    return 0;
}
