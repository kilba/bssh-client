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
#include <bsshstrs.h>
#include <toml.h>

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
int exe_path_len = 0;
char *bas_path = NULL;

typedef struct {
    int tot;
    int last;
    int object;
    int index;
    int until_next_print;
} Progress;
Progress progress = {0};

typedef struct {
    cJSON *data;
    cJSON *is_installed_json;

    char *path;
    bool has_changed;
} BsshData;
BsshData bssh = {0};

void loadBsshData();

void checkTomlErr(char *name) {
    switch(toml_err()->code) {
	case TOML_OK: return;
	case TOML_ERR_SYNTAX: printf("%sERROR: %sSyntax error in \"%s\"\n", RED, RES, name); exit(1);
	case TOML_ERR_OS: printf("%sERROR: %s\"%s\" was not found!\n", RED, RES, name); exit(1);
	case TOML_ERR_UNICODE: printf("%sERROR: %sInvalid unicode scalar in \"%s\n", RED, RES, name); exit(1);
	default: printf("%sERROR: %sUnknown error when parsing \"%s\"\n", RED, RES, name); return;
    }
}

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

/* COMMANDS */
void *initReplaceTable(TomlTable *table_main, int *num_elems_out) {
    *num_elems_out = 0;
    TomlTable *table_replace = toml_table_get_as_table(table_main, "replace");
    if(!table_replace) {
	printf("%sWARNING:%s The \"replace\" table in \"bssh.toml\" has been removed\n", YEL, RES);
	return NULL;
    }
    
    TomlValue *search, *match, *replace;
    if((search  = toml_table_get(table_replace, "search" )) == NULL) { return NULL; };
    if((match   = toml_table_get(table_replace, "match"  )) == NULL) { return NULL; };
    if((replace = toml_table_get(table_replace, "replace")) == NULL) { return NULL; };

    TomlArray *asearch, *amatch, *areplace;
    asearch  = search->value.array;
    amatch   = match->value.array;
    areplace = replace->value.array;

    if ((asearch->len != amatch->len) || 
	(asearch->len != areplace->len) || 
	(amatch->len  != areplace->len)) {
	printf("%sERROR: %sThe \"replace\" table in \"bssh.toml\" contains mismatching array lengths\n", RED, RES);
	exit(1);
    }

    int num_elems = *num_elems_out = asearch->len;
    struct ReplaceBuf {
	char *search, **match, **replace;
	int num_elems;
    } *ret = malloc(num_elems * sizeof(struct ReplaceBuf));


    for(int i = 0; i < num_elems; i++) {
	TomlArray *aimatch   = amatch->elements[i]->value.array;
	TomlArray *aireplace = areplace->elements[i]->value.array;
	if(aimatch->len != aireplace->len) {
	    printf("%sERROR: %sThe \"replace\" table in \"bssh.toml\" contains mismatching array lengths\n", RED, RES);
	    exit(1);
	}

	struct ReplaceBuf *repl = ret + i;
	repl->search  = asearch->elements[i]->value.string->str;
	repl->match   = malloc(aimatch->len * sizeof(char *));
	repl->replace = malloc(aireplace->len * sizeof(char *));
	repl->num_elems = aireplace->len;

	for(int j = 0; j < aimatch->len; j++) {
	    char *aimatch_s   = aimatch->elements[j]->value.string->str;
	    char *aireplace_s = aireplace->elements[j]->value.string->str;
	    repl->match[j]   = aimatch_s;
	    repl->replace[j] = aireplace_s;
	}
    }
    return ret;
}

void init() {
    char *name = cur_opt->name;

    if(name == NULL) {
	printf("Initializes a new project\n");
	printf("\nUSAGE\n");
	printf("    bssh new <name> [flags]\n");
	exit(0);
    }

    loadBsshData();

    int name_len = strlen(name);
    char path[exe_path_len + sizeof(INITFILES_PATH)];

    sprintf(path, "%s%s", exe_path, INITFILES_PATH);

    /* Parse .TOML data */
    char settings_path[exe_path_len + sizeof(INITFILES_PATH) + sizeof("/bssh.toml")];
    sprintf(settings_path, "%s%s", exe_path, INITFILES_PATH "/bssh.toml");

    TomlTable *table_main;

    table_main = toml_load_filename(settings_path);
    checkTomlErr("bssh.toml");
   
    int num_elems;
    struct ReplaceBuf {
	char *search, **match, **replace;
	int num_elems;
    } *repl = initReplaceTable(table_main, &num_elems);

    char *skip[3 + num_elems];
    skip[0] = ".";
    skip[1] = "..";
    skip[2] = "bssh.toml";
    for(int i = 0; i < num_elems; i++)
	skip[i+3] = repl[i].search;

    copyDirSkip(path, name, 3 + num_elems, skip);

    for(int i = 0; i < num_elems; i++) {
	int strlen_search = strlen(repl[i].search);
	char repl_path[exe_path_len + sizeof(INITFILES_PATH) + strlen_search + 1];
	sprintf(repl_path, "%s%s/%s", exe_path, INITFILES_PATH, repl[i].search);

	int err, len;
	char *repl_contents = readFile(repl_path, &len, &err);
	char *new_contents = repl_contents;
	if(err != 0) {
	    printf("%sWARNING: %sThe file \"%s\" as specified in \"bssh.toml\" was not found\n",
	    YEL, RES, repl[i].search);
	    continue;
	}

	for(int j = 0; j < repl[i].num_elems; j++) {
	    char *repl_with = repl[i].replace[j];
	    if(strcmp(repl_with, "PROJ_NAME") == 0) {
		new_contents = replaceStrStr(new_contents, repl[i].match[j], name);
	    } else if(strcmp(repl_with, "BASI_PATH") == 0) {
		char basi_path[exe_path_len + sizeof("Basilisk")];
		sprintf(basi_path, "%s%s", exe_path, "Basilisk");
		new_contents = replaceStrStr(new_contents, repl[i].match[j], basi_path);
	    } else {
		new_contents = replaceStrStr(new_contents, repl[i].match[j], repl_with);
	    }
	}
	char new_path[name_len + strlen_search + 2];
	sprintf(new_path, "%s/%s", name, repl[i].search);
	writeFile(new_path, new_contents);

	free(repl_contents); repl_contents = NULL;
	free(new_contents);
    }

    printf("Initialized a new project \"%s\"", name);
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
    progress.object = stats->received_objects / (stats->total_objects * 25);

    progress.tot = stats->total_objects;

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
    exe_path = malloc(sizeof(LINUX_SELF_PATH));
    strcpy(exe_path, LINUX_SELF_PATH);
#endif

#define BSSH_NAME "bssh_data.json"
#define BASI_NAME "Basilisk"

    exe_path_len = strlen(exe_path);
    /* Replace '\' with '/' */ /* TODO: Skip this step on UNIX */
    for(int i = 0; i < exe_path_len; i++)
	exe_path[i] = (exe_path[i] == '\\') ? '/' : exe_path[i];


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
