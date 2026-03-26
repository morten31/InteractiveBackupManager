#define _XOPEN_SOURCE 500
#include "parser.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int parse_command(char* input_line, char* argument_array[MAX_ARGUMENTS])
{
    int argument_count = 0;
    int is_inside_quote = 0;
    char current_quote_char = '\0';

    char* read_ptr = input_line;
    char* write_ptr = input_line;

    // białe znaki z końca linii
    int len = strlen(input_line);
    while (len > 0 && isspace((unsigned char)input_line[len - 1]))
    {
        input_line[len - 1] = '\0';
        len--;
    }

    while (*read_ptr != '\0' && argument_count < MAX_ARGUMENTS)
    {
        // pomija spacje przed argumentem
        while (isspace((unsigned char)*read_ptr))
        {
            read_ptr++;
        }

        if (*read_ptr == '\0')
        {
            break;
        }

        // początek argumentu
        argument_array[argument_count++] = write_ptr;

        // treść argumentu
        while (*read_ptr != '\0')
        {
            char c = *read_ptr;

            if (isspace((unsigned char)c) && !is_inside_quote)
            {
                read_ptr++;
                break;
            }

            if (c == '"' || c == '\'')
            {
                if (is_inside_quote && c == current_quote_char)
                {
                    // koniec cudzysłowu
                    is_inside_quote = 0;
                    current_quote_char = '\0';
                }
                else if (!is_inside_quote)
                {
                    // początek cudzysłowu
                    is_inside_quote = 1;
                    current_quote_char = c;
                }
                else
                {
                    // kopiuje
                    *write_ptr++ = c;
                }
            }
            else
            {
                // kopiuje
                *write_ptr++ = c;
            }
            // nastepny znak
            read_ptr++;
        }
        *write_ptr++ = '\0';
    }

    argument_array[argument_count] = NULL;
    return argument_count;
}
