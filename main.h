#ifndef MAIN_H
#define MAIN_H
#include "compiler_common.h"

#define code(format, ...) \
    fprintf(yyout, "%*s" format "\n", scopeLevel << 2, "", __VA_ARGS__)
#define codeRaw(code) \
    fprintf(yyout, "%*s" code "\n", scopeLevel << 2, "")

extern FILE* yyout;
extern FILE* yyin;
extern bool compileError;
// extern int scopeLevel;
int yyparse();
int yylex();
int yylex_destroy();
extern int scopeLevel;

#define VAR_FLAG_DEFAULT 0
#define VAR_FLAG_ARRAY 0b00000001
#define VAR_FLAG_POINTER 0b00000010

void pushScope();
void dumpScope();

// stack
extern struct list_head *scopeList[1024]; /* scope list */

void pushFunParm(ObjectType variableType, char* variableName, int parmFlag);
void pushVariable(ObjectType variableType, char* variableName, int variableFlag, Object *variableValue);
void pushVariableList(ObjectType varType);
void pushArrayVariable(ObjectType variableType, char* variableName, int variableFlag);
void incrementArrayElement();
void nonInitArray();
void createFunction(ObjectType variableType, char* funcName);
void pushFunInParm(Object* b);

Object* findVariable(char* variableName, ObjectType variableType);
Object* createVariable(ObjectType variableType, char* variableName, int variableFlag);
bool objectExpression(char op, Object* a, Object* b, Object* out);
bool objectExpBinary(char op, Object* a, Object* b, Object* out);
bool objectExpBoolean(char op, Object* a, Object* b, Object* out);
bool objectExpAssign(char op, Object* dest, Object* val, Object* out);
bool objectValueAssign(Object* dest, Object* val, Object* out);
bool objectNotBinaryExpression(Object* dest, Object* out);
bool objectNotExpression(Object* dest, Object* out);
bool objectNegExpression(Object* dest, Object* out);
bool objectIncAssign(Object* a, Object* out);
bool objectDecAssign(Object* a, Object* out);
bool objectCast(ObjectType variableType, Object* dest, Object* out);
bool objectFunctionCall(char* name, Object* out);
bool addFunctionParam(char* name);

Object processIdentifier(char* identifier);

void stdoutPrint();

#endif