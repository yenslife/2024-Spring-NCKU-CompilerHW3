/* Definition section */
%{
    #include "compiler_common.h"
    #include "compiler_util.h"
    #include "main.h"

    int yydebug = 1;
%}

/* Variable or self-defined structure */
%union {
    ObjectType var_type;

    bool b_var;
    int i_var;
    float f_var;
    char *s_var;
    char c_var;

    Object object_val;
    struct IdentListNode* ident_list;
}

/* Token without return */
%token COUT
%token SHR SHL BAN BOR BNT BXO ADD SUB MUL DIV REM NOT GTR LES GEQ LEQ EQL NEQ LAN LOR
%token VAL_ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN REM_ASSIGN BAN_ASSIGN BOR_ASSIGN BXO_ASSIGN SHR_ASSIGN SHL_ASSIGN INC_ASSIGN DEC_ASSIGN
%token IF ELSE FOR WHILE RETURN BREAK CONTINUE

/* Nonterminal with return, which need to sepcify type */
%type <object_val> Expression
%type <object_val> ConditionalExpr
%type <object_val> LogicalOrExpr
%type <object_val> LogicalAndExpr
%type <object_val> InclusiveOrExpr
%type <object_val> ExclusiveOrExpr
%type <object_val> AndExpr
%type <object_val> EqualityExpr
%type <object_val> RelationalExpr
%type <object_val> ShiftExpr
%type <object_val> AdditiveExpr
%type <object_val> MultiplicativeExpr
%type <object_val> UnaryExpr
%type <object_val> PostfixExpr
%type <object_val> PrimaryExpr
%type <object_val> CoutExpr
%type <ident_list> IdentList
%type <object_val> FunctionCallStmt
%type <object_val> ArrayElementExpr

/* Token with return, which need to sepcify type */
%token <var_type> VARIABLE_T
%token <b_var> BOOL_LIT
%token <c_var> CHAR_LIT
%token <i_var> INT_LIT
%token <f_var> FLOAT_LIT
%token <s_var> STR_LIT
%token <s_var> IDENT

%left ADD SUB
%left MUL DIV REM

/* Yacc will start at this nonterminal */
%start Program

%%
/* Grammar section */

Program
    : { pushScope(); } GlobalStmtList { dumpScope(); }
    | /* Empty file */
;

GlobalStmtList 
    : GlobalStmtList GlobalStmt
    | GlobalStmt
;

GlobalStmt
    : DefineVariableStmt
    | FunctionDefStmt
;

DefineVariableStmt
    : VARIABLE_T { pushVariableList($1);} IdentList
;

IdentList
    : IDENT  { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>1, VAR_FLAG_DEFAULT, NULL); }
    | IDENT VAL_ASSIGN Expression { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>1, VAR_FLAG_DEFAULT, &$<object_val>3); objectExpAssign('=', $<s_var>1, &$<object_val>3, &$<object_val>3); }
    | IdentList ',' IDENT { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>3, VAR_FLAG_DEFAULT, NULL); }
    | IdentList ',' IDENT VAL_ASSIGN Expression { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>3, VAR_FLAG_DEFAULT, &$<object_val>5); objectExpAssign('=', $<s_var>3, &$<object_val>5, &$<object_val>5); }
    // array
    | IDENT ArrayDimensions ArrayInitList {pushArrayVariable(OBJECT_TYPE_UNDEFINED, $<s_var>1, VAR_FLAG_ARRAY);} 
;

ArrayDimensions
    : ArrayDimensions '[' INT_LIT ']' {printf("INT_LIT %d\n", $<i_var>3);}
    | '[' INT_LIT ']' {printf("INT_LIT %d\n", $<i_var>2); newarray($<i_var>2);}
;

ArrayInitList
    : VAL_ASSIGN '{' ArrayElementList '}' 
    | /* Empty array init list, don't print create array */{ nonInitArray();}
;

ArrayElementList
    : ArrayElementList ',' { arrDup(); } Expression { arrayElementStore();}
    | { arrDup(); } Expression { arrayElementStore();}
    | /* Empty array element list */ 
;

/* Function */
FunctionDefStmt
    : 
     /* VARIABLE_T IDENT '(' FunctionParameterStmtList ')' { createFunction($<var_type>1, $<s_var>2); } '{' '}' { dumpScope(); } */
    | VARIABLE_T IDENT '(' { createFunction($<var_type>1, $<s_var>2); } FunctionParameterStmtList ')' { addFunctionParam($<s_var>2, $<var_type>1); } '{' StmtList { codeReturn($<var_type>1, $<s_var>2); } '}'  { dumpScope(); codeRaw(".end method");}
;
FunctionParameterStmtList 
    : FunctionParameterStmtList ',' FunctionParameterStmt
    | FunctionParameterStmt
    | /* Empty function parameter */
;
FunctionParameterStmt
    : VARIABLE_T IDENT { pushFunParm($<var_type>1, $<s_var>2, VAR_FLAG_DEFAULT); }
    | VARIABLE_T IDENT '[' ']' { pushFunParm($<var_type>1, $<s_var>2, VAR_FLAG_ARRAY); }
;

/* Scope */
StmtList 
    : StmtList Stmt
    | Stmt
;
Stmt
    : ';'
    | COUT CoutParmListStmt ';' { stdoutPrint(); }
    | RETURN Expression ';' { /*printf("RETURN\n");*/ }
    | DefineVariableStmt
    | AssignVariableStmt
    | IFStmt
    | WHILEStmt
    | FORStmt
    | FunctionCallStmt
    | ArrayElementExpr VAL_ASSIGN Expression { if (!objectExpAssign('=', &$<object_val>1, &$<object_val>3, &$<object_val>1)) YYABORT; }';'
    | BREAK { breakGoto(); } ';' 
;

FunctionCallStmt
    : IDENT '(' FunctionArgs {processIdentifier($<s_var>1);} ')' { if (!objectFunctionCall($<s_var>1, &$$)) YYABORT; }
;

FunctionArgs
    : FunctionArgs ',' Expression 
    | Expression 
    | /* Empty function argument */

FORStmt
    : FOR {printf("FOR\n"); pushScope();} '(' LoopLogic ')' '{' StmtList '}' { forStmtEnd(); dumpScope(); }
;

LoopLogic
    : forInit {forBranchInit(); } forCondition {forCondition();} {forIncrementLabel();} forIncrement {forIncrement();}
;


forInit
    : VARIABLE_T IDENT VAL_ASSIGN Expression { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>2, VAR_FLAG_DEFAULT, &$<object_val>4); objectExpAssign('=', $<s_var>2, &$<object_val>4, &$<object_val>4); }';'
    | IDENT VAL_ASSIGN Expression { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>1, VAR_FLAG_DEFAULT, &$<object_val>3); objectExpAssign('=', $<s_var>1, &$<object_val>3, &$<object_val>3); }';'
    | VARIABLE_T IDENT  ':' { pushVariable(OBJECT_TYPE_UNDEFINED, $<s_var>2, VAR_FLAG_DEFAULT, NULL); } IDENT { processIdentifier($<s_var>5); } /* for range-based loop */
    | ';'
;

forCondition
    : Expression ';'
    | Expression /* for range-based loop */
    | ';'
    | /* Empty for condition */

forIncrement
    : AssignVariableStmtWithoutSemi 
    | PostfixExpr
    | /* Empty for increment, and for range-based loop */
;

AssignVariableStmtWithoutSemi
    : IDENT {processIdentifier($<s_var>1);} VAL_ASSIGN Expression { if (!objectExpAssign('=', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} ADD_ASSIGN Expression { if (!objectExpAssign('+', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SUB_ASSIGN Expression { if (!objectExpAssign('-', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} MUL_ASSIGN Expression { if (!objectExpAssign('*', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} DIV_ASSIGN Expression { if (!objectExpAssign('/', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} REM_ASSIGN Expression { if (!objectExpAssign('%', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BAN_ASSIGN Expression { if (!objectExpAssign('&', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BOR_ASSIGN Expression { if (!objectExpAssign('|', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BXO_ASSIGN Expression { if (!objectExpAssign('^', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SHR_ASSIGN Expression { if (!objectExpAssign('>', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SHL_ASSIGN Expression { if (!objectExpAssign('<', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
;

WHILEStmt
    : WHILE { printf("WHILE\n"); whileBranchInit(); } Expression { whileBranch(); pushScope();} '{' StmtList '}' { dumpScope(); whileStmtEnd(); }


IFStmt
    : IF '(' Expression ')' { ifBranch_init(); printf("IF\n"); pushScope();} '{' StmtList '}' {dumpScope();} { ifStmtEnd(); } ElseStmt { ifEnd(); }
    /* | IF '(' Expression ')' {printf("IF\n"); pushScope();} '{' StmtList '}' {dumpScope();} */
    | IF '(' Expression ')' { ifBranch_init(); printf("IF\n"); } Stmt { ifEnd(); } 
;

ElseStmt
    : ELSE { printf("ELSE\n"); pushScope();}'{' StmtList '}' {dumpScope();}
    | /* Empty else */

AssignVariableStmt
    : IDENT {processIdentifier($<s_var>1);} VAL_ASSIGN Expression ';' { if (!objectExpAssign('=', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} ADD_ASSIGN Expression ';' { if (!objectExpAssign('+', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SUB_ASSIGN Expression ';' { if (!objectExpAssign('-', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} MUL_ASSIGN Expression ';' { if (!objectExpAssign('*', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} DIV_ASSIGN Expression ';' { if (!objectExpAssign('/', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} REM_ASSIGN Expression ';' { if (!objectExpAssign('%', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BAN_ASSIGN Expression ';' { if (!objectExpAssign('&', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BOR_ASSIGN Expression ';' { if (!objectExpAssign('|', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} BXO_ASSIGN Expression ';' { if (!objectExpAssign('^', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SHR_ASSIGN Expression ';' { if (!objectExpAssign('>', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    | IDENT {processIdentifier($<s_var>1);} SHL_ASSIGN Expression ';' { if (!objectExpAssign('<', $<s_var>1, &$<object_val>4, &$<object_val>1)) YYABORT; }
    // array assign
    | IDENT '[' Expression ']' {processArrayIdentifier($<s_var>1); } VAL_ASSIGN Expression ';' { arrayAssign($<s_var>1, &$<object_val>3, &$<object_val>5); }
;

CoutParmListStmt
    : CoutParmListStmt SHL {codeRaw("getstatic java/lang/System/out Ljava/io/PrintStream;");} CoutExpr { pushFunInParm($<object_val>4); }
    | SHL {codeRaw("getstatic java/lang/System/out Ljava/io/PrintStream;"); } CoutExpr { pushFunInParm($<object_val>3); }
;

CoutExpr 
    : PrimaryExpr { $$ = $1; }
    | AdditiveExpr { $$ = $1; }
    | MultiplicativeExpr { $$ = $1; }
    | UnaryExpr { $$ = $1; }
    | PostfixExpr { $$ = $1; }
    | FunctionCallStmt { $$ = $1; }
    ;

Expression : '(' Expression ')' { $$ = $2; }
           | ConditionalExpr { $$ = $1;}
           | /* Empty expression */
           ;

ArrayElementExpr
    : IDENT ArraySubscripts { $$ = processArrayIdentifier($<s_var>1); arrayElementLoad($<s_var>1); }
;

ArraySubscripts
    : ArraySubscripts '[' Expression ']' 
    | '[' Expression ']' 

ConditionalExpr : LogicalOrExpr { $$ = $1;}
                | LogicalOrExpr '?' Expression ':' ConditionalExpr { $$ = $3; }
                ;

LogicalOrExpr : LogicalAndExpr
              | LogicalOrExpr LOR LogicalAndExpr { if (!objectExpBoolean('|', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
              ;

LogicalAndExpr : InclusiveOrExpr
               | LogicalAndExpr LAN InclusiveOrExpr { if (!objectExpBoolean('&', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
               ;

InclusiveOrExpr : ExclusiveOrExpr
                | InclusiveOrExpr BOR ExclusiveOrExpr { if (!objectExpBinary('|', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
                ;

ExclusiveOrExpr : AndExpr
                | ExclusiveOrExpr BXO AndExpr { if (!objectExpBinary('^', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
                ;

AndExpr : EqualityExpr
        | AndExpr BAN EqualityExpr { if (!objectExpBinary('&', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
        ;

EqualityExpr : RelationalExpr { $$ = $1;}
             | EqualityExpr EQL RelationalExpr { if (!objectExpBoolean('=', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
             | EqualityExpr NEQ RelationalExpr { if (!objectExpBoolean('!', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
             ;

RelationalExpr : ShiftExpr { $$ = $1;}
               | RelationalExpr LES AdditiveExpr { if (!objectExpBoolean('<', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
               | RelationalExpr LEQ AdditiveExpr { if (!objectExpBoolean('L', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
               | RelationalExpr GTR AdditiveExpr { if (!objectExpBoolean('>', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
               | RelationalExpr GEQ AdditiveExpr { if (!objectExpBoolean('G', &$<object_val>1, &$<object_val>3, &$$)) YYABORT; }
               ;

ShiftExpr : AdditiveExpr { $$ = $1;}
          | ShiftExpr SHL AdditiveExpr { objectExpBinary('<', &$<object_val>1, &$<object_val>3, &$$); }
          | ShiftExpr SHR AdditiveExpr { objectExpBinary('>', &$<object_val>1, &$<object_val>3, &$$); }
          ; 

AdditiveExpr : MultiplicativeExpr { $$ = $1;}
             | AdditiveExpr ADD MultiplicativeExpr { if (!objectExpression('+', &$<object_val>1, &$<object_val>3, &$$)) YYABORT;}
             | AdditiveExpr SUB MultiplicativeExpr { if (!objectExpression('-', &$<object_val>1, &$<object_val>3, &$$)) YYABORT;}
             ;

MultiplicativeExpr : UnaryExpr { $$ = $1;}
                   | MultiplicativeExpr MUL UnaryExpr { if (!objectExpression('*', &$<object_val>1, &$<object_val>3, &$$)) YYABORT;}
                   | MultiplicativeExpr DIV UnaryExpr { if (!objectExpression('/', &$<object_val>1, &$<object_val>3, &$$)) YYABORT;}
                   | MultiplicativeExpr REM UnaryExpr { if (!objectExpression('%', &$<object_val>1, &$<object_val>3, &$$)) YYABORT;}
                   ;

UnaryExpr : PostfixExpr
          | NOT UnaryExpr { if (!objectNotExpression(&$<object_val>2, &$$)) YYABORT;}
          | BNT UnaryExpr { if (!objectNotBinaryExpression(&$<object_val>2, &$$)) YYABORT;}
          | SUB UnaryExpr { if (!objectNegExpression(&$<object_val>2, &$$)) YYABORT;}
          | '(' VARIABLE_T ')' UnaryExpr { objectCast($<var_type>2, &$<object_val>4, &$$); }
          ;
    


PostfixExpr : PrimaryExpr { $$ = $1; }
            | '(' Expression ')' { $$ = $2; }
            | PostfixExpr INC_ASSIGN { if (!objectExpAssign('I', $$.symbol->name, &$$, &$$)) YYABORT; } // $$ 的資料型態是 Object，所以不能直接印出來
            | PostfixExpr DEC_ASSIGN { printf("DEC_ASSIGN\n"); }
            | FunctionCallStmt { $$ = $1;}
            ;

PrimaryExpr
    : STR_LIT { 
        Object* obj = malloc(sizeof(Object));
        obj->value = (uint64_t) $<s_var>1;
        obj->type = OBJECT_TYPE_STR;
        obj->symbol = NULL;
        $$ = *obj;
        // code("ldc \"%s\"", $<s_var>1);
        // printf("STR_LIT \"%s\"\n", (char *) $$.value); 
        processExp($$);
    }
    | CHAR_LIT { 
        Object* obj = malloc(sizeof(Object));
        obj->value = (uint64_t) $<c_var>1;
        obj->type = OBJECT_TYPE_CHAR;
        obj->symbol = NULL;
        $$ = *obj;
        // printf("CHAR_LIT '%c'\n", (char) $<c_var>1); 
        processExp($$);
    }
    | INT_LIT { 
        Object* obj = malloc(sizeof(Object));
        obj->value = $<i_var>1;
        obj->type = OBJECT_TYPE_INT;
        obj->symbol = NULL;
        $$ = *obj;
        // printf("INT_LIT %d\n", (int) $$.value); 
        processExp($$);
    }
    | FLOAT_LIT { 
        Object* obj = malloc(sizeof(Object));
        // obj->value = $<f_var>1; // 會有問題，因為 value 是 uint64_t
        setFloat(obj, $<f_var>1); // 這樣才對
        obj->type = OBJECT_TYPE_FLOAT;
        obj->symbol = NULL;
        $$ = *obj;
        // printf("FLOAT_LIT %f\n", $<f_var>1); 
        processExp($$);
    }
    | BOOL_LIT {
        Object* obj = malloc(sizeof(Object));
        obj->value = ($<b_var>1);
        obj->type = OBJECT_TYPE_BOOL;
        obj->symbol = NULL;
        $$ = *obj;
        // printf("BOOL_LIT %s\n", (bool) $$.value ? "TRUE" : "FALSE");
        processExp($$);
    }
    | IDENT { $$ = processIdentifier($<s_var>1); printf("IDENT %s\n", $<s_var>1); } 
    | ArrayElementExpr { $$ = $1; }
;
%%
/* C code section */