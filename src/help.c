#include <stdio.h>

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
	"    bssh update [flags]\n"\
	  "\nFLAGS\n"\
	"    -b --bssh      Updates bssh\n";

    printf("%s", msg);
}

void printUpgradeUsage() {
    printf("Upgrades project to latest update\n");

    char *msg = \
	  "\nUSAGE\n"\
	"    bssh upgrade [flags]\n"\
	  "\nFLAGS\n"\
	"    -u --update     Also updates the engine\n"
    ;

    printf("%s", msg);
}

void printHelp() {
    char *help_msg = \
	  "USAGE\n"\
	"    bssh <command> <subcommands> [flags]\n"\
	"\nCOMMANDS\n"\
	"    new             Initialized a new project\n"\
	"    update          Updates Basilisk to the latest release\n"\
	"    upgrade         Updates the current project\n"\
	"\nFLAGS\n"\
	"    -h, --help      Prints help for a command\n"\
    ;

    printf("%s", help_msg);
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
