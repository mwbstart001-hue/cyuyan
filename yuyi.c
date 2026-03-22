/*
 * 编译器实现：词法分析、语法分析、语义分析、中间代码生成
 * 语言：C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ==================== 常量定义 ==================== */
#define MAX_TOKEN_LEN 256
#define MAX_LINE_LEN 1024
#define MAX_KEYWORDS 20
#define MAX_SYMBOLS 100
#define MAX_ERRORS 100
#define MAX_CODE_LEN 1000
#define MAX_TREE_NODES 1000

/* ==================== 词法单元类型 ==================== */
typedef enum {
    /* 保留字 */
    TOKEN_VOID, TOKEN_MAIN, TOKEN_VAR, TOKEN_INT, TOKEN_IF,
    TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN, TOKEN_BREAK, TOKEN_CONTINUE,
    /* 运算符 */
    TOKEN_ASSIGN, TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV,
    TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE, TOKEN_EQ, TOKEN_NE,
    /* 分隔符 */
    TOKEN_LP, TOKEN_RP, TOKEN_LC, TOKEN_RC, TOKEN_SEMI, TOKEN_COLON,
    /* 其他 */
    TOKEN_ID, TOKEN_TYPE, TOKEN_INT_CONST, TOKEN_EOF, TOKEN_ERROR
} TokenType;

/* ==================== 保留字表（已排序，用于折半查找） ==================== */
typedef struct {
    char *word;
    TokenType type;
} Keyword;

Keyword keyword_table[MAX_KEYWORDS] = {
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"else", TOKEN_ELSE},
    {"if", TOKEN_IF},
    {"int", TOKEN_TYPE},
    {"main", TOKEN_MAIN},
    {"return", TOKEN_RETURN},
    {"var", TOKEN_VAR},
    {"void", TOKEN_VOID},
    {"while", TOKEN_WHILE}
};

const int keyword_count = 10;

/* ==================== 词法单元结构 ==================== */
typedef struct {
    TokenType type;
    char text[MAX_TOKEN_LEN];
    int value;          /* 对于INT类型 */
    int line;           /* 所在行号 */
} Token;

/* ==================== 错误信息结构 ==================== */
typedef struct {
    int line;
    char message[MAX_TOKEN_LEN];
} ErrorInfo;

ErrorInfo errors[MAX_ERRORS];
int error_count = 0;

/* ==================== 符号表结构 ==================== */
typedef struct {
    char name[MAX_TOKEN_LEN];
    char type[MAX_TOKEN_LEN];  /* int */
    int scope;
    int offset;     /* 相对地址 */
} Symbol;

Symbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;
int current_scope = 0;
int current_offset = 0;

/* ==================== 语法树节点 ==================== */
typedef struct TreeNode {
    char name[MAX_TOKEN_LEN];       /* 节点名称 */
    char extra[MAX_TOKEN_LEN];      /* 额外信息（如ID值、类型等） */
    int line;                       /* 行号 */
    bool is_terminal;               /* 是否为终结符 */
    bool is_empty;                  /* 是否产生空串 */
    struct TreeNode *children[10];  /* 子节点 */
    int child_count;
} TreeNode;

TreeNode *root = NULL;

/* ==================== 中间代码结构 ==================== */
typedef struct {
    int label;          /* 标号 */
    char op[10];        /* 操作码 */
    char arg1[MAX_TOKEN_LEN];
    char arg2[MAX_TOKEN_LEN];
    char result[MAX_TOKEN_LEN];
} Quad;

Quad code[MAX_CODE_LEN];
int code_count = 0;
int temp_count = 0;
int label_count = 0;

/* ==================== 全局变量 ==================== */
FILE *source_file;
char current_line[MAX_LINE_LEN];
int current_line_num = 0;
int line_pos = 0;
Token current_token;
bool has_error = false;

/* ==================== 函数声明 ==================== */
/* 词法分析 */
TokenType binary_search_keyword(const char *word);
void get_next_token();
void skip_whitespace_and_comments();
void report_lexical_error(int line, const char *msg);

/* 语法分析 */
void match(TokenType expected);
TreeNode* create_node(const char *name, int line, bool is_terminal);
TreeNode* parse_program();
TreeNode* parse_main_declaration();
TreeNode* parse_function_body();
TreeNode* parse_declaration_list();
TreeNode* parse_declaration_stat();
TreeNode* parse_statement_list();
TreeNode* parse_statement();
TreeNode* parse_assignment_stat();
TreeNode* parse_if_stat();
TreeNode* parse_while_stat();
TreeNode* parse_return_stat();
TreeNode* parse_expression();
TreeNode* parse_term();
TreeNode* parse_factor();
TreeNode* parse_condition();
TreeNode* parse_type();
void report_syntax_error(int line, const char *msg);

/* 语义分析 */
bool lookup_symbol(const char *name);
void add_symbol(const char *name, const char *type);
void check_undefined_id(const char *name, int line);
void check_type_compatibility(const char *type1, const char *type2, int line);

/* 中间代码生成 */
char* new_temp();
int new_label();
void gen_code(const char *op, const char *arg1, const char *arg2, const char *result);
void gen_code_with_label(int label, const char *op, const char *arg1, const char *arg2, const char *result);
void backpatch(int *labels, int count, int target);

/* 工具函数 */
void print_tree(TreeNode *node, int indent);
void print_code();
void print_errors();

/* ==================== 词法分析实现 ==================== */

/* 折半查找保留字 */
TokenType binary_search_keyword(const char *word) {
    int left = 0, right = keyword_count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(word, keyword_table[mid].word);
        if (cmp == 0) {
            return keyword_table[mid].type;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return TOKEN_ID;  /* 不是保留字，是标识符 */
}

/* 跳过空白字符和注释 */
void skip_whitespace_and_comments() {
    while (1) {
        /* 跳过空白字符 */
        while (current_line[line_pos] != '\0' && 
               isspace((unsigned char)current_line[line_pos])) {
            line_pos++;
        }
        
        /* 检查行尾 */
        if (current_line[line_pos] == '\0') {
            return;
        }
        
        /* 检查单行注释 // */
        if (current_line[line_pos] == '/' && current_line[line_pos + 1] == '/') {
            current_line[line_pos] = '\0';  /* 忽略该行剩余部分 */
            return;
        }
        
        /* 检查多行注释 /* */
        if (current_line[line_pos] == '/' && current_line[line_pos + 1] == '*') {
            line_pos += 2;
            bool comment_closed = false;
            
            while (!comment_closed) {
                /* 在当前行查找 */
                while (current_line[line_pos] != '\0') {
                    if (current_line[line_pos] == '*' && current_line[line_pos + 1] == '/') {
                        line_pos += 2;
                        comment_closed = true;
                        break;
                    }
                    line_pos++;
                }
                
                if (!comment_closed) {
                    /* 读取下一行 */
                    if (fgets(current_line, MAX_LINE_LEN, source_file) == NULL) {
                        report_lexical_error(current_line_num, "未闭合的注释");
                        return;
                    }
                    current_line_num++;
                    line_pos = 0;
                }
            }
            continue;  /* 继续检查是否有更多空白或注释 */
        }
        
        break;  /* 不是空白也不是注释 */
    }
}

/* 报告词法错误 */
void report_lexical_error(int line, const char *msg) {
    if (error_count < MAX_ERRORS) {
        errors[error_count].line = line;
        strcpy(errors[error_count].message, msg);
        error_count++;
    }
    has_error = true;
}

/* 获取下一个词法单元 */
void get_next_token() {
    skip_whitespace_and_comments();
    
    /* 检查是否需要读取新行 */
    while (current_line[line_pos] == '\0') {
        if (fgets(current_line, MAX_LINE_LEN, source_file) == NULL) {
            current_token.type = TOKEN_EOF;
            strcpy(current_token.text, "EOF");
            current_token.line = current_line_num;
            return;
        }
        current_line_num++;
        line_pos = 0;
        skip_whitespace_and_comments();
    }
    
    char ch = current_line[line_pos];
    current_token.line = current_line_num;
    
    /* 标识符或保留字 */
    if (isalpha((unsigned char)ch) || ch == '_') {
        int i = 0;
        while (isalnum((unsigned char)current_line[line_pos]) || current_line[line_pos] == '_') {
            if (i < MAX_TOKEN_LEN - 1) {
                current_token.text[i++] = current_line[line_pos];
            }
            line_pos++;
        }
        current_token.text[i] = '\0';
        current_token.type = binary_search_keyword(current_token.text);
        
        /* 特殊处理 TYPE */
        if (strcmp(current_token.text, "int") == 0) {
            current_token.type = TOKEN_TYPE;
        }
        
        return;
    }
    
    /* 数字 */
    if (isdigit((unsigned char)ch)) {
        int i = 0;
        current_token.value = 0;
        while (isdigit((unsigned char)current_line[line_pos])) {
            if (i < MAX_TOKEN_LEN - 1) {
                current_token.text[i++] = current_line[line_pos];
            }
            current_token.value = current_token.value * 10 + (current_line[line_pos] - '0');
            line_pos++;
        }
        current_token.text[i] = '\0';
        current_token.type = TOKEN_INT_CONST;
        return;
    }
    
    /* 运算符和分隔符 */
    line_pos++;
    switch (ch) {
        case '+': current_token.type = TOKEN_PLUS; strcpy(current_token.text, "+"); break;
        case '-': current_token.type = TOKEN_MINUS; strcpy(current_token.text, "-"); break;
        case '*': current_token.type = TOKEN_MUL; strcpy(current_token.text, "*"); break;
        case '/': current_token.type = TOKEN_DIV; strcpy(current_token.text, "/"); break;
        case '(': current_token.type = TOKEN_LP; strcpy(current_token.text, "("); break;
        case ')': current_token.type = TOKEN_RP; strcpy(current_token.text, ")"); break;
        case '{': current_token.type = TOKEN_LC; strcpy(current_token.text, "{"); break;
        case '}': current_token.type = TOKEN_RC; strcpy(current_token.text, "}"); break;
        case ';': current_token.type = TOKEN_SEMI; strcpy(current_token.text, ";"); break;
        case ':': current_token.type = TOKEN_COLON; strcpy(current_token.text, ":"); break;
        case '=':
            if (current_line[line_pos] == '=') {
                line_pos++;
                current_token.type = TOKEN_EQ;
                strcpy(current_token.text, "==");
            } else {
                current_token.type = TOKEN_ASSIGN;
                strcpy(current_token.text, "=");
            }
            break;
        case '<':
            if (current_line[line_pos] == '=') {
                line_pos++;
                current_token.type = TOKEN_LE;
                strcpy(current_token.text, "<=");
            } else {
                current_token.type = TOKEN_LT;
                strcpy(current_token.text, "<");
            }
            break;
        case '>':
            if (current_line[line_pos] == '=') {
                line_pos++;
                current_token.type = TOKEN_GE;
                strcpy(current_token.text, ">=");
            } else {
                current_token.type = TOKEN_GT;
                strcpy(current_token.text, ">");
            }
            break;
        case '!':
            if (current_line[line_pos] == '=') {
                line_pos++;
                current_token.type = TOKEN_NE;
                strcpy(current_token.text, "!=");
            } else {
                report_lexical_error(current_line_num, "非法字符 '!'");
                current_token.type = TOKEN_ERROR;
                current_token.text[0] = ch;
                current_token.text[1] = '\0';
            }
            break;
        default:
            /* 非法字符 */
            sprintf(current_token.text, "%c", ch);
            report_lexical_error(current_line_num, "非法单词");
            current_token.type = TOKEN_ERROR;
            break;
    }
}

/* ==================== 语法分析实现 ==================== */

/* 创建语法树节点 */
TreeNode* create_node(const char *name, int line, bool is_terminal) {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    strcpy(node->name, name);
    node->extra[0] = '\0';
    node->line = line;
    node->is_terminal = is_terminal;
    node->is_empty = false;
    node->child_count = 0;
    for (int i = 0; i < 10; i++) {
        node->children[i] = NULL;
    }
    return node;
}

/* 添加子节点 */
void add_child(TreeNode *parent, TreeNode *child) {
    if (parent->child_count < 10 && child != NULL) {
        parent->children[parent->child_count++] = child;
    }
}

/* 匹配期望的词法单元 */
void match(TokenType expected) {
    if (current_token.type == expected) {
        get_next_token();
    } else {
        char msg[MAX_TOKEN_LEN];
        switch (expected) {
            case TOKEN_ID: strcpy(msg, "缺少ID"); break;
            case TOKEN_LP: strcpy(msg, "缺少 '('"); break;
            case TOKEN_RP: strcpy(msg, "缺少 ')'"); break;
            case TOKEN_LC: strcpy(msg, "缺少 '{'"); break;
            case TOKEN_RC: strcpy(msg, "缺少 '}'"); break;
            case TOKEN_SEMI: strcpy(msg, "缺少 ';'"); break;
            case TOKEN_COLON: strcpy(msg, "缺少 ':'"); break;
            case TOKEN_ASSIGN: strcpy(msg, "缺少 '='"); break;
            default: sprintf(msg, "期望 %d", expected); break;
        }
        report_syntax_error(current_token.line, msg);
    }
}

/* 报告语法错误 */
void report_syntax_error(int line, const char *msg) {
    if (error_count < MAX_ERRORS) {
        errors[error_count].line = line;
        strcpy(errors[error_count].message, msg);
        error_count++;
    }
    has_error = true;
    /* 错误恢复：跳到下一个同步点 */
    while (current_token.type != TOKEN_SEMI && 
           current_token.type != TOKEN_RC && 
           current_token.type != TOKEN_EOF) {
        get_next_token();
    }
}

/* Program → main_declaration */
TreeNode* parse_program() {
    TreeNode *node = create_node("Program", current_token.line, false);
    TreeNode *main_decl = parse_main_declaration();
    add_child(node, main_decl);
    return node;
}

/* main_declaration → void main ( ) function_body */
TreeNode* parse_main_declaration() {
    int line = current_token.line;
    TreeNode *node = create_node("main_declaration", line, false);
    
    if (current_token.type == TOKEN_VOID) {
        get_next_token();
    } else {
        report_syntax_error(current_token.line, "期望 'void'");
    }
    
    if (current_token.type == TOKEN_MAIN) {
        TreeNode *main_id = create_node("ID", current_token.line, true);
        strcpy(main_id->extra, "main");
        add_child(node, main_id);
        get_next_token();
    } else {
        report_syntax_error(current_token.line, "期望 'main'");
    }
    
    match(TOKEN_LP);
    match(TOKEN_RP);
    
    TreeNode *body = parse_function_body();
    add_child(node, body);
    
    return node;
}

/* function_body → { declaration_list statement_list } */
TreeNode* parse_function_body() {
    int line = current_token.line;
    TreeNode *node = create_node("function_body", line, false);
    
    match(TOKEN_LC);
    
    /* 进入新的作用域 */
    current_scope++;
    int saved_offset = current_offset;
    
    TreeNode *decl_list = parse_declaration_list();
    if (decl_list != NULL && !decl_list->is_empty) {
        add_child(node, decl_list);
    }
    
    TreeNode *stmt_list = parse_statement_list();
    if (stmt_list != NULL && !stmt_list->is_empty) {
        add_child(node, stmt_list);
    }
    
    match(TOKEN_RC);
    
    /* 退出作用域 */
    current_scope--;
    current_offset = saved_offset;
    
    return node;
}

/* declaration_list → declaration_stat declaration_list | ε */
TreeNode* parse_declaration_list() {
    int line = current_token.line;
    TreeNode *node = create_node("declaration_list", line, false);
    
    if (current_token.type == TOKEN_VAR) {
        TreeNode *decl = parse_declaration_stat();
        add_child(node, decl);
        
        TreeNode *rest = parse_declaration_list();
        if (rest != NULL && !rest->is_empty) {
            for (int i = 0; i < rest->child_count; i++) {
                add_child(node, rest->children[i]);
            }
            free(rest);
        }
        return node;
    }
    
    /* 空产生式 */
    node->is_empty = true;
    return node;
}

/* declaration_stat → var ID : type ; */
TreeNode* parse_declaration_stat() {
    int line = current_token.line;
    TreeNode *node = create_node("declaration_stat", line, false);
    
    match(TOKEN_VAR);
    
    if (current_token.type == TOKEN_ID) {
        TreeNode *id_node = create_node("ID", current_token.line, true);
        strcpy(id_node->extra, current_token.text);
        add_child(node, id_node);
        
        /* 语义分析：添加到符号表 */
        add_symbol(current_token.text, "int");
        
        get_next_token();
    } else {
        report_syntax_error(current_token.line, "缺少ID");
    }
    
    match(TOKEN_COLON);
    
    TreeNode *type_node = parse_type();
    add_child(node, type_node);
    
    match(TOKEN_SEMI);
    
    return node;
}

/* type → int */
TreeNode* parse_type() {
    int line = current_token.line;
    TreeNode *node = create_node("TYPE", line, true);
    
    if (current_token.type == TOKEN_TYPE && strcmp(current_token.text, "int") == 0) {
        strcpy(node->extra, "int");
        get_next_token();
    } else {
        report_syntax_error(current_token.line, "期望类型 'int'");
    }
    
    return node;
}

/* statement_list → statement statement_list | ε */
TreeNode* parse_statement_list() {
    int line = current_token.line;
    TreeNode *node = create_node("statement_list", line, false);
    
    if (current_token.type == TOKEN_ID ||
        current_token.type == TOKEN_IF ||
        current_token.type == TOKEN_WHILE ||
        current_token.type == TOKEN_RETURN ||
        current_token.type == TOKEN_BREAK ||
        current_token.type == TOKEN_CONTINUE) {
        
        TreeNode *stmt = parse_statement();
        add_child(node, stmt);
        
        TreeNode *rest = parse_statement_list();
        if (rest != NULL && !rest->is_empty) {
            for (int i = 0; i < rest->child_count; i++) {
                add_child(node, rest->children[i]);
            }
            free(rest);
        }
        return node;
    }
    
    /* 空产生式 */
    node->is_empty = true;
    return node;
}

/* statement → assignment_stat | if_stat | while_stat | return_stat | break ; | continue ; | { statement_list } */
TreeNode* parse_statement() {
    int line = current_token.line;
    TreeNode *node = create_node("statement", line, false);
    
    switch (current_token.type) {
        case TOKEN_LC:
            /* 复合语句 */
            match(TOKEN_LC);
            {
                TreeNode *stmt_list = parse_statement_list();
                if (stmt_list != NULL && !stmt_list->is_empty) {
                    add_child(node, stmt_list);
                }
            }
            match(TOKEN_RC);
            break;
        case TOKEN_ID:
            add_child(node, parse_assignment_stat());
            break;
        case TOKEN_IF:
            add_child(node, parse_if_stat());
            break;
        case TOKEN_WHILE:
            add_child(node, parse_while_stat());
            break;
        case TOKEN_RETURN:
            add_child(node, parse_return_stat());
            break;
        case TOKEN_BREAK:
            get_next_token();
            match(TOKEN_SEMI);
            break;
        case TOKEN_CONTINUE:
            get_next_token();
            match(TOKEN_SEMI);
            break;
        default:
            report_syntax_error(current_token.line, "非法语句");
            break;
    }
    
    return node;
}

/* assignment_stat → ID = expression ; */
TreeNode* parse_assignment_stat() {
    int line = current_token.line;
    TreeNode *node = create_node("assignment_stat", line, false);
    
    if (current_token.type == TOKEN_ID) {
        /* 语义检查：检查变量是否已定义 */
        check_undefined_id(current_token.text, current_token.line);
        
        TreeNode *id_node = create_node("ID", current_token.line, true);
        strcpy(id_node->extra, current_token.text);
        add_child(node, id_node);
        
        char var_name[MAX_TOKEN_LEN];
        strcpy(var_name, current_token.text);
        
        get_next_token();
        match(TOKEN_ASSIGN);
        
        TreeNode *expr = parse_expression();
        add_child(node, expr);
        
        match(TOKEN_SEMI);
        
        /* 生成中间代码 */
        gen_code("=", expr->extra, "", var_name);
    } else {
        report_syntax_error(current_token.line, "赋值语句需要标识符");
    }
    
    return node;
}

/* if_stat → if ( condition ) statement else_stat */
TreeNode* parse_if_stat() {
    int line = current_token.line;
    TreeNode *node = create_node("if_stat", line, false);
    
    match(TOKEN_IF);
    match(TOKEN_LP);
    
    TreeNode *cond = parse_condition();
    add_child(node, cond);
    
    match(TOKEN_RP);
    
    /* 生成条件跳转代码 */
    int label_else = new_label();
    int label_end = new_label();
    gen_code_with_label(label_else, "if_false", cond->extra, "goto", "");
    
    TreeNode *then_stmt = parse_statement();
    add_child(node, then_stmt);
    
    gen_code_with_label(label_end, "goto", "", "", "");
    gen_code_with_label(label_else, "label", "", "", "");
    
    if (current_token.type == TOKEN_ELSE) {
        get_next_token();
        TreeNode *else_stmt = parse_statement();
        add_child(node, else_stmt);
    }
    
    gen_code_with_label(label_end, "label", "", "", "");
    
    return node;
}

/* while_stat → while ( condition ) statement */
TreeNode* parse_while_stat() {
    int line = current_token.line;
    TreeNode *node = create_node("while_stat", line, false);
    
    int label_start = new_label();
    int label_end = new_label();
    
    gen_code_with_label(label_start, "label", "", "", "");
    
    match(TOKEN_WHILE);
    match(TOKEN_LP);
    
    TreeNode *cond = parse_condition();
    add_child(node, cond);
    
    gen_code_with_label(label_end, "if_false", cond->extra, "goto", "");
    
    match(TOKEN_RP);
    
    TreeNode *body = parse_statement();
    add_child(node, body);
    
    gen_code_with_label(label_start, "goto", "", "", "");
    gen_code_with_label(label_end, "label", "", "", "");
    
    return node;
}

/* return_stat → return expression ; */
TreeNode* parse_return_stat() {
    int line = current_token.line;
    TreeNode *node = create_node("return_stat", line, false);
    
    match(TOKEN_RETURN);
    
    if (current_token.type != TOKEN_SEMI) {
        TreeNode *expr = parse_expression();
        add_child(node, expr);
        gen_code("return", expr->extra, "", "");
    } else {
        gen_code("return", "", "", "");
    }
    
    match(TOKEN_SEMI);
    
    return node;
}

/* condition → expression relop expression */
TreeNode* parse_condition() {
    int line = current_token.line;
    TreeNode *node = create_node("condition", line, false);
    
    TreeNode *left = parse_expression();
    add_child(node, left);
    
    char op[10];
    if (current_token.type == TOKEN_LT || current_token.type == TOKEN_GT ||
        current_token.type == TOKEN_LE || current_token.type == TOKEN_GE ||
        current_token.type == TOKEN_EQ || current_token.type == TOKEN_NE) {
        strcpy(op, current_token.text);
        get_next_token();
    } else {
        report_syntax_error(current_token.line, "期望关系运算符");
        strcpy(op, "<");
    }
    
    TreeNode *right = parse_expression();
    add_child(node, right);
    
    /* 生成临时变量存储条件结果 */
    char *temp = new_temp();
    strcpy(node->extra, temp);
    gen_code(op, left->extra, right->extra, temp);
    
    return node;
}

/* expression → term expression' */
TreeNode* parse_expression() {
    int line = current_token.line;
    TreeNode *node = create_node("expression", line, false);
    
    TreeNode *term = parse_term();
    add_child(node, term);
    
    /* 处理 + 或 - */
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        char op[10];
        strcpy(op, current_token.text);
        get_next_token();
        
        TreeNode *next_term = parse_term();
        add_child(node, next_term);
        
        /* 生成中间代码 */
        char *temp = new_temp();
        gen_code(op, term->extra, next_term->extra, temp);
        strcpy(term->extra, temp);
    }
    
    strcpy(node->extra, term->extra);
    return node;
}

/* term → factor term' */
TreeNode* parse_term() {
    int line = current_token.line;
    TreeNode *node = create_node("term", line, false);
    
    TreeNode *factor = parse_factor();
    add_child(node, factor);
    
    /* 处理 * 或 / */
    while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV) {
        char op[10];
        strcpy(op, current_token.text);
        get_next_token();
        
        TreeNode *next_factor = parse_factor();
        add_child(node, next_factor);
        
        /* 生成中间代码 */
        char *temp = new_temp();
        gen_code(op, factor->extra, next_factor->extra, temp);
        strcpy(factor->extra, temp);
    }
    
    strcpy(node->extra, factor->extra);
    return node;
}

/* factor → ID | INT | ( expression ) */
TreeNode* parse_factor() {
    int line = current_token.line;
    TreeNode *node = create_node("factor", line, false);
    
    if (current_token.type == TOKEN_ID) {
        /* 语义检查 */
        check_undefined_id(current_token.text, current_token.line);
        
        TreeNode *id_node = create_node("ID", current_token.line, true);
        strcpy(id_node->extra, current_token.text);
        add_child(node, id_node);
        
        strcpy(node->extra, current_token.text);
        get_next_token();
    } else if (current_token.type == TOKEN_INT_CONST) {
        TreeNode *int_node = create_node("INT", current_token.line, true);
        sprintf(int_node->extra, "%d", current_token.value);
        add_child(node, int_node);
        
        sprintf(node->extra, "%d", current_token.value);
        get_next_token();
    } else if (current_token.type == TOKEN_LP) {
        get_next_token();
        TreeNode *expr = parse_expression();
        add_child(node, expr);
        strcpy(node->extra, expr->extra);
        match(TOKEN_RP);
    } else {
        report_syntax_error(current_token.line, "期望标识符、整数或表达式");
        strcpy(node->extra, "0");
    }
    
    return node;
}

/* ==================== 语义分析实现 ==================== */

/* 查找符号 */
bool lookup_symbol(const char *name) {
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (strcmp(symbol_table[i].name, name) == 0 &&
            symbol_table[i].scope <= current_scope) {
            return true;
        }
    }
    return false;
}

/* 添加符号 */
void add_symbol(const char *name, const char *type) {
    /* 检查重复定义 */
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (strcmp(symbol_table[i].name, name) == 0 &&
            symbol_table[i].scope == current_scope) {
            /* 重复定义错误 */
            if (error_count < MAX_ERRORS) {
                errors[error_count].line = current_token.line;
                sprintf(errors[error_count].message, "变量 '%s' 重复定义", name);
                error_count++;
            }
            has_error = true;
            return;
        }
    }
    
    if (symbol_count < MAX_SYMBOLS) {
        strcpy(symbol_table[symbol_count].name, name);
        strcpy(symbol_table[symbol_count].type, type);
        symbol_table[symbol_count].scope = current_scope;
        symbol_table[symbol_count].offset = current_offset;
        current_offset += 4;  /* int 占4字节 */
        symbol_count++;
    }
}

/* 检查未定义标识符 */
void check_undefined_id(const char *name, int line) {
    if (!lookup_symbol(name)) {
        if (error_count < MAX_ERRORS) {
            errors[error_count].line = line;
            sprintf(errors[error_count].message, "变量 '%s' 未定义", name);
            error_count++;
        }
        has_error = true;
    }
}

/* 类型兼容性检查 */
void check_type_compatibility(const char *type1, const char *type2, int line) {
    if (strcmp(type1, type2) != 0) {
        if (error_count < MAX_ERRORS) {
            errors[error_count].line = line;
            sprintf(errors[error_count].message, "类型不兼容: %s 和 %s", type1, type2);
            error_count++;
        }
        has_error = true;
    }
}

/* ==================== 中间代码生成实现 ==================== */

/* 生成新临时变量 */
char* new_temp() {
    static char temp[16];
    sprintf(temp, "t%d", temp_count++);
    return strdup(temp);
}

/* 生成新标号 */
int new_label() {
    return label_count++;
}

/* 生成四元式 */
void gen_code(const char *op, const char *arg1, const char *arg2, const char *result) {
    if (code_count < MAX_CODE_LEN) {
        code[code_count].label = -1;
        strcpy(code[code_count].op, op);
        strcpy(code[code_count].arg1, arg1);
        strcpy(code[code_count].arg2, arg2);
        strcpy(code[code_count].result, result);
        code_count++;
    }
}

/* 生成带标号的四元式 */
void gen_code_with_label(int label, const char *op, const char *arg1, const char *arg2, const char *result) {
    if (code_count < MAX_CODE_LEN) {
        code[code_count].label = label;
        strcpy(code[code_count].op, op);
        strcpy(code[code_count].arg1, arg1);
        strcpy(code[code_count].arg2, arg2);
        strcpy(code[code_count].result, result);
        code_count++;
    }
}

/* ==================== 输出函数 ==================== */

/* 打印语法树 */
void print_tree(TreeNode *node, int indent) {
    if (node == NULL || node->is_empty) return;
    
    /* 打印缩进 */
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    /* 打印节点信息 */
    if (node->is_terminal) {
        /* 词法单元 */
        if (strlen(node->extra) > 0) {
            printf("%s: %s\n", node->name, node->extra);
        } else {
            printf("%s\n", node->name);
        }
    } else {
        /* 语法单元 */
        printf("%s (%d)\n", node->name, node->line);
    }
    
    /* 递归打印子节点 */
    for (int i = 0; i < node->child_count; i++) {
        print_tree(node->children[i], indent + 1);
    }
}

/* 打印中间代码 */
void print_code() {
    printf("\n========== 中间代码 ==========\n");
    for (int i = 0; i < code_count; i++) {
        if (code[i].label >= 0) {
            printf("L%d: ", code[i].label);
        } else {
            printf("    ");
        }
        
        if (strcmp(code[i].op, "=") == 0) {
            printf("%s = %s\n", code[i].result, code[i].arg1);
        } else if (strcmp(code[i].op, "+") == 0 || strcmp(code[i].op, "-") == 0 ||
                   strcmp(code[i].op, "*") == 0 || strcmp(code[i].op, "/") == 0) {
            printf("%s = %s %s %s\n", code[i].result, code[i].arg1, code[i].op, code[i].arg2);
        } else if (strcmp(code[i].op, "if_false") == 0) {
            printf("if_false %s goto L%s\n", code[i].arg1, code[i].result);
        } else if (strcmp(code[i].op, "goto") == 0) {
            printf("goto L%s\n", code[i].result);
        } else if (strcmp(code[i].op, "label") == 0) {
            printf("\n");
        } else if (strcmp(code[i].op, "return") == 0) {
            if (strlen(code[i].arg1) > 0) {
                printf("return %s\n", code[i].arg1);
            } else {
                printf("return\n");
            }
        } else {
            printf("%s %s %s %s\n", code[i].op, code[i].arg1, code[i].arg2, code[i].result);
        }
    }
}

/* 打印错误 */
void print_errors() {
    if (error_count > 0) {
        printf("\n========== 错误信息 ==========\n");
        for (int i = 0; i < error_count; i++) {
            printf("第%d行: %s\n", errors[i].line, errors[i].message);
        }
    }
}

/* 打印符号表 */
void print_symbol_table() {
    printf("\n========== 符号表 ==========\n");
    printf("%-15s %-10s %-8s %-8s\n", "名称", "类型", "作用域", "偏移量");
    printf("----------------------------------------\n");
    for (int i = 0; i < symbol_count; i++) {
        printf("%-15s %-10s %-8d %-8d\n",
               symbol_table[i].name,
               symbol_table[i].type,
               symbol_table[i].scope,
               symbol_table[i].offset);
    }
}

/* ==================== 主函数 ==================== */
int main(int argc, char *argv[]) {
    /* 打开源文件 */
    const char *filename;
    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "test.txt";
    }
    
    source_file = fopen(filename, "r");
    if (source_file == NULL) {
        printf("无法打开文件: %s\n", filename);
        printf("使用示例代码进行测试...\n\n");
        
        /* 创建测试文件 */
        FILE *test = fopen("test.txt", "w");
        fprintf(test, "void main()\n");
        fprintf(test, "{\n");
        fprintf(test, "    var i:int;\n");
        fprintf(test, "    var j:int;\n");
        fprintf(test, "    i = 10;\n");
        fprintf(test, "    j = 20;\n");
        fprintf(test, "    if (i < j) {\n");
        fprintf(test, "        i = i + 1;\n");
        fprintf(test, "    } else {\n");
        fprintf(test, "        j = j - 1;\n");
        fprintf(test, "    }\n");
        fprintf(test, "    while (i < 100) {\n");
        fprintf(test, "        i = i * 2;\n");
        fprintf(test, "    }\n");
        fprintf(test, "    return i;\n");
        fprintf(test, "}\n");
        fclose(test);
        
        source_file = fopen("test.txt", "r");
    }
    
    /* 初始化 */
    current_line[0] = '\0';
    line_pos = 0;
    current_line_num = 0;
    
    /* 获取第一个词法单元 */
    get_next_token();
    
    printf("========== 词法分析结果 ==========\n");
    printf("%-20s %-15s %s\n", "词法单元", "类型", "行号");
    printf("-------------------------------------------\n");
    
    /* 保存词法单元用于显示 */
    Token tokens[100];
    int token_count = 0;
    
    /* 词法分析并保存所有token */
    while (current_token.type != TOKEN_EOF) {
        if (token_count < 100) {
            tokens[token_count++] = current_token;
        }
        get_next_token();
    }
    
    /* 打印词法分析结果 */
    for (int i = 0; i < token_count; i++) {
        const char *type_name;
        switch (tokens[i].type) {
            case TOKEN_VOID: type_name = "VOID"; break;
            case TOKEN_MAIN: type_name = "MAIN"; break;
            case TOKEN_VAR: type_name = "VAR"; break;
            case TOKEN_INT: type_name = "INT"; break;
            case TOKEN_IF: type_name = "IF"; break;
            case TOKEN_ELSE: type_name = "ELSE"; break;
            case TOKEN_WHILE: type_name = "WHILE"; break;
            case TOKEN_RETURN: type_name = "RETURN"; break;
            case TOKEN_BREAK: type_name = "BREAK"; break;
            case TOKEN_CONTINUE: type_name = "CONTINUE"; break;
            case TOKEN_ID: type_name = "ID"; break;
            case TOKEN_TYPE: type_name = "TYPE"; break;
            case TOKEN_INT_CONST: type_name = "INT_CONST"; break;
            case TOKEN_ASSIGN: type_name = "ASSIGN"; break;
            case TOKEN_PLUS: type_name = "PLUS"; break;
            case TOKEN_MINUS: type_name = "MINUS"; break;
            case TOKEN_MUL: type_name = "MUL"; break;
            case TOKEN_DIV: type_name = "DIV"; break;
            case TOKEN_LT: type_name = "LT"; break;
            case TOKEN_GT: type_name = "GT"; break;
            case TOKEN_LE: type_name = "LE"; break;
            case TOKEN_GE: type_name = "GE"; break;
            case TOKEN_EQ: type_name = "EQ"; break;
            case TOKEN_NE: type_name = "NE"; break;
            case TOKEN_LP: type_name = "LP"; break;
            case TOKEN_RP: type_name = "RP"; break;
            case TOKEN_LC: type_name = "LC"; break;
            case TOKEN_RC: type_name = "RC"; break;
            case TOKEN_SEMI: type_name = "SEMI"; break;
            case TOKEN_COLON: type_name = "COLON"; break;
            default: type_name = "UNKNOWN"; break;
        }
        printf("%-20s %-15s %d\n", tokens[i].text, type_name, tokens[i].line);
    }
    
    /* 重新打开文件进行语法分析 */
    fclose(source_file);
    source_file = fopen(filename, "r");
    if (source_file == NULL) {
        source_file = fopen("test.txt", "r");
    }
    
    /* 重置状态 */
    current_line[0] = '\0';
    line_pos = 0;
    current_line_num = 0;
    error_count = 0;
    has_error = false;
    symbol_count = 0;
    code_count = 0;
    temp_count = 0;
    label_count = 0;
    current_scope = 0;
    current_offset = 0;
    
    /* 获取第一个词法单元 */
    get_next_token();
    
    /* 语法分析 */
    printf("\n========== 语法分析结果 ==========\n");
    root = parse_program();
    
    /* 打印语法树 */
    printf("\n========== 语法树 ==========\n");
    print_tree(root, 0);
    
    /* 打印符号表 */
    print_symbol_table();
    
    /* 打印中间代码 */
    print_code();
    
    /* 打印错误 */
    print_errors();
    
    /* 总结 */
    printf("\n========== 分析总结 ==========\n");
    printf("词法单元数: %d\n", token_count);
    printf("错误数: %d\n", error_count);
    printf("符号数: %d\n", symbol_count);
    printf("中间代码条数: %d\n", code_count);
    
    if (has_error) {
        printf("\n分析完成，发现 %d 个错误。\n", error_count);
    } else {
        printf("\n分析成功完成，未发现错误。\n");
    }
    
    /* 清理 */
    fclose(source_file);
    
    return 0;
}
