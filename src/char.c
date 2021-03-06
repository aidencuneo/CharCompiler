#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "char.h"

#include "vmem.c"
#include "stack.c"
#include "varlist.c"
// #include "var.c"

char * current_file;
char * buffer = 0;

struct vmem * vmem;
struct stack * stack;
struct varlist * registers;
struct varlist * varlist;
struct varlist * funcs;
FILE *  file_descriptor;

char * tokens;
int tokens_len;

int * tokeninds;
int tokeninds_len;

// CLI Options
int minify = 0;
int verbose = 0;

void error(char * text, char * buffer, int pos)
{
    // int MAXLINE = 128;

    printf("\nProgram execution terminated:\n\n");

    if (buffer == NULL || pos < 0)
    {
        printf("(At %s)\n", current_file);
    }
    else
    {
        char * linepreview = malloc(5 + 1);
        linepreview[0] = buffer[pos - 2];
        linepreview[1] = buffer[pos - 1];
        linepreview[2] = buffer[pos];
        linepreview[3] = buffer[pos + 1];
        linepreview[4] = buffer[pos + 2];
        linepreview[5] = 0;

        printf("At %s : Pos %d\n\n", current_file, pos);
        printf("> {{ %s }}\n\n", linepreview);

        free(linepreview);
    }

    printf("Error: %s\n\n", text);

    quit(1);
}

void pushToken(char tok, int ind)
{
    if (tokens_len >= MAXIMUM_RECURSION ||
        tokeninds_len >= MAXIMUM_RECURSION
    )
        error("maximum scope depth exceeded", NULL, -1);

    tokens[tokens_len++] = tok;
    tokeninds[tokeninds_len++] = ind;
    // printf("\n]::+%d\n", tokens_len);
}

char popToken(int * ind)
{
    // printf("\n]::-%d\n", tokens_len - 1);
    if (tokens_len && tokeninds_len)
    {
        *ind = tokeninds[--tokeninds_len];
        return tokens[--tokens_len];
    }

    *ind = -1;
    return 0;
}

char * minify_code(char * code)
{
    int len = strlen(code);
    char * min = malloc(len + 1);
    min[0] = 0;
    int ind = 0;

    int comment = 0;
    int scharmode = 0; // Single quotes
    int dcharmode = 0; // Double quotes

    for (int i = 0; i < len; i++)
    {
        if (scharmode || dcharmode)
            ;
        else if (code[i] == '#')
            comment = 1;
        else if (code[i] == '\n')
            comment = 0;

        if (comment)
            continue;

        if (code[i] == '\'' && !dcharmode)
            scharmode = !scharmode;
        else if (code[i] == '"' && !scharmode)
            dcharmode = !dcharmode;

        if (!isspace(code[i]) || scharmode || dcharmode)
            min[ind++] = code[i];
    }

    min[ind] = 0;
    return min;
}

void quit(int code)
{
    free(stack->items);
    free(stack);

    // Free register names
    for (int i = 0; i < registers->size; i++)
        free(registers->names[i]);
    free(registers->names);
    free(registers->values);
    free(registers);

    // Free varlist names
    for (int i = 0; i < varlist->size; i++)
        free(varlist->names[i]);
    free(varlist->names);
    free(varlist->values);
    free(varlist);

    // Free function names
    for (int i = 0; i < funcs->size; i++)
        free(funcs->names[i]);
    free(funcs->names);
    free(funcs->values);
    free(funcs);



    free(buffer);

    free(tokens);
    free(tokeninds);

    exit(code);
}

int main(int argc, char ** argv)
{
    char ** args = malloc(argc * sizeof(char *));
    int newargc = 0;

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))
        {
            printf("Char %s\n", VERSION);
            free(args);
            return 0;
        }
        if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--minify"))
            minify = 1;
        if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--verbose"))
            verbose = 1;
        else
        {
            args[newargc] = argv[i];
            newargc++;
        }
    }

    if (!newargc)
        return 0;

    current_file = args[0];

    long length;
    FILE * f = fopen(current_file, "rb");

    if (!f)
    {
        printf("File at path '%s' does not exist or is not accessible\n", current_file);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length);
    int res = 0;
    if (buffer)
        res = fread(buffer, 1, length, f);
    fclose(f);

    if (!buffer)
        return 0;

    if (minify)
    {
        char * minified = minify_code(buffer);

        printf("%s\n", minified);

        free(minified);
        free(buffer);

        return 0;
    }

    // Interpreter vars
    vmem = newVMem(1024); // Block size is 1024
    stack = newStack(512); // Block size is 512
    int ptr = 0;
    int mult = 1; // 1 or -1
    file_descriptor = stdin;

    // Was the last if statement's condition true?
    int last_if_result = 0;

    // This is for saving the pointers of strings in memory
    int last_str_ptr = 0;

    int line = 1;
    int skipping = 0;
    int scope = 0;
    int comment = 0;
    int scharmode = 0; // Single quotes
    int dcharmode = 0; // Double quotes

    tokens = malloc(MAXIMUM_RECURSION);
    tokens[0] = 0;
    tokens_len = 0;

    tokeninds = malloc(MAXIMUM_RECURSION * sizeof(int));
    tokeninds_len = 0;

    // Custom variables
    registers = newVarlist(64);
    varlist = newVarlist(32);
    funcs = newVarlist(16);

    int is_defining = 0; // Currently defining a function?
    int ret_index = -1; // Index to return to after function call

    free(args);

    for (int i = 0; i < length; i++)
    {
        char ch = buffer[i];
        // printf("[%c:%d]\n", ch, last_if_result);
        if (!comment && ch != ' ' && ch != '\n' && ch != '\r' && ch != '#' && verbose)
        {
            printf("\n!%d |%c| > %d (%d) stack=[", skipping, ch, scope, ptr);
            for (int a = 0; a < stack->top + 1; a++)
                printf("%d,", stack->items[a]);
            if (stack->top + 1)
                printf("\b");
            printf("] mem=[");
            for (int a = 0; a < vmem->size; a++)
            {
                if (vmem->items[a] < 32)
                    printf("\\(%d)", vmem->items[a]);
                else
                    printf("%c", vmem->items[a]);
            }
            printf("]\n");
        }

        if (scharmode || dcharmode)
            ;
        else if (ch == '#')
            comment = 1;
        else if (ch == '\n')
        {
            comment = 0;
            line++;
        }

        if (comment)
            continue;

        // Debugging
        // printf("INST: %c\n", ch);

        if (scharmode || dcharmode)
            ;
        else if (ch == ';')
        {
            int last_tokenind = -1;
            char last_token = popToken(&last_tokenind);

            // printf("((%c)) ", last_token);
            // printf("{%d}\n", last_tokenind);

            if (scope <= 0 || (
                last_tokenind < 0 && (last_token == ':' || last_token == 'F')
            ))
                error("program is already at minimum scope", buffer, i);

            if (skipping > 0)
                --skipping;

            --scope;

            if (last_token == 'F')
            {
                if (is_defining)
                {
                    // printf("FINISHED FUNCTION DEFINITION\n");
                    is_defining = 0;
                }
                else
                {
                    // printf("RETURNING AFTER FUNCTION CALL\n");
                    // printf("%d\n", last_tokenind);
                    i = last_tokenind - 1;
                }
            }
            else if (last_token == ':')
            {
                // Make sure while loops don't work when defining functions
                // (I learnt that this was necessary the hard way...)
                if (ptr && !is_defining)
                {
                    // ++scope;
                    i = last_tokenind - 1;
                }
            }
        }
        else if (ch == '?')
        {
            ++scope;
            pushToken(ch, i);

            if (skipping > 0)
            {
                ++skipping;
                continue;
            }

            // Check if this is an elif statement (??), not just (?)
            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (n != '?')
                --i;
            // If it is an elif statement, only run if it's
            // condition is true and if the last if statement's
            // condition was false
            else if (last_if_result)
                ++skipping;

            // Neither an if statement nor an elif statement can run it's code
            // if the given condition is false
            if (!ptr)
                ++skipping;

            // Record the value of this if statement's condition
            last_if_result = ptr;
        }
        else if (ch == ':')
        {
            ++scope;
            pushToken(ch, i);

            if (skipping > 0)
            {
                ++skipping;
                continue;
            }

            if (!ptr)
                ++skipping;
        }

        if (ch == '\'' && !dcharmode)
        {
            // If end of string, add null byte and save pointer to ptr
            if (scharmode)
            {
                vmemAdd(vmem, 0);
                ptr = last_str_ptr;
            }
            // If beginning of string, save this string's pointer for later
            else
                last_str_ptr = vmem->size;

            scharmode = !scharmode;
        }

        else if (ch == '"' && !scharmode)
        {
            // If end of string, add null byte and save pointer to ptr
            if (dcharmode)
            {
                vmemAdd(vmem, 0);
                ptr = last_str_ptr;
            }
            // If beginning of string, save this string's pointer for later
            else
                last_str_ptr = vmem->size;

            dcharmode = !dcharmode;
        }

        else if (skipping)
            continue;

        // Add bytes of string to memory in string mode
        else if (scharmode || dcharmode)
            vmemAdd(vmem, ch);

        else if (ch == '!')
            ptr = !ptr;
        else if (ch == '&')
            ptr = ptr && varlistGet(registers, "0");
        else if (ch == '|')
            ptr = ptr || varlistGet(registers, "0");
        else if (ch == '{')
            ptr = ptr < varlistGet(registers, "0");
        else if (ch == '}')
            ptr = ptr > varlistGet(registers, "0");
        else if (ch == '@')
        {
            // The @ sign will commonly be used as a way to perform complex
            // operations on the stack with ease

            // You may call them 'stack macros'

            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (isspace(n))
                n = 0;

            if (n == 'c')
            {
                // Count the number of times the pointer is found in the stack
                int c = 0;

                for (int a = 0; a <= stack->top; a++)
                    if (stack->items[a] == ptr)
                        ++c;

                // Set the pointer to the result
                ptr = c;
            }
            else if (n == 's')
                // Set the top of the stack to the pointer
                if (ptr < -1)
                    stack->top = -1;
                else
                    stack->top = ptr;
            else
            {
                // Add argv to memory and push a pointer to each argument into the stack
                for (int a = argc - 1; a >= 0; a--)
                {
                    // For each argument, push it's entire contents with a null byte separator
                    int arglen = strlen(argv[a]);

                    // Make sure to save the pointer to this argument into the stack
                    int ptr = vmem->size;

                    // Add the string to memory
                    for (int b = 0; b < arglen; b++)
                        vmemAdd(vmem, argv[a][b]);
                    // Add null byte
                    vmemAdd(vmem, 0);

                    // Push the pointer to the stack
                    stackPush(stack, ptr);
                }

                // Push argc to stack
                stackPush(stack, argc);

                --i;
            }
        }
        else if (ch == '+')
        {
            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (n == '+')
                ptr += varlistGet(registers, "0");
            else
            {
                mult = 1;
                --i;
            }
        }
        else if (ch == '-')
        {
            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (n == '-')
                ptr -= varlistGet(registers, "0");
            else
            {
                mult = -1;
                --i;
            }
        }
        else if (ch == '*')
        {
            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (n == '*')
                ptr *= varlistGet(registers, "0");
            else
            {
                mult = ptr;
                --i;
            }
        }
        else if (ch == '/')
        {
            char n = 0;
            if (++i < length)
                n = buffer[i];

            if (n == '/')
                ptr /= varlistGet(registers, "0");
            else
                --i;
        }
        else if (ch == '%')
            ptr %= varlistGet(registers, "0");
        else if (ch == '=')
        {
            // Get length of name
            int start = i + 1;
            int nameLen = 0;
            while (++i < length && buffer[i] && !isspace(buffer[i]) && buffer[i] != '~')
                ++nameLen;

            // Get name
            char * name = malloc(nameLen + 1);
            for (int a = 0; a < nameLen; a++)
                name[a] = buffer[start + a];

            // Null byte
            name[nameLen] = 0;

            if (!name || !name[0])
                error("'=' keyword requires a variable name to store into (example: =name~)",
                    buffer, i - 1);

            varlistAdd(varlist, name, ptr);
        }
        else if (ch == '$')
        {
            // Get length of name
            int start = i + 1;
            int nameLen = 0;
            while (++i < length && buffer[i] && !isspace(buffer[i]) && buffer[i] != '~')
                ++nameLen;

            // Get name
            char * name = malloc(nameLen + 1);
            for (int a = 0; a < nameLen; a++)
                name[a] = buffer[start + a];

            // Null byte
            name[nameLen] = 0;

            if (!name || !name[0])
                error("'$' keyword requires a variable name to retrieve from (example: $name~)",
                    buffer, i - 1);

            if (!strcmp(name, "@"))
                ptr = stack->top + 1;
            else
                ptr = varlistGet(varlist, name);

            // Name is no longer required
            free(name);
        }
        else if (ch == 'F')
        {
            // Get length of name
            int start = i + 1;
            int nameLen = 0;
            while (++i < length && buffer[i] && !isspace(buffer[i]) && buffer[i] != '~')
                ++nameLen;

            // Get name
            char * name = malloc(nameLen + 1);
            for (int a = 0; a < nameLen; a++)
                name[a] = buffer[start + a];

            // Null byte
            name[nameLen] = 0;

            if (!name || !name[0])
                error("'F' keyword requires a function name to define or call (example: Fname~)",
                    buffer, i - 1);

            ++scope;
            pushToken(ch, i + 1);

            if (skipping > 0)
            {
                ++skipping;
                continue;
            }

            int ind = varlistGetDef(funcs, name, -1);

            // If function exists
            if (ind > -1) {
                // printf("%d (%c)\n", ind, buffer[ind - 1]);
                // Name is no longer required
                free(name);
                i = ind - 1;
            }
            // If function doesn't exist yet
            else
            {
                pushToken(ch, i + 1);
                is_defining = 1;
                varlistAdd(funcs, name, i + 1);
                ++skipping;
            }
        }
        else if (ch == '0')
            ptr = 0;
        else if (ch == '1')
            ptr += mult;
        else if (ch == '2')
            ptr += mult * 2;
        else if (ch == '3')
            ptr += mult * 3;
        else if (ch == '4')
            ptr += mult * 4;
        else if (ch == '5')
            ptr += mult * 5;
        else if (ch == '6')
            ptr += mult * 6;
        else if (ch == '7')
            ptr += mult * 7;
        else if (ch == '8')
            ptr += mult * 8;
        else if (ch == '9')
            ptr += mult * 9;
        else if (ch == 'a')
            ptr += mult * 10;
        else if (ch == 'b')
            ptr += mult * 11;
        else if (ch == 'c')
            ptr += mult * 12;
        else if (ch == 'd')
            ptr += mult * 13;
        else if (ch == 'e')
            ptr += mult * 14;
        else if (ch == 'f')
            ptr += mult * 15;
        else if (ch == 'o' || ch == 'O')
        {
            if (file_descriptor != stdin)
                fclose(file_descriptor);

            // For now, filenames must be 1024 characters or less
            char * filename = malloc(1024 + 1);
            int len = 0;

            int p;
            for (p = ptr ;; p++)
            {
                filename[len] = vmemGet(vmem, p);

                if (!filename[len])
                    break;

                // Increment filename len
                ++len;
            }

            // (Null byte already appended)

            // Open file for reading if lowercase 'o' was used, or open file
            // for writing if uppercase 'O' was used
            if (ch == 'o')
                file_descriptor = fopen(filename, "r+");
            else if (ch == 'O')
                file_descriptor = fopen(filename, "w+");

            // Free filename
            free(filename);

            // ptr will be 0 if the file opened properly
            ptr = 0;

            if (!file_descriptor)
            {
                // Set ptr to 1 to tell the code that this file wasn't opened
                // properly
                ptr = 1;

                // Set file_descriptor back to stdin to make sure there aren't
                // any major bugs
                file_descriptor = stdin;
            }
        }
        else if (ch == 'r')
        {
            // Get length of name
            int start = i + 1;
            int nameLen = 0;
            while (++i < length && buffer[i] && !isspace(buffer[i]) && buffer[i] != '~')
                ++nameLen;

            // Get name
            char * name = malloc(nameLen + 1);
            for (int a = 0; a < nameLen; a++)
                name[a] = buffer[start + a];

            // Null byte
            name[nameLen] = 0;

            if (!name || !name[0])
                error("'r' keyword requires a register name to retrieve from (example: r1~)",
                    buffer, i - 1);

            ptr = varlistGet(registers, name);

            // Name is no longer required
            free(name);
        }
        else if (ch == 'R')
        {
            // Get length of name
            int start = i + 1;
            int nameLen = 0;
            while (++i < length && buffer[i] && !isspace(buffer[i]) && buffer[i] != '~')
                ++nameLen;

            // Get name
            char * name = malloc(nameLen + 1);
            for (int a = 0; a < nameLen; a++)
                name[a] = buffer[start + a];

            // Null byte
            name[nameLen] = 0;

            if (!name || !name[0])
                error("'R' keyword requires a register name to store into (example: R1~)",
                    buffer, i - 1);

            varlistAdd(registers, name, ptr);
        }
        // Useful memory things
        else if (ch == 'm')
        {
            char n = 0;
            if (++i < length)
                n = buffer[i];

            // Set ptr to the current memory size
            if (n == 's')
                ptr = vmem->size;
            else if (n == 'S')
                vmem->items[varlistGet(registers, "0")] = ptr;
            // Print string from memory using ptr as the pointer
            else if (n == 'P')
            {
                for (int p = ptr ;; p++)
                {
                    int c = vmemGet(vmem, p);

                    if (!c)
                        break;

                    printf("%c", c);
                }
            }
            // Write string to file from memory using ptr as the pointer
            else if (n == '.')
            {
                for (int p = ptr ;; p++)
                {
                    int c = vmemGet(vmem, p);

                    if (!c)
                        break;

                    if (file_descriptor == stdin)
                        printf("%c", c);
                    else
                    {
                        char in[2] = {0};
                        in[0] = c;
                        fputs(in, file_descriptor);
                    }
                }
            }
            else
            {
                // Retrieve memory using ptr as the pointer
                ptr = vmemGet(vmem, ptr);
                --i;
            }
        }
        // Add ptr to memory (on top)
        else if (ch == 'M')
            vmemAdd(vmem, ptr);
        else if (ch == 'p')
            printf("%d", ptr);
        else if (ch == 'P')
            printf("%c", ptr);
        else if (ch == '<')
            ptr = stackPop(stack);
        else if (ch == '>')
        {
            stackPush(stack, ptr);
            ptr = 0;
        }
        else if (ch == '(')
            ptr = stackPeek(stack);
        else if (ch == ')')
            stackPush(stack, ptr);
        else if (ch == '[')
            ptr = stackPopBottom(stack);
        else if (ch == ']')
        {
            stackPushBottom(stack, ptr);
            ptr = 0;
        }
        else if (ch == ',')
        {
            // Those sneaky [0]'s after each `fgets` call are to prevent the
            // -Wunused-result warning from coming up during compilation
            char in[2] = {0};
            fgets(in, 2, file_descriptor)[0];

            // If pesky carriage returns are in the way in a file, skip them
            while (file_descriptor != stdin && *in == '\r')
                fgets(in, 2, file_descriptor)[0];

            if (file_descriptor == stdin && (
                *in == '\n' || *in == '\r' || *in < 0)
            )
                ptr = 0;
            else
                ptr = *in;
        }
        else if (ch == '.')
        {
            if (file_descriptor == stdin)
                printf("%c", ptr);
            else
            {
                char in[2] = {0};
                in[0] = ptr;
                fputs(in, file_descriptor);
            }
        }
        else if (ch == 'q')
            quit(ptr);
    }

    quit(0);

    return 0;
}
