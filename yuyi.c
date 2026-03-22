#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LEN 100
#define MAX_SYMBOLS 1000
#define MAX_ERRORS 100
#define MAX_CODE_LINES 1000

typedef enum {
    TOKEN_ID, TOKEN_INT, TOKEN_TYPE, TOKEN_VOID, TOKEN_VAR,
    TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN,
    TOKEN_ASSIGN, TOKEN_EQ, TOKEN_NE, TOKEN_LT, TOKEN_LE, TOKEN_GT, TOKEN_GE,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV,
    TOKEN_LP, TOKEN_RP, TOKEN_LC, TOKEN_RC, TOKEN_SEMI, TOKEN_COMMA, TOKEN_COLON,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LEN];
    int line;
    int int_value;
} Token;

typedef struct TreeNode {
    char name[50];
    int line;
    int is_terminal;
    char extra_info[100];
    struct TreeNode *children[20];
    int child_count;
} TreeNode;

typedef struct {
    char name[MAX_TOKEN_LEN];
    char type[20];
    int scope_level;
    int line_declared;
} Symbol;

typedef struct {
    char op[10];
    char arg1[MAX_TOKEN_LEN];
    char arg2[MAX_TOKEN_LEN];
    char result[MAX_TOKEN_LEN];
} Quad;

typedef struct {
    int line;
    char message[200];
} Error;

FILE *input_file;
FILE *output_file;
char current_char;
int current_line = 1;
Token current_token;

Symbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;
int current_scope = 0;

Quad intermediate_code[MAX_CODE_LINES];
int code_count = 0;

Error errors[MAX_ERRORS];
int error_count = 0;

TreeNode *syntax_tree = NULL;

const char *keywords[] = {
    "else", "if", "int", "return", "var", "void", "while"
};
const int keyword_count = 7;

const char *token_type_names[] = {
    "ID", "INT", "TYPE", "VOID", "VAR",
    "IF", "ELSE", "WHILE", "RETURN",
    "ASSIGN", "EQ", "NE", "LT", "LE", "GT", "GE",
    "PLUS", "MINUS", "MUL", "DIV",
    "LP", "RP", "LC", "RC", "SEMI", "COMMA", "COLON",
    "EOF", "ERROR"
};

void add_error(int line, const char *message) {
    if (error_count < MAX_ERRORS) {
        errors[error_count].line = line;
        strcpy(errors[error_count].message, message);
        error_count++;
    }
}

void print_errors() {
    for (int i = 0; i < error_count; i++) {
        printf("第%d行: %s\n", errors[i].line, errors[i].message);
    }
}

int binary_search_keyword(const char *word) {
    int left = 0, right = keyword_count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(word, keywords[mid]);
        if (cmp == 0) return mid;
        else if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return -1;
}

void get_next_char() {
    current_char = fgetc(input_file);
    if (current_char == '\n') {
        current_line++;
    }
}

void skip_whitespace() {
    while (isspace(current_char)) {
        get_next_char();
    }
}

void skip_comment() {
    if (current_char == '/') {
        get_next_char();
        if (current_char == '*') {
            get_next_char();
            while (1) {
                if (current_char == '*') {
                    get_next_char();
                    if (current_char == '/') {
                        get_next_char();
                        return;
                    }
                } else if (current_char == EOF) {
                    add_error(current_line, "注释未闭合");
                    return;
                } else {
                    get_next_char();
                }
            }
        } else {
            ungetc(current_char, input_file);
            current_char = '/';
        }
    }
}

Token get_next_token() {
    Token token;
    token.line = current_line;
    
    skip_whitespace();
    
    while (current_char == '/') {
        char next = fgetc(input_file);
        if (next == '*') {
            ungetc(next, input_file);
            skip_comment();
            skip_whitespace();
        } else {
            ungetc(next, input_file);
            break;
        }
    }
    
    if (current_char == EOF) {
        token.type = TOKEN_EOF;
        strcpy(token.value, "EOF");
        return token;
    }
    
    if (isalpha(current_char) || current_char == '_') {
        int i = 0;
        while (isalnum(current_char) || current_char == '_') {
            token.value[i++] = current_char;
            get_next_char();
        }
        token.value[i] = '\0';
        
        int keyword_idx = binary_search_keyword(token.value);
        if (keyword_idx >= 0) {
            if (strcmp(token.value, "int") == 0) token.type = TOKEN_TYPE;
            else if (strcmp(token.value, "void") == 0) token.type = TOKEN_VOID;
            else if (strcmp(token.value, "var") == 0) token.type = TOKEN_VAR;
            else if (strcmp(token.value, "if") == 0) token.type = TOKEN_IF;
            else if (strcmp(token.value, "else") == 0) token.type = TOKEN_ELSE;
            else if (strcmp(token.value, "while") == 0) token.type = TOKEN_WHILE;
            else if (strcmp(token.value, "return") == 0) token.type = TOKEN_RETURN;
        } else {
            token.type = TOKEN_ID;
        }
        return token;
    }
    
    if (isdigit(current_char)) {
        int i = 0;
        while (isdigit(current_char)) {
            token.value[i++] = current_char;
            get_next_char();
        }
        token.value[i] = '\0';
        token.type = TOKEN_INT;
        token.int_value = atoi(token.value);
        return token;
    }
    
    switch (current_char) {
        case '=':
            get_next_char();
            if (current_char == '=') {
                token.type = TOKEN_EQ;
                strcpy(token.value, "==");
                get_next_char();
            } else {
                token.type = TOKEN_ASSIGN;
                strcpy(token.value, "=");
            }
            break;
        case '!':
            get_next_char();
            if (current_char == '=') {
                token.type = TOKEN_NE;
                strcpy(token.value, "!=");
                get_next_char();
            } else {
                token.type = TOKEN_ERROR;
                strcpy(token.value, "!");
                add_error(token.line, "非法单词 \"!\"");
            }
            break;
        case '<':
            get_next_char();
            if (current_char == '=') {
                token.type = TOKEN_LE;
                strcpy(token.value, "<=");
                get_next_char();
            } else {
                token.type = TOKEN_LT;
                strcpy(token.value, "<");
            }
            break;
        case '>':
            get_next_char();
            if (current_char == '=') {
                token.type = TOKEN_GE;
                strcpy(token.value, ">=");
                get_next_char();
            } else {
                token.type = TOKEN_GT;
                strcpy(token.value, ">");
            }
            break;
        case '+':
            token.type = TOKEN_PLUS;
            strcpy(token.value, "+");
            get_next_char();
            break;
        case '-':
            token.type = TOKEN_MINUS;
            strcpy(token.value, "-");
            get_next_char();
            break;
        case '*':
            token.type = TOKEN_MUL;
            strcpy(token.value, "*");
            get_next_char();
            break;
        case '/':
            token.type = TOKEN_DIV;
            strcpy(token.value, "/");
            get_next_char();
            break;
        case '(':
            token.type = TOKEN_LP;
            strcpy(token.value, "(");
            get_next_char();
            break;
        case ')':
            token.type = TOKEN_RP;
            strcpy(token.value, ")");
            get_next_char();
            break;
        case '{':
            token.type = TOKEN_LC;
            strcpy(token.value, "{");
            get_next_char();
            break;
        case '}':
            token.type = TOKEN_RC;
            strcpy(token.value, "}");
            get_next_char();
            break;
        case ';':
            token.type = TOKEN_SEMI;
            strcpy(token.value, ";");
            get_next_char();
            break;
        case ',':
            token.type = TOKEN_COMMA;
            strcpy(token.value, ",");
            get_next_char();
            break;
        case ':':
            token.type = TOKEN_COLON;
            strcpy(token.value, ":");
            get_next_char();
            break;
        default:
            token.type = TOKEN_ERROR;
            token.value[0] = current_char;
            token.value[1] = '\0';
            char err_msg[100];
            sprintf(err_msg, "非法单词 \"%s\"", token.value);
            add_error(token.line, err_msg);
            get_next_char();
            break;
    }
    
    return token;
}

TreeNode *create_node(const char *name, int line, int is_terminal) {
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    strcpy(node->name, name);
    node->line = line;
    node->is_terminal = is_terminal;
    node->extra_info[0] = '\0';
    node->child_count = 0;
    return node;
}

void add_child(TreeNode *parent, TreeNode *child) {
    if (parent && child && parent->child_count < 20) {
        parent->children[parent->child_count++] = child;
    }
}

void print_tree(TreeNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    if (node->is_terminal) {
        if (strcmp(node->name, "ID") == 0) {
            printf("%s: %s\n", node->name, node->extra_info);
        } else if (strcmp(node->name, "TYPE") == 0) {
            printf("%s: int\n", node->name);
        } else if (strcmp(node->name, "INT") == 0) {
            printf("%s: %d\n", node->name, atoi(node->extra_info));
        } else {
            printf("%s\n", node->name);
        }
    } else {
        if (node->line > 0) {
            printf("%s (%d)\n", node->name, node->line);
        }
    }
    
    for (int i = 0; i < node->child_count; i++) {
        print_tree(node->children[i], indent + 1);
    }
}

void match(TokenType type) {
    if (current_token.type == type) {
        current_token = get_next_token();
    } else {
        char err_msg[100];
        sprintf(err_msg, "缺少%s", token_type_names[type]);
        add_error(current_token.line, err_msg);
    }
}

TreeNode *parse_program();
TreeNode *parse_declaration();
TreeNode *parse_main_declaration();
TreeNode *parse_function_body();
TreeNode *parse_declaration_list();
TreeNode *parse_declaration_stat();
TreeNode *parse_statement();
TreeNode *parse_statement_list();
TreeNode *parse_expression();
TreeNode *parse_expression_stat();
TreeNode *parse_assign_stat();
TreeNode *parse_if_stat();
TreeNode *parse_while_stat();
TreeNode *parse_return_stat();
TreeNode *parse_bool_expression();
TreeNode *parse_arithmetic_expression();
TreeNode *parse_term();
TreeNode *parse_factor();
TreeNode *parse_var_declaration();

TreeNode *parse_program() {
    TreeNode *node = create_node("Program", current_token.line, 0);
    
    while (current_token.type != TOKEN_EOF) {
        TreeNode *decl = parse_declaration();
        if (decl) add_child(node, decl);
    }
    
    return node;
}

TreeNode *parse_declaration() {
    if (current_token.type == TOKEN_ID) {
        return parse_main_declaration();
    } else if (current_token.type == TOKEN_VOID) {
        TreeNode *node = create_node("void_declaration", current_token.line, 0);
        match(TOKEN_VOID);
        TreeNode *id_node = create_node("ID", current_token.line, 1);
        strcpy(id_node->extra_info, current_token.value);
        add_child(node, id_node);
        match(TOKEN_ID);
        match(TOKEN_LP);
        match(TOKEN_RP);
        TreeNode *body = parse_function_body();
        if (body) add_child(node, body);
        return node;
    } else if (current_token.type == TOKEN_VAR) {
        return parse_var_declaration();
    } else {
        add_error(current_token.line, "语法错误：期望声明");
        current_token = get_next_token();
        return NULL;
    }
}

TreeNode *parse_main_declaration() {
    TreeNode *node = create_node("main_declaration", current_token.line, 0);
    
    TreeNode *id_node = create_node("ID", current_token.line, 1);
    strcpy(id_node->extra_info, current_token.value);
    add_child(node, id_node);
    match(TOKEN_ID);
    match(TOKEN_LP);
    match(TOKEN_RP);
    
    TreeNode *body = parse_function_body();
    if (body) add_child(node, body);
    
    return node;
}

TreeNode *parse_function_body() {
    TreeNode *node = create_node("function_body", current_token.line, 0);
    match(TOKEN_LC);
    
    TreeNode *decl_list = parse_declaration_list();
    if (decl_list) add_child(node, decl_list);
    
    match(TOKEN_RC);
    return node;
}

TreeNode *parse_declaration_list() {
    TreeNode *node = create_node("declaration_list", current_token.line, 0);
    
    while (current_token.type != TOKEN_RC && current_token.type != TOKEN_EOF) {
        TreeNode *decl = parse_declaration_stat();
        if (decl) add_child(node, decl);
        else break;
    }
    
    return node;
}

TreeNode *parse_declaration_stat() {
    if (current_token.type == TOKEN_TYPE) {
        TreeNode *node = create_node("declaration_stat", current_token.line, 0);
        
        TreeNode *type_node = create_node("TYPE", current_token.line, 1);
        strcpy(type_node->extra_info, "int");
        add_child(node, type_node);
        match(TOKEN_TYPE);
        
        if (current_token.type == TOKEN_ID) {
            TreeNode *id_node = create_node("ID", current_token.line, 1);
            strcpy(id_node->extra_info, current_token.value);
            add_child(node, id_node);
            match(TOKEN_ID);
        } else {
            add_error(current_token.line, "缺少ID");
        }
        
        match(TOKEN_SEMI);
        return node;
    } else if (current_token.type == TOKEN_VAR) {
        return parse_var_declaration();
    } else {
        return parse_statement();
    }
}

TreeNode *parse_var_declaration() {
    TreeNode *node = create_node("var_declaration", current_token.line, 0);
    match(TOKEN_VAR);
    
    if (current_token.type == TOKEN_ID) {
        TreeNode *id_node = create_node("ID", current_token.line, 1);
        strcpy(id_node->extra_info, current_token.value);
        add_child(node, id_node);
        match(TOKEN_ID);
    } else {
        add_error(current_token.line, "缺少ID");
    }
    
    match(TOKEN_COLON);
    
    if (current_token.type == TOKEN_TYPE) {
        TreeNode *type_node = create_node("TYPE", current_token.line, 1);
        strcpy(type_node->extra_info, "int");
        add_child(node, type_node);
        match(TOKEN_TYPE);
    } else {
        add_error(current_token.line, "缺少类型");
    }
    
    return node;
}

TreeNode *parse_statement() {
    switch (current_token.type) {
        case TOKEN_IF:
            return parse_if_stat();
        case TOKEN_WHILE:
            return parse_while_stat();
        case TOKEN_RETURN:
            return parse_return_stat();
        case TOKEN_ID:
            return parse_assign_stat();
        case TOKEN_LC:
            return parse_function_body();
        default:
            return parse_expression_stat();
    }
}

TreeNode *parse_assign_stat() {
    TreeNode *node = create_node("assign_stat", current_token.line, 0);
    
    TreeNode *id_node = create_node("ID", current_token.line, 1);
    strcpy(id_node->extra_info, current_token.value);
    add_child(node, id_node);
    match(TOKEN_ID);
    
    match(TOKEN_ASSIGN);
    
    TreeNode *expr = parse_expression();
    if (expr) add_child(node, expr);
    
    match(TOKEN_SEMI);
    return node;
}

TreeNode *parse_if_stat() {
    TreeNode *node = create_node("if_stat", current_token.line, 0);
    match(TOKEN_IF);
    match(TOKEN_LP);
    
    TreeNode *cond = parse_bool_expression();
    if (cond) add_child(node, cond);
    
    match(TOKEN_RP);
    
    TreeNode *then_stmt = parse_statement();
    if (then_stmt) add_child(node, then_stmt);
    
    if (current_token.type == TOKEN_ELSE) {
        match(TOKEN_ELSE);
        TreeNode *else_stmt = parse_statement();
        if (else_stmt) add_child(node, else_stmt);
    }
    
    return node;
}

TreeNode *parse_while_stat() {
    TreeNode *node = create_node("while_stat", current_token.line, 0);
    match(TOKEN_WHILE);
    match(TOKEN_LP);
    
    TreeNode *cond = parse_bool_expression();
    if (cond) add_child(node, cond);
    
    match(TOKEN_RP);
    
    TreeNode *body = parse_statement();
    if (body) add_child(node, body);
    
    return node;
}

TreeNode *parse_return_stat() {
    TreeNode *node = create_node("return_stat", current_token.line, 0);
    match(TOKEN_RETURN);
    
    if (current_token.type != TOKEN_SEMI) {
        TreeNode *expr = parse_expression();
        if (expr) add_child(node, expr);
    }
    
    match(TOKEN_SEMI);
    return node;
}

TreeNode *parse_expression_stat() {
    TreeNode *node = create_node("expression_stat", current_token.line, 0);
    
    if (current_token.type != TOKEN_SEMI) {
        TreeNode *expr = parse_expression();
        if (expr) add_child(node, expr);
    }
    
    match(TOKEN_SEMI);
    return node;
}

TreeNode *parse_expression() {
    return parse_arithmetic_expression();
}

TreeNode *parse_bool_expression() {
    TreeNode *node = create_node("bool_expression", current_token.line, 0);
    
    TreeNode *left = parse_arithmetic_expression();
    if (left) add_child(node, left);
    
    if (current_token.type >= TOKEN_EQ && current_token.type <= TOKEN_GE) {
        TreeNode *op_node = create_node(token_type_names[current_token.type], current_token.line, 1);
        add_child(node, op_node);
        match(current_token.type);
        
        TreeNode *right = parse_arithmetic_expression();
        if (right) add_child(node, right);
    }
    
    return node;
}

TreeNode *parse_arithmetic_expression() {
    TreeNode *node = create_node("arithmetic_expression", current_token.line, 0);
    
    TreeNode *left = parse_term();
    if (left) add_child(node, left);
    
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        TreeNode *op_node = create_node(token_type_names[current_token.type], current_token.line, 1);
        add_child(node, op_node);
        match(current_token.type);
        
        TreeNode *right = parse_term();
        if (right) add_child(node, right);
    }
    
    return node;
}

TreeNode *parse_term() {
    TreeNode *node = create_node("term", current_token.line, 0);
    
    TreeNode *left = parse_factor();
    if (left) add_child(node, left);
    
    while (current_token.type == TOKEN_MUL || current_token.type == TOKEN_DIV) {
        TreeNode *op_node = create_node(token_type_names[current_token.type], current_token.line, 1);
        add_child(node, op_node);
        match(current_token.type);
        
        TreeNode *right = parse_factor();
        if (right) add_child(node, right);
    }
    
    return node;
}

TreeNode *parse_factor() {
    TreeNode *node = create_node("factor", current_token.line, 0);
    
    if (current_token.type == TOKEN_LP) {
        match(TOKEN_LP);
        TreeNode *expr = parse_expression();
        if (expr) add_child(node, expr);
        match(TOKEN_RP);
    } else if (current_token.type == TOKEN_INT) {
        TreeNode *int_node = create_node("INT", current_token.line, 1);
        sprintf(int_node->extra_info, "%d", current_token.int_value);
        add_child(node, int_node);
        match(TOKEN_INT);
    } else if (current_token.type == TOKEN_ID) {
        TreeNode *id_node = create_node("ID", current_token.line, 1);
        strcpy(id_node->extra_info, current_token.value);
        add_child(node, id_node);
        match(TOKEN_ID);
    } else {
        add_error(current_token.line, "表达式语法错误");
    }
    
    return node;
}

void add_symbol(const char *name, const char *type, int line) {
    if (symbol_count < MAX_SYMBOLS) {
        strcpy(symbol_table[symbol_count].name, name);
        strcpy(symbol_table[symbol_count].type, type);
        symbol_table[symbol_count].scope_level = current_scope;
        symbol_table[symbol_count].line_declared = line;
        symbol_count++;
    }
}

Symbol *lookup_symbol(const char *name) {
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return &symbol_table[i];
        }
    }
    return NULL;
}

void semantic_analysis(TreeNode *node) {
    if (!node) return;
    
    if (strcmp(node->name, "declaration_stat") == 0 ||
        strcmp(node->name, "var_declaration") == 0) {
        
        char id_name[MAX_TOKEN_LEN] = "";
        char type_name[20] = "int";
        
        for (int i = 0; i < node->child_count; i++) {
            TreeNode *child = node->children[i];
            if (strcmp(child->name, "ID") == 0) {
                strcpy(id_name, child->extra_info);
            } else if (strcmp(child->name, "TYPE") == 0) {
                strcpy(type_name, child->extra_info);
            }
        }
        
        if (strlen(id_name) > 0) {
            Symbol *existing = lookup_symbol(id_name);
            if (existing && existing->scope_level == current_scope) {
                char err_msg[100];
                sprintf(err_msg, "变量\"%s\"重复声明", id_name);
                add_error(node->line, err_msg);
            } else {
                add_symbol(id_name, type_name, node->line);
            }
        }
    }
    
    if (strcmp(node->name, "ID") == 0 && node->is_terminal) {
        Symbol *sym = lookup_symbol(node->extra_info);
        if (!sym) {
            char err_msg[100];
            sprintf(err_msg, "变量\"%s\"未声明", node->extra_info);
            add_error(node->line, err_msg);
        }
    }
    
    if (strcmp(node->name, "function_body") == 0) {
        current_scope++;
    }
    
    for (int i = 0; i < node->child_count; i++) {
        semantic_analysis(node->children[i]);
    }
    
    if (strcmp(node->name, "function_body") == 0) {
        current_scope--;
    }
}

int temp_count = 0;
int label_count = 0;

char *new_temp() {
    static char temp[20];
    sprintf(temp, "t%d", temp_count++);
    return temp;
}

char *new_label() {
    static char label[20];
    sprintf(label, "L%d", label_count++);
    return label;
}

void emit(const char *op, const char *arg1, const char *arg2, const char *result) {
    strcpy(intermediate_code[code_count].op, op);
    strcpy(intermediate_code[code_count].arg1, arg1);
    strcpy(intermediate_code[code_count].arg2, arg2);
    strcpy(intermediate_code[code_count].result, result);
    code_count++;
}

char *generate_code(TreeNode *node) {
    if (!node) return "";
    
    if (strcmp(node->name, "INT") == 0) {
        return node->extra_info;
    }
    
    if (strcmp(node->name, "ID") == 0) {
        return node->extra_info;
    }
    
    if (strcmp(node->name, "factor") == 0 && node->child_count == 1) {
        return generate_code(node->children[0]);
    }
    
    if (strcmp(node->name, "term") == 0 || strcmp(node->name, "arithmetic_expression") == 0) {
        if (node->child_count == 1) {
            return generate_code(node->children[0]);
        }
        
        char *left = generate_code(node->children[0]);
        char *right = generate_code(node->children[2]);
        char *result = new_temp();
        
        char *op = "";
        if (strcmp(node->children[1]->name, "PLUS") == 0) op = "+";
        else if (strcmp(node->children[1]->name, "MINUS") == 0) op = "-";
        else if (strcmp(node->children[1]->name, "MUL") == 0) op = "*";
        else if (strcmp(node->children[1]->name, "DIV") == 0) op = "/";
        
        emit(op, left, right, result);
        return result;
    }
    
    if (strcmp(node->name, "assign_stat") == 0) {
        char *value = generate_code(node->children[1]);
        char *id = node->children[0]->extra_info;
        emit("=", value, "", id);
        return id;
    }
    
    if (strcmp(node->name, "if_stat") == 0) {
        char *cond = generate_code(node->children[0]);
        char *label_else = new_label();
        char *label_end = new_label();
        
        emit("if_false", cond, "", label_else);
        generate_code(node->children[1]);
        emit("goto", "", "", label_end);
        emit("label", "", "", label_else);
        
        if (node->child_count > 2) {
            generate_code(node->children[2]);
        }
        
        emit("label", "", "", label_end);
        return "";
    }
    
    if (strcmp(node->name, "while_stat") == 0) {
        char *label_start = new_label();
        char *label_end = new_label();
        
        emit("label", "", "", label_start);
        char *cond = generate_code(node->children[0]);
        emit("if_false", cond, "", label_end);
        generate_code(node->children[1]);
        emit("goto", "", "", label_start);
        emit("label", "", "", label_end);
        return "";
    }
    
    for (int i = 0; i < node->child_count; i++) {
        generate_code(node->children[i]);
    }
    
    return "";
}

void print_intermediate_code() {
    printf("\n=== 中间代码 ===\n");
    for (int i = 0; i < code_count; i++) {
        Quad *q = &intermediate_code[i];
        if (strcmp(q->op, "label") == 0) {
            printf("%s:\n", q->result);
        } else if (strcmp(q->op, "if_false") == 0) {
            printf("if_false %s goto %s\n", q->arg1, q->result);
        } else if (strcmp(q->op, "goto") == 0) {
            printf("goto %s\n", q->result);
        } else if (strcmp(q->op, "=") == 0) {
            printf("%s = %s\n", q->result, q->arg1);
        } else {
            printf("%s = %s %s %s\n", q->result, q->arg1, q->op, q->arg2);
        }
    }
}

void print_symbol_table() {
    printf("\n=== 符号表 ===\n");
    printf("%-15s %-10s %-10s %s\n", "名称", "类型", "作用域", "声明行号");
    printf("----------------------------------------\n");
    for (int i = 0; i < symbol_count; i++) {
        printf("%-15s %-10s %-10d %d\n",
               symbol_table[i].name,
               symbol_table[i].type,
               symbol_table[i].scope_level,
               symbol_table[i].line_declared);
    }
}

void write_token_to_file(Token token) {
    if (token.type == TOKEN_EOF) return;
    
    fprintf(output_file, "<%s", token_type_names[token.type]);
    
    if (token.type == TOKEN_ID) {
        fprintf(output_file, ", %s", token.value);
    } else if (token.type == TOKEN_INT) {
        fprintf(output_file, ", %d", token.int_value);
    } else if (token.type >= TOKEN_ASSIGN && token.type <= TOKEN_DIV) {
        fprintf(output_file, ", %s", token.value);
    } else if (token.type >= TOKEN_LP && token.type <= TOKEN_COLON) {
        fprintf(output_file, ", %s", token.value);
    }
    
    fprintf(output_file, ">\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("用法: %s <输入文件> [输出文件]\n", argv[0]);
        return 1;
    }
    
    input_file = fopen(argv[1], "r");
    if (!input_file) {
        printf("无法打开输入文件: %s\n", argv[1]);
        return 1;
    }
    
    const char *output_filename = "tokens.txt";
    if (argc >= 3) {
        output_filename = argv[2];
    }
    
    output_file = fopen(output_filename, "w");
    if (!output_file) {
        printf("无法创建输出文件: %s\n", output_filename);
        fclose(input_file);
        return 1;
    }
    
    printf("=== 词法分析开始 ===\n");
    
    get_next_char();
    
    current_token = get_next_token();
    while (current_token.type != TOKEN_EOF) {
        write_token_to_file(current_token);
        current_token = get_next_token();
    }
    
    fclose(output_file);
    printf("词法分析完成，单词流已输出到: %s\n", output_filename);
    
    if (error_count > 0) {
        printf("\n=== 词法错误 ===\n");
        print_errors();
    }
    
    rewind(input_file);
    current_line = 1;
    error_count = 0;
    get_next_char();
    current_token = get_next_token();
    
    printf("\n=== 语法分析开始 ===\n");
    syntax_tree = parse_program();
    
    if (error_count > 0) {
        printf("\n=== 语法错误 ===\n");
        print_errors();
    } else {
        printf("\n=== 语法树 ===\n");
        print_tree(syntax_tree, 0);
    }
    
    printf("\n=== 语义分析开始 ===\n");
    error_count = 0;
    semantic_analysis(syntax_tree);
    
    if (error_count > 0) {
        printf("\n=== 语义错误 ===\n");
        print_errors();
    } else {
        printf("语义分析通过，未发现错误。\n");
    }
    
    print_symbol_table();
    
    printf("\n=== 中间代码生成开始 ===\n");
    temp_count = 0;
    label_count = 0;
    code_count = 0;
    generate_code(syntax_tree);
    print_intermediate_code();
    
    fclose(input_file);
    
    printf("\n=== 编译完成 ===\n");
    
    return 0;
}
