#ifndef OPENBOR_SMART_C_CONVERTER_H
#define OPENBOR_SMART_C_CONVERTER_H
#include <stddef.h>
char *convert_c_source(const char *source, char *error, size_t error_size);
char *convert_c_script_body(const char *source, char *error, size_t error_size);
char *convert_animation_text(const char *source, char *error, size_t error_size);
#endif
