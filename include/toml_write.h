#ifndef TOML_WRITE_H
#define TOML_WRITE_H

#include <toml.h>

void print_table(const TomlTable *table, char *buf, int *len);
void print_value(const TomlValue *value, int print_eq, char *buf, int *len);
void print_array(const TomlArray *array, char *buf, int *len);
void print_keyval(const TomlKeyValue *keyval, char *buf, int *len);

#endif /* TOML_WRITE_H */
