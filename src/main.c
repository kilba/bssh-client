#include "git2/commit.h"
#include "git2/errors.h"
#include "git2/index.h"
#include "git2/merge.h"
#include "git2/remote.h"
#include "git2/signature.h"
#include "git2/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <git2.h>
#include <windows.h>
#include <math.h>
#include <wchar.h>
#include <locale.h>
#include <time.h>
#include <cJSON.h>

//Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define RES "\e[0m"

enum Flags {
    FLAG_HELP = 1,
    FLAG_VERSION = 2,
    FLAG_LATEST = 4,
};

enum {
    OPT_INIT,
    OPT_UPDATE,

    OPT_COUNT
};

typedef struct {
    int idx;
    int num_sub_opts;
    int max_sub_opts;
    char *name;
} Option;
Option opts[OPT_COUNT];
Option *cur_opt = NULL;

int flags = 0;

char *exe_path = NULL;
char *bas_path = NULL;

typedef struct {
    int tot;
    int last;
    int object;
    int index;
} Progress;
Progress progress = {0};

typedef struct {
    cJSON *data;
    cJSON *is_installed_json;

    char *path;
    bool has_changed;
} BsshData;
BsshData bssh = {0};

void dataWasEditedError() {
    printf("%sERROR:%s bssh_data.json was edited and is now invalid!\n", RED, RES);
    exit(1);
}

bool argCmp(char *first, char *second) {
    return strcmp(first, second) == 0;
}

bool hasFlag(int flag) {
    return (flags & flag) == flag;
}

char* readFile(char *path, int *content_len, int *errcode) {
    if(path == 0) {
        *errcode = 1;
        return NULL;
    }

    char *buffer = 0;
    long length;
    FILE * f = fopen (path, "rb");

    if (f)
    {
      fseek (f, 0, SEEK_END);
      length = ftell (f) + 1;
      fseek (f, 0, SEEK_SET);
      buffer = malloc (length);
      if (buffer)
      {
        fread (buffer, 1, length, f);
      }
      fclose (f);
    } else {
        *errcode = 2;
        return NULL;
    }

    *errcode = 0;
    *content_len = length;
    buffer[length - 1] = '\0';
    return buffer;
}

void writeFile(char *name, char *data) {
    FILE *fp = fopen(name, "w");
    if (fp != NULL) {
        fputs(data, fp);
        fclose(fp);
    }  
}

void printInitUsage() {
    char *msg = \
	  "USAGE\n"\
	"    bssh init <name> [flags]\n";

    printf("\n%s", msg);
}

void printUpdateUsage() {
    char *msg = \
	  "USAGE\n"\
	"    bssh update [flags]\n";

    printf("\n%s", msg);
}

void printUsage(Option *opt) {
    switch(opt->idx) {
	case OPT_INIT: printInitUsage(); break;
	case OPT_UPDATE: printUpdateUsage(); break;
    }
}

void printHelp() {
    char *help_msg = \
	  "USAGE\n"\
	"    bssh <command> <subcommands> [flags]\n"\
	"\nCOMMANDS\n"\
	"    new <name>      Initialized a new project\n"\
	"    update          Updates Basilisk to the latest release\n"\
	"\nFLAGS\n"\
	"    -h, --help      Prints this menu\n"\
	"    -v, --version   Prints the current engine version\n";

    printf("%s", help_msg);
}

void printVersion() {
    printf("v1.0\n");
}

void stringFlag(char *arg, int arg_len) {
    arg++;
    arg_len--;

    if(argCmp(arg, "help")) {
	flags |= FLAG_HELP;
	return;
    }

    if(argCmp(arg, "version")) {
	flags |= FLAG_VERSION;
	return;
    }

    if(argCmp(arg, "latest")) {
	flags |= FLAG_VERSION;
	return;
    }
}

void charFlag(char arg) {
    switch(arg) {
	case 'h': flags |= FLAG_HELP; break;
	case 'v': flags |= FLAG_VERSION; break;
	case 'l': flags |= FLAG_LATEST; break;
    }
}

void flag(char *arg, int arg_len) {
    arg++;
    arg_len--;

    if(arg_len == 0)
	return;

    if(arg[0] == '-') {
	stringFlag(arg, arg_len);
	return;
    }

    charFlag(arg[0]);
}

void parseOption(char *arg) {
    int new_option = -1;

    if(argCmp(arg, "new")) {
	new_option = OPT_INIT;
    } else if(argCmp(arg, "update")) {
	new_option = OPT_UPDATE;
    }

    if(cur_opt == NULL) {
	/* Invalid Command */
	if(new_option == -1) {
	    printf("%s is not a valid command!\n", arg);
	    exit(1);
	}

	cur_opt = opts + new_option;
	return;
    }

    cur_opt->num_sub_opts++;
    if(cur_opt->num_sub_opts > cur_opt->max_sub_opts) {
	printf("%sERROR: %sToo many subcommands\n", RED, RES);
	printUsage(cur_opt);
	exit(1);
    }

    switch(cur_opt->idx) {
	case OPT_INIT: cur_opt->name = arg; break;
	default: printf("%sERROR: %sUnknown subcommand \"%s\"\n", RED, RES, arg); printUsage(cur_opt); exit(1);
    }
}

void parse(char *arg) {
    int arg_len = strlen(arg);
    
    /* If current arg is a flag */
    if(arg[0] == '-') {
	flag(arg, arg_len);
	return;
    }

    /* If current arg is an option */
    parseOption(arg);
}

void createDir(char *name) {
#ifdef _WIN32
    CreateDirectory(cur_opt->name, NULL);
    int err_win = GetLastError();
    if(err_win == ERROR_ALREADY_EXISTS) {
	printf("%sERROR: %sDirectory \"%s\" already exists!\n", RED, RES, cur_opt->name);
	exit(1);
    }
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
    struct stat st = {0};
    if(stat(name &st) == -1) {
	printf("%sERROR: %sDirectory \"%s\" already exists!\n", RED, RES, cur_opt->name);
	exit(1);
    }
    mkdir(name, 0700);
#endif
}

/* COMMANDS */
void init() {
    if(cur_opt->name == NULL) {
	printf("Initializes a new project\n");
	printf("\nUSAGE\n");
	printf("    bssh new <name> [flags]\n");
	exit(0);
    }

    printf("%s\n", cur_opt->name);
}

void printProgress(int cur_object, int tot_objects, char *color) {
    char str[256];
    int progress25  = (cur_object *  25) / tot_objects;
    int progress100 = (cur_object * 100) / tot_objects;

    int offset = 7;
    sprintf(str, "%s", color);
    for(int i = 0; i < 25; i++, offset++) {
	if(i == progress25) {
	    sprintf(str + offset, "%s", WHT);
	    offset += 7;
	}

	sprintf(str + offset, "%c", 196);
    }

    sprintf(str + offset, " %d%s%s (%d / %d) ", progress100, "%", CYN, cur_object, tot_objects);
    printf("%s\r", str);
}

int fetchProgress(const git_transfer_progress *stats, void *payload) {
    progress.index++;

    progress.last = progress.object;
    progress.object = stats->received_objects; 

    progress.tot = stats->total_objects;

    if(progress.object != progress.last)
	printProgress(stats->received_objects, stats->total_objects, YEL);

    return 0;
}

void loadBsshData() {
    if(bssh.data != NULL)
	return;

    /* Get path to the executable folder */
#ifdef _WIN32
    exe_path = malloc(512);
    GetModuleFileName(NULL, exe_path, 512);
    char *p = strrchr(exe_path, '\\');
    int index = p - exe_path;
    exe_path[index+1] = '\0';
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
    exe_path = malloc(7);
    strcpy(exe_path, "/proc/\0");
#endif

#define BSSH_NAME "bssh_data.json"
#define BASI_NAME "Basilisk"

    int exe_path_len = strlen(exe_path);

    bssh.path = malloc(exe_path_len + sizeof(BSSH_NAME));
    sprintf(bssh.path, "%s%s", exe_path, BSSH_NAME);

    bas_path = malloc(exe_path_len + sizeof(BASI_NAME));
    sprintf(bas_path, "%s%s", exe_path, BASI_NAME);

    int err, len;
    char *raw = readFile(bssh.path, &err, &len);
    bssh.data = cJSON_Parse(raw);

    if(bssh.data == NULL) {
	printf("%sERROR:%s %s does not exist!", RED, RES, bssh.path);
	exit(1);
    }

    cJSON *engine = cJSON_GetObjectItem(bssh.data, "Engine");
    if(engine == NULL)
	dataWasEditedError();

    bssh.is_installed_json = cJSON_GetObjectItem(engine, "isInstalled");
    if(bssh.is_installed_json == NULL)
	dataWasEditedError();
}

void clone(char *url) {
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

    clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.fetch_opts.callbacks.transfer_progress = fetchProgress;

    git_repository *repo = NULL;
    int err = git_clone(&repo, url, bas_path, &clone_opts);
    if(err != 0) {
	printf("%sERROR:%s Couldn't clone the repository! (libgit2 err %d)\n", RED, RES, err);
	exit(1);
    }

    bssh.has_changed = true;
    cJSON_SetBoolValue(bssh.is_installed_json, true);
    printProgress(progress.tot, progress.tot, GRN);
}

void fetch(char *url) {
    int err;
    git_remote *remote;
    git_repository *repo;
    err = git_repository_open(&repo, bas_path);
    if(err != 0) {
	if(err == GIT_ENOTFOUND) {
	   clone(url);
	   return;
	}

	if(err == GIT_EEXISTS) {
	    printf("%sERROR:%s Current repository is invalid!\n", RED, RES);
	    return;
	}
	return;
    }

    /* Fetch new files */
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    fetch_opts.callbacks.transfer_progress = fetchProgress;
    err = git_remote_lookup(&remote, repo, "origin");
    err = git_remote_fetch(remote, NULL, &fetch_opts, NULL);

    if(progress.tot == 0) {
	printf("Already up to date.\n");
	return;
    }

    /* Checkout to FETCH_HEAD, discards local files */
    git_object *treeish = NULL;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;

    err = git_revparse_single(&treeish, repo, "FETCH_HEAD");
    err = git_checkout_tree(repo, treeish, &opts);

    /* Print progress as green */
    printProgress(progress.tot, progress.tot, GRN);
}

void update() {
    if(hasFlag(FLAG_LATEST))
	printf("Latest\n");

    loadBsshData();

    /* Clone the repo if it's not installed, otherwise fetch newest  */
    if(!bssh.is_installed_json->valueint) {
	clone("https://github.com/nusbog/Basilisk");
    } else {
	fetch("https://github.com/nusbog/Basilisk");
    }

    printf("%s", RES);
}

void interpretOpts() {
    if(cur_opt == NULL) {
	if(hasFlag(FLAG_HELP)) {
	    printHelp();
	    return;
	}

	if(hasFlag(FLAG_VERSION)) {
	    printVersion();
	    return;
	}

	printHelp();
	return;
    }

    switch(cur_opt->idx) {
	case OPT_INIT       : init();        break;
	case OPT_UPDATE     : update();      break;
    }
}

int main(int argc, char **argv) {
    argv++;
    argc--;

    opts[OPT_INIT]   = (Option){ OPT_INIT  , 0, 1, NULL };
    opts[OPT_UPDATE] = (Option){ OPT_UPDATE, 0, 0, NULL };

    git_libgit2_init();

    for(int i = 0; i < argc; i++) {
	parse(argv[i]);
    }

    interpretOpts();

    if(bssh.has_changed && bssh.data != NULL)
	writeFile(bssh.path, cJSON_Print(bssh.data));

    return 0;
}
