#ifndef BSSHSTRS_HPP
#define BSSHSTRS_HPP

#include <cstring>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <errhandlingapi.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

namespace fs = std::filesystem;

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

std::string replaceSubstrings(std::string string, std::string sub_string, std::string replacement) {
    return std::regex_replace(string, std::regex("\\" + sub_string), replacement);
}

char *replaceStrStr(const char *orig, char *rep, char *with) {
    char *result; // the return string
    const char *ins;    // the next insert point
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

    tmp = result = (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

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

auto readFile(std::string_view path) -> std::string {
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(path.data());
    stream.exceptions(std::ios_base::badbit);

    if (not stream) {
        throw std::ios_base::failure("file does not exist");
    }
    
    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(& buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
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
    char *result = (char *)malloc(new_size+16);
    int offset = 0;

    strncpy(result + offset, str, replace_offset); offset += replace_offset;
    strncpy(result + offset, new_str, new_len);    offset += new_len;
    strncpy(result + offset, start + old_len, str_len - offset);
    result[new_size-1] = '\0';

    return result;
}

void writeFile(std::string name, std::string data) {
    std::ofstream file;
    file.open(name);
    file << data;
    file.close();
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

void createDir(std::string name) {
    fs::create_directory(name);
}

void moveFile(char *src, char *dest) {
    if(!MoveFile(src, dest)) {
	int err = GetLastError();
	switch(err) {
	    case ERROR_FILE_NOT_FOUND: printf("%sERROR: %sFile \"%s\" does not exist\n", RED, RES, src); break;
	    case ERROR_FILE_EXISTS: printf("%sERROR: %sFile \"%s\" already exists\n", RED, RES, dest); break;
	    default: printf("%sERROR: %sGetLastError returned %d\n", RED, RES, err);
	}
    }
}

void copyFile(std::string src, std::string dst, bool overwrite) {
    if(overwrite)
	fs::remove_all(dst);

    fs::copy(src, dst);
}

void copyDirSkip(std::string src, std::string dst, std::vector<std::string> skip, bool overwrite) {
    if(overwrite)
	fs::remove_all(dst);

    fs::create_directory(dst);

    for(const auto &entry : fs::recursive_directory_iterator(src)) {
	if(std::find(skip.begin(), skip.end(), entry.path().filename().string()) == skip.end()) {
	    std::string path = entry.path().string();
	    path.erase(path.begin(), path.begin() + src.size());
	    if(entry.is_directory()) {
		fs::create_directory(dst + "\\" + path);
	    } else {
		fs::copy(entry.path().string(), dst + "\\" + path);
	    }
	}
    }
}

void copyDir(std::string src, std::string dst, bool overwrite) {
    if(overwrite)
	fs::remove_all(dst);

    fs::copy(src, dst, fs::copy_options::recursive);
}

#endif /* BSSHSTRS_HPP */
