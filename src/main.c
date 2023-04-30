#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <conio.h>

#include <net.h>
#include <git2.h>
#include <toml.h>
#include <toml_write.h>
#include <bsshstrs.h>
#include <qrcodegen.h>
#include <sha1.h>
#include <base64.h>

#ifdef _WIN32
    #include <winsock.h>
    #include <windows.h>
    #include <wchar.h>
    #include <locale.h>
#endif

enum {
    FLAG_HELP,
    FLAG_VERSION,
    FLAG_LATEST,
    FLAG_GEOM,

    FLAG_COUNT
};

enum {
    OPT_INIT,
    OPT_UPDATE,
    OPT_SHADER,

    OPT_COUNT
};

typedef struct {
    char *skey;
    char  ckey;
    bool is_set;
} Flag;
Flag flags[FLAG_COUNT];

typedef struct {
    char *key;
    void (*func)();
    void (*usage)();

    int num_sub_opts;
    int min_sub_opts;
    int max_sub_opts;
    char *args[4];
} Option;
Option opts[OPT_COUNT];
Option *cur_opt = NULL;

#define TYPE 0
#define NAME 0

int arg_offset = 0;

char *app_data_path = NULL;
char *cookie_path = NULL;
char *bssh_path = NULL;
char *bas_path = NULL;
size_t bssh_path_len = 0;

typedef struct {
    int tot;
    int last;
    int object;
    int index;
    int until_next_print;
} Progress;
Progress progress = {0};

typedef struct {
    TomlTable *main, *engine;

    char *path;
    bool installed;
    bool has_changed;
    bool logged_in;
} BsshData;
BsshData bssh = {0};

void loadBsshData();
void exitError() {
    exit(1);
}

char *userInput(char *buf, int len) {
    buf = fgets(buf, 32, stdin);
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

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
    printf("%sERROR:%s bssh.toml was edited and is now invalid!\n", RED, RES);
    exit(1);
}

bool argCmp(char *first, char *second) {
    return strcmp(first, second) == 0;
}

bool hasFlag(int flag) {
    return flags[flag].is_set;
}

void printInitUsage() {
    printf("Initializes a new project\n");
    
    char *msg = \
	  "\nUSAGE\n"\
	"    bssh new <name> [flags]\n";

    printf("%s", msg);
}

void printUpdateUsage() {
    printf("Fetches the latest update\n");

    char *msg = \
	  "\nUSAGE\n"\
	"    bssh update [flags]\n";

    printf("%s", msg);
}

void printAuthUsage() {
    printf("Authentication commands\n");
    char *msg = \
	  "\nUSAGE\n"\
	"    bssh auth <command> [flags]\n"\
	  "\nCOMMANDS\n"\
	"    create         Creates a new account\n"\
	"    login          Logs in to an account\n"\
	"    logout         Logs out of the account\n";

    printf("%s", msg);
}

void printHelp() {
    char *help_msg = \
	  "USAGE\n"\
	"    bssh <command> <subcommands> [flags]\n"\
	"\nCOMMANDS\n"\
	"    new             Initialized a new project\n"\
	"    update          Updates Basilisk to the latest release\n"\
	"    auth            Log in or out of accounts, create accounts\n"\
	"    profile         Check out someones profile\n"\
	"\nFLAGS\n"\
	"    -h, --help      Prints this menu\n"\
	"    -v, --version   Prints the current engine version\n";

    printf("%s", help_msg);
}

void printProfileUsage() {
    char *msg = \
	  "\nUSAGE\n"\
	"    bssh profile <name> [flags]\n";

    printf("%s\n", msg);
}

void printBefriendUsage() {
    printf("Sends a friend request to a user\n");

    char *msg = \
	  "\nUSAGE\n"\
	"    bssh befriend <name> [flags]\n";

    printf("%s\n", msg);
}

void printUnfriendUsage() {
    printf("Removes a friend from your friend list\n");

    char *msg = \
	  "\nUSAGE\n"\
	"    bssh unfriend <name> [flags]\n";

    printf("%s\n", msg);
}

void printShaderUsage() {
    printf("Initializes a new shader program\n");

    char *msg = \
	  "\nUSAGE\n"\
	"    bssh shader <name> [flags]\n"\
	  "\nFLAGS\n"\
	"    -g, --geom      Also creates a geometry shader\n";

    printf("%s\n", msg);
}

void printStatusUsage() {
    printf("Gets any local or remote status updates, all status\nupdates will be requested if no flags are specified\n");
    
    char *msg = \
	  "\nUSAGE\n"\
	"    bssh status [flags]\n"\
	  "\nFLAGS\n"\
	"    -f, --friends    Gets friend requests"\
	"    -u, --updates    Checks if updates are available";

    printf("%s\n", msg);
}

void printVersion() {
    printf("v1.0\n");
}

void stringFlag(char *arg, int arg_len) {
    arg++;
    arg_len--;

    for(int i = 0; i < FLAG_COUNT; i++) {
	if(argCmp(arg, flags[i].skey)) {
	    flags[i].is_set = true;
	    return;
	}
    }
}

void charFlag(char arg) {
    for(int i = 0; i < FLAG_COUNT; i++) {
	if(arg == flags[i].ckey) {
	    flags[i].is_set = true;
	    return;
	}
    }
}

void parseFlag(char *arg, int arg_len) {
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

    for(int i = 0; i < OPT_COUNT; i++)
	if(argCmp(arg, opts[i].key))
	    new_option = i;

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
	cur_opt->usage();
	exit(1);
    }

    cur_opt->args[arg_offset++] = arg;
}

void parse(char *arg) {
    int arg_len = strlen(arg);
    
    /* If current arg is a flag */
    if(arg[0] == '-') {
	parseFlag(arg, arg_len);
	return;
    }

    /* If current arg is an option */
    parseOption(arg);
}

char *cookiePath() {
    loadBsshData();
    
    if(cookie_path == NULL) {
	cookie_path = malloc(bssh_path_len + sizeof("cookies"));
	sprintf(cookie_path, "%s%s", bssh_path, "cookies");
    }

    return cookie_path;
}

char *cookiePathChkError() {
    char *path = cookiePath();

    if(!fileExists(path)) {
	printf("%sERROR: %sYou need to be logged in to perform this action\n", RED, RES);
	exit(1);
    }

    return path;
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
    char *name = cur_opt->args[NAME];

    if(name == NULL) {
	printInitUsage();
	exit(0);
    }

    loadBsshData();

    int name_len = strlen(name);
    char path[bssh_path_len + sizeof(INITFILES_PATH)];

    sprintf(path, "%s%s", bssh_path, INITFILES_PATH);

    // Parse .TOML data
    char settings_path[bssh_path_len + sizeof(INITFILES_PATH) + sizeof("bssh.toml")];
    sprintf(settings_path, "%s%s", bssh_path, INITFILES_PATH "bssh.toml");

    TomlTable *table_main;

    table_main = toml_load_filename(settings_path);
    checkTomlErr(settings_path);
   
    int num_elems;
    struct ReplaceBuf {
	char *search, **match, **replace;
	int num_elems;
    } *repl = initReplaceTable(table_main, &num_elems);

    char *skip[3 + num_elems];
    skip[0] = ".";
    skip[1] = "..";
    skip[2] = "bssh.toml";
    for(int i = 0; i < num_elems; i++) {
	skip[i+3] = repl[i].search;
    }

    char proj_bas_path[name_len + sizeof("/Basilisk")];
    sprintf(proj_bas_path, "%s/Basilisk", name);

    // Copy initfiles to new project
    copyDirSkip(path, name, 3 + num_elems, skip);
    // Copy Basilisk to new project
    copyDir(bas_path, proj_bas_path);

    for(int i = 0; i < num_elems; i++) {
	int strlen_search = strlen(repl[i].search);
	char repl_path[bssh_path_len + sizeof(INITFILES_PATH) + strlen_search + 1];
	sprintf(repl_path, "%s%s/%s", bssh_path, INITFILES_PATH, repl[i].search);

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
		char basi_path[bssh_path_len + sizeof("Basilisk")];
		sprintf(basi_path, "%s%s", bssh_path, "Basilisk");
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

    printf("Initialized a new project \"%s\"\n", name);
}

void shader() {
    loadBsshData();

    char *name = cur_opt->args[0];
    char shader_path[bssh_path_len + sizeof("shaders/shader.__")];
    char shader_dest[strlen(name) + sizeof(".__")];

    int offset_path = sprintf(shader_path, "%s%s", bssh_path, "shaders/shader.");
    int offset_dest = sprintf(shader_dest, "%s%c", name, '.');

    strncpy(shader_path + offset_path, "vs", 3);
    strncpy(shader_dest + offset_dest, "vs", 3);
    copyFile(shader_path, shader_dest);

    strncpy(shader_path + offset_path, "fs", 3);
    strncpy(shader_dest + offset_dest, "fs", 3);
    copyFile(shader_path, shader_dest);

    if(hasFlag(FLAG_GEOM)) {
	strncpy(shader_path + offset_path, "gs", 3);
	strncpy(shader_dest + offset_dest, "gs", 3);
	copyFile(shader_path, shader_dest);
    }
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
    progress.object = (stats->received_objects * 25) / stats->total_objects;

    progress.tot = stats->total_objects;

    if(progress.object != progress.last)
	printProgress(stats->received_objects, stats->total_objects, YEL);

    return 0;
}

void loadBsshData() {
    if(bssh.engine != NULL)
	return;

    /* Get path to the executable folder */
#ifdef _WIN32
    char *appdata = getenv("APPDATA");
    int strlen_appdata = strlen(appdata);
    bssh_path_len = strlen_appdata + sizeof("(/bssh/");
    bssh_path = malloc(strlen_appdata + sizeof("/bssh/"));

    sprintf(bssh_path, "%s/bssh/", appdata);
    /* Replace '\' with '/' */
    for(int i = 0; i < bssh_path_len; i++)
	bssh_path[i] = (bssh_path[i] == '\\') ? '/' : bssh_path[i];

#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
#endif

#define BSSH_NAME "bssh.toml"
#define BASI_NAME "Basilisk"

    bssh.path = malloc(bssh_path_len + sizeof(BSSH_NAME));
    sprintf(bssh.path, "%s%s", bssh_path, BSSH_NAME);

    bas_path = malloc(bssh_path_len + sizeof(BASI_NAME));
    sprintf(bas_path, "%s%s", bssh_path, BASI_NAME);

    bssh.main = toml_load_filename(bssh.path);

    if(bssh.main == NULL) {
	printf("%sERROR:%s %s does not exist!", RED, RES, bssh.path);
	exit(1);
    }

    TomlValue *engine = toml_table_get(bssh.main, "Basi");
    if(engine == NULL)
	dataWasEditedError();
    bssh.engine = engine->value.table;

    TomlValue *installed_toml = toml_table_get(bssh.engine, "installed");
    if(installed_toml == NULL)
	dataWasEditedError();

    bssh.installed = installed_toml->value.boolean;
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

    TomlValue *val = toml_table_get(bssh.engine, "installed");
    bssh.has_changed = true;
    val->value.boolean = true;

    printProgress(progress.tot, progress.tot, GRN);
}

void fetch(char *url) {
    int err;
    git_remote *remote;
    git_repository *repo;
    err = git_repository_open(&repo, bas_path);
    if(err != 0) {
	if(err == GIT_ENOTFOUND) {
	printf("%s\n", bas_path);
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
    if(!bssh.installed) {
	clone("https://github.com/kilbaa/Basilisk");
    } else {
	fetch("https://github.com/kilbaa/Basilisk");
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

    if(cur_opt->num_sub_opts < cur_opt->min_sub_opts) {
	printf("%sERROR: %sToo few subcommands\n", RED, RES);
	cur_opt->usage();
	exit(1);
    }

    cur_opt->func();
}

void flag(char *key, char single_key, void (*func)(), void (*usage)()) {

}

void opt() {
}

int main(int argc, char **argv) {
    argv++;
    argc--;

    flags[FLAG_HELP]       = (Flag){ "help"    , 'h', 0 };
    flags[FLAG_VERSION]    = (Flag){ "version" , 'v', 0 };
    flags[FLAG_GEOM]       = (Flag){ "geom"    , 'g', 0 };

    opts[OPT_INIT]         = (Option){ "new"       , init      , printInitUsage      , 0, 1, 1, { NULL }};
    opts[OPT_UPDATE]       = (Option){ "update"    , update    , printUpdateUsage    , 0, 0, 0, { NULL }};
    opts[OPT_SHADER]       = (Option){ "shader"    , shader    , printShaderUsage    , 0, 1, 1, { NULL }};

    git_libgit2_init();

    for(int i = 0; i < argc; i++) {
	parse(argv[i]);
    }

    interpretOpts();

    if(bssh.has_changed && bssh.main != NULL) {
	char *buf = malloc(2048);
	int buf_len = 0;

	print_table(bssh.main, buf, &buf_len);
	writeFile(bssh.path, buf);
	free(buf);
    }

    return 0;
}
