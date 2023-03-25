
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__unix__) || defined(__linux__)
    #include <dirent.h>
#endif

#if !defined(__TINYC__) && !defined(__cppcheck__)
    #if __has_include(<readline/readline.h>)
        #include <readline/readline.h>
        #include <readline/history.h>
        #define TIN_HAVE_READLINE
    #endif
#endif

#include "priv.h"

enum
{
    MAX_RESTARGS = 64,
    MAX_OPTS = 64/4,
};

typedef struct Flag_t Flag_t;
typedef struct FlagContext_t FlagContext_t;

struct Flag_t
{
    char flag;
    char* value;
};

struct FlagContext_t
{
    int nargc;
    int fcnt;
    int poscnt;
    char* positional[MAX_RESTARGS + 1];
    Flag_t flags[MAX_OPTS + 1];
};

typedef struct Options_t Options_t;

// Used for clean up on Ctrl+C / Ctrl+Z
static TinState* replstate;

static bool populate_flags(int argc, int begin, char** argv, const char* expectvalue, FlagContext_t* fx)
{
    int i;
    int nextch;
    int psidx;
    int flidx;
    char* arg;
    char* nextarg;
    psidx = 0;
    flidx = 0;
    fx->fcnt = 0;
    fx->poscnt = 0;
    for(i=begin; i<argc; i++)
    {
        arg = argv[i];
        nextarg = NULL;
        if((i+1) < argc)
        {
            nextarg = argv[i+1];
        }
        if(arg[0] == '-')
        {
            fx->flags[flidx].flag = arg[1];
            fx->flags[flidx].value = NULL;
            if(strchr(expectvalue, arg[1]) != NULL)
            {
                nextch = arg[2];
                /* -e "somecode(...)" */
                /* -e is followed by text: -e"somecode(...)" */
                if(nextch != 0)
                {
                    fx->flags[flidx].value = arg + 2;
                }
                else if(nextarg != NULL)
                {
                    if(nextarg[0] != '-')
                    {
                        fx->flags[flidx].value = nextarg;
                        i++;
                    }
                }
                else
                {
                    fx->flags[flidx].value = NULL;
                }
            }
            flidx++;
        }
        else
        {
            fx->positional[psidx] = arg;
            psidx++;
        }
    }
    fx->fcnt = flidx;
    fx->poscnt = psidx;
    fx->nargc = i;
    return true;
}

static void show_help()
{
    printf("lit [options] [files]\n");
    printf("    -o --output [file]  Instead of running the file the compiled bytecode will be saved.\n");
    printf(" -O[name] [string] Enables given optimization. For the list of aviable optimizations run with -Ohelp\n");
    printf(" -D[name]  Defines given symbol.\n");
    printf(" -e --eval [string] Runs the given code string.\n");
    printf(" -p --pass [args] Passes the rest of the arguments to the script.\n");
    printf(" -i --interactive Starts an interactive shell.\n");
    printf(" -d --dump  Dumps all the bytecode chunks from the given file.\n");
    printf(" -t --time  Measures and prints the compilation timings.\n");
    printf(" -h --help  I wonder, what this option does.\n");
    printf(" If no code to run is provided, lit will try to run either main.lbc or main.lit and, if fails, default to an interactive shell will start.\n");
}

/*
static void show_optimization_help()
{
    int i;
    printf(
        "Tin has a lot of optimzations.\n"
        "You can turn each one on or off or use a predefined optimization level to set them to a default value.\n"
        "The more optimizations are enabled, the longer it takes to compile, but the program should run better.\n"
        "So I recommend using low optimization for development and high optimization for release.\n"
        "To enable an optimization, run lit with argument -O[optimization], for example -Oconstant-folding.\n"
        "Using flag -Oall will enable all optimizations.\n"
        "To disable an optimization, run lit with argument -Ono-[optimization], for example -Ono-constant-folding.\n"
        "Using flag -Oall will disable all optimizations.\n"
    );
    printf("Here is a list of all supported optimizations:\n\n");
    for(i = 0; i < TINOPTSTATE_TOTAL; i++)
    {
        printf(" %s  %s\n", tin_astopt_getoptname((TinAstOptType)i),
               tin_astopt_getoptdescr((TinAstOptType)i));
    }
    printf("\nIf you want to use a predefined optimization level (recommended), run lit with argument -O[optimization level], for example -O1.\n\n");
    for(i = 0; i < TINOPTLEVEL_TOTAL; i++)
    {
        printf("\t-O%i\t\t%s\n", i, tin_astopt_getoptleveldescr((TinAstOptLevel)i));
    }
}
*/

int exitstate(TinState* state, TinStatus result)
{
    int64_t amount;
    amount = tin_destroy_state(state);
    if((result != TINSTATE_COMPILEERROR) && amount != 0)
    {
        fprintf(stderr, "gc: freed residual %i bytes\n", (int)amount);
        //return TIN_EXIT_CODE_MEM_LEAK;
        return 0;
    }
    if(result != TINSTATE_OK)
    {
        /*
        if(result == TINSTATE_RUNTIMEERROR)
        {
            return TIN_EXIT_CODE_RUNTIME_ERROR;
        }
        else
        {
            return TIN_EXIT_CODE_COMPILE_ERROR;
        }
        */
        return 1;
    }
    return 0;
}

struct Options_t
{
    char* debugmode;
    char* codeline;
};


static bool parse_options(Options_t* opts, Flag_t* flags, int fcnt)
{
    int i;
    opts->codeline = NULL;
    opts->debugmode = NULL;
    for(i=0; i<fcnt; i++)
    {
        switch(flags[i].flag)
        {
            case 'h':
                {
                    show_help();
                    return false;
                }
                break;
            case 'e':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-e' expects a string\n");
                        return false;
                    }
                    opts->codeline = flags[i].value;
                }
                break;
            case 'c':
                {
                    
                }
                break;
            case 'd':
                {
                    if(flags[i].value == NULL)
                    {
                        fprintf(stderr, "flag '-d' expects a value. run '-h' for possible values\n");
                        return false;
                    }
                    opts->debugmode = flags[i].value;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

void interupt_handler(int signalid)
{
    (void)signalid;
    tin_destroy_state(replstate);
    printf("\nExiting.\n");
    exit(0);
}

static int run_repl(TinState* state)
{
    #if defined(TIN_HAVE_READLINE)
        fprintf(stderr, "in repl...\n");
        char* line;
        replstate = state;
        signal(SIGINT, interupt_handler);
        //signal(SIGTSTP, interupt_handler);
        tin_astopt_setoptlevel(TINOPTLEVEL_REPL);
        printf("lit v%s, developed by @egordorichev\n", TIN_VERSION_STRING);
        while(true)
        {
            line = readline("> ");
            if(line == NULL)
            {
                return 0;
            }
            add_history(line);
            TinInterpretResult result = tin_state_execsource(state, "repl", line, strlen(line));
            if(result.type == TINSTATE_OK && !tin_value_isnull(result.result))
            {
                printf("%s%s%s\n", COLOR_GREEN, tin_string_getdata(tin_value_tostring(state, result.result)), COLOR_RESET);
            }
        }
    #else
        fprintf(stderr, "no repl compiled in. sorry\n");
    #endif
    return 0;
}

int main(int argc, char* argv[])
{
    int i;
    bool cmdfailed;
    const char* dm;
    const char* filename;
    TinArray* argarray;
    TinState* state;
    FlagContext_t fx;
    Options_t opts;
    TinStatus result;
    cmdfailed = false;
    result = TINSTATE_OK;
    populate_flags(argc, 1, argv, "ed", &fx);
    state = tin_make_state();
    tin_open_libraries(state);

    if(!parse_options(&opts, fx.flags, fx.fcnt))
    {
        cmdfailed = true;
    }
    else
    {
        if(opts.debugmode != NULL)
        {
            dm = opts.debugmode;
            if(strcmp(dm, "bc") == 0)
            {
                state->config.dumpbytecode = true;
            }
            else if(strcmp(dm, "ast") == 0)
            {
                state->config.dumpast = true;
            }
            else
            {
                fprintf(stderr, "unrecognized dump mode '%s'\n", dm);
                cmdfailed = true;
            }
        }
    }
    if(!cmdfailed)
    {
        if((fx.poscnt > 0) || (opts.codeline != NULL))
        {
            argarray = tin_object_makearray(state);
            for(i=0; i<fx.poscnt; i++)
            {
                tin_vallist_push(state, &argarray->list, tin_value_makestring(state, fx.positional[i]));
            }
            tin_state_setglobal(state, tin_string_copyconst(state, "ARGV"), tin_value_fromobject(argarray));
            if(opts.codeline)
            {
                result = tin_state_execsource(state, "<-e>", opts.codeline, strlen(opts.codeline)).type;
            }
            else
            {
                filename = fx.positional[0];
                result = tin_state_execfile(state, filename).type;
            }
        }
        else
        {
            #if defined(TIN_HAVE_READLINE)
                run_repl(state);
            #else
                fprintf(stderr, "no repl support compiled in\n");
            #endif
        }
    }
    return exitstate(state, result);
}

