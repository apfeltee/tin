
#include "priv.h"

#define LIT_DEBUG_OPTIMIZER

#define optc_do_binary_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant binary expression of '" # op "' to constant value"); \
        return lit_value_makenumber(optimizer->state, lit_value_asnumber(a) op lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_bitwise_op(op) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant bitwise expression of '" #op "' to constant value"); \
        return lit_value_makenumber(optimizer->state, (int)lit_value_asnumber(a) op(int) lit_value_asnumber(b)); \
    } \
    return NULL_VALUE;

#define optc_do_fn_op(fn, tokstr) \
    if(lit_value_isnumber(a) && lit_value_isnumber(b)) \
    { \
        lit_astopt_optdbg("translating constant expression of '" tokstr "' to constant value via lit_vm_callcallable to '" #fn "'"); \
        return lit_value_makenumber(optimizer->state, fn(lit_value_asnumber(a), lit_value_asnumber(b))); \
    } \
    return NULL_VALUE;



static void lit_astopt_optexpression(LitAstOptimizer* optimizer, LitAstExpression** slot);
static void lit_astopt_optexprlist(LitAstOptimizer* optimizer, LitAstExprList* expressions);
static void lit_astopt_optstmtlist(LitAstOptimizer* optimizer, LitAstExprList* statements);

static const char* optimization_level_descriptions[LITOPTLEVEL_TOTAL]
= { "No optimizations (same as -Ono-all)", "Super light optimizations, sepcific to interactive shell.",
    "(default) Recommended optimization level for the development.", "Medium optimization, recommended for the release.",
    "(default for bytecode) Extreme optimization, throws out most of the variable/function names, used for bytecode compilation." };

static const char* optimization_names[LITOPTSTATE_TOTAL]
= { "constant-folding", "literal-folding", "unused-var",    "unreachable-code",
    "empty-body",       "line-info",       "private-names", "c-for" };

static const char* optimization_descriptions[LITOPTSTATE_TOTAL]
= { "Replaces constants in code with their values.",
    "Precalculates literal expressions (3 + 4 is replaced with 7).",
    "Removes user-declared all variables, that were not used.",
    "Removes code that will never be reached.",
    "Removes loops with empty bodies.",
    "Removes line information from chunks to save on space.",
    "Removes names of the private locals from modules (they are indexed by id at runtime).",
    "Replaces for-in loops with c-style for loops where it can." };

static bool optimization_states[LITOPTSTATE_TOTAL];

static bool optimization_states_setup = false;
static bool any_optimization_enabled = false;

static void lit_astopt_setupstates();

#if defined(LIT_DEBUG_OPTIMIZER)
void lit_astopt_optdbg(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "optimizer: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}
#else
    #define lit_astopt_optdbg(msg, ...)
#endif

void lit_varlist_init(LitVarList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_varlist_destroy(LitState* state, LitVarList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitVariable), array->values, array->capacity);
    lit_varlist_init(array);
}

void lit_varlist_push(LitState* state, LitVarList* array, LitVariable value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(oldcapacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitVariable), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_astopt_init(LitState* state, LitAstOptimizer* optimizer)
{
    optimizer->state = state;
    optimizer->depth = -1;
    optimizer->mark_used = false;
    lit_varlist_init(&optimizer->variables);
}

static void lit_astopt_beginscope(LitAstOptimizer* optimizer)
{
    optimizer->depth++;
}

static void lit_astopt_endscope(LitAstOptimizer* optimizer)
{
    bool remove_unused;
    LitVariable* variable;
    LitVarList* variables;
    optimizer->depth--;
    variables = &optimizer->variables;
    remove_unused = lit_astopt_isoptenabled(LITOPTSTATE_UNUSED_VAR);
    while(variables->count > 0 && variables->values[variables->count - 1].depth > optimizer->depth)
    {
        if(remove_unused && !variables->values[variables->count - 1].used)
        {
            variable = &variables->values[variables->count - 1];
            lit_ast_destroyexpression(optimizer->state, *variable->declaration);
            *variable->declaration = NULL;
        }
        variables->count--;
    }
}

static LitVariable* lit_astopt_addvar(LitAstOptimizer* optimizer, const char* name, size_t length, bool constant, LitAstExpression** declaration)
{
    lit_varlist_push(optimizer->state, &optimizer->variables,
                        (LitVariable){ name, length, optimizer->depth, constant, optimizer->mark_used, NULL_VALUE, declaration });

    return &optimizer->variables.values[optimizer->variables.count - 1];
}

static LitVariable* lit_astopt_resolvevar(LitAstOptimizer* optimizer, const char* name, size_t length)
{
    int i;
    LitVarList* variables;
    LitVariable* variable;
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

static bool lit_astopt_isemptyexpr(LitAstExpression* expression)
{
    return expression == NULL || (expression->type == LITEXPR_BLOCK && ((LitAstBlockExpr*)expression)->statements.count == 0);
}

static LitValue lit_astopt_evalunaryop(LitAstOptimizer* optimizer, LitValue value, LitAstTokType op)
{
    switch(op)
    {
        case LITTOK_MINUS:
            {
                if(lit_value_isnumber(value))
                {
                    lit_astopt_optdbg("translating constant unary minus on number to literal value");
                    return lit_value_makenumber(optimizer->state, -lit_value_asnumber(value));
                }
            }
            break;
        case LITTOK_BANG:
            {
                lit_astopt_optdbg("translating constant expression of '=' to literal value");
                return lit_value_makebool(optimizer->state, lit_value_isfalsey(value));
            }
            break;
        case LITTOK_TILDE:
            {
                if(lit_value_isnumber(value))
                {
                    lit_astopt_optdbg("translating unary tile (~) on number to literal value");
                    return lit_value_makenumber(optimizer->state, ~((int)lit_value_asnumber(value)));
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

static LitValue lit_astopt_evalbinaryop(LitAstOptimizer* optimizer, LitValue a, LitValue b, LitAstTokType op)
{
    switch(op)
    {
        case LITTOK_PLUS:
            {
                optc_do_binary_op(+);
            }
            break;
        case LITTOK_MINUS:
            {
                optc_do_binary_op(-);
            }
            break;
        case LITTOK_STAR:
            {
                optc_do_binary_op(*);
            }
            break;
        case LITTOK_SLASH:
            {
                optc_do_binary_op(/);
            }
            break;
        case LITTOK_STAR_STAR:
            {
                optc_do_fn_op(pow, "**");
            }
            break;
        case LITTOK_PERCENT:
            {
                optc_do_fn_op(fmod, "%");
            }
            break;
        case LITTOK_GREATER:
            {
                optc_do_binary_op(>);
            }
            break;
        case LITTOK_GREATER_EQUAL:
            {
                optc_do_binary_op(>=);
            }
            break;
        case LITTOK_LESS:
            {
                optc_do_binary_op(<);
            }
            break;
        case LITTOK_LESS_EQUAL:
            {
                optc_do_binary_op(<=);
            }
            break;
        case LITTOK_LESS_LESS:
            {
                optc_do_bitwise_op(<<);
            }
            break;
        case LITTOK_GREATER_GREATER:
            {
                optc_do_bitwise_op(>>);
            }
            break;
        case LITTOK_BAR:
            {
                optc_do_bitwise_op(|);
            }
            break;
        case LITTOK_AMPERSAND:
            {
                optc_do_bitwise_op(&);
            }
            break;
        case LITTOK_CARET:
            {
                optc_do_bitwise_op(^);
            }
            break;
        case LITTOK_SHARP:
            {
                if(lit_value_isnumber(a) && lit_value_isnumber(b))
                {
                    return lit_value_makenumber(optimizer->state, floor(lit_value_asnumber(a) / lit_value_asnumber(b)));
                }
                return NULL_VALUE;
            }
            break;
        case LITTOK_EQUAL_EQUAL:
            {
                return lit_value_makebool(optimizer->state, lit_value_compare(optimizer->state, a, b));
            }
            break;
        case LITTOK_BANG_EQUAL:
            {
                return lit_value_makebool(optimizer->state, !lit_value_compare(optimizer->state, a, b));
            }
            break;
        case LITTOK_IS:
        default:
            {
            }
            break;
    }
    return NULL_VALUE;
}

static LitValue lit_astopt_attemptoptbinary(LitAstOptimizer* optimizer, LitAstBinaryExpr* expression, LitValue value, bool left)
{
    double number;
    LitAstTokType op;
    op = expression->op;
    LitAstExpression* branch;
    branch = left ? expression->left : expression->right;
    if(lit_value_isnumber(value))
    {
        number = lit_value_asnumber(value);
        if(op == LITTOK_STAR)
        {
            if(number == 0)
            {
                lit_astopt_optdbg("reducing expression to literal '0'");
                return lit_value_makenumber(optimizer->state, 0);
            }
            else if(number == 1)
            {
                lit_astopt_optdbg("reducing expression to literal '1'");
                lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
                expression->left = branch;
                expression->right = NULL;
            }
        }
        else if((op == LITTOK_PLUS || op == LITTOK_MINUS) && number == 0)
        {
            lit_astopt_optdbg("reducing expression that would result in '0' to literal '0'");
            lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
        else if(((left && op == LITTOK_SLASH) || op == LITTOK_STAR_STAR) && number == 1)
        {
            lit_astopt_optdbg("reducing expression that would result in '1' to literal '1'");
            lit_ast_destroyexpression(optimizer->state, left ? expression->right : expression->left);
            expression->left = branch;
            expression->right = NULL;
        }
    }
    return NULL_VALUE;
}

static LitValue lit_astopt_evalexpr(LitAstOptimizer* optimizer, LitAstExpression* expression)
{
    LitAstUnaryExpr* uexpr;
    LitAstBinaryExpr* bexpr;
    LitValue a;
    LitValue b;
    LitValue branch;
    if(expression == NULL)
    {
        return NULL_VALUE;
    }
    switch(expression->type)
    {
        case LITEXPR_LITERAL:
            {
                return ((LitAstLiteralExpr*)expression)->value;
            }
            break;
        case LITEXPR_UNARY:
            {
                uexpr = (LitAstUnaryExpr*)expression;
                branch = lit_astopt_evalexpr(optimizer, uexpr->right);
                if(!lit_value_isnull(branch))
                {
                    return lit_astopt_evalunaryop(optimizer, branch, uexpr->op);
                }
            }
            break;
        case LITEXPR_BINARY:
            {
                bexpr = (LitAstBinaryExpr*)expression;
                a = lit_astopt_evalexpr(optimizer, bexpr->left);
                b = lit_astopt_evalexpr(optimizer, bexpr->right);
                if(!lit_value_isnull(a) && !lit_value_isnull(b))
                {
                    return lit_astopt_evalbinaryop(optimizer, a, b, bexpr->op);
                }
                else if(!lit_value_isnull(a))
                {
                    return lit_astopt_attemptoptbinary(optimizer, bexpr, a, false);
                }
                else if(!lit_value_isnull(b))
                {
                    return lit_astopt_attemptoptbinary(optimizer, bexpr, b, true);
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

static void lit_astopt_optexprlist(LitAstOptimizer* optimizer, LitAstExprList* expressions)
{
    for(size_t i = 0; i < expressions->count; i++)
    {
        lit_astopt_optexpression(optimizer, &expressions->values[i]);
    }
}

static void lit_astopt_optforstmt(LitState* state, LitAstOptimizer* optimizer, LitAstExpression* expression, LitAstExpression** slot)
{
    LitAstForExpr* stmt = (LitAstForExpr*)expression;
    lit_astopt_beginscope(optimizer);
    // This is required, so that optimizer doesn't optimize out our i variable (and such)
    optimizer->mark_used = true;
    lit_astopt_optexpression(optimizer, &stmt->init);
    lit_astopt_optexpression(optimizer, &stmt->condition);
    lit_astopt_optexpression(optimizer, &stmt->increment);
    lit_astopt_optexpression(optimizer, &stmt->var);
    optimizer->mark_used = false;
    lit_astopt_optexpression(optimizer, &stmt->body);
    lit_astopt_endscope(optimizer);
    if(lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY) && lit_astopt_isemptyexpr(stmt->body))
    {
        lit_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
        return;
    }
    if(stmt->c_style || !lit_astopt_isoptenabled(LITOPTSTATE_C_FOR) || stmt->condition->type != LITEXPR_RANGE)
    {
        return;
    }
    LitAstRangeExpr* range = (LitAstRangeExpr*)stmt->condition;
    LitValue from = lit_astopt_evalexpr(optimizer, range->from);
    LitValue to = lit_astopt_evalexpr(optimizer, range->to);
    if(!lit_value_isnumber(from) || !lit_value_isnumber(to))
    {
        return;
    }
    bool reverse = lit_value_asnumber(from) > lit_value_asnumber(to);
    LitAstAssignVarExpr* var = (LitAstAssignVarExpr*)stmt->var;
    size_t line = range->exobj.line;
    // var i = from
    var->init = range->from;
    // i <= to
    stmt->condition = (LitAstExpression*)lit_ast_make_binaryexpr(
    state, line, (LitAstExpression*)lit_ast_make_varexpr(state, line, var->name, var->length), range->to, LITTOK_LESS_EQUAL);
    // i++ (or i--)
    LitAstExpression* var_get = (LitAstExpression*)lit_ast_make_varexpr(state, line, var->name, var->length);
    LitAstBinaryExpr* assign_value = lit_ast_make_binaryexpr(
    state, line, var_get, (LitAstExpression*)lit_ast_make_literalexpr(state, line, lit_value_makenumber(optimizer->state, 1)),
    reverse ? LITTOK_MINUS_MINUS : LITTOK_PLUS);
    assign_value->ignore_left = true;
    LitAstExpression* increment
    = (LitAstExpression*)lit_ast_make_assignexpr(state, line, var_get, (LitAstExpression*)assign_value);
    stmt->increment = (LitAstExpression*)increment;
    range->from = NULL;
    range->to = NULL;
    stmt->c_style = true;
    lit_ast_destroyexpression(state, (LitAstExpression*)range);
}

static void lit_astopt_optwhilestmt(LitState* state, LitAstOptimizer* optimizer, LitAstExpression* expression, LitAstExpression** slot)
{
    LitValue optimized;
    LitAstWhileExpr* stmt;
    (void)state;
    stmt = (LitAstWhileExpr*)expression;
    lit_astopt_optexpression(optimizer, &stmt->condition);
    if(lit_astopt_isoptenabled(LITOPTSTATE_UNREACHABLE_CODE))
    {
        optimized = lit_astopt_evalexpr(optimizer, stmt->condition);
        if(!lit_value_isnull(optimized) && lit_value_isfalsey(optimized))
        {
            lit_ast_destroyexpression(optimizer->state, expression);
            *slot = NULL;
            return;
        }
    }
    lit_astopt_optexpression(optimizer, &stmt->body);
    if(lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY) && lit_astopt_isemptyexpr(stmt->body))
    {
        lit_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
    }
}

static void lit_astopt_optifstmt(LitState* state, LitAstOptimizer* optimizer, LitAstExpression* expression, LitAstExpression** slot)
{
    bool dead;
    bool empty;
    size_t i;
    LitValue value;
    LitValue optimized;
    LitAstIfExpr* stmt;
    (void)state;
    (void)slot;
    stmt = (LitAstIfExpr*)expression;
    lit_astopt_optexpression(optimizer, &stmt->condition);
    lit_astopt_optexpression(optimizer, &stmt->if_branch);
    empty = lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY);
    dead = lit_astopt_isoptenabled(LITOPTSTATE_UNREACHABLE_CODE);
    optimized = empty ? lit_astopt_evalexpr(optimizer, stmt->condition) : NULL_VALUE;
    if((!lit_value_isnull(optimized) && lit_value_isfalsey(optimized)) || (dead && lit_astopt_isemptyexpr(stmt->if_branch)))
    {
        lit_ast_destroyexpression(state, stmt->condition);
        stmt->condition = NULL;
        lit_ast_destroyexpression(state, stmt->if_branch);
        stmt->if_branch = NULL;
    }
    if(stmt->elseif_conditions != NULL)
    {
        lit_astopt_optexprlist(optimizer, stmt->elseif_conditions);
        lit_astopt_optstmtlist(optimizer, stmt->elseif_branches);
        if(dead || empty)
        {
            for(i = 0; i < stmt->elseif_conditions->count; i++)
            {
                if(empty && lit_astopt_isemptyexpr(stmt->elseif_branches->values[i]))
                {
                    lit_ast_destroyexpression(state, stmt->elseif_conditions->values[i]);
                    stmt->elseif_conditions->values[i] = NULL;
                    lit_ast_destroyexpression(state, stmt->elseif_branches->values[i]);
                    stmt->elseif_branches->values[i] = NULL;
                    continue;
                }
                if(dead)
                {
                    value = lit_astopt_evalexpr(optimizer, stmt->elseif_conditions->values[i]);
                    if(!lit_value_isnull(value) && lit_value_isfalsey(value))
                    {
                        lit_ast_destroyexpression(state, stmt->elseif_conditions->values[i]);
                        stmt->elseif_conditions->values[i] = NULL;
                        lit_ast_destroyexpression(state, stmt->elseif_branches->values[i]);
                        stmt->elseif_branches->values[i] = NULL;
                    }
                }
            }
        }
    }
    lit_astopt_optexpression(optimizer, &stmt->else_branch);
}

static void lit_astopt_optblockstmt(LitState* state, LitAstOptimizer* optimizer, LitAstExpression* expression, LitAstExpression** slot)
{
    bool found;
    size_t i;
    size_t j;
    LitAstExpression* step;
    LitAstBlockExpr* stmt;
    (void)state;
    stmt = (LitAstBlockExpr*)expression;
    if(stmt->statements.count == 0)
    {
        lit_ast_destroyexpression(state, expression);
        *slot = NULL;
        return;
    }
    lit_astopt_beginscope(optimizer);
    lit_astopt_optstmtlist(optimizer, &stmt->statements);
    lit_astopt_endscope(optimizer);
    found = false;
    for(i = 0; i < stmt->statements.count; i++)
    {
        step = stmt->statements.values[i];
        if(!lit_astopt_isemptyexpr(step))
        {
            found = true;
            if(step->type == LITEXPR_RETURN)
            {
                // Remove all the statements post return
                for(j = i + 1; j < stmt->statements.count; j++)
                {
                    step = stmt->statements.values[j];
                    if(step != NULL)
                    {
                        lit_ast_destroyexpression(state, step);
                        stmt->statements.values[j] = NULL;
                    }
                }
                stmt->statements.count = i + 1;
                break;
            }
        }
    }
    if(!found && lit_astopt_isoptenabled(LITOPTSTATE_EMPTY_BODY))
    {
        lit_ast_destroyexpression(optimizer->state, expression);
        *slot = NULL;
    }
}

static void lit_astopt_optvarstmt(LitState* state, LitAstOptimizer* optimizer, LitAstExpression* expression, LitAstExpression** slot)
{
    LitValue value;
    LitVariable* variable;
    LitAstAssignVarExpr* stmt;
    (void)state;
    stmt = (LitAstAssignVarExpr*)expression;
    variable = lit_astopt_addvar(optimizer, stmt->name, stmt->length, stmt->constant, slot);
    lit_astopt_optexpression(optimizer, &stmt->init);
    if(stmt->constant && lit_astopt_isoptenabled(LITOPTSTATE_CONSTANT_FOLDING))
    {
        value = lit_astopt_evalexpr(optimizer, stmt->init);
        if(!lit_value_isnull(value))
        {
            variable->constant_value = value;
        }
    }
}

static void lit_astopt_optexpression(LitAstOptimizer* optimizer, LitAstExpression** slot)
{
    LitValue optimized;
    LitState* state;
    LitAstBinaryExpr* binexpr;
    LitAstAssignExpr* assignexpr;
    LitAstCallExpr* callexpr;
    LitAstSetExpr* setexpr;
    LitAstIndexExpr* indexexpr;
    LitAstExpression* expression;
    expression = *slot;
    state = optimizer->state;
    if(expression == NULL)
    {
        return;
    }
    switch(expression->type)
    {
        case LITEXPR_UNARY:
        case LITEXPR_BINARY:
            {
                if(lit_astopt_isoptenabled(LITOPTSTATE_LITERAL_FOLDING))
                {
                    optimized = lit_astopt_evalexpr(optimizer, expression);
                    if(!lit_value_isnull(optimized))
                    {
                        *slot = (LitAstExpression*)lit_ast_make_literalexpr(state, expression->line, optimized);
                        lit_ast_destroyexpression(state, expression);
                        break;
                    }
                }
                switch(expression->type)
                {
                    case LITEXPR_UNARY:
                        {
                            lit_astopt_optexpression(optimizer, &((LitAstUnaryExpr*)expression)->right);
                        }
                        break;
                    case LITEXPR_BINARY:
                        {
                            binexpr = (LitAstBinaryExpr*)expression;
                            lit_astopt_optexpression(optimizer, &binexpr->left);
                            lit_astopt_optexpression(optimizer, &binexpr->right);
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
        case LITEXPR_ASSIGN:
            {
                assignexpr = (LitAstAssignExpr*)expression;
                lit_astopt_optexpression(optimizer, &assignexpr->to);
                lit_astopt_optexpression(optimizer, &assignexpr->value);
            }
            break;
        case LITEXPR_CALL:
            {
                callexpr = (LitAstCallExpr*)expression;
                lit_astopt_optexpression(optimizer, &callexpr->callee);
                lit_astopt_optexprlist(optimizer, &callexpr->args);
            }
            break;
        case LITEXPR_SET:
            {
                setexpr = (LitAstSetExpr*)expression;
                lit_astopt_optexpression(optimizer, &setexpr->where);
                lit_astopt_optexpression(optimizer, &setexpr->value);
            }
            break;
        case LITEXPR_GET:
            {
                lit_astopt_optexpression(optimizer, &((LitAstGetExpr*)expression)->where);
            }
            break;
        case LITEXPR_LAMBDA:
            {
                lit_astopt_beginscope(optimizer);
                lit_astopt_optexpression(optimizer, &((LitAstFunctionExpr*)expression)->body);
                lit_astopt_endscope(optimizer);
            }
            break;
        case LITEXPR_ARRAY:
            {
                lit_astopt_optexprlist(optimizer, &((LitAstArrayExpr*)expression)->values);
            }
            break;
        case LITEXPR_OBJECT:
            {
                lit_astopt_optexprlist(optimizer, &((LitAstObjectExpr*)expression)->values);
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                indexexpr = (LitAstIndexExpr*)expression;
                lit_astopt_optexpression(optimizer, &indexexpr->array);
                lit_astopt_optexpression(optimizer, &indexexpr->index);
            }
            break;
        case LITEXPR_RANGE:
            {
                LitAstRangeExpr* expr = (LitAstRangeExpr*)expression;
                lit_astopt_optexpression(optimizer, &expr->from);
                lit_astopt_optexpression(optimizer, &expr->to);
            }
            break;
        case LITEXPR_TERNARY:
            {
                LitAstTernaryExpr* expr = (LitAstTernaryExpr*)expression;
                LitValue optimized = lit_astopt_evalexpr(optimizer, expr->condition);
                if(!lit_value_isnull(optimized))
                {
                    if(lit_value_isfalsey(optimized))
                    {
                        *slot = expr->else_branch;
                        expr->else_branch = NULL;// So that it doesn't get freed
                    }
                    else
                    {
                        *slot = expr->if_branch;
                        expr->if_branch = NULL;// So that it doesn't get freed
                    }

                    lit_astopt_optexpression(optimizer, slot);
                    lit_ast_destroyexpression(state, expression);
                }
                else
                {
                    lit_astopt_optexpression(optimizer, &expr->if_branch);
                    lit_astopt_optexpression(optimizer, &expr->else_branch);
                }
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                lit_astopt_optexprlist(optimizer, &((LitAstStrInterExpr*)expression)->expressions);
            }
            break;
        case LITEXPR_VAREXPR:
            {
                LitAstVarExpr* expr = (LitAstVarExpr*)expression;
                LitVariable* variable = lit_astopt_resolvevar(optimizer, expr->name, expr->length);
                if(variable != NULL)
                {
                    variable->used = true;

                    // Not checking here for the enable-ness of constant-folding, since if its off
                    // the constant_value would be NULL_VALUE anyway (:thinkaboutit:)
                    if(variable->constant && !lit_value_isnull(variable->constant_value))
                    {
                        *slot = (LitAstExpression*)lit_ast_make_literalexpr(state, expression->line, variable->constant_value);
                        lit_ast_destroyexpression(state, expression);
                    }
                }
            }
            break;
        case LITEXPR_REFERENCE:
            {
                lit_astopt_optexpression(optimizer, &((LitAstRefExpr*)expression)->to);
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                lit_astopt_optexpression(optimizer, &((LitAstExprExpr*)expression)->expression);
            }
            break;
        case LITEXPR_BLOCK:
            {
                lit_astopt_optblockstmt(state, optimizer, expression, slot);
            }
            break;
        case LITEXPR_IFSTMT:
            {
                lit_astopt_optifstmt(state, optimizer, expression, slot);
            }
            break;
        case LITEXPR_WHILE:
            {
                lit_astopt_optwhilestmt(state, optimizer, expression, slot);
            }
            break;
        case LITEXPR_FOR:
            {
                lit_astopt_optforstmt(state, optimizer, expression, slot);
            }
            break;
        case LITEXPR_VARSTMT:
            {
                lit_astopt_optvarstmt(state, optimizer, expression, slot);
            }
            break;
        case LITEXPR_FUNCTION:
            {
                LitAstFunctionExpr* stmt = (LitAstFunctionExpr*)expression;
                LitVariable* variable = lit_astopt_addvar(optimizer, stmt->name, stmt->length, false, slot);
                if(stmt->exported)
                {
                    // Otherwise it will get optimized-out with a big chance
                    variable->used = true;
                }
                lit_astopt_beginscope(optimizer);
                lit_astopt_optexpression(optimizer, &stmt->body);
                lit_astopt_endscope(optimizer);
            }
            break;
        case LITEXPR_RETURN:
            {
                lit_astopt_optexpression(optimizer, &((LitAstReturnExpr*)expression)->expression);
            }
            break;
        case LITEXPR_METHOD:
            {
                lit_astopt_beginscope(optimizer);
                lit_astopt_optexpression(optimizer, &((LitAstMethodExpr*)expression)->body);
                lit_astopt_endscope(optimizer);
            }
            break;
        case LITEXPR_CLASS:
            {
                lit_astopt_optstmtlist(optimizer, &((LitAstClassExpr*)expression)->fields);
            }
            break;
        case LITEXPR_FIELD:
            {
                LitAstFieldExpr* stmt = (LitAstFieldExpr*)expression;
                if(stmt->getter != NULL)
                {
                    lit_astopt_beginscope(optimizer);
                    lit_astopt_optexpression(optimizer, &stmt->getter);
                    lit_astopt_endscope(optimizer);
                }
                if(stmt->setter != NULL)
                {
                    lit_astopt_beginscope(optimizer);
                    lit_astopt_optexpression(optimizer, &stmt->setter);
                    lit_astopt_endscope(optimizer);
                }
            }
            break;
        // Nothing to optimize there
        case LITEXPR_LITERAL:
        case LITEXPR_THIS:
        case LITEXPR_SUPER:
        case LITEXPR_CONTINUE:
        case LITEXPR_BREAK:
            {
                // Nothing, that we can do here
            }
            break;
    }
}

static void lit_astopt_optstmtlist(LitAstOptimizer* optimizer, LitAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_astopt_optexpression(optimizer, &statements->values[i]);
    }
}

void lit_astopt_optast(LitAstOptimizer* optimizer, LitAstExprList* statements)
{
    return;
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
    }
    if(!any_optimization_enabled)
    {
        return;
    }
    lit_astopt_beginscope(optimizer);
    lit_astopt_optstmtlist(optimizer, statements);
    lit_astopt_endscope(optimizer);
    lit_varlist_destroy(optimizer->state, &optimizer->variables);
}

static void lit_astopt_setupstates()
{
    lit_astopt_setoptlevel(LITOPTLEVEL_DEBUG);
}

bool lit_astopt_isoptenabled(LitAstOptType optimization)
{
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
    }
    return optimization_states[(int)optimization];
}

void lit_astopt_setoptenabled(LitAstOptType optimization, bool enabled)
{
    size_t i;
    if(!optimization_states_setup)
    {
        lit_astopt_setupstates();
    }
    optimization_states[(int)optimization] = enabled;
    if(enabled)
    {
        any_optimization_enabled = true;
    }
    else
    {
        for(i = 0; i < LITOPTSTATE_TOTAL; i++)
        {
            if(optimization_states[i])
            {
                return;
            }
        }
        any_optimization_enabled = false;
    }
}

void lit_astopt_setalloptenabled(bool enabled)
{
    size_t i;
    optimization_states_setup = true;
    any_optimization_enabled = enabled;
    for(i = 0; i < LITOPTSTATE_TOTAL; i++)
    {
        optimization_states[i] = enabled;
    }
}

void lit_astopt_setoptlevel(LitAstOptLevel level)
{
    switch(level)
    {
        case LITOPTLEVEL_NONE:
            {
                lit_astopt_setalloptenabled(false);
            }
            break;
        case LITOPTLEVEL_REPL:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_astopt_setoptenabled(LITOPTSTATE_UNREACHABLE_CODE, false);
                lit_astopt_setoptenabled(LITOPTSTATE_EMPTY_BODY, false);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
                lit_astopt_setoptenabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_DEBUG:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_UNUSED_VAR, false);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
                lit_astopt_setoptenabled(LITOPTSTATE_PRIVATE_NAMES, false);
            }
            break;
        case LITOPTLEVEL_RELEASE:
            {
                lit_astopt_setalloptenabled(true);
                lit_astopt_setoptenabled(LITOPTSTATE_LINE_INFO, false);
            }
            break;
        case LITOPTLEVEL_EXTREME:
            {
                lit_astopt_setalloptenabled(true);
            }
            break;
        case LITOPTLEVEL_TOTAL:
            {
            }
            break;

    }
}

const char* lit_astopt_getoptname(LitAstOptType optimization)
{
    return optimization_names[(int)optimization];
}

const char* lit_astopt_getoptdescr(LitAstOptType optimization)
{
    return optimization_descriptions[(int)optimization];
}

const char* lit_astopt_getoptleveldescr(LitAstOptLevel level)
{
    return optimization_level_descriptions[(int)level];
}

