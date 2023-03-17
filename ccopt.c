
#include "priv.h"

#define TIN_DEBUG_OPTIMIZER

#define optc_do_binary_op(op) \
    if(tin_value_isnumber(a) && tin_value_isnumber(b)) \
    { \
        tin_astopt_optdbg("translating constant binary expression of '" # op "' to constant value"); \
        return tin_value_makenumber(optimizer->state, tin_value_asnumber(a) op tin_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_bitwise_op(op) \
    if(tin_value_isnumber(a) && tin_value_isnumber(b)) \
    { \
        tin_astopt_optdbg("translating constant bitwise expression of '" #op "' to constant value"); \
        return tin_value_makenumber(optimizer->state, (int)tin_value_asnumber(a) op(int) tin_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_fn_op(fn, tokstr) \
    if(tin_value_isnumber(a) && tin_value_isnumber(b)) \
    { \
        tin_astopt_optdbg("translating constant expression of '" tokstr "' to constant value via tin_vm_callcallable to '" #fn "'"); \
        return tin_value_makenumber(optimizer->state, fn(tin_value_asnumber(a), tin_value_asnumber(b))); \
    } \
    return NULL_VALUE;



static void tin_astopt_optexpression(TinAstOptimizer* optimizer, TinAstExpression** slot);
static void tin_astopt_optexprlist(TinAstOptimizer* optimizer, TinAstExprList* expressions);
static void tin_astopt_optstmtlist(TinAstOptimizer* optimizer, TinAstExprList* statements);

static const char* optimization_level_descriptions[TINOPTLEVEL_TOTAL]
= { "No optimizations (same as -Ono-all)", "Super light optimizations, sepcific to interactive shell.",
    "(default) Recommended optimization level for the development.", "Medium optimization, recommended for the release.",
    "(default for bytecode) Extreme optimization, throws out most of the variable/function names, used for bytecode compilation." };

static const char* optimization_names[TINOPTSTATE_TOTAL]
= { "constant-folding", "literal-folding", "unused-var",    "unreachable-code",
    "empty-body",       "line-info",       "private-names", "c-for" };

static const char* optimization_descriptions[TINOPTSTATE_TOTAL]
= { "Replaces constants in code with their values.",
    "Precalculates literal expressions (3 + 4 is replaced with 7).",
    "Removes user-declared all variables, that were not used.",
    "Removes code that will never be reached.",
    "Removes loops with empty bodies.",
    "Removes line information from chunks to save on space.",
    "Removes names of the private locals from modules (they are indexed by id at runtime).",
    "Replaces for-in loops with c-style for loops where it can." };

static bool optimization_states[TINOPTSTATE_TOTAL];

static bool optimization_states_setup = false;
static bool any_optimization_enabled = false;

static void tin_astopt_setupstates();

#if defined(TIN_DEBUG_OPTIMIZER)
void tin_astopt_optdbg(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "optimizer: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}
#else
    #define tin_astopt_optdbg(msg, ...)
#endif

void tin_varlist_init(TinVarList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_varlist_destroy(TinState* state, TinVarList* array)
{
    TIN_FREE_ARRAY(state, sizeof(TinVariable), array->values, array->capacity);
    tin_varlist_init(array);
}

void tin_varlist_push(TinState* state, TinVarList* array, TinVariable value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_GROW_CAPACITY(oldcapacity);
        array->values = TIN_GROW_ARRAY(state, array->values, sizeof(TinVariable), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void tin_astopt_init(TinState* state, TinAstOptimizer* optimizer)
{
    optimizer->state = state;
    optimizer->depth = -1;
    optimizer->mark_used = false;
    tin_varlist_init(&optimizer->variables);
}

static void tin_astopt_beginscope(TinAstOptimizer* optimizer)
{
    optimizer->depth++;
}

static void tin_astopt_endscope(TinAstOptimizer* optimizer)
{
    bool remove_unused;
    TinVariable* variable;
    TinVarList* variables;
    optimizer->depth--;
    variables = &optimizer->variables;
    remove_unused = tin_astopt_isoptenabled(TINOPTSTATE_UNUSEDVAR);
    while(variables->count > 0 && variables->values[variables->count - 1].depth > optimizer->depth)
    {
        if(remove_unused && !variables->values[variables->count - 1].used)
        {
            variable = &variables->values[variables->count - 1];
            tin_ast_destroyexpression(optimizer->state, *variable->declaration);
            *variable->declaration = NULL;
        }
        variables->count--;
    }
}

static TinVariable* tin_astopt_addvar(TinAstOptimizer* optimizer, const char* name, size_t length, bool constant, TinAstExpression** declaration)
{
    tin_varlist_push(optimizer->state, &optimizer->variables,
                        (TinVariable){ name, length, optimizer->depth, constant, optimizer->mark_used, NULL_VALUE, declaration });

    return &optimizer->variables.values[optimizer->variables.count - 1];
}

static TinVariable* tin_astopt_resolvevar(TinAstOptimizer* optimizer, const char* name, size_t length)
{
    int i;
    TinVarList* variables;
    TinVariable* variable;
    variables = &optimizer->variables;
    for(i = variables->count - 1; i >= 0; i--)
    {
        variable = &variables->values[i];
        if(length == variable->length && memcmp(variable->name, name, length) == 0)
        {
            return variable;
        }
    }
    return NULL;
}

static bool tin_astopt_isemptyexpr(TinAstExpression* expression)
{
    return expression == NULL || (expression->type == TINEXPR_BLOCK && ((TinAstBlockExpr*)expression)->statements.count == 0);
}

static TinValue tin_astopt_evalunaryop(TinAstOptimizer* optimizer, TinValue value, TinAstTokType op)
{
    switch(op)
    {
        case TINTOK_MINUS:
            {
                if(tin_value_isnumber(value))
                {
                    tin_astopt_optdbg("translating constant unary minus on number to literal value");
                    return tin_value_makenumber(optimizer->state, -tin_value_asnumber(value));
                }
            }
            break;
        case TINTOK_BANG:
            {
                tin_astopt_optdbg("translating constant expression of '=' to literal value");
                return tin_value_makebool(optimizer->state, tin_value_isfalsey(value));
            }
            break;
        case TINTOK_TILDE:
            {
                if(tin_value_isnumber(value))
                {
                    tin_astopt_optdbg("translating unary tile (~) on number to literal value");
                    return tin_value_makenumber(optimizer->state, ~((int)tin_value_asnumber(value)));
                }
            }
            break;
        default:
            {
            }
            break;
    }
    return NULL_VALUE;
}

static TinValue tin_astopt_evalbinaryop(TinAstOptimizer* optimizer, TinValue a, TinValue b, TinAstTokType op)
{
    switch(op)
    {
        case TINTOK_PLUS:
            {
                optc_do_binary_op(+);
            }
            break;
        case TINTOK_MINUS:
            {
                optc_do_binary_op(-);
            }
            break;
        case TINTOK_STAR:
            {
                optc_do_binary_op(*);
            }
            break;
        case TINTOK_SLASH:
            {
                optc_do_binary_op(/);
            }
            break;
        case TINTOK_DOUBLESTAR:
            {
                optc_do_fn_op(pow, "**");
            }
            break;
        case TINTOK_PERCENT:
            {
                optc_do_fn_op(fmod, "%");
            }
            break;
        case TINTOK_GREATERTHAN:
            {
                optc_do_binary_op(>);
            }
            break;
        case TINTOK_GREATEREQUAL:
            {
                optc_do_binary_op(>=);
            }
            break;
        case TINTOK_LESSTHAN:
            {
                optc_do_binary_op(<);
            }
            break;
        case TINTOK_LESSEQUAL:
            {
                optc_do_binary_op(<=);
            }
            break;
        case TINTOK_SHIFTLEFT:
            {
                optc_do_bitwise_op(<<);
            }
            break;
        case TINTOK_SHIFTRIGHT:
            {
                optc_do_bitwise_op(>>);
            }
            break;
        case TINTOK_BAR:
            {
                optc_do_bitwise_op(|);
            }
            break;
        case TINTOK_AMPERSAND:
            {
                optc_do_bitwise_op(&);
            }
            break;
        case TINTOK_CARET:
            {
                optc_do_bitwise_op(^);
            }
            break;
        case TINTOK_SHARP:
            {
                if(tin_value_isnumber(a) && tin_value_isnumber(b))
                {
                    return tin_value_makenumber(optimizer->state, floor(tin_value_asnumber(a) / tin_value_asnumber(b)));
                }
                return NULL_VALUE;
            }
            break;
        case TINTOK_EQUAL:
            {
                return tin_value_makebool(optimizer->state, tin_value_compare(optimizer->state, a, b));
            }
            break;
        case TINTOK_BANGEQUAL:
            {
                return tin_value_makebool(optimizer->state, !tin_value_compare(optimizer->state, a, b));
            }
            break;
        case TINTOK_KWIS:
        default:
            {
            }
            break;
    }
    return NULL_VALUE;
}

static TinValue tin_astopt_attemptoptbinary(TinAstOptimizer* optimizer, TinAstBinaryExpr* expression, TinValue value, bool left)
{
    double number;
    TinAstTokType op;
    op = expression->op;
    TinAstExpression* branch;
    branch = left ? expression->left : expression->right;
    if(tin_value_isnumber(value))
    {
        number = tin_value_asnumber(value);
        if(op == TINTOK_STAR)
        {
            if(number == 0)
            {
                tin_astopt_optdbg("reducing expression to literal '0'");
                return tin_value_makenumber(optimizer->state, 0);
            }
            else if(number == 1)
            {
                tin_astopt_optdbg("reducing expression to literal '1'");
                tin_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
                expression->left = branch;
                expression->right = NULL;
            }
        }
        else if((op == TINTOK_PLUS || op == TINTOK_MINUS) && number == 0)
        {
            tin_astopt_optdbg("reducing expression that would result in '0' to literal '0'");
            tin_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
        else if(((left && op == TINTOK_SLASH) || op == TINTOK_DOUBLESTAR) && number == 1)
        {
            tin_astopt_optdbg("reducing expression that would result in '1' to literal '1'");
            tin_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
    }
    return NULL_VALUE;
}

static TinValue tin_astopt_evalexpr(TinAstOptimizer* optimizer, TinAstExpression* expression)
{
    TinAstUnaryExpr* uexpr;
    TinAstBinaryExpr* bexpr;
    TinValue a;
    TinValue b;
    TinValue branch;
    if(expression == NULL)
    {
        return NULL_VALUE;
    }
    switch(expression->type)
    {
        case TINEXPR_LITERAL:
            {
                return ((TinAstLiteralExpr*)expression)->value;
            }
            break;
        case TINEXPR_UNARY:
            {
                uexpr = (TinAstUnaryExpr*)expression;
                branch = tin_astopt_evalexpr(optimizer, uexpr->right);
                if(!tin_value_isnull(branch))
                {
                    return tin_astopt_evalunaryop(optimizer, branch, uexpr->op);
                }
            }
            break;
        case TINEXPR_BINARY:
            {
                bexpr = (TinAstBinaryExpr*)expression;
                a = tin_astopt_evalexpr(optimizer, bexpr->left);
                b = tin_astopt_evalexpr(optimizer, bexpr->right);
                if(!tin_value_isnull(a) && !tin_value_isnull(b))
                {
                    return tin_astopt_evalbinaryop(optimizer, a, b, bexpr->op);
                }
                else if(!tin_value_isnull(a))
                {
                    return tin_astopt_attemptoptbinary(optimizer, bexpr, a, false);
                }
                else if(!tin_value_isnull(b))
                {
                    return tin_astopt_attemptoptbinary(optimizer, bexpr, b, true);
                }
            }
            break;
        default:
            {
                return NULL_VALUE;
            }
            break;
    }
    return NULL_VALUE;
}

static void tin_astopt_optexprlist(TinAstOptimizer* optimizer, TinAstExprList* expressions)
{
    for(size_t i = 0; i < expressions->count; i++)
    {
        tin_astopt_optexpression(optimizer, &expressions->values[i]);
    }
}

static void tin_astopt_optforstmt(TinState* state, TinAstOptimizer* optimizer, TinAstExpression* expression, TinAstExpression** slot)
{
    TinAstForExpr* stmt = (TinAstForExpr*)expression;
    tin_astopt_beginscope(optimizer);
    // This is required, so that optimizer doesn't optimize out our i variable (and such)
    optimizer->mark_used = true;
    tin_astopt_optexpression(optimizer, &stmt->init);
    tin_astopt_optexpression(optimizer, &stmt->condition);
    tin_astopt_optexpression(optimizer, &stmt->increment);
    tin_astopt_optexpression(optimizer, &stmt->var);
    optimizer->mark_used = false;
    tin_astopt_optexpression(optimizer, &stmt->body);
    tin_astopt_endscope(optimizer);
    if(tin_astopt_isoptenabled(TINOPTSTATE_EMPTYBODY) && tin_astopt_isemptyexpr(stmt->body))
    {
        tin_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
        return;
    }
    if(stmt->cstyle || !tin_astopt_isoptenabled(TINOPTSTATE_CFOR) || stmt->condition->type != TINEXPR_RANGE)
    {
        return;
    }
    TinAstRangeExpr* range = (TinAstRangeExpr*)stmt->condition;
    TinValue from = tin_astopt_evalexpr(optimizer, range->from);
    TinValue to = tin_astopt_evalexpr(optimizer, range->to);
    if(!tin_value_isnumber(from) || !tin_value_isnumber(to))
    {
        return;
    }
    bool reverse = tin_value_asnumber(from) > tin_value_asnumber(to);
    TinAstAssignVarExpr* var = (TinAstAssignVarExpr*)stmt->var;
    size_t line = range->exobj.line;
    // var i = from
    var->init = range->from;
    // i <= to
    stmt->condition = (TinAstExpression*)tin_ast_make_binaryexpr(
    state, line, (TinAstExpression*)tin_ast_make_varexpr(state, line, var->name, var->length), range->to, TINTOK_LESSEQUAL);
    // i++ (or i--)
    TinAstExpression* var_get = (TinAstExpression*)tin_ast_make_varexpr(state, line, var->name, var->length);
    TinAstBinaryExpr* assign_value = tin_ast_make_binaryexpr(
    state, line, var_get, (TinAstExpression*)tin_ast_make_literalexpr(state, line, tin_value_makenumber(optimizer->state, 1)),
    reverse ? TINTOK_DOUBLEMINUS : TINTOK_PLUS);
    assign_value->ignore_left = true;
    TinAstExpression* increment
    = (TinAstExpression*)tin_ast_make_assignexpr(state, line, var_get, (TinAstExpression*)assign_value);
    stmt->increment = (TinAstExpression*)increment;
    range->from = NULL;
    range->to = NULL;
    stmt->cstyle = true;
    tin_ast_destroyexpression(state, (TinAstExpression*)range);
}

static void tin_astopt_optwhilestmt(TinState* state, TinAstOptimizer* optimizer, TinAstExpression* expression, TinAstExpression** slot)
{
    TinValue optimized;
    TinAstWhileExpr* stmt;
    (void)state;
    stmt = (TinAstWhileExpr*)expression;
    tin_astopt_optexpression(optimizer, &stmt->condition);
    if(tin_astopt_isoptenabled(TINOPTSTATE_UNREACHABLECODE))
    {
        optimized = tin_astopt_evalexpr(optimizer, stmt->condition);
        if(!tin_value_isnull(optimized) && tin_value_isfalsey(optimized))
        {
            tin_ast_destroyexpression(optimizer->state, expression);
            *slot = NULL;
            return;
        }
    }
    tin_astopt_optexpression(optimizer, &stmt->body);
    if(tin_astopt_isoptenabled(TINOPTSTATE_EMPTYBODY) && tin_astopt_isemptyexpr(stmt->body))
    {
        tin_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
    }
}

static void tin_astopt_optifstmt(TinState* state, TinAstOptimizer* optimizer, TinAstExpression* expression, TinAstExpression** slot)
{
    bool dead;
    bool empty;
    size_t i;
    TinValue value;
    TinValue optimized;
    TinAstIfExpr* stmt;
    (void)state;
    (void)slot;
    stmt = (TinAstIfExpr*)expression;
    tin_astopt_optexpression(optimizer, &stmt->condition);
    tin_astopt_optexpression(optimizer, &stmt->ifbranch);
    empty = tin_astopt_isoptenabled(TINOPTSTATE_EMPTYBODY);
    dead = tin_astopt_isoptenabled(TINOPTSTATE_UNREACHABLECODE);
    optimized = empty ? tin_astopt_evalexpr(optimizer, stmt->condition) : NULL_VALUE;
    if((!tin_value_isnull(optimized) && tin_value_isfalsey(optimized)) || (dead && tin_astopt_isemptyexpr(stmt->ifbranch)))
    {
        tin_ast_destroyexpression(state, stmt->condition);
        stmt->condition = NULL;
        tin_ast_destroyexpression(state, stmt->ifbranch);
        stmt->ifbranch = NULL;
    }
    if(stmt->elseifconds != NULL)
    {
        tin_astopt_optexprlist(optimizer, stmt->elseifconds);
        tin_astopt_optstmtlist(optimizer, stmt->elseifbranches);
        if(dead || empty)
        {
            for(i = 0; i < stmt->elseifconds->count; i++)
            {
                if(empty && tin_astopt_isemptyexpr(stmt->elseifbranches->values[i]))
                {
                    tin_ast_destroyexpression(state, stmt->elseifconds->values[i]);
                    stmt->elseifconds->values[i] = NULL;
                    tin_ast_destroyexpression(state, stmt->elseifbranches->values[i]);
                    stmt->elseifbranches->values[i] = NULL;
                    continue;
                }
                if(dead)
                {
                    value = tin_astopt_evalexpr(optimizer, stmt->elseifconds->values[i]);
                    if(!tin_value_isnull(value) && tin_value_isfalsey(value))
                    {
                        tin_ast_destroyexpression(state, stmt->elseifconds->values[i]);
                        stmt->elseifconds->values[i] = NULL;
                        tin_ast_destroyexpression(state, stmt->elseifbranches->values[i]);
                        stmt->elseifbranches->values[i] = NULL;
                    }
                }
            }
        }
    }
    tin_astopt_optexpression(optimizer, &stmt->elsebranch);
}

static void tin_astopt_optblockstmt(TinState* state, TinAstOptimizer* optimizer, TinAstExpression* expression, TinAstExpression** slot)
{
    bool found;
    size_t i;
    size_t j;
    TinAstExpression* step;
    TinAstBlockExpr* stmt;
    (void)state;
    stmt = (TinAstBlockExpr*)expression;
    if(stmt->statements.count == 0)
    {
        tin_ast_destroyexpression(state, expression);
        *slot = NULL;
        return;
    }
    tin_astopt_beginscope(optimizer);
    tin_astopt_optstmtlist(optimizer, &stmt->statements);
    tin_astopt_endscope(optimizer);
    found = false;
    for(i = 0; i < stmt->statements.count; i++)
    {
        step = stmt->statements.values[i];
        if(!tin_astopt_isemptyexpr(step))
        {
            found = true;
            if(step->type == TINEXPR_RETURN)
            {
                // Remove all the statements post return
                for(j = i + 1; j < stmt->statements.count; j++)
                {
                    step = stmt->statements.values[j];
                    if(step != NULL)
                    {
                        tin_ast_destroyexpression(state, step);
                        stmt->statements.values[j] = NULL;
                    }
                }
                stmt->statements.count = i + 1;
                break;
            }
        }
    }
    if(!found && tin_astopt_isoptenabled(TINOPTSTATE_EMPTYBODY))
    {
        tin_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
    }
}

static void tin_astopt_optvarstmt(TinState* state, TinAstOptimizer* optimizer, TinAstExpression* expression, TinAstExpression** slot)
{
    TinValue value;
    TinVariable* variable;
    TinAstAssignVarExpr* stmt;
    (void)state;
    stmt = (TinAstAssignVarExpr*)expression;
    variable = tin_astopt_addvar(optimizer, stmt->name, stmt->length, stmt->constant, slot);
    tin_astopt_optexpression(optimizer, &stmt->init);
    if(stmt->constant && tin_astopt_isoptenabled(TINOPTSTATE_CONSTANTFOLDING))
    {
        value = tin_astopt_evalexpr(optimizer, stmt->init);
        if(!tin_value_isnull(value))
        {
            variable->constvalue = value;
        }
    }
}

static void tin_astopt_optexpression(TinAstOptimizer* optimizer, TinAstExpression** slot)
{
    TinValue optimized;
    TinState* state;
    TinAstBinaryExpr* binexpr;
    TinAstAssignExpr* assignexpr;
    TinAstCallExpr* callexpr;
    TinAstSetExpr* setexpr;
    TinAstIndexExpr* indexexpr;
    TinAstExpression* expression;
    expression = *slot;
    state = optimizer->state;
    if(expression == NULL)
    {
        return;
    }
    switch(expression->type)
    {
        case TINEXPR_UNARY:
        case TINEXPR_BINARY:
            {
                if(tin_astopt_isoptenabled(TINOPTSTATE_LITERALFOLDING))
                {
                    optimized = tin_astopt_evalexpr(optimizer, expression);
                    if(!tin_value_isnull(optimized))
                    {
                        *slot = (TinAstExpression*)tin_ast_make_literalexpr(state, expression->line, optimized);
                        tin_ast_destroyexpression(state, expression);
                        break;
                    }
                }
                switch(expression->type)
                {
                    case TINEXPR_UNARY:
                        {
                            tin_astopt_optexpression(optimizer, &((TinAstUnaryExpr*)expression)->right);
                        }
                        break;
                    case TINEXPR_BINARY:
                        {
                            binexpr = (TinAstBinaryExpr*)expression;
                            tin_astopt_optexpression(optimizer, &binexpr->left);
                            tin_astopt_optexpression(optimizer, &binexpr->right);
                        }
                        break;
                    default:
                        {
                            UNREACHABLE
                        }
                        break;
                }
            }
            break;
        case TINEXPR_ASSIGN:
            {
                assignexpr = (TinAstAssignExpr*)expression;
                tin_astopt_optexpression(optimizer, &assignexpr->to);
                tin_astopt_optexpression(optimizer, &assignexpr->value);
            }
            break;
        case TINEXPR_CALL:
            {
                callexpr = (TinAstCallExpr*)expression;
                tin_astopt_optexpression(optimizer, &callexpr->callee);
                tin_astopt_optexprlist(optimizer, &callexpr->args);
            }
            break;
        case TINEXPR_SET:
            {
                setexpr = (TinAstSetExpr*)expression;
                tin_astopt_optexpression(optimizer, &setexpr->where);
                tin_astopt_optexpression(optimizer, &setexpr->value);
            }
            break;
        case TINEXPR_GET:
            {
                tin_astopt_optexpression(optimizer, &((TinAstGetExpr*)expression)->where);
            }
            break;
        case TINEXPR_LAMBDA:
            {
                tin_astopt_beginscope(optimizer);
                tin_astopt_optexpression(optimizer, &((TinAstFunctionExpr*)expression)->body);
                tin_astopt_endscope(optimizer);
            }
            break;
        case TINEXPR_ARRAY:
            {
                tin_astopt_optexprlist(optimizer, &((TinAstArrayExpr*)expression)->values);
            }
            break;
        case TINEXPR_OBJECT:
            {
                tin_astopt_optexprlist(optimizer, &((TinAstObjectExpr*)expression)->values);
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                indexexpr = (TinAstIndexExpr*)expression;
                tin_astopt_optexpression(optimizer, &indexexpr->array);
                tin_astopt_optexpression(optimizer, &indexexpr->index);
            }
            break;
        case TINEXPR_RANGE:
            {
                TinAstRangeExpr* expr = (TinAstRangeExpr*)expression;
                tin_astopt_optexpression(optimizer, &expr->from);
                tin_astopt_optexpression(optimizer, &expr->to);
            }
            break;
        case TINEXPR_TERNARY:
            {
                TinAstTernaryExpr* expr = (TinAstTernaryExpr*)expression;
                optimized = tin_astopt_evalexpr(optimizer, expr->condition);
                if(!tin_value_isnull(optimized))
                {
                    if(tin_value_isfalsey(optimized))
                    {
                        *slot = expr->elsebranch;
                        expr->elsebranch = NULL;// So that it doesn't get freed
                    }
                    else
                    {
                        *slot = expr->ifbranch;
                        expr->ifbranch = NULL;// So that it doesn't get freed
                    }

                    tin_astopt_optexpression(optimizer, slot);
                    tin_ast_destroyexpression(state, expression);
                }
                else
                {
                    tin_astopt_optexpression(optimizer, &expr->ifbranch);
                    tin_astopt_optexpression(optimizer, &expr->elsebranch);
                }
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                tin_astopt_optexprlist(optimizer, &((TinAstStrInterExpr*)expression)->expressions);
            }
            break;
        case TINEXPR_VAREXPR:
            {
                TinAstVarExpr* expr = (TinAstVarExpr*)expression;
                TinVariable* variable = tin_astopt_resolvevar(optimizer, expr->name, expr->length);
                if(variable != NULL)
                {
                    variable->used = true;

                    // Not checking here for the enable-ness of constant-folding, since if its off
                    // the constvalue would be NULL_VALUE anyway (:thinkaboutit:)
                    if(variable->constant && !tin_value_isnull(variable->constvalue))
                    {
                        *slot = (TinAstExpression*)tin_ast_make_literalexpr(state, expression->line, variable->constvalue);
                        tin_ast_destroyexpression(state, expression);
                    }
                }
            }
            break;
        case TINEXPR_REFERENCE:
            {
                tin_astopt_optexpression(optimizer, &((TinAstRefExpr*)expression)->to);
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                tin_astopt_optexpression(optimizer, &((TinAstExprExpr*)expression)->expression);
            }
            break;
        case TINEXPR_BLOCK:
            {
                tin_astopt_optblockstmt(state, optimizer, expression, slot);
            }
            break;
        case TINEXPR_IFSTMT:
            {
                tin_astopt_optifstmt(state, optimizer, expression, slot);
            }
            break;
        case TINEXPR_WHILE:
            {
                tin_astopt_optwhilestmt(state, optimizer, expression, slot);
            }
            break;
        case TINEXPR_FOR:
            {
                tin_astopt_optforstmt(state, optimizer, expression, slot);
            }
            break;
        case TINEXPR_VARSTMT:
            {
                tin_astopt_optvarstmt(state, optimizer, expression, slot);
            }
            break;
        case TINEXPR_FUNCTION:
            {
                TinAstFunctionExpr* stmt = (TinAstFunctionExpr*)expression;
                TinVariable* variable = tin_astopt_addvar(optimizer, stmt->name, stmt->length, false, slot);
                if(stmt->exported)
                {
                    // Otherwise it will get optimized-out with a big chance
                    variable->used = true;
                }
                tin_astopt_beginscope(optimizer);
                tin_astopt_optexpression(optimizer, &stmt->body);
                tin_astopt_endscope(optimizer);
            }
            break;
        case TINEXPR_RETURN:
            {
                tin_astopt_optexpression(optimizer, &((TinAstReturnExpr*)expression)->expression);
            }
            break;
        case TINEXPR_METHOD:
            {
                tin_astopt_beginscope(optimizer);
                tin_astopt_optexpression(optimizer, &((TinAstMethodExpr*)expression)->body);
                tin_astopt_endscope(optimizer);
            }
            break;
        case TINEXPR_CLASS:
            {
                tin_astopt_optstmtlist(optimizer, &((TinAstClassExpr*)expression)->fields);
            }
            break;
        case TINEXPR_FIELD:
            {
                TinAstFieldExpr* stmt = (TinAstFieldExpr*)expression;
                if(stmt->getter != NULL)
                {
                    tin_astopt_beginscope(optimizer);
                    tin_astopt_optexpression(optimizer, &stmt->getter);
                    tin_astopt_endscope(optimizer);
                }
                if(stmt->setter != NULL)
                {
                    tin_astopt_beginscope(optimizer);
                    tin_astopt_optexpression(optimizer, &stmt->setter);
                    tin_astopt_endscope(optimizer);
                }
            }
            break;
        // Nothing to optimize there
        case TINEXPR_LITERAL:
        case TINEXPR_THIS:
        case TINEXPR_SUPER:
        case TINEXPR_CONTINUE:
        case TINEXPR_BREAK:
            {
                // Nothing, that we can do here
            }
            break;
    }
}

static void tin_astopt_optstmtlist(TinAstOptimizer* optimizer, TinAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        tin_astopt_optexpression(optimizer, &statements->values[i]);
    }
}

void tin_astopt_optast(TinAstOptimizer* optimizer, TinAstExprList* statements)
{
    return;
    if(!optimization_states_setup)
    {
        tin_astopt_setupstates();
    }
    if(!any_optimization_enabled)
    {
        return;
    }
    tin_astopt_beginscope(optimizer);
    tin_astopt_optstmtlist(optimizer, statements);
    tin_astopt_endscope(optimizer);
    tin_varlist_destroy(optimizer->state, &optimizer->variables);
}

static void tin_astopt_setupstates()
{
    tin_astopt_setoptlevel(TINOPTLEVEL_DEBUG);
}

bool tin_astopt_isoptenabled(TinAstOptType optimization)
{
    if(!optimization_states_setup)
    {
        tin_astopt_setupstates();
    }
    return optimization_states[(int)optimization];
}

void tin_astopt_setoptenabled(TinAstOptType optimization, bool enabled)
{
    size_t i;
    if(!optimization_states_setup)
    {
        tin_astopt_setupstates();
    }
    optimization_states[(int)optimization] = enabled;
    if(enabled)
    {
        any_optimization_enabled = true;
    }
    else
    {
        for(i = 0; i < TINOPTSTATE_TOTAL; i++)
        {
            if(optimization_states[i])
            {
                return;
            }
        }
        any_optimization_enabled = false;
    }
}

void tin_astopt_setalloptenabled(bool enabled)
{
    size_t i;
    optimization_states_setup = true;
    any_optimization_enabled = enabled;
    for(i = 0; i < TINOPTSTATE_TOTAL; i++)
    {
        optimization_states[i] = enabled;
    }
}

void tin_astopt_setoptlevel(TinAstOptLevel level)
{
    switch(level)
    {
        case TINOPTLEVEL_NONE:
            {
                tin_astopt_setalloptenabled(false);
            }
            break;
        case TINOPTLEVEL_REPL:
            {
                tin_astopt_setalloptenabled(true);
                tin_astopt_setoptenabled(TINOPTSTATE_UNUSEDVAR, false);
                tin_astopt_setoptenabled(TINOPTSTATE_UNREACHABLECODE, false);
                tin_astopt_setoptenabled(TINOPTSTATE_EMPTYBODY, false);
                tin_astopt_setoptenabled(TINOPTSTATE_LINEINFO, false);
                tin_astopt_setoptenabled(TINOPTSTATE_PRIVATENAMES, false);
            }
            break;
        case TINOPTLEVEL_DEBUG:
            {
                tin_astopt_setalloptenabled(true);
                tin_astopt_setoptenabled(TINOPTSTATE_UNUSEDVAR, false);
                tin_astopt_setoptenabled(TINOPTSTATE_LINEINFO, false);
                tin_astopt_setoptenabled(TINOPTSTATE_PRIVATENAMES, false);
            }
            break;
        case TINOPTLEVEL_RELEASE:
            {
                tin_astopt_setalloptenabled(true);
                tin_astopt_setoptenabled(TINOPTSTATE_LINEINFO, false);
            }
            break;
        case TINOPTLEVEL_EXTREME:
            {
                tin_astopt_setalloptenabled(true);
            }
            break;
        case TINOPTLEVEL_TOTAL:
            {
            }
            break;

    }
}

const char* tin_astopt_getoptname(TinAstOptType optimization)
{
    return optimization_names[(int)optimization];
}

const char* tin_astopt_getoptdescr(TinAstOptType optimization)
{
    return optimization_descriptions[(int)optimization];
}

const char* tin_astopt_getoptleveldescr(TinAstOptLevel level)
{
    return optimization_level_descriptions[(int)level];
}

