#define READOPT_IMPLEMENTATION

#include "../readopt.h"

int main(int argc, char **argv)
{
    struct readopt_opt opts[] = {
        {
            .names = {
                [READOPT_FORM_SHORT] = READOPT_STRINGS("e", "x"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("expr", "expression"),
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
                [READOPT_FORM_SHORT] = READOPT_STRINGS("c"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("config"),
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
                [READOPT_FORM_SHORT] = READOPT_STRINGS("i"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("uri"),
            },
            .cont = {
                .req = 1,
                .oper.bounds.inf = 1,
            },
        },
        {
            .names = {
                [READOPT_FORM_SHORT] = READOPT_STRINGS("b"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("backup", "backup-file"),
            },
            .cont = {
                .req = 1,
                .oper.bounds.inf = 1,
            },
        },
        {
            .names = {
                [READOPT_FORM_SHORT] = READOPT_STRINGS("v"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("verbose"),
            },
            .cont.oper.bounds.val = {
                3,
            },
        },
        {
            .names = {
                [READOPT_FORM_SHORT] = READOPT_STRINGS("s"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("sort"),
            },
            .cont.oper.bounds.inf = 1,
        },
        {
            .names = {
                [READOPT_FORM_LONG] = READOPT_STRINGS("help"),
            },
            .cont.oper.bounds.val = {
                1,
            },
        },
        {
            .names = {
                [READOPT_FORM_SHORT] = READOPT_STRINGS("V"),
                [READOPT_FORM_LONG] = READOPT_STRINGS("version"),
            },
            .cont.oper.bounds.val = {
                1,
            },
        },
        {0},
    };

    struct readopt_oper opers[] = {
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

    struct readopt_parser rp;
    readopt_parser_init(
        &rp,
        opts,
        opers,
        (struct readopt_view_strings){
            .strings = (const char **)argv + 1,
            .len = argc - 1});

    while (readopt_parse(&rp))
        ;

    fprintf(stderr, "error: %d\n", rp.error);
    if (rp.error != READOPT_ERROR_SUCCESS)
    {
        return 1;
    }

    printf("opt:\n");
    {
        struct readopt_opt *curr = rp.opts;
        for (size_t i = 0; readopt_validate_opt(curr + i); i++)
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
                struct readopt_view_strings val = curr[i].cont.oper.val;
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
        struct readopt_oper *curr = rp.opers;
        for (size_t i = 0; readopt_validate_oper(curr + i); i++)
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
