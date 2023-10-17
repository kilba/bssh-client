#include <cpr/cpr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <map>
#include <stdbool.h>
#include <math.h>
#include <conio.h>

#include <json.hpp>
#include <git2.h>
#include <bsshstrs.hpp>
#include <help.hpp>
#include <bssh.hpp>

Flag flags[FLAG_COUNT];
std::map<std::string, std::shared_ptr<Option>> opts;
std::shared_ptr<Option> cur_opt = NULL;

std::string bssh_path;

Progress progress = {0};

nlohmann::json json;

void loadBsshData();
bool argCmp(const char *first, const char *second) {
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
    std::shared_ptr<Option> new_opt = NULL;
    auto it = opts.find(std::string(arg));

    if(it != opts.end())
	new_opt = it->second;

    if(cur_opt == NULL) {
	/* Invalid Command */
	if(new_opt == NULL) {
	    printf("%s is not a valid command!\n", arg);
	    exit(1);
	}

	cur_opt = new_opt;
	return;
    }

    cur_opt->num_sub_opts++;
    if(cur_opt->num_sub_opts > cur_opt->max_sub_opts) {
	printf("%sERROR: %sToo many subcommands\n", RED, RES);
	cur_opt->usage();
	exit(1);
    }

    static int arg_offset;
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

void init() {
    std::string name = cur_opt->args[NAME];
    loadBsshData();

    if(name == ".") {
	createDir("./external/Basilisk");
	for(unsigned int i = 0; i < json["files"].size(); i++) {
	    copyDir(
		bssh_path + "Basilisk/" + std::string(json["files"][i]),
		name + "/external/Basilisk/" + std::string(json["files"][i]),
		false
	    );
	}
	printf("Added Basilisk to current project.\n");
	return;
    }

    // Copy initfiles to new project
    copyDir(bssh_path + "initfiles/", name, false);
    createDir(name + "/external/Basilisk");

    // Copy Basilisk to new project
    for(unsigned int i = 0; i < json["files"].size(); i++)
	copyDir(
	    bssh_path + "Basilisk/" + std::string(json["files"][i]),
	    name + "/external/Basilisk/" + std::string(json["files"][i]),
	    false
	);

    for(unsigned int i = 0; i < json["search"].size(); i++) {
	std::string contents = readFile(bssh_path + "initfiles/" + std::string(json["search"][i]));
	for(unsigned int j = 0; j < json["match"][i].size(); j++) {
	    std::string match = json["match"][i][j];
	    std::string replace = json["replace"][i][j];

	    if(replace == "I_PROJECT_NAME") {
		contents = replaceSubstrings(contents, match, name);
		continue;
	    }

	    if(match == "I_BS_PATH") {
		contents = replaceSubstrings(contents, match, bssh_path + "Basilisk");
		continue;
	    }
	    
	    contents = replaceSubstrings(contents, match, replace);
	}

	writeFile(name + "/" + std::string(json["search"][i]), contents);
    }

    std::cout << ("Initialized a new project \"" + name + "\"!\n");
}

void config() {
    loadBsshData();
    system(("vim " + bssh_path + "bssh.json").c_str());
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

void loadBsshPaths() {
    std::string appdata = getenv("APPDATA");
    bssh_path = std::string(appdata) + "/bssh/";
}

void loadBsshData() {
    json = nlohmann::json::parse(std::ifstream(bssh_path + "bssh.json"));
    if(json["match"].size() != json["replace"].size() || json["match"].size() != json["search"].size()) {
	printf("%s\"search\", \"match\" and \"replace\" needs to have the same no. of elements in bssh.json%s\n", RED, RES);
	exit(1);
    }
}

void clone(const char *url) {
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

    clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.fetch_opts.callbacks.transfer_progress = fetchProgress;

    git_repository *repo = NULL;
    int err = git_clone(&repo, url, (bssh_path + "Basilisk").c_str(), &clone_opts);
    if(err != 0) {
	printf("%sERROR:%s Couldn't clone the repository! (libgit2 err %d)\n", RED, RES, err);
	exit(1);
    }

    printProgress(progress.tot, progress.tot, GRN);
}

void fetch(const char *url) {
    int err;
    git_remote *remote;
    git_repository *repo;
    err = git_repository_open(&repo, (bssh_path + "Basilisk").c_str());
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
    if(!json["bssh_installed"]) {
	clone("https://github.com/kilba/Basilisk");
	json["bssh_installed"] = true;
    } else {
	fetch("https://github.com/kilba/Basilisk");
    }

    std::cout << RES;
}

void upgrade() {
    loadBsshData();

    createDir("./external/Basilisk");
    if(hasFlag(FLAG_LOCAL)) {
	for(unsigned int i = 0; i < json["files"].size(); i++) {
	    copyDir(
		std::string(json["local"]) + "/" + std::string(json["files"][i]), 
		"./external/Basilisk/" + std::string(json["files"][i]),
		true
	    );
	}
	exit(0);
    }

    if(hasFlag(FLAG_UPDATE))
	update();

    for(unsigned int i = 0; i < json["files"].size(); i++) {
	copyDir(
	    bssh_path + "Basilisk/" + std::string(json["files"][i]),
	    "./external/Basilisk/" + std::string(json["files"][i]),
	    true
	);
    }
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

void opt(std::string name, void (*func)(), void (*help)(), int num_sub_opts = 0, int min_sub_opts = 0, int max_sub_opts = 0) {
    opts.insert(std::pair<std::string, std::shared_ptr<Option>>(
	name, 
	std::make_shared<Option>(
	    (Option){ func, help, num_sub_opts, min_sub_opts, max_sub_opts, { NULL } }
	)
    ));
}

int main(int argc, char **argv) {
    loadBsshPaths();
/*
    cpr::Response t = cpr::Post(
	cpr::Url{"https://localhost:7031/Account"}, cpr::Payload{{"account", "{\"name\":\"test\",\"creationDate\":\"2023-08-05T09:18:41.2892579+02:00\"}"}},
	cpr::Header{{"Content-Type", "application/json"}}
    );

    std::cout << t.text << std::endl;

    return 0;
    */
    argv++;
    argc--;

    flags[FLAG_HELP]       = (Flag){ "help"    , 'h', 0 };
    flags[FLAG_GEOM]       = (Flag){ "geom"    , 'g', 0 };
    flags[FLAG_UPDATE]     = (Flag){ "update"  , 'u', 0 };
    flags[FLAG_BSSH]       = (Flag){ "bssh"    , 'b', 0 };
    flags[FLAG_LOCAL]      = (Flag){ "local"   , 'l', 0 };

    opt("new"		, init		, printInitUsage	, 0, 1, 1);
    opt("update"	, update	, printUpdateUsage	, 0, 0, 1);
    opt("upgrade"	, upgrade	, printUpgradeUsage);
    opt("config"	, config	, printConfigUsage);

    git_libgit2_init();

    for(int i = 0; i < argc; i++) {
	parse(argv[i]);
    }

    interpretOpts();
    return 0;
}
