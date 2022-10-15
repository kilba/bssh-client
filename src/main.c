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
    OPT_INIT = 1,
    OPT_UPDATE = 2,
    OPT_UPDATE_BSSH = 4,
};

int flags = 0;
int sub_options = 0;
int option = -1;

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

void printHelp() {
    char *help_msg = \
	  "USAGE\n"\
	"    bssh <command> <subcommands> [flags]\n"\
	"\nCOMMANDS\n"\
	"    init            Initialized a new project\n"\
	"    get-pkg         Installs a package from the repository\n"\
	"    put-pkg         Uploads a package to the reposity\n"\
	"    update          Updates Basilisk to the latest release\n"\
	"    update-shell    Updates bssh to the latest release\n"\
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
    } else if(argCmp(arg, "update-shell")) {
	new_option = OPT_UPDATE_BSSH;
    }

    /* Invalid Command */
    if(new_option == -1) {
	printf("%s is not a valid command!\n", arg);
	exit(1);
    }

    if(option == -1) {
	option = new_option;
    }

    sub_options |= new_option;
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
void init() {
    printf("Initializing...\n");
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
    if(progress.index % 10 != 0)
	return 0;

    progress.last = progress.object;
    progress.object = stats->received_objects; 

    progress.tot = stats->total_objects;

    if(progress.object != progress.last)
	printProgress(stats->received_objects, stats->total_objects, YEL);

    return 0;
}


int testProgress(const git_transfer_progress *stats, void *payload) {
    printf("PROGRESS\n");
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

    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    fetch_opts.callbacks.transfer_progress = testProgress;
    err = git_remote_lookup(&remote, repo, "origin");
    printf("%d\n", err);
    err = git_remote_fetch(remote, NULL, &fetch_opts, NULL);
    printf("%d\n", err);

    git_oid fetch_oid, head_oid;
    err = git_reference_name_to_id(&fetch_oid, repo, "FETCH_HEAD");
    printf("%d\n", err);
    err = git_reference_name_to_id(&head_oid, repo, "HEAD");
    printf("%d\n", err);

    git_annotated_commit *fetchhead_commit;
    err = git_annotated_commit_lookup(&fetchhead_commit,
        repo,
        &fetch_oid
    );
    printf("%d\n", err);

    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    err = git_merge_analysis(&analysis, &preference, repo, (const git_annotated_commit**)&fetchhead_commit, 1);

    if(analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
	printf("Already up to date.\n");
	exit(1);
    }
    printf("%d\n", analysis);
    printf("%d\n", GIT_MERGE_ANALYSIS_FASTFORWARD);
    printf("%d\n", GIT_MERGE_ANALYSIS_NORMAL);
    printf("%d\n", GIT_MERGE_ANALYSIS_NONE);
    printf("%d\n", GIT_MERGE_ANALYSIS_UNBORN);

    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    merge_opts.flags = 0;
    merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;

    checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE|GIT_CHECKOUT_ALLOW_CONFLICTS;

    err = git_merge(repo,
                    (const git_annotated_commit **)&fetchhead_commit, 1,
                    &merge_opts, &checkout_opts);

    if(err != 0) {
	printf("Merge failed! %d\n", err);
	exit(1);
    }

    git_index *index;
    err = git_repository_index(&index, repo);

    if(git_index_has_conflicts(index)) {
	printf("Conflicts!\n");
	exit(1);
    }

    git_oid commit_oid,tree_oid;
    git_tree *tree;
    git_object *parent = NULL;
    git_reference *ref = NULL;
    git_signature *signature;

    err = git_revparse_ext(&parent, &ref, repo, "HEAD");
    if (err == GIT_ENOTFOUND) {
	printf("HEAD not found. Creating first commit\n");
	err = 0;
    }

    git_index_write_tree(&tree_oid, index);
    git_index_write(index);
    git_tree_lookup(&tree, repo, &tree_oid);
    git_signature_default(&signature, repo);

    git_commit_create_v(
	&commit_oid,
	repo,
	"HEAD",
	signature,
	signature,
	NULL,
	"Merged FETCH_HEAD into HEAD",
	tree,
	parent ? 1 : 0, parent
    );
    git_repository_state_cleanup(repo);
    printf("Updated! %d\n", err);
}

void update() {
    if(hasFlag(FLAG_LATEST))
	printf("Latest\n");

    loadBsshData();

    if(!bssh.is_installed_json->valueint) {
	clone("https://github.com/nusbog/Basilisk");
    } else {
	fetch("https://github.com/nusbog/Basilisk");
    }
    printf("%s", RES);
}

void updateShell() {
    printf("Updating Shell...\n");
}

void interpretOpts() {
    if(option == -1) {
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

    switch(option) {
	case OPT_INIT       : init();        break;
	case OPT_UPDATE     : update();      break;
	case OPT_UPDATE_BSSH: updateShell(); break;
    }
}

int main(int argc, char **argv) {
    argv++;
    argc--;

    git_libgit2_init();

    for(int i = 0; i < argc; i++) {
	parse(argv[i]);
    }

    interpretOpts();

    if(bssh.has_changed && bssh.data != NULL)
	writeFile(bssh.path, cJSON_Print(bssh.data));

    return 0;
}
