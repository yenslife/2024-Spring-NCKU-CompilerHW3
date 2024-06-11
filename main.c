#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define WJCL_LINKED_LIST_IMPLEMENTATION
#include "main.h"
#define WJCL_HASH_MAP_IMPLEMENTATION
// #include "value_operation.h"

#define debug printf("%s:%d: ############### debug\n", __FILE__, __LINE__)

#define iload(var) code("iload %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define lload(var) code("lload %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define fload(var) code("fload %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define dload(var) code("dload %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define aload(var) code("aload %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)

#define istore(var) code("istore %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define lstore(var) code("lstore %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define fstore(var) code("fstore %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define dstore(var) code("dstore %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)
#define astore(var) code("astore %" PRId64 " ; %s", (var)->symbol->addr, (var)->symbol->name)

#define ldz(val) code("ldc %d", getBool(val))
#define ldb(val) code("ldc %d", getByte(val))
#define ldc(val) code("ldc %d", getChar(val))
#define lds(val) code("ldc %d", getShort(val))
#define ldi(val) code("ldc %d", getInt(val))
#define ldl(val) code("ldc_w %" PRId64, getLong(val))
#define ldf(val) code("ldc %.6f", getFloat(val))
#define ldd(val) code("ldc_w %lf", getDouble(val))
#define ldt(val) code("ldc \"%s\"", getString(val))

const char* objectTypeName[] = {
    [OBJECT_TYPE_UNDEFINED] = "undefined",
    [OBJECT_TYPE_VOID] = "void",
    [OBJECT_TYPE_BOOL] = "bool",
    [OBJECT_TYPE_BYTE] = "byte",
    [OBJECT_TYPE_CHAR] = "char",
    [OBJECT_TYPE_SHORT] = "short",
    [OBJECT_TYPE_INT] = "int",
    [OBJECT_TYPE_LONG] = "long",
    [OBJECT_TYPE_FLOAT] = "float",
    [OBJECT_TYPE_DOUBLE] = "double",
    [OBJECT_TYPE_STR] = "string",
};
const char* objectJavaTypeName[] = {
    [OBJECT_TYPE_UNDEFINED] = "V",
    [OBJECT_TYPE_VOID] = "V",
    [OBJECT_TYPE_BOOL] = "Z",
    [OBJECT_TYPE_BYTE] = "B",
    [OBJECT_TYPE_CHAR] = "C",
    [OBJECT_TYPE_SHORT] = "S",
    [OBJECT_TYPE_INT] = "I",
    [OBJECT_TYPE_LONG] = "J",
    [OBJECT_TYPE_FLOAT] = "F",
    [OBJECT_TYPE_DOUBLE] = "D",
    [OBJECT_TYPE_STR] = "Ljava/lang/String;",
};


char* yyInputFileName;
bool compileError;

int indent = 0;
int scopeLevel = -1;
int funcLineNo = 0;
int variableAddress = 0;
int conditionIndex = 0;
bool emptyArray = false;
int arraySize = 0;
ObjectType variableIdentType;

// stack
struct list_head *scopeList[1024];

// cout list
Object coutList[1024];
int coutIndex = 0;

void pushScope() {
    scopeLevel++;
    // printf("> Create symbol table (scope level %d)\n", scopeLevel);
    scopeList[scopeLevel] = (struct list_head*)malloc(sizeof(struct list_head));
    INIT_LIST_HEAD(scopeList[scopeLevel]);
}

void dumpScope() {
    printf("\n> Dump symbol table (scope level: %d)\n", scopeLevel);
    printf("Index     Name                Type      Addr      Lineno    Func_sig  \n");
    struct list_head *pos;
    Object *obj;
    list_for_each(pos, scopeList[scopeLevel]) {
        obj = list_entry(pos, Object, list);
        printf("%-10d%-20s%-10s%-10ld%-10d%-10s\n"
            , obj->symbol->index, obj->symbol->name, objectTypeName[obj->type], obj->symbol->addr, obj->symbol->lineno, obj->symbol->func_sig);
    }
    scopeLevel--;
}

Object* createVariable(ObjectType variableType, char* variableName, int variableFlag) {
    // create variable object
    Object* variable = (Object*)malloc(sizeof(Object));
    variable->type = variableType;
    variable->symbol = (SymbolData*)malloc(sizeof(SymbolData));
    variable->symbol->name = strdup(variableName);
    variable->symbol->addr = variableAddress++;
    variable->symbol->func_sig = "-";
    variable->symbol->lineno = yylineno;
    return variable;
}

void pushFunParm(ObjectType variableType, char* variableName, int variableFlag) {
    // create variable object
    Object* variable = createVariable(variableType, variableName, variableFlag);
    // printf("> Insert `%s` (addr: %ld) to scope level %d\n", variableName, variable->symbol->addr, scopeLevel);

    // calculate index
    variable->symbol->index = list_empty(scopeList[scopeLevel]) ? 0 : list_entry(scopeList[scopeLevel]->prev, Object, list)->symbol->index + 1;

    // add to scope list
    list_add_tail(&variable->list, scopeList[scopeLevel]);
}

void pushVariable(ObjectType variableType, char* variableName, int variableFlag, Object *variable) {
    // create variable object
    if (variableType == OBJECT_TYPE_UNDEFINED) {
        variableType = variableIdentType;
    }
    if (variableType == OBJECT_TYPE_AUTO) {
        variableType = variable->type;
    }
    Object* mainVariable = createVariable(variableType, variableName, variableFlag);
    // printf("> Insert `%s` (addr: %ld) to scope level %d\n", variableName, mainVariable->symbol->addr, scopeLevel);

    // calculate index
    mainVariable->symbol->index = list_empty(scopeList[scopeLevel]) ? 0 : list_entry(scopeList[scopeLevel]->prev, Object, list)->symbol->index + 1;

    // add to scope list
    list_add_tail(&mainVariable->list, scopeList[scopeLevel]);

    codeRaw("ldc 0");
    istore(mainVariable);
}

void pushVariableList(ObjectType varType) {
    variableIdentType = varType;
}

void pushArrayVariable(ObjectType variableType, char* variableName, int variableFlag) {
    // create variable object
    if (variableType == OBJECT_TYPE_UNDEFINED) {
        variableType = variableIdentType;
    }
    Object* variable = createVariable(variableType, variableName, variableFlag);

    if (arraySize != -1)
        printf("create array: %d\n", arraySize);
    // printf("> Insert `%s` (addr: %ld) to scope level %d\n", variableName, variable->symbol->addr, scopeLevel);

    // calculate index
    variable->symbol->index = list_empty(scopeList[scopeLevel]) ? 0 : list_entry(scopeList[scopeLevel]->prev, Object, list)->symbol->index + 1;

    // add to scope list
    list_add_tail(&variable->list, scopeList[scopeLevel]);
    arraySize = 0;
}

void incrementArrayElement () {
    arraySize++;
}

void nonInitArray() {
    arraySize = -1;
}

void createFunction(ObjectType variableType, char* funcName) {
    // create function object
    Object* function = (Object*)malloc(sizeof(Object));
    function->type = OBJECT_TYPE_FUNCTION;
    function->symbol = (SymbolData*)malloc(sizeof(SymbolData));
    function->symbol->name = strdup(funcName);
    function->symbol->addr = -1; // 如果是 function 就不用 addr
    function->symbol->func_sig = "([Ljava/lang/String;)I"; // 暫時先用 ([Ljava/lang/String;)I，之後再來改
    function->symbol->returnType = variableType;
    function->symbol->lineno = yylineno; // 暫時先用 lineno 來記錄 function 的行數
    // printf("func: %s\n", funcName);
    // printf("> Insert `%s` (addr: %ld) to scope level %d\n", funcName, function->symbol->addr, scopeLevel);
    code(".method public static %s([Ljava/lang/String;)V", funcName, objectJavaTypeName[variableType]);
    codeRaw(".limit stack 100");
    codeRaw(".limit locals 100");
    
    // calculate index
    function->symbol->index = list_empty(scopeList[scopeLevel]) ? 0 : list_entry(scopeList[scopeLevel]->prev, Object, list)->symbol->index + 1;

    // add to scope list
    list_add_tail(&function->list, scopeList[scopeLevel]);

    // push scope
    pushScope();
}

void debugPrintInst(char instc, Object* a, Object* b, Object* out) {
}

bool outTypeConvert(Object* a, Object* b, Object* out) {
    if (a->type == OBJECT_TYPE_INT && b->type == OBJECT_TYPE_INT) {
        out->type = OBJECT_TYPE_INT;
    } else if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
        out->type = OBJECT_TYPE_FLOAT;
    } else if (a->type == OBJECT_TYPE_BOOL && b->type == OBJECT_TYPE_INT) {
        out->type = OBJECT_TYPE_INT;
    } else if (a->type == OBJECT_TYPE_INT && b->type == OBJECT_TYPE_BOOL) {
        out->type = OBJECT_TYPE_INT;
    } 
    else {
        out->type = OBJECT_TYPE_UNDEFINED;
        printf("a type: %s, b type: %s\n", objectTypeName[a->type], objectTypeName[b->type]);
        printf("a value: %ld, b value: %ld\n", a->value, b->value);
    }
    return true;
}

bool objectExpression(char op, Object* dest, Object* val, Object* out) {

    // type convert
    if (!outTypeConvert(dest, val, out)) {
        // out->type = OBJECT_TYPE_UNDEFINED;
        return false;
    }

    if (op == '+') {
        if (out->type == OBJECT_TYPE_FLOAT) {            
            float tmp = getFloat(dest) + getFloat(val);
            setFloat(out, tmp);
            codeRaw("fadd");
        } else {
            out->value = dest->value + val->value;
            codeRaw("iadd");
        }
        // out->value = dest->value + val->value;
        // printf("dest value: %f, val value: %f, out value: %f, calculate value: %f\n", (float)dest->value, (float)val->value, (float)out->value, (float)dest->value + val->value);
        // printf("ADD\n");
    } else if (op == '-') {
        if (out->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) - getFloat(val);
            setFloat(out, tmp);
            codeRaw("fsub");
        } else {
            out->value = dest->value - val->value;
            codeRaw("isub");
        }
        // out->value = dest->value - val->value;
        // printf("dest value: %f, val value: %f, out value: %f, calculate value: %f\n", (float)dest->value, (float)val->value, (float)out->value, (float)dest->value - val->value);
        // printf("SUB\n");
    } else if (op == '*') {
        if (out->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) * getFloat(val);
            setFloat(out, tmp);
            codeRaw("fmul");
        } else {
            out->value = dest->value * val->value;
            codeRaw("imul");
        }
        // out->value = dest->value * val->value;
        // printf("dest value: %d, val value: %f, out value: %f, calculate value: %f\n", (float)dest->value, (float)val->value, getFloat(out), (float)dest->value * val->value);
        // printf("MUL\n");
    } else if (op == '/') {
        if (out->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) / getFloat(val);
            setFloat(out, tmp);
            codeRaw("fdiv");
        } else {
            out->value = dest->value / val->value;
            codeRaw("idiv");
        }
        // out->value = dest->value / val->value;
        // printf("dest value: %ld, val value: %ld, out value: %ld\n", dest->value, val->value, out->value);
        // printf("dest value: %f, val value: %f, out value: %f, calculate / value: %f\n", (float)dest->value, (float)val->value, (float)out->value, (float)dest->value / val->value);
        // printf("DIV\n");
    } else if (op == '%') {
        out->value = dest->value % val->value;
        // printf("REM\n");
        codeRaw("irem");
    }
    return true;
}

bool objectExpBinary(char op, Object* a, Object* b, Object* out) {
    if (op == '>') {
        out->value = a->value >> b->value;
        // printf("SHR\n");
        codeRaw("ishr");
    } else if (op == '<') {
        out->value = a->value << b->value;
        // printf("SHL\n");
        codeRaw("ishl");
    } else if (op == '|') {
        out->value = a->value | b->value;
        // printf("BOR\n");
        codeRaw("ior");
    } else if (op == '&') {
        out->value = a->value & b->value;
        // printf("BAN\n");
        codeRaw("iand");
    } else if (op == '^') {
        out->value = a->value ^ b->value;
        // printf("BXO\n");
        codeRaw("ixor");
    } 
    return true;
}

bool objectExpBoolean(char op, Object* a, Object* b, Object* out) {
    // type convert
    float tmp1 = getFloat(a);
    float tmp2 = getFloat(b);
    out->type = OBJECT_TYPE_BOOL;
    if (op == '>') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 > tmp2;
            codeRaw("fcmpl");
        } else {
            out->value = a->value > b->value;
            // out->value = a->value > b->value;
            // printf("GTR\n");
            // 可以用相減的方式來判斷是否大於
            codeRaw("isub");
        }
        code("ifgt greater_than%d", conditionIndex);
        codeRaw("ldc 0");
        code("goto end_greater_than%d", conditionIndex);
        code("greater_than%d:", conditionIndex);
        codeRaw("ldc 1");
        code("end_greater_than%d:", conditionIndex);
        conditionIndex++;
    } else if (op == '<') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 < tmp2;
            codeRaw("fcmpl");
        } else {
            out->value = a->value < b->value;
            codeRaw("isub");
        }
        out->value = a->value < b->value;
        // printf("LES\n");
        // 可以用相減的方式來判斷是否小於
        code("iflt less_than%d", conditionIndex);
        codeRaw("ldc 0");
        code("goto end_less_than%d", conditionIndex);
        code("less_than%d:", conditionIndex);   
        codeRaw("ldc 1");
        code("end_less_than%d:", conditionIndex);
        conditionIndex++;
    } else if (op == '&') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 && tmp2;
            codeRaw("fmul");
            codeRaw("ldc 0.0");
            codeRaw("fcmpl");
            code("ifeq and%d", conditionIndex);
            codeRaw("ldc 0");
            code("goto end_and%d", conditionIndex);
            code("and%d:", conditionIndex);
            codeRaw("ldc 1");
            code("end_and%d:", conditionIndex);
        } else {
            out->value = a->value && b->value;
            // out->value = a->value && b->value;
            // printf("LAN\n");
            // 可以用相乘的方式來判斷是否為 true
            codeRaw("imul");
            code("ifeq and%d", conditionIndex);
            codeRaw("ldc 0");
            code("goto end_and%d", conditionIndex);
            code("and%d:", conditionIndex);
            codeRaw("ldc 1");
            code("end_and%d:", conditionIndex);
        }
        conditionIndex++;
    } else if (op == '|') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 || tmp2;
        } else {
            out->value = a->value || b->value;
        }
        // out->value = a->value || b->value;
        // printf("LOR\n");
        // 可以用相加的方式來判斷是否為 true
        codeRaw("iadd");
        code("ifne or%d", conditionIndex);
        codeRaw("ldc 0");
        code("goto end_or%d", conditionIndex);
        code("or%d:", conditionIndex);
        codeRaw("ldc 1");
        code("end_or%d:", conditionIndex);
        conditionIndex++;
    } else if (op == '=') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 == tmp2;
        } else {
            out->value = a->value == b->value;
        }
        // out->value = a->value == b->value;
        // printf("EQL\n");
        // 用相減的方式來判斷是否相等
        codeRaw("isub");
        code("ifeq equal%d", conditionIndex);
        codeRaw("ldc 0");
        code("goto end_equal%d", conditionIndex);
        code("equal%d:", conditionIndex);
        codeRaw("ldc 1");
        code("end_equal%d:", conditionIndex);
        conditionIndex++;
    } else if (op == '!') {
        if (a->type == OBJECT_TYPE_FLOAT || b->type == OBJECT_TYPE_FLOAT) {
            out->value = tmp1 != tmp2;
        } else {
            out->value = a->value != b->value;
        }
        // out->value = a->value != b->value;
        // printf("NEQ\n");
        // 用相減的方式來判斷是否不相等
        codeRaw("isub");
        code("ifne not_equal%d", conditionIndex);
        codeRaw("ldc 0");
        code("goto end_not_equal%d", conditionIndex);
        code("not_equal%d:", conditionIndex);
        codeRaw("ldc 1");
        code("end_not_equal%d:", conditionIndex);
        conditionIndex++;
    }
    else {
        // out->type = OBJECT_TYPE_UNDEFINED;
        // return false;
    }
    return true;
}

bool objectExpAssign(char op, char* identifier, Object* val, Object* out) {
    // printf("id: %s op: %c\n", identifier, op);
    Object* dest = findVariable(identifier, OBJECT_TYPE_UNDEFINED);
    // float tmp;
    if (dest == NULL) {
        return false;
    } 
    if (dest->type == OBJECT_TYPE_UNDEFINED) {
        dest->type = dest->type;
    } 
    if (dest->type == OBJECT_TYPE_AUTO) {
        dest->type = dest->type;
    }
    if (op == '=') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(val);
            // printf("getFloat(dest): %f, getFloat(val): %f\n", getFloat(dest), getFloat(val));
            setFloat(dest, tmp);
            fstore(dest);
        } else if (dest->type == OBJECT_TYPE_INT) {
            dest->value = val->value;
            istore(dest);
        } else if (dest->type == OBJECT_TYPE_STR) {
            dest->value = val->value;
            astore(dest);
        } else if (dest->type == OBJECT_TYPE_BOOL) {
            dest->value = val->value;
            // 儲存 boolean 要用 1 來表示 true
            if (val->value == 0) {
                ldz(dest);
            } else {
                ldi(dest);
            }
            istore(dest);
        }
    } else if (op == '+') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) + getFloat(val);
            // printf("getFloat(dest): %f, getFloat(val): %f\n", getFloat(dest), getFloat(val));
            setFloat(dest, tmp);
            fload(dest);
            codeRaw("fadd");
            fstore(dest);
        } else {
            dest->value = dest->value + val->value;
            // load and add
            iload(dest);
            codeRaw("iadd");
            istore(dest);
        }
        // printf("ADD_ASSIGN\n");
    } else if (op == '-') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) - getFloat(val);
            // printf("getFloat(dest): %f, getFloat(val): %f\n", getFloat(dest), getFloat(val));
            setFloat(dest, tmp);
            fload(dest);
            codeRaw("swap");
            codeRaw("fsub");
            fstore(dest);
        } else {
            dest->value = dest->value - val->value;
            // load and sub
            iload(dest);
            codeRaw("swap");
            codeRaw("isub");
            istore(dest);
        }
        // printf("SUB_ASSIGN\n");
    } else if (op == '*') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) * getFloat(val);
            // printf("getFloat(dest): %f, getFloat(val): %f\n", getFloat(dest), getFloat(val));
            setFloat(dest, tmp);
            fload(dest);
            codeRaw("fmul");
            fstore(dest);
        } else {
            dest->value = dest->value * val->value;
            // load and mul
            iload(dest);
            codeRaw("imul");
            istore(dest);
        }
        // printf("MUL_ASSIGN\n");
    } else if (op == '/') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) / getFloat(val);
            setFloat(dest, tmp);
            fload(dest);
            codeRaw("swap");
            codeRaw("fdiv");
            fstore(dest);
        } else {
            dest->value = dest->value / val->value;
            // load and div
            iload(dest);
            codeRaw("swap");
            codeRaw("idiv");
            istore(dest);
        }   
        // printf("DIV_ASSIGN\n");
    } else if (op == '%') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            float tmp = getFloat(dest) / getFloat(val);
            setFloat(dest, tmp);
            fload(dest);
            codeRaw("swap");
            codeRaw("frem");
            fstore(dest);
        } else {
            dest->value = dest->value % val->value;
            // load and rem
            iload(dest);
            codeRaw("swap");
            codeRaw("irem");
            istore(dest);
        }
        // printf("REM_ASSIGN\n");
    } else if (op == '|') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            return false;
            fload(dest);
            codeRaw("swap");
            codeRaw("ior");
            fstore(dest);
        } else {
            dest->value = dest->value | val->value;
            iload(dest);
            codeRaw("swap");
            codeRaw("ior");
            istore(dest);
        }
        // printf("BOR_ASSIGN\n");
    } else if (op == '&') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            return false;
            fload(dest);
            codeRaw("swap");
            codeRaw("fand");
            fstore(dest);
        } else {
            dest->value = dest->value & val->value;
            iload(dest);
            codeRaw("swap");
            codeRaw("iand");
            istore(dest);
        }
        // printf("BAN_ASSIGN\n");
    } else if (op == '^') {
        if (dest->type == OBJECT_TYPE_FLOAT) {
            return false;
        } else {
            dest->value = dest->value ^ val->value;
        }
        // printf("BXO_ASSIGN\n");
    } else if (op == '>') {
        dest->value = dest->value >> val->value;
        codeRaw("iushr");
        istore(dest);
        // printf("SHR_ASSIGN\n");
    } else if (op == '<') {
        dest->value = dest->value << val->value;
        codeRaw("ishl");
        istore(dest);
        // printf("SHL_ASSIGN\n");
    }
    return true;
}

bool objectValueAssign(Object* dest, Object* val, Object* out) {
    return false;
}

bool objectNotBinaryExpression(Object* dest, Object* out) {
    if (!dest || !out) {
        return false;
    }
    out->type = OBJECT_TYPE_INT;
    out->value = ~dest->value;
    // printf("BNT\n");
    codeRaw("ldc -1");
    codeRaw("ixor");
    return true;
}

bool objectNegExpression(Object* dest, Object* out) {
    if (!dest || !out) {
        return false;
    }
    if (dest->type == OBJECT_TYPE_FLOAT) {
        out->type = OBJECT_TYPE_FLOAT;
        float tmp = -getFloat(dest);
        setFloat(out, tmp);
        codeRaw("fneg");
    } else {
        out->type = OBJECT_TYPE_INT;
        out->value = -dest->value;
        codeRaw("ineg");
    }
    // out->type = OBJECT_TYPE_INT;
    // out->value = -dest->value;
    // printf("NEG\n");
    return true;
}
bool objectNotExpression(Object* dest, Object* out) {
    if (!dest || !out) {
        return false;
    }
    if (out->type == OBJECT_TYPE_FLOAT) {
        out->type = OBJECT_TYPE_FLOAT;
        float tmp = !getFloat(dest);
        setFloat(out, tmp);
    } else {
        out->type = OBJECT_TYPE_INT;
        out->value = !dest->value;
    }
    // out->type = OBJECT_TYPE_BOOL;
    // out->value = !dest->value;
    // printf("NOT\n");
    codeRaw("ldc 1");
    codeRaw("ixor");
    return true;
}

bool objectIncAssign(Object* a, Object* out) {
    return false;
}

bool objectDecAssign(Object* a, Object* out) {
    return false;
}

bool objectCast(ObjectType variableType, Object* dest, Object* out) {
    if (!dest || !out) {
        return false;
    }
    out->type = variableType;
    out->value = dest->value;
    printf("Cast to %s\n", objectTypeName[variableType]);
    return true;
}

Object* findVariable(char* variableName, ObjectType variableType) {
    Object* variable = NULL;
    struct list_head *pos;
    Object *obj;
    for (int i = scopeLevel; i >= 0; i--) {
        list_for_each(pos, scopeList[i]) {
            obj = list_entry(pos, Object, list);
            if (strcmp(obj->symbol->name, variableName) == 0) {
                variable = obj;
                break;
            }
        }
        if (variable) {
            break;
        }
    }
    return variable;
}

void pushFunInParm(Object variable) {
    // 目前只有 cout 用到
    coutList[coutIndex++] = variable;
    if (variable.type == OBJECT_TYPE_INT) {
        codeRaw("invokevirtual java/io/PrintStream/print(I)V");
    } else if (variable.type == OBJECT_TYPE_FLOAT) {
        codeRaw("invokevirtual java/io/PrintStream/print(F)V");
    } else if (variable.type == OBJECT_TYPE_STR) {
        codeRaw("invokevirtual java/io/PrintStream/print(Ljava/lang/String;)V");
    } else if (variable.type == OBJECT_TYPE_BOOL) {
        codeRaw("invokevirtual java/io/PrintStream/print(Z)V");
    } else if (variable.type == OBJECT_TYPE_ENDL) {
        codeRaw("invokevirtual java/io/PrintStream/println()V");
    } else if (variable.type == OBJECT_TYPE_CHAR) {
        codeRaw("invokevirtual java/io/PrintStream/print(Ljava/lang/String;)V");
    }
}

bool objectFunctionCall(char* name, Object* out) {
    Object *obj = findVariable(name, OBJECT_TYPE_FUNCTION);
    if (obj == NULL) {
        printf("Function `%s` not found\n", name);
        return false;
    }
    if (obj->type != OBJECT_TYPE_FUNCTION) {
        printf("`%s` is not a function\n", name);
        return false;
    }
    // convert to JNI func_sig
    /* rule:
    “([Ljava/lang/String;)V” 它是一种对函数返回值和参数的编码。这种编码叫做JNI字段描述符（JavaNative Interface FieldDescriptors)。一个数组int[]，就需要表示为这样"[I"。如果多个数组double[][][]就需要表示为这样 "[[[D"。也就是说每一个方括号开始，就表示一个数组维数。多个方框后面，就是数组 的类型。 如果以一个L开头的描述符，就是类描述符，它后紧跟着类的字符串，然后分号“；”结束。 比如"Ljava/lang/String;"就是表示类型String； "[I"就是表示int[]; "[Ljava/lang/Object;"就是表示Object[]。 JNI方法描述符，主要就是在括号里放置参数，在括号后面放置返回类型，如下： （参数描述符）返回类型 当一个函数不需要返回参数类型时，就使用”V”来表示。 比如"()Ljava/lang/String;"就是表示String f(); "(ILjava/lang/Class;)J"就是表示long f(int i, Class c); "([B)V"就是表示void String(byte[] bytes); Java 类型符号BooleanZByteBCharCShortSIntILongJFloatFDoubleDVoidVobjects对象以"L"开头，以";"结尾，中间是用"/" 隔开的包及类名。比如：Ljava/lang/String;如果是嵌套类，则用$来表示嵌套。例如 "(Ljava/lang/String;Landroid/os/FileUtils$FileStatus;)Z" 另外数组类型的简写,则用"["加上如表A所示的对应类型的简写形式进行表示就可以了，比如：[I 表示 int [];[L/java/lang/objects;表示Objects[],另外。引用类型（除基本类型的数组外）的标示最后都有个";" 例如： "()V" 就表示void Func(); "(II)V" 表示 void Func(int, int); "(Ljava/lang/String;Ljava/lang/String;)I".表示 int Func(String,String)
    */
    char* func_sig = (char*)malloc(1024);
    strcpy(func_sig, "(");
    for (int i = 0; i < obj->symbol->paramCount; i++) {
        switch (obj->symbol->paramTypes[i]) {
            case OBJECT_TYPE_INT:
                strcat(func_sig, "I");
                break;
            case OBJECT_TYPE_FLOAT:
                strcat(func_sig, "F");
                break;
            case OBJECT_TYPE_BOOL:
                strcat(func_sig, "B");
                break;
            case OBJECT_TYPE_STR:
                strcat(func_sig, "Ljava/lang/String;");
                break;
            case OBJECT_TYPE_VOID:
                strcat(func_sig, "V");
                break;
            default:
                strcat(func_sig, "L");
                strcat(func_sig, objectTypeName[obj->symbol->paramTypes[i]]);
                strcat(func_sig, ";");
                break;
        }
    }
    strcat(func_sig, ")");
    switch (obj->symbol->returnType) {
        case OBJECT_TYPE_INT:
            strcat(func_sig, "I");
            break;
        case OBJECT_TYPE_FLOAT:
            strcat(func_sig, "F");
            break;
        case OBJECT_TYPE_BOOL:
            strcat(func_sig, "B");
            break;
        case OBJECT_TYPE_STR:
            strcat(func_sig, "Ljava/lang/String;");
            break;
        case OBJECT_TYPE_VOID:
            strcat(func_sig, "V");
            break;
        default:
            strcat(func_sig, "L");
            strcat(func_sig, objectTypeName[obj->symbol->returnType]);
            strcat(func_sig, ";");
            break;
    }
    obj->symbol->func_sig = func_sig;
    printf("call: %s%s\n", obj->symbol->name, obj->symbol->func_sig);

    coutIndex = 0;
    out->type = obj->symbol->returnType;
    return true;

}

bool addFunctionParam(char* name) {
    Object* obj = findVariable(name, OBJECT_TYPE_FUNCTION);
    if (obj == NULL) {
        printf("Variable `%s` not found\n", name);
        return false;
    }
    int paramCount = 0;
    struct list_head *pos;
    Object *function;
    list_for_each(pos, scopeList[scopeLevel]) {
        Object *param = list_entry(pos, Object, list);
        obj->symbol->paramTypes[paramCount++] = param->type;
    }
    obj->symbol->paramCount = paramCount;
    return true;

}

Object processIdentifier(char* identifier) {
    Object* obj = findVariable(identifier, OBJECT_TYPE_UNDEFINED);
    if (obj == NULL) {
        obj = (Object*)malloc(sizeof(Object));
        obj->symbol = (SymbolData*)malloc(sizeof(SymbolData));
        obj->symbol->name = strdup(identifier); // 注意，这里使用 strdup 来复制字符串

        if (strcmp(identifier, "endl") == 0) {
            obj->symbol->addr = -1;
            obj->value = (uint64_t) "\n";
            obj->type = OBJECT_TYPE_ENDL;
        } else {
            obj->symbol->addr = 0; // 如果不是特殊符号，默认地址为 0，可以根据需要修改
            obj->value = 0; // 默认值
            obj->type = OBJECT_TYPE_UNDEFINED; // 默认类型
        }
    } else {
        if (obj->type == OBJECT_TYPE_INT) {
            iload(obj);
        } else if (obj->type == OBJECT_TYPE_FLOAT) {
            fload(obj);
        } else if (obj->type == OBJECT_TYPE_STR) {
            aload(obj);
        } else if (obj->type == OBJECT_TYPE_BOOL) {
            iload(obj);
        } else if (obj->type == OBJECT_TYPE_CHAR) {
            aload(obj);
        }
    }
    // printf("IDENT (name=%s, address=%ld)\n", obj->symbol->name, obj->symbol->addr);
    return *obj;
}

void stdoutPrint() {
    printf("cout");
    for (int i = 0; i < coutIndex; i++) {
        // printf(" %s", objectTypeName[coutList[i].type]);
        
        if (coutList[i].type == OBJECT_TYPE_ENDL) {
            // codeRaw("invokevirtual java/io/PrintStream/println()V");
        }
        else if (coutList[i].type == OBJECT_TYPE_INT) {
            // code("ldc \"%d\"", (int)coutList[i].value);
            // ldi(&coutList[i]);
            // codeRaw("invokevirtual java/io/PrintStream/print(I)V");
        } else if (coutList[i].type == OBJECT_TYPE_FLOAT) {
            // char format[10];
            // char output[100];
            // // 根據值去計算小數點後幾位，存在變數 decimal p
            // int decimal = 0;
            // float tmp = getFloat(&coutList[i]);
            // while (tmp != (int)tmp) {
            //     tmp *= 10;
            //     decimal++;
            // }
            // sprintf(format, "\"%%.%df\"\n", decimal);
            // sprintf(output, format, getFloat(&coutList[i]));
            // fprintf(yyout, "ldc %s\n", output);
            // code("ldc \"%f\"", getFloat(&coutList[i]));
            // ldf(&coutList[i]);
            // codeRaw("invokevirtual java/io/PrintStream/print(F)V");
        }
        else if (coutList[i].type == OBJECT_TYPE_STR){
            // code("ldc \"%s\"", (char*)coutList[i].value);
            // ldt(&coutList[i]);
            // codeRaw("invokevirtual java/io/PrintStream/print(Ljava/lang/String;)V");
        }
        else if (coutList[i].type == OBJECT_TYPE_CHAR) {
            // code ("ldc \"%c\"", (char)coutList[i].value);
            // lds(&coutList[i]);
            // codeRaw("invokevirtual java/io/PrintStream/print(C)V");
        }
        else if (coutList[i].type == OBJECT_TYPE_BOOL) {
            // code("ldc \"%s\"", (int)coutList[i].value ? "true" : "false");
            // ldz(&coutList[i]);
            // codeRaw("invokevirtual java/io/PrintStream/print(Z)V");
        } else {
            // code("ldc \"%s\"", objectTypeName[coutList[i].type]);
            printf(" ctype: %s\n", objectTypeName[coutList[i].type]);
        }
        // codeRaw("swap");
        // printf("print: %s\n", coutList[i].value);
    }
    // printf("\n");
    // codeRaw("invokevirtual java/io/PrintStream/print(I)V");
    // codeRaw("invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V");
    coutIndex = 0;
}

void processExp(Object obj){
    if (obj.type == OBJECT_TYPE_INT) {
        code("ldc %d", (int)obj.value);
    } else if (obj.type == OBJECT_TYPE_FLOAT) {
        code("ldc %f", getFloat(&obj));
        printf("processExp: %f\n", getFloat(&obj));
    } else if (obj.type == OBJECT_TYPE_STR) {
        code("ldc \"%s\"", (char*)obj.value);
        printf("processExp: %s\n", (char*)obj.value);
    } else if (obj.type == OBJECT_TYPE_CHAR) {
        code("ldc \"%c\"", (char)obj.value);
    } else if (obj.type == OBJECT_TYPE_BOOL) {
        code("ldc %d", (int)obj.value ? 1 : 0);
    }
}

int main(int argc, char* argv[]) {
    char* outputFileName = NULL;
    if (argc == 3) {
        yyin = fopen(yyInputFileName = argv[1], "r");
        yyout = fopen(outputFileName = argv[2], "w");
    } else if (argc == 2) {
        yyin = fopen(yyInputFileName = argv[1], "r");
        yyout = stdout;
    } else {
        printf("require input file");
        exit(1);
    }
    if (!yyin) {
        printf("file `%s` doesn't exists or cannot be opened\n", yyInputFileName);
        exit(1);
    }
    if (!yyout) {
        printf("file `%s` doesn't exists or cannot be opened\n", outputFileName);
        exit(1);
    }

    scopeLevel = 0;
    codeRaw(".source Main.j");
    codeRaw(".class public Main");
    codeRaw(".super java/lang/Object");
    // codeRaw(".method public static main([Ljava/lang/String;)V");
    // scopeLevel = 1;
    // codeRaw(".limit stack 100");
    // codeRaw(".limit locals 100");
    // codeRaw("getstatic java/lang/System/out Ljava/io/PrintStream;");
    // codeRaw("ldc \"Hello World\"");
    // codeRaw("invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V");
    // codeRaw("return");
    // scopeLevel = 0;
    // codeRaw(".end method");
    scopeLevel = -1;

    yyparse();
    // printf("Total lines: %d\n", yylineno);
    fclose(yyin);

    yylex_destroy();
    return 0;
}