#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LEN 100
#define MAX_ERRORS 100
#define MAX_SYMBOLS 100
#define MAX_QUADS 100

typedef enum {
    TOKEN_VOID, TOKEN_INT, TOKEN_VAR, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE,
    TOKEN_RETURN, TOKEN_ID, TOKEN_NUM, TOKEN_ASSIGN, TOKEN_SEMI, TOKEN_COMMA,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LT, TOKEN_GT,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV, TOKEN_ILLEGAL, TOKEN_EOF,
    TOKEN_COLON, TOKEN_EQ, TOKEN_NE, TOKEN_LE, TOKEN_GE
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_TOKEN_LEN];
    int line;
    int value;
} Token;

typedef struct {
    char message[200];
    int line;
} Error;

typedef struct Symbol {
    char name[MAX_TOKEN_LEN];
    char type[10];
    int scope;
} Symbol;

typedef struct Quad {
    char op[10];
    char arg1[50];
    char arg2[50];
    char result[50];
} Quad;

typedef struct ASTNode {
    char name[50];
    int line;
    char value[100];
    struct ASTNode *children[10];
    int child_count;
} ASTNode;

Token current_token;
char *src;
int pos = 0;
int line = 1;
Error errors[MAX_ERRORS];
int error_count = 0;
Symbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;
Quad quads[MAX_QUADS];
int quad_count = 0;
int temp_count = 0;

char *keywords[] = {"else", "if", "int", "return", "var", "void", "while"};
TokenType keyword_types[] = {TOKEN_ELSE, TOKEN_IF, TOKEN_INT, TOKEN_RETURN, TOKEN_VAR, TOKEN_VOID, TOKEN_WHILE};
int keyword_count = 7;

void add_error(int l, const char *msg) {
    if (error_count < MAX_ERRORS) {
        errors[error_count].line = l;
        snprintf(errors[error_count].message, 200, "%s", msg);
        error_count++;
    }
}

int binary_search_keyword(const char *str) {
    int low = 0, high = keyword_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = strcmp(str, keywords[mid]);
        if (cmp == 0) return mid;
        else if (cmp < 0) high = mid - 1;
        else low = mid + 1;
    }
    return -1;
}

void skip_whitespace() {
    while (src[pos] && isspace(src[pos])) {
        if (src[pos] == '\n') line++;
        pos++;
    }
}

void skip_comment() {
    if (src[pos] == '/' && src[pos+1] == '*') {
        pos += 2;
        while (src[pos]) {
            if (src[pos] == '*' && src[pos+1] == '/') {
                pos += 2;
                return;
            }
            if (src[pos] == '\n') line++;
            pos++;
        }
        add_error(line, "未闭合的注释");
    }
}

Token next_token() {
    Token t;
    t.line = line;
    skip_whitespace();
    while (src[pos] == '/' && src[pos+1] == '*') {
        skip_comment();
        skip_whitespace();
    }
    if (!src[pos]) {
        t.type = TOKEN_EOF;
        strcpy(t.lexeme, "EOF");
        return t;
    }
    if (isalpha(src[pos])) {
        int len = 0;
        while (src[pos] && (isalnum(src[pos]) || src[pos] == '_')) {
            t.lexeme[len++] = src[pos++];
        }
        t.lexeme[len] = '\0';
        int idx = binary_search_keyword(t.lexeme);
        if (idx != -1) {
            t.type = keyword_types[idx];
        } else {
            t.type = TOKEN_ID;
        }
        return t;
    }
    if (isdigit(src[pos])) {
        int len = 0;
        t.value = 0;
        while (src[pos] && isdigit(src[pos])) {
            t.value = t.value * 10 + (src[pos] - '0');
            t.lexeme[len++] = src[pos++];
        }
        t.lexeme[len] = '\0';
        t.type = TOKEN_NUM;
        return t;
    }
    switch (src[pos]) {
        case '=':
            pos++;
            if (src[pos] == '=') {
                pos++;
                t.type = TOKEN_EQ;
                strcpy(t.lexeme, "==");
            } else {
                t.type = TOKEN_ASSIGN;
                strcpy(t.lexeme, "=");
            }
            return t;
        case '!':
            pos++;
            if (src[pos] == '=') {
                pos++;
                t.type = TOKEN_NE;
                strcpy(t.lexeme, "!=");
            } else {
                t.type = TOKEN_ILLEGAL;
                strcpy(t.lexeme, "!");
            }
            return t;
        case '<':
            pos++;
            if (src[pos] == '=') {
                pos++;
                t.type = TOKEN_LE;
                strcpy(t.lexeme, "<=");
            } else {
                t.type = TOKEN_LT;
                strcpy(t.lexeme, "<");
            }
            return t;
        case '>':
            pos++;
            if (src[pos] == '=') {
                pos++;
                t.type = TOKEN_GE;
                strcpy(t.lexeme, ">=");
            } else {
                t.type = TOKEN_GT;
                strcpy(t.lexeme, ">");
            }
            return t;
        case '+': t.type = TOKEN_PLUS; strcpy(t.lexeme, "+"); pos++; return t;
        case '-': t.type = TOKEN_MINUS; strcpy(t.lexeme, "-"); pos++; return t;
        case '*': t.type = TOKEN_MUL; strcpy(t.lexeme, "*"); pos++; return t;
        case '/': t.type = TOKEN_DIV; strcpy(t.lexeme, "/"); pos++; return t;
        case ';': t.type = TOKEN_SEMI; strcpy(t.lexeme, ";"); pos++; return t;
        case ',': t.type = TOKEN_COMMA; strcpy(t.lexeme, ","); pos++; return t;
        case '(': t.type = TOKEN_LPAREN; strcpy(t.lexeme, "("); pos++; return t;
        case ')': t.type = TOKEN_RPAREN; strcpy(t.lexeme, ")"); pos++; return t;
        case '{': t.type = TOKEN_LBRACE; strcpy(t.lexeme, "{"); pos++; return t;
        case '}': t.type = TOKEN_RBRACE; strcpy(t.lexeme, "}"); pos++; return t;
        case ':': t.type = TOKEN_COLON; strcpy(t.lexeme, ":"); pos++; return t;
        default:
            t.type = TOKEN_ILLEGAL;
            t.lexeme[0] = src[pos];
            t.lexeme[1] = '\0';
            pos++;
            char msg[100];
            snprintf(msg, 100, "非法单词 \"%s\"", t.lexeme);
            add_error(line, msg);
            return t;
    }
}

ASTNode *create_node(const char *name, int line, const char *value) {
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    strcpy(node->name, name);
    node->line = line;
    strcpy(node->value, value ? value : "");
    node->child_count = 0;
    return node;
}

void add_child(ASTNode *parent, ASTNode *child) {
    if (parent && child && parent->child_count < 10) {
        parent->children[parent->child_count++] = child;
    }
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf(" ");
    if (strcmp(node->name, "ID") == 0) {
        printf("%s: %s\n", node->name, node->value);
    } else if (strcmp(node->name, "TYPE") == 0) {
        printf("%s: %s\n", node->name, node->value);
    } else if (strcmp(node->name, "INT") == 0) {
        printf("%s: %s\n", node->name, node->value);
    } else if (node->line > 0) {
        printf("%s (%d)\n", node->name, node->line);
    } else {
        printf("%s\n", node->name);
    }
    for (int i = 0; i < node->child_count; i++) {
        print_ast(node->children[i], indent + 2);
    }
}

void add_symbol(const char *name, const char *type) {
    if (symbol_count < MAX_SYMBOLS) {
        strcpy(symbol_table[symbol_count].name, name);
        strcpy(symbol_table[symbol_count].type, type);
        symbol_table[symbol_count].scope = 0;
        symbol_count++;
    }
}

Symbol *find_symbol(const char *name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return &symbol_table[i];
        }
    }
    return NULL;
}

void new_temp(char *result) {
    snprintf(result, 50, "t%d", temp_count++);
}

void gen_quad(const char *op, const char *arg1, const char *arg2, const char *result) {
    if (quad_count < MAX_QUADS) {
        strcpy(quads[quad_count].op, op);
        strcpy(quads[quad_count].arg1, arg1 ? arg1 : "");
        strcpy(quads[quad_count].arg2, arg2 ? arg2 : "");
        strcpy(quads[quad_count].result, result ? result : "");
        quad_count++;
    }
}

void print_quads() {
    printf("\n===== 中间代码（四元式） =====\n");
    for (int i = 0; i < quad_count; i++) {
        printf("(%d) (%s, %s, %s, %s)\n", i, quads[i].op, quads[i].arg1, quads[i].arg2, quads[i].result);
    }
}

void match(TokenType expected) {
    if (current_token.type == expected) {
        current_token = next_token();
    } else {
        char expected_str[50], got_str[50];
        sprintf(expected_str, "%d", expected);
        sprintf(got_str, "%s", current_token.lexeme);
        add_error(current_token.line, "语法错误");
    }
}

ASTNode *parse_factor();
ASTNode *parse_term();
ASTNode *parse_expression();

ASTNode *parse_factor() {
    ASTNode *node = NULL;
    if (current_token.type == TOKEN_ID) {
        node = create_node("ID", 0, current_token.lexeme);
        match(TOKEN_ID);
    } else if (current_token.type == TOKEN_NUM) {
        char val[20];
        sprintf(val, "%d", current_token.value);
        node = create_node("INT", 0, val);
        match(TOKEN_NUM);
    } else if (current_token.type == TOKEN_LPAREN) {
        match(TOKEN_LPAREN);
        node = parse_expression();
        match(TOKEN_RPAREN);
    }
    return node;
}

ASTNode *parse_term() {
    ASTNode *node = parse_factor();
    while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV) {
        Token op = current_token;
        match(current_token.type);
        ASTNode *new_node = create_node(op.lexeme, op.line, NULL);
        add_child(new_node, node);
        add_child(new_node, parse_factor());
        node = new_node;
    }
    return node;
}

ASTNode *parse_expression() {
    ASTNode *node = parse_term();
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        Token op = current_token;
        match(current_token.type);
        ASTNode *new_node = create_node(op.lexeme, op.line, NULL);
        add_child(new_node, node);
        add_child(new_node, parse_term());
        node = new_node;
    }
    return node;
}

ASTNode *parse_statement();
ASTNode *parse_declaration() {
    ASTNode *node = create_node("declaration_stat", current_token.line, NULL);
    if (current_token.type == TOKEN_VAR) {
        match(TOKEN_VAR);
        if (current_token.type == TOKEN_ID) {
            add_child(node, create_node("ID", 0, current_token.lexeme));
            add_symbol(current_token.lexeme, "int");
            match(TOKEN_ID);
            if (current_token.type == TOKEN_COLON) {
                match(TOKEN_COLON);
                if (current_token.type == TOKEN_INT) {
                    add_child(node, create_node("TYPE", 0, "int"));
                    match(TOKEN_INT);
                } else {
                    add_error(current_token.line, "缺少类型说明符int");
                }
            }
            match(TOKEN_SEMI);
        } else if (current_token.type == TOKEN_COLON) {
            add_error(current_token.line, "缺少ID");
            match(TOKEN_COLON);
            match(TOKEN_INT);
            match(TOKEN_SEMI);
        } else {
            add_error(current_token.line, "语法错误在声明中");
        }
    } else if (current_token.type == TOKEN_INT) {
        add_child(node, create_node("TYPE", 0, "int"));
        match(TOKEN_INT);
        if (current_token.type == TOKEN_ID) {
            add_child(node, create_node("ID", 0, current_token.lexeme));
            add_symbol(current_token.lexeme, "int");
            match(TOKEN_ID);
            match(TOKEN_SEMI);
        } else {
            add_error(current_token.line, "缺少变量名ID");
        }
    }
    return node;
}

ASTNode *parse_assign_stat() {
    ASTNode *node = create_node("assign_stat", current_token.line, NULL);
    if (current_token.type == TOKEN_ID) {
        add_child(node, create_node("ID", 0, current_token.lexeme));
        char id_name[100];
        strcpy(id_name, current_token.lexeme);
        match(TOKEN_ID);
        match(TOKEN_ASSIGN);
        ASTNode *expr = parse_expression();
        add_child(node, expr);
        match(TOKEN_SEMI);
        gen_quad(":=", id_name, NULL, id_name);
    } else {
        add_error(current_token.line, "赋值语句需要ID");
    }
    return node;
}

ASTNode *parse_statement() {
    switch (current_token.type) {
        case TOKEN_VAR:
        case TOKEN_INT:
            return parse_declaration();
        case TOKEN_ID:
            return parse_assign_stat();
        default:
            add_error(current_token.line, "未知的语句类型");
            current_token = next_token();
            return NULL;
    }
}

ASTNode *parse_function_body() {
    ASTNode *node = create_node("function_body", current_token.line, NULL);
    match(TOKEN_LBRACE);
    ASTNode *decl_list = create_node("declaration_list", current_token.line, NULL);
    while (current_token.type == TOKEN_VAR || current_token.type == TOKEN_INT || current_token.type == TOKEN_ID) {
        ASTNode *stmt = parse_statement();
        if (stmt) add_child(decl_list, stmt);
    }
    if (decl_list->child_count > 0) add_child(node, decl_list);
    match(TOKEN_RBRACE);
    return node;
}

ASTNode *parse_main_declaration() {
    ASTNode *node = create_node("main_declaration", current_token.line, NULL);
    if (current_token.type == TOKEN_VOID) {
        match(TOKEN_VOID);
    }
    if (current_token.type == TOKEN_ID && strcmp(current_token.lexeme, "main") == 0) {
        add_child(node, create_node("ID", 0, "main"));
        match(TOKEN_ID);
        match(TOKEN_LPAREN);
        match(TOKEN_RPAREN);
        add_child(node, parse_function_body());
    } else {
        add_error(current_token.line, "期望main函数");
    }
    return node;
}

ASTNode *parse_program() {
    ASTNode *node = create_node("Program", current_token.line, NULL);
    add_child(node, parse_main_declaration());
    if (current_token.type != TOKEN_EOF) {
        add_error(current_token.line, "程序末尾有多余字符");
    }
    return node;
}

void semantic_analysis(ASTNode *node) {
    if (!node) return;
    if (strcmp(node->name, "ID") == 0) {
        if (!find_symbol(node->value)) {
            char msg[100];
            snprintf(msg, 100, "未定义的标识符: %s", node->value);
            add_error(node->line, msg);
        }
    }
    for (int i = 0; i < node->child_count; i++) {
        semantic_analysis(node->children[i]);
    }
}

void generate_code(ASTNode *node) {
    if (!node) return;
    if (strcmp(node->name, "assign_stat") == 0) {
        generate_code(node->children[1]);
    }
    for (int i = 0; i < node->child_count; i++) {
        generate_code(node->children[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("用法: %s <输入文件>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("无法打开文件");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    src = (char*)malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);
    current_token = next_token();
    printf("===== 词法分析结果 =====\n");
    Token t;
    int save_pos = pos, save_line = line;
    Token save_token = current_token;
    while (1) {
        t = next_token();
        if (t.type == TOKEN_EOF) break;
        printf("行%d: 类型=%d, 词素=%s", t.line, t.type, t.lexeme);
        if (t.type == TOKEN_NUM) printf(", 值=%d", t.value);
        printf("\n");
    }
    pos = save_pos;
    line = save_line;
    current_token = save_token;
    printf("\n===== 语法分析 =====\n");
    ASTNode *root = parse_program();
    if (error_count > 0) {
        printf("\n===== 错误信息 =====\n");
        for (int i = 0; i < error_count; i++) {
            printf("第%d行: %s\n", errors[i].line, errors[i].message);
        }
    } else {
        printf("\n===== 语法树 =====\n");
        print_ast(root, 0);
        semantic_analysis(root);
        if (error_count > 0) {
            printf("\n===== 语义错误 =====\n");
            for (int i = 0; i < error_count; i++) {
                printf("第%d行: %s\n", errors[i].line, errors[i].message);
            }
        } else {
            generate_code(root);
            print_quads();
        }
    }
    free(src);
    return 0;
}
