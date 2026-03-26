#ifndef PARSER
#define PARSER

#include <limits.h>
#include <sys/types.h>

#define MAX_ARGUMENTS 32

//  funkcja parsowania
int parse_command(char* input_line, char* argument_array[MAX_ARGUMENTS]);

#endif
