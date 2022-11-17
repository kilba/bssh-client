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

#define HOST "192.168.10.189"
#define PORT 8001

#define QR_FUL 219 
#define QR_BOT 220
#define QR_TOP 223

enum Flags {
    FLAG_HELP = 1,
    FLAG_VERSION = 2,
    FLAG_LATEST = 4,
    FLAG_GEOM = 8
};

enum {
    OPT_INIT,
    OPT_UPDATE,
    OPT_AUTH,
    OPT_PROFILE,
    OPT_TRY_PROFILE,
    OPT_BEFRIEND,
    OPT_UNFRIEND,
    OPT_SHADER,

    OPT_COUNT
};

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

#define USER 1
#define PASS 2

int flags = 0;
int arg_offset = 0;

char *cookie_path = NULL;
char *app_data_path = NULL;
char *bssh_path = NULL;
size_t bssh_path_len = 0;
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
    return (flags & flag) == flag;
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

void printTryProfileUsage() {
    char *msg = \
	  "\nUSAGE\n"\
	"    bssh tryprofile <name> [flags]\n";

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
    return;
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

    if(argCmp(arg, "geom")) {
	flags |= FLAG_GEOM;
	return;
    }
}

void charFlag(char arg) {
    switch(arg) {
	case 'h': flags |= FLAG_HELP; break;
	case 'v': flags |= FLAG_VERSION; break;
	case 'l': flags |= FLAG_LATEST; break;
	case 'g': flags |= FLAG_GEOM; break;
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
	flag(arg, arg_len);
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

    /* Parse .TOML data */
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
    for(int i = 0; i < num_elems; i++)
	skip[i+3] = repl[i].search;

    copyDirSkip(path, name, 3 + num_elems, skip);

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

    printf("Initialized a new project \"%s\"", name);
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

wchar_t subscript(char ascii) {
    const wchar_t table[] = { 
	L'ᵃ', L'ᵇ', L'ᶜ', L'ᵈ', L'ᵉ', L'ᶠ', L'ᵍ',
	L'ʰ', L'ⁱ', L'ʲ', L'ᵏ', L'ˡ', L'ᵐ', L'ⁿ',
	L'ᵒ', L'ᵖ', L'ᵖ', L'ʳ', L'ˢ', L'ᵗ', L'ᵘ',
	L'ᵛ', L'ʷ', L'ˣ', L'ʸ', L'ᶻ',

	L'ᴬ', L'ᴮ', L'ꟲ', L'ᴰ', L'ᴱ', L'ꟳ', L'ᴳ',
	L'ᴴ', L'ᴵ', L'ᴶ', L'ᴷ', L'ᴸ', L'ᴹ', L'ᴺ',
	L'ᴼ', L'ᴾ', L'ꟴ', L'ᴿ', L'ˢ', L'ᵀ', L'ᵁ', 
	L'ⱽ', L'ᵂ', L'ˣ', L'ʸ', L'ᶻ'
    };

    if(ascii >= 'a' && ascii <= 'z')
	return table[ascii - 'a'];

    if(ascii >= 'A' && ascii <= 'Z')
	return table[ascii - 'A'];

    return ' ';
}

void welcomeText(char *name) {
    int strlen_name = strlen(name) + 1;

    wchar_t wname[strlen_name];
    for(int i = 0; i < strlen_name; i++)
	wname[i] = subscript(name[i]);

    _setmode(_fileno(stdout), _O_U16TEXT);
    for(int i = 0; i < strlen_name; i++) {
	if(i == 0)
	    Sleep(200);
	wchar_t curr[strlen_name];
	for(int j = 0; j < (strlen_name-1); j++) {
	    curr[j] = (i == j) ? wname[j] : name[j];
	}
	curr[strlen(name)] = '\0';
	wprintf(L"\rWelcome, %s%S%s", NGRN, curr, RES);

	Sleep(80);
    }

    _setmode(_fileno(stdout), _O_TEXT);
    printf("%s", RES);
}

void auth();
void checkAuthError(char *body, void (*restart)(), int err) {
    err = clog_error();
    switch(err) {
	case 0: break;
	case WSAECONNRESET: printf("%sERROR: %sConnection Reset\n", RED, RES); exit(1);
	case WSAETIMEDOUT: printf("%sERROR: %sTimed out\n", RED, RES); exit(1);
	case WSAECONNREFUSED: printf("%sERROR: %sConnection refused, server might be offline\n", RED, RES); exit(1);

	default: printf("%sERROR: %sWSA error code %d\n", RED, RES, err); exit(1);
    }

    if(body[0] == '0')
	return;

    printf("%sERROR: %s%s\n", RED, RES, body + 1);

    switch(body[0]) {
	case '1': exit(1);
	case '2': restart();
	case '3': return;

	default: exit(1);
    }
}

/* Will be NULL until "clog_InitGET" is called,
 * then it will be allocated 2048 bytes. This will
 * be freed upon calling "clog_FreeGET" */
char *CLOG_INTERNAL_GET_BUF = NULL;

int clog_GET(clog_HTTP *data) {
    int err, siz;

    Clog chost;
    err = clog_conn(data->host, data->port, &chost);
    if(err == -1)
	return err;

    err = clog_send(&chost, CLOG_INTERNAL_GET_BUF, data->offset);
    if(err == -1)
	return err;

    siz = clog_recv( chost, CLOG_INTERNAL_GET_BUF, 2048);
    if(siz == -1)
	return siz;

    CLOG_INTERNAL_GET_BUF[siz] = '\0';
    data->body = strstr(CLOG_INTERNAL_GET_BUF, "\r\n\r\n") + 4;

    return 0;
}

clog_HTTP clog_InitGET(char *host, int port) {
    clog_HTTP http;
    http.host = host;
    http.port = port;
    http.offset = 0;

    if(CLOG_INTERNAL_GET_BUF == NULL)
	CLOG_INTERNAL_GET_BUF = malloc(2048);

    http.offset += sprintf(CLOG_INTERNAL_GET_BUF, "%s %s\r\n", CLOG_BEG_HTTP_1_1, host);

    return http;
}

void clog_AddHeader(clog_HTTP *data, char *key, char *value) {
    data->offset += sprintf(
	CLOG_INTERNAL_GET_BUF + data->offset,
	"%s: %s\r\n",
	key, value
    );
}

void clog_AddCookieF(clog_HTTP *data, char *path) {
    int err, len;
    char *fdata = readFile(path, &len, &err);
    fdata[strcspn(fdata, "\r\n")] = 0;
    data->offset += sprintf(
	CLOG_INTERNAL_GET_BUF + data->offset,
	"Cookie: %s\r\n",
	fdata
    );
    free(fdata);
}

void clog_AddBody(clog_HTTP *data, char *body) {
    data->offset += sprintf(
	CLOG_INTERNAL_GET_BUF + data->offset,
	"\r\n%s",
	body
    );
}

void verifyEmail(char *user, char *mail, char *step_0, char *step_1, void (*reset)()) {
    clog_HTTP data;

    printf("Sending email verification...\n");

    data = clog_InitGET(HOST, PORT);
    clog_AddHeader(&data, "Type", step_0);
    clog_AddHeader(&data, "User", user);
    clog_AddHeader(&data, "Mail", mail);
    clog_AddBody(&data, "");
    checkAuthError(data.body, reset, clog_GET(&data));

    printf("Sent!\n\n");

    for(int i = 0; i < 3; i++) {
	char emtok[64];
	printf("Enter the token sent to your email: ");
	userInput(emtok, 64);

	data = clog_InitGET(HOST, PORT);
	clog_AddHeader(&data, "Type", step_1);
	clog_AddHeader(&data, "User", user);
	clog_AddHeader(&data, "Toke", emtok);
	clog_AddBody(&data, "");
	checkAuthError(data.body, reset, clog_GET(&data));

	if(data.body[0] == '0')
	    break;
    }
}

void verifyTotp(char *user, char *step, void (*callback)()) {
    clog_HTTP data;
    for(int i = 0; i < 3; i++) {
	char totp[8];
	printf("Enter the TOTP: ");
	fflush (stdin);
	fgets(totp, 8, stdin);

	data = clog_InitGET(HOST, PORT);
	clog_AddHeader(&data, "Type", step);
	clog_AddHeader(&data, "User", user);
	clog_AddHeader(&data, "Totp", totp);
	clog_AddBody(&data, "");
	checkAuthError(data.body, callback, clog_GET(&data));

	if(data.body[0] == '0')
	    break;
    }
}

void clog_saveCookies(char *path) {
    char *cookie_data = strstr(CLOG_INTERNAL_GET_BUF, "Set-Cookie:");
    if(cookie_data == NULL)
	return;
    cookie_data += sizeof("Set-Cookie:");
    int cookie_len = strcspn(cookie_data, "\r\n");

    char save[cookie_len + 1];
    memcpy(save, cookie_data, cookie_len);
    save[cookie_len] = '\0';

    writeFile(path, save);
}

void autoLogin();
void logoutAccount() {
    writeFile(cookiePath(), "");
    printf("Logged out\n");
}

void loginAccount() {
    char user[32];

    /* Verify email */
    printf("Username : ");
    userInput(user, 32);

    verifyEmail(user, "", "login_0", "login_1", loginAccount);

    /* Verify Totp */
    verifyTotp(user, "login_2", loginAccount);
}

void createAccount() {
    clog_HTTP data;
    char user[32], mail[64];

    printf("Username (1/2) : ");
    userInput(user, 32);
    printf("Email    (2/2) : ");
    userInput(mail, 32);
    printf("\n");

    verifyEmail(user, mail, "auth_0", "auth_1", createAccount);

    data = clog_InitGET(HOST, PORT);
    clog_AddHeader(&data, "Type", "auth_2");
    clog_AddHeader(&data, "User", user);
    clog_AddBody(&data, "");
    checkAuthError(data.body, createAccount, clog_GET(&data));

    uint8_t qr0[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    char *qr_buf = data.body + 1;

    bool ok = qrcodegen_encodeText(qr_buf,
	tempBuffer, qr0, qrcodegen_Ecc_MEDIUM,
	qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
	qrcodegen_Mask_AUTO, true);

    if(!ok) {
	printf("%sERROR: %sQR Code could not be generated!\n", RED, RES);
	exit(1);
    }

    /* Print "____[ QR CODE ]____, based on qr code width" */
    int size = qrcodegen_getSize(qr0);

    float underscore_len;
    underscore_len  = (float)size / 2.0 + 1.0;
    underscore_len -= sizeof("[ QR Code ]") / 2.0;
    underscore_len  = ceil(underscore_len);

    printf("\n  ");
    for(int i = 0; i < underscore_len; i++)
	printf("_");
    printf("[ QR CODE ]");
    for(int i = 0; i < underscore_len; i++)
	printf("_");
    printf("\n%s  ", NWHE);

    /* Print row of bottom halves */
    for(int i = 0; i < size+2; i++)
	printf("%c", QR_BOT);

    printf("%s\n", RES);

    /* Display QR Code */
    for (int y = 0; y < size; y+=2) {
	printf("  %s%s ", NWHEB, NGRY);

	for (int x = 0; x < size; x++) {
	    bool top = qrcodegen_getModule(qr0, x, y + 0);
	    bool bot = qrcodegen_getModule(qr0, x, y + 1);
	    
	    if(top && bot) {
		printf("%c", QR_FUL);
	    } else if(top) {
		printf("%c", QR_TOP);
	    } else if(bot) {
		printf("%c", QR_BOT);
	    } else {
		printf(" ");
	    }
	}
	printf(" %s", RES);

	switch(y) {
	    case 2 : printf("    Username : %s%s%s", NGRN, user, RES); break;
	    case 4 : printf("    Mail     : %s%s%s", NGRN, mail, RES); break;
	} 

	printf("\n");
    }
    printf("\n");

    verifyTotp(user, "auth_3", createAccount);
    clog_saveCookies(cookiePath());

    welcomeText(user);
    exit(1);
}

void autoLogin() {
    if(bssh.logged_in)
	return;

    clog_HTTP data = clog_InitGET(HOST, PORT);
    clog_AddHeader( &data, "Type", "autoLogin");
    clog_AddCookieF(&data, cookiePathChkError());
    clog_AddBody(&data, "");
    checkAuthError(data.body, loginAccount, clog_GET(&data));
    
    if(data.body[0] == '0')
	bssh.logged_in = true;
}

void auth() {
    char *type = cur_opt->args[0];
    if(strcmp(type, "create") == 0) {
	createAccount();
	return;
    }

    if(strcmp(type, "login") == 0) {
	loginAccount();
	return;
    }

    if(strcmp(type, "logout") == 0) {
	logoutAccount();
	return;
    }
}

void applyArt(char *key, int key_len, int indices[8][16], int posx, int posy) {
    for(int i = 0; i < key_len; i++) {
	int8_t c = key[i];

	for(int j = 0; j < 4; j+=2) {
	    int8_t b0 = (c >> (j + 0)) & 0x01;
	    int8_t b1 = (c >> (j + 1)) & 0x01;

	    b0 = (b0 == 0) ? -1 : 1;
	    b1 = (b1 == 0) ? -1 : 1;

	    posx += b0;
	    posy += b1;

	    if(posx >= 8 || posx < 0)
		continue;

	    if(posy >= 16 || posy < 0)
		continue;

	    indices[posx][posy]++;
	}
    }
}

void displayProfile(char *name, int num_pkgs) {
    const char chars[] = " .+*oO";
    int indices[8][16];

    /* Zero initialize */
    for(int i = 0; i < 8; i++)
	for(int j = 0; j < 16; j++)
	    indices[i][j] = 0;
    
    /* Generate SHA1 from name, then encode with Base64 */
    char sha1[21];
    int name_len = strlen(name);
    SHA1(sha1, name, name_len);
    sha1[20] = '\0';
    size_t b64_len;
    char *b64 = (char *)base64_encode((const unsigned char *)sha1, 20, &b64_len);

    /* Fill indices[8][16] with art */
    applyArt(b64, 20, indices, 4, 8);

    /* Print name bar at center: [ username ] */
    int len = sizeof("              ");
    len -= sizeof("[  ]");
    len -= name_len;
    len /= 2;
    len += 1;

    printf("  + ");
    for(int i = 0; i < len; i++)
	printf(" ");
    printf("[ %s ]", name);
    for(int i = 0; i < len; i++)
	printf(" ");
    printf(" +\n%s", NBLU);

    /* Print the art with colors */
    for(int i = 0; i < 8; i++) {
	printf("   ");
	for(int j = 0; j < 16; j++) {
	    int index = indices[i][j];
	    index = (index >= (sizeof(chars)-1)) ? (sizeof(chars)-1) : index;

	    if(index >= 3) {
		printf("%s%c%s", CYN, chars[index], NBLU);
		continue;
	    }

	    printf("%c", chars[index]);
	}

	printf("\n");
    }
    printf("  %s+                 +\n", RES);
}

void profile() {
    char *name = cur_opt->args[0];
    displayProfile(name, 0);
}

void tryProfile() {
    char *name = cur_opt->args[0];
    displayProfile(name, 0);
}

void befriend() {
    loadBsshData();

    clog_HTTP data;
    char *name = cur_opt->args[0];

    data = clog_InitGET(HOST, PORT);
    clog_AddHeader( &data, "Type" , "befriend");
    clog_AddHeader( &data, "Friend", name);
    clog_AddCookieF(&data, cookiePathChkError());
    clog_AddBody(&data, "");
    checkAuthError(data.body, exitError, clog_GET(&data));

    switch(data.body[1]) {
	case '0': printf("%sFriend request sent%s\n", GRN, RES); break;
	case '1': printf("You are now friends with %s\"%s\"%s\n", NPUR, name, RES); break;
    }
}

void unfriend() {
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

    if(cur_opt->num_sub_opts < cur_opt->min_sub_opts) {
	printf("%sERROR: %sToo few subcommands\n", RED, RES);
	cur_opt->usage();
	exit(1);
    }

    cur_opt->func();
}

int main(int argc, char **argv) {
    argv++;
    argc--;

    opts[OPT_INIT]         = (Option){ "new"       , init      , printInitUsage      , 0, 1, 1, { NULL }};
    opts[OPT_UPDATE]       = (Option){ "update"    , update    , printUpdateUsage    , 0, 0, 0, { NULL }};
    opts[OPT_AUTH]         = (Option){ "auth"      , auth      , printAuthUsage      , 0, 1, 1, { NULL }};
    opts[OPT_PROFILE]      = (Option){ "profile"   , profile   , printProfileUsage   , 0, 1, 1, { NULL }};
    opts[OPT_TRY_PROFILE]  = (Option){ "tryprofile", tryProfile, printTryProfileUsage, 0, 1, 1, { NULL }};
    opts[OPT_BEFRIEND]     = (Option){ "befriend"  , befriend  , printBefriendUsage  , 0, 1, 1, { NULL }};
    opts[OPT_UNFRIEND]     = (Option){ "unfriend"  , unfriend  , printUnfriendUsage  , 0, 1, 1, { NULL }};
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
