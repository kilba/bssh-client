#include <toml.h>
#include <toml_write.h>
#include <inttypes.h>

void print_array(const TomlArray *array, char *buf, int *len) {
    *len += sprintf(buf + *len, "[");
    for (size_t i = 0; i < array->len; i++) {
        if (i > 0) {
            *len += sprintf(buf + *len, ", ");
        }
        print_value(array->elements[i], 0, buf, len);
    }
    *len += sprintf(buf + *len, "]");
}

void print_value(const TomlValue *value, int print_eq, char *buf, int *len) {
    if(print_eq)
	*len += sprintf(buf + *len, " = ");

    switch (value->type) {
        case TOML_TABLE:
            print_table(value->value.table, buf, len);
            break;
        case TOML_ARRAY:
            print_array(value->value.array, buf, len);
            break;
        case TOML_STRING:
            *len += sprintf(buf + *len, "\"%s\"", value->value.string->str);
            break;
        case TOML_INTEGER:
            *len += sprintf(buf + *len, "%" PRId64, value->value.integer);
            break;
        case TOML_FLOAT:
            *len += sprintf(buf + *len, "%f", value->value.float_);
            break;
        case TOML_DATETIME:
            *len += sprintf(buf + *len, "(datetime)");
            break;
        case TOML_BOOLEAN:
            *len += sprintf(buf + *len, "%s", value->value.boolean ? "true" : "false");
            break;
    }
}

void print_keyval(const TomlKeyValue *keyval, char *buf, int *len) {
    if(keyval->value->type == TOML_TABLE) {
	*len += sprintf(buf + *len, "[%s]\n", keyval->key->str);
	print_value(keyval->value, 0, buf, len);
    } else {
	*len += sprintf(buf + *len, "    %s", keyval->key->str);
	print_value(keyval->value, 1, buf, len);
	*len += sprintf(buf + *len, "\n");
    }
}

void print_table(const TomlTable *table, char *buf, int *len) {
    TomlTableIter it = toml_table_iter_new((TomlTable *)table);

    size_t i = 0;
    while (toml_table_iter_has_next(&it)) {
        TomlKeyValue *keyval = toml_table_iter_get(&it);

        print_keyval(keyval, buf, len);

        toml_table_iter_next(&it);
        i++;
    }
    *len += sprintf(buf + *len, "\n");
}
