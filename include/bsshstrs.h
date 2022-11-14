#ifndef BSSHSTRS_H
#define BSSHSTRS_H

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define RES "\e[0m"

#define NBLU		"\x1b[38;2;105;120;237m"
#define NPUR		"\x1b[38;2;185;70;237m"
#define NGRN		"\x1b[38;2;105;160;125m"
#define NORA		"\x1b[38;2;250;120;100m"
#define NYEL		"\x1b[38;2;200;60;120m"
#define NWHEB		"\e[47m"
#define NWHE		"\e[1;37m"
#define NGRY		"\x1b[38;2;50;50;50m"

#define LINUX_SELF_PATH "/proc/self/exe/"

#ifdef _WIN32
    #include <windows.h>
    #include <fileapi.h>

    #define INITFILES_PATH "initfiles_win/"

    #define CREAT_WRITE _S_IWRITE
#endif
#if defined(unix) || defined(__unix__) || defined(__unix)
    #define INITFILES_PATH "initfiles_linux/"

    #define CREAT_WRITE COPYMORE
#endif

char *replaceStrStr(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
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

char* replaceFirstSubstring(char* str, char* old_str, char* new_str) {
    int new_len = strlen(new_str);
    int old_len = strlen(old_str);
    int str_len = strlen(str);

    // Counting the number of times old word
    // occur in the string
    char *start = strstr(str, old_str);
    int new_size = str_len - old_len + new_len;

    if(start == NULL) 
	return NULL;

    int replace_offset = start-str;

    // Making new string of enough length
    char *result = malloc(new_size+16);
    int offset = 0;

    strncpy(result + offset, str, replace_offset); offset += replace_offset;
    strncpy(result + offset, new_str, new_len);    offset += new_len;
    strncpy(result + offset, start + old_len, str_len - offset);
    result[new_size-1] = '\0';

    return result;
}

void writeFile(char *name, char *data) {
    errno = 0;
    FILE *fp = fopen(name, "w");
    if (fp != NULL) {
	fprintf(fp, "%s", data);
        fclose(fp);
	return;
    }

    printf("%sWARNING: %sCouldn't write file \"%s\", errno %d\n", YEL, RES, name, errno);
}

bool dirExists(LPCTSTR szPath) {
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool fileExists(LPCTSTR szPath) {
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void createDir(char *name) {
#ifdef _WIN32
    CreateDirectory(name, NULL);
    int err_win = GetLastError();
    if(err_win == ERROR_ALREADY_EXISTS) {
	printf("%sERROR: %sDirectory \"%s\" already exists!\n", RED, RES, name);
	exit(1);
    }
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
    struct stat st = {0};
    if(stat(name &st) == -1) {
	printf("%sERROR: %sDirectory \"%s\" already exists!\n", RED, RES, name);
	exit(1);
    }
    mkdir(name, 0700);
#endif
}

void copyFiles(char *source, char *destination) {
    int in_fd, out_fd, n_chars;
    char buf[512];

    /* Open files */
    if((in_fd = open(source, _O_RDONLY)) == -1) {
	printf("%sERROR: %sCould not open \"%s\"\n", RED, RES, source);
	exit(1);
    }

    if((out_fd = creat(destination, CREAT_WRITE)) == -1) {
	if(errno == EACCES) {
	    printf("%sERROR: %s\"%s\" already exists and is read-only\n", RED, RES, destination);
	} else {
	    printf("%sERROR: %sCould not copy to \"%s\", error code %d\n", RED, RES, destination, errno);
	}
	exit(1);
    }

    /* Copy files */
    while((n_chars = read(in_fd, buf, 512)) > 0) {
	if(write(out_fd, buf, n_chars) != n_chars) {
	    printf("%sERROR: %sCould not copy to \"%s\"\n", RED, RES, destination);
	    exit(1);
	}

	if(n_chars == -1) {
	    printf("%sERROR: %sCould not read data from \"%s\"\n", RED, RES, source);
	}
    }
}

void copyDir(char *src, char *dst);

#if defined(unix) || defined(__unix__) || defined(__unix)
int copyDirSkip(char *source, char *destination, int skip_count, char **skip) {
    DIR *dir_ptr = NULL;
    struct dirent *direntp;
    char tempDest[strlen(destination)+1];
    char tempSrc[strlen(source)+1];
    strcat(destination, "/");
    strcat(source, "/");
    strcpy(tempDest, destination);
    strcpy(tempSrc, source);

    struct stat fileinfo;

    if((dir_ptr = opendir(source)) == NULL) {
	printf("%sERROR: %sCannot open %s for copying\n", RES, RES, source);
	return 0;
    } else {
	while((direntp = readdir(dir_ptr))) {
	    if(dostat(direntp->d_name)) {   
		strcat(tempDest, direntp->d_name);
		strcat(tempSrc, direntp->d_name);
		copyFiles(tempSrc, tempDest);
		strcpy(tempDest, destination);
		strcpy(tempSrc, source);            
	    }
	}

	closedir(dir_ptr);
	return 1;
    }
}

#endif

#ifdef _WIN32
void copyDirSkip(char *src, char *dst, int skip_count, char **skip) {
    createDir(dst);

    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;

    int strlen_src = strlen(src);
    int strlen_dst = strlen(dst);

    char srca[strlen_src+3];
    sprintf(srca, "%s/*", src);

    if((hFind = FindFirstFile(srca, &FindFileData)) != INVALID_HANDLE_VALUE) {
	do {
	    char *name = FindFileData.cFileName;

	    for(int i = 0; i < skip_count; i++) {
		if(strcmp(skip[i], name) == 0)
		    goto next;
	    }

	    int strlen_name = strlen(name);

	    /* If it is a subdirectory, recursively copy that folders files */
	    if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		char sub_name[strlen_src + strlen_name + 2];
		char sub_dst [strlen_dst + strlen_name + 2];
		sprintf(sub_name, "%s/%s", src, name);
		sprintf(sub_dst , "%s/%s", dst, name);
		
		char *sub_skip[2] = { ".", ".." };
		copyDirSkip(sub_name, sub_dst, 2, sub_skip);
	    } else {
		char file_path[strlen_dst + strlen_name + 2];
		char sub_name [strlen_src + strlen_name + 2];
		sprintf(sub_name , "%s/%s", src, name);
		sprintf(file_path, "%s/%s", dst, name);
		copyFiles(sub_name, file_path);
	    }
next: continue;
	} while(FindNextFile(hFind, &FindFileData));

	FindClose(hFind);
    }
}
#endif

void copyDir(char *src, char *dst) {
    char *skip[2] = { ".", ".." };
    copyDirSkip(src, dst, 2, skip);
}

#endif /* BSSHSTRS_H */
