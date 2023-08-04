#ifndef BSSH_HPP
#define BSSH_HPP

#include <string>
#include <vector>

enum {
    FLAG_HELP,
    FLAG_UPDATE,
    FLAG_GEOM,
    FLAG_BSSH,
    FLAG_LOCAL,

    FLAG_COUNT
};

enum {
    OPT_INIT,
    OPT_UPDATE,
    OPT_UGRADE,

    OPT_COUNT
};

typedef struct {
    unsigned char commit_hash[20];
} Project;

typedef struct {
    const char *skey;
    char  ckey;
    bool is_set;
} Flag;

typedef struct {
    void (*func)();
    void (*usage)();

    int num_sub_opts;
    int min_sub_opts;
    int max_sub_opts;
    char *args[4];
} Option;
  
typedef struct {
    int tot;
    int last;
    int object;
    int index;
    int until_next_print;
} Progress;

#define TYPE 0
#define NAME 0

#endif /* BSSH_HPP */
