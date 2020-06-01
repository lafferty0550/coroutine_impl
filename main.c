#include <setjmp.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define LOCAL_DATA struct {                \
    int some_data_here;                    \
}

struct crt {
    jmp_buf e_pnt;
    jmp_buf *r_pnts;
    int r_count, r_cap;
    bool finished;
    LOCAL_DATA;
};

#define CRT_COUNT 3
struct crt crts[CRT_COUNT];

int cur_crt = 0;

#define crt_this() (&crts[cur_crt])

#define crt_finish() ({ crt_this()->finished = true; })

#define crt_yield() ({                                   \
    int old_i = cur_crt;                                 \
    cur_crt = (cur_crt + 1) % CRT_COUNT;                 \
    if (setjmp(crts[old_i].e_pnt) == 0)                  \
        longjmp(crts[cur_crt].e_pnt, 1);                 \
})

#define crt_init(crt) ({                                 \
    (crt)->finished = false;                             \
    (crt)->r_count = 0;                                  \
    (crt)->r_cap = 0;                                    \
    (crt)->r_pnts = NULL;                                \
    setjmp((crt)->e_pnt);                                \
})

#define crt_call(func, ...) ({                           \
    struct crt *c = crt_this();                          \
    if (c->r_count + 1 > c->r_cap) {                     \
        int new_cap = (c->r_cap + 1) * 2;                \
        int new_size = new_cap * sizeof(jmp_buf);        \
        c->r_pnts =                                      \
            (jmp_buf *) realloc(c->r_pnts,               \
                        new_size);                       \
        assert(c->r_pnts != NULL);                       \
        c->r_cap = new_cap;                              \
    }                                                    \
    if (setjmp(c->r_pnts[c->r_count]) == 0) {            \
        ++c->r_count;                                    \
        func(__VA_ARGS__);                               \
    }                                                    \
})

#define crt_return() ({                                  \
    struct crt *c = crt_this();                          \
    longjmp(c->r_pnts[--c->r_count], 1);                 \
})

#define crt_wait_all() do {                              \
    bool all_finished = true;                            \
    for (int i = 0; i < CRT_COUNT; ++i)                  \
        if (!crts[i].finished) {                         \
            fprintf(stderr, "waiting for crt #%d\n", i); \
            all_finished = false;                        \
            break;                                       \
        }                                                \
    if (all_finished)                                    \
        break;                                           \
    crt_yield();                                         \
} while (true)

void fn() {
    printf("Crt #%d is here\n", cur_crt);
    crt_yield();
    printf("Crt #%d is here secondly\n", cur_crt);
    crt_yield();
    printf("Crt #%d is here thirdly\n", cur_crt);
    crt_finish();
    crt_wait_all();
}

int main(int argc, char **argv) {
    for (int i = 0; i < CRT_COUNT; ++i) {
        if (crt_init(&crts[i]) != 0)
            break;
    }
    crt_call(fn);
    printf("Finished\n");
    return 0;
}