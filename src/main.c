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
#include <help.h>

#ifdef _WIN32
    #include <winsock.h>
    #include <windows.h>
    #include <wchar.h>
    #include <locale.h>
#endif

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
    OPT_SHADER,

    OPT_COUNT
};

typedef struct {
    unsigned char commit_hash[20];
} Project;

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

char *toml_buf = NULL;

// bssh data
char *app_data_path = NULL;
char *cookie_path = NULL;
char *bssh_path = NULL;
char *bas_path = NULL;
size_t bssh_path_len = 0;

char *exe_path;
int exe_path_len = 0;

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
    char *local_src;
    char *local_include;
    char *local_cmake;
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

void setHeadHash(git_repository *repo, Project *project) {
    int err;

    if(repo == NULL) {
	loadBsshData();
	err = git_repository_open(&repo, bas_path);
	if(err != 0) {
	    printf("%sERROR:%s Couldn't open git repository at path \"%s\" (libgit err %d)\n", RED, RES, bas_path, err);
	    exit(1);
	}
    }

    err = git_reference_name_to_id((git_oid *)&project->commit_hash, repo, "HEAD");
    if(err != 0) {
	printf("%sERROR:%s Couldn't get HEAD hash! (libgit err %d)\n", RED, RES, err);
	exit(1);
    }
}

void initProjectFile(char *dir, int dir_len) {
    char path[dir_len + sizeof("/.bssh")];
    sprintf(path, "%s/.bssh", dir);

    Project project;
    setHeadHash(NULL, &project);

    printf("%s\n", path);
    writeFile(path, (char *)&project);
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

    char proj_bas_path_src[name_len + sizeof("external/Basilisk/src")];
    char proj_bas_path_include[name_len + sizeof("external/Basilisk/include")];
    char proj_bas_path_cmake[name_len + sizeof("external/Basilisk/CMakeLists.txt")];

    sprintf(proj_bas_path_src, "%s/external/Basilisk/src", name);
    sprintf(proj_bas_path_include, "%s/external/Basilisk/include", name);
    sprintf(proj_bas_path_cmake, "%s/external/Basilisk/CMakeLists.txt", name);

    char bas_path_src[bssh_path_len + sizeof("Basilisk/src")];
    char bas_path_include[bssh_path_len + sizeof("Basilisk/include")];
    char bas_path_cmake[bssh_path_len + sizeof("Basilisk/CMakeLists.txt")];

    sprintf(bas_path_src, "%sBasilisk/src", bssh_path);
    sprintf(bas_path_include, "%sBasilisk/include", bssh_path);
    sprintf(bas_path_cmake, "%sBasilisk/CMakeLists.txt", bssh_path);

    if(strcmp(name, ".") == 0) {
	createDir("./external", false);
	createDir("./external/Basilisk", false);
	copyDir(bas_path_src, proj_bas_path_src, false);
	copyDir(bas_path_include, proj_bas_path_include, false);
	copyFile(bas_path_cmake, proj_bas_path_cmake, false);
	printf("Added Basilisk to current project.\n");
	return;
    }

    char proj_external_path[name_len + sizeof("external")];
    char proj_bas_path[name_len + sizeof("external/Basilisk")];
    sprintf(proj_external_path, "%s/external", name);
    sprintf(proj_bas_path, "%s/external/Basilisk", name);

    // Copy initfiles to new project
    copyDirSkip(path, name, 3 + num_elems, skip, false);
    createDir(proj_external_path, false);
    createDir(proj_bas_path, false);
    copyFile(bas_path_cmake, proj_bas_path_cmake, false);

    // Copy Basilisk to new project
    copyDir(bas_path_src, proj_bas_path_src, false);
    copyDir(bas_path_include, proj_bas_path_include, false);
    // Create project file
    // initProjectFile(name, name_len);

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
    copyFile(shader_path, shader_dest, false);

    strncpy(shader_path + offset_path, "fs", 3);
    strncpy(shader_dest + offset_dest, "fs", 3);
    copyFile(shader_path, shader_dest, false);

    if(hasFlag(FLAG_GEOM)) {
	strncpy(shader_path + offset_path, "gs", 3);
	strncpy(shader_dest + offset_dest, "gs", 3);
	copyFile(shader_path, shader_dest, false);
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

bool loadBsshPaths() {
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

    // Return value of GetModuleFileName is 0 on fail (wtf)
    char temp_exe_path[MAX_PATH];
    int err = GetModuleFileName(NULL, temp_exe_path, MAX_PATH);
    if(err == 0) {
	err = GetLastError();
	switch(err) {
	    case ERROR_INSUFFICIENT_BUFFER: printf("%sERROR:%s Path to \"bssh.exe\" is too long!\n", RED, RES); break;
	    default: printf("%sERROR:%s GetLastError returned %d\n", RED, RES, err); break;
	}
	exit(1);
    } else {
	exe_path_len = err;
	exe_path = malloc(exe_path_len + 1);
	strncpy(exe_path, temp_exe_path, exe_path_len);
    }

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

    return bssh.main != NULL;
}

void loadBsshData() {
    if(bssh.engine != NULL)
	return;

    TomlValue *engine = toml_table_get(bssh.main, "Basi");
    if(engine == NULL)
	dataWasEditedError();
    bssh.engine = engine->value.table;

    TomlValue *installed_toml = toml_table_get(bssh.engine, "installed");
    if(installed_toml == NULL)
	dataWasEditedError();

    TomlValue *local_src_toml = toml_table_get(bssh.engine, "local_src");
    if(local_src_toml != NULL) {
	bssh.local_src = local_src_toml->value.string->str;
    }
    TomlValue *local_include_toml = toml_table_get(bssh.engine, "local_include");
    if(local_include_toml != NULL) {
	bssh.local_include = local_include_toml->value.string->str;
    }
    TomlValue *local_cmake_toml = toml_table_get(bssh.engine, "local_cmake");
    if(local_cmake_toml != NULL) {
	bssh.local_cmake = local_cmake_toml->value.string->str;
    }

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
	   clone(url);
	   return;
	}

	if(err == GIT_EEXISTS) {
	    // TODO: Attempt fix
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

void updateBssh() {
    printf("%s\n", "updating bssh");
}

void update() {
    loadBsshData();

    if(hasFlag(FLAG_BSSH)) {
	updateBssh();
	return;
    }

    /* Clone the repo if it's not installed, otherwise fetch newest  */
    if(!bssh.installed) {
	clone("https://github.com/kilba/Basilisk");
    } else {
	fetch("https://github.com/kilba/Basilisk");
    }

    printf("%s", RES);
}

void upgrade() {
    loadBsshData();

    createDir("./external", false);
    createDir("./external/Basilisk", false);
    if(hasFlag(FLAG_LOCAL)) {
	if(bssh.local_src != NULL) {
	    copyDir(bssh.local_src, "./external/Basilisk/src", true);
	} else {
	    printf("%sWARNING: %sNo local src directory found\n", YEL, RES);
	}

	if(bssh.local_include != NULL) {
	    copyDir(bssh.local_src, "./external/Basilisk/src", true);
	} else {
	    printf("%sWARNING: %sNo local src directory found\n", YEL, RES);
	}

	if(bssh.local_cmake != NULL) {
	    copyFile(bssh.local_cmake, "./external/Basilisk/CMakeLists.txt", true);
	} else {
	    printf("%sWARNING: %sNo local CMakeLists.txt found\n", YEL, RES);
	}
	exit(0);
    }

    if(hasFlag(FLAG_UPDATE))
	update();

    char bas_path_src[bssh_path_len + sizeof("Basilisk/src")];
    char bas_path_include[bssh_path_len + sizeof("Basilisk/include")];
    char bas_path_cmake[bssh_path_len + sizeof("Basilisk/CMakeLists.txt")];

    sprintf(bas_path_src, "%sBasilisk/src", bssh_path);
    sprintf(bas_path_include, "%sBasilisk/include", bssh_path);
    sprintf(bas_path_cmake, "%sBasilisk/CMakeLists.txt", bssh_path);

    copyDir(bas_path_src, "./external/Basilisk/src", true);
    copyDir(bas_path_include, "./external/Basilisk/include", true);
    copyFile(bas_path_cmake, "./external/Basilisk/CMakeLists.txt", true);
}

void interpretOpts() {
    if(cur_opt == NULL) {
	if(hasFlag(FLAG_HELP)) {
	    printHelp();
	    return;
	}

	printHelp();
	return;
    }

    if(hasFlag(FLAG_HELP)) {
	cur_opt->usage();
	exit(0);
    }

    if(cur_opt->num_sub_opts < cur_opt->min_sub_opts) {
	printf("%sERROR: %sToo few subcommands\n", RED, RES);
	cur_opt->usage();
	exit(1);
    }

    cur_opt->func();
}

void initBssh() {
    // Don't try to initialize if this succeeds
    if(loadBsshPaths())
	return;
/*
    char bssh_exe[exe_path_len + sizeof("/bssh.exe")];
    char bssh_old_exe[exe_path_len + sizeof("/bssh-old.exe")];

    sprintf(bssh_exe, "%s/bssh.exe", exe_path);
    sprintf(bssh_old_exe, "%s/bssh-old.exe", exe_path);

    printf("%s\n", bssh_exe);
    printf("%s\n", bssh_old_exe);
    //moveFile(bssh_exe, bssh_old_exe);
//    copyFile("C:\\Users\\henry\\Desktop\\test21\\bssh.exe", "build\\bssh.exe", true);

    clog_HTTP http = clog_InitGET("https://basilisk.sh/", NULL, 443);
    printf("%s\n", http.data);
    int x = clog_GET(&http);
    printf("%s\n", http.data);
    printf("%d\n", x);
    printf("%d\n", clog_lastError());
*/
    exit(0);
}

int main(int argc, char **argv) {
    initBssh();

    argv++;
    argc--;

    flags[FLAG_HELP]       = (Flag){ "help"    , 'h', 0 };
    flags[FLAG_GEOM]       = (Flag){ "geom"    , 'g', 0 };
    flags[FLAG_UPDATE]     = (Flag){ "update"  , 'u', 0 };
    flags[FLAG_BSSH]       = (Flag){ "bssh"    , 'b', 0 };
    flags[FLAG_LOCAL]      = (Flag){ "local"   , 'l', 0 };

    opts[OPT_INIT]         = (Option){ "new"       , init      , printInitUsage      , 0, 1, 1, { NULL }};
    opts[OPT_UPDATE]       = (Option){ "update"    , update    , printUpdateUsage    , 0, 0, 1, { NULL }};
    opts[OPT_UGRADE]       = (Option){ "upgrade"   , upgrade   , printUpgradeUsage   , 0, 0, 0, { NULL }};
    opts[OPT_SHADER]       = (Option){ "shader"    , shader    , printShaderUsage    , 0, 1, 1, { NULL }};

    toml_buf = malloc(2048);

    git_libgit2_init();

    for(int i = 0; i < argc; i++) {
	parse(argv[i]);
    }

    interpretOpts();

    if(bssh.has_changed && bssh.main != NULL) {
	int buf_len = 0;

	print_table(bssh.main, toml_buf, &buf_len);
	writeFile(bssh.path, toml_buf);
    }

    free(toml_buf);
    return 0;
}
