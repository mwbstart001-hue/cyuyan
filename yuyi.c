#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- 词法分析定义 --- */

typedef enum {
  T_VOID,
  T_MAIN,
  T_VAR,
  T_INT_TYPE,
  T_INT_VAL,
  T_ID,
  T_LPAREN,
  T_RPAREN,
  T_LBRACE,
  T_RBRACE,
  T_COLON,
  T_ASSIGN,
  T_SEMI,
  T_PLUS,
  T_MINUS,
  T_MUL,
  T_DIV,
  T_EOF,
  T_ERROR
} TokenType;

const char *token_names[] = {"VOID",  "MAIN",   "VAR",  "TYPE", "INT",
                             "ID",    "LP",     "RP",   "LC",   "RC",
                             "COLON", "ASSIGN", "SEMI", "PLUS", "MINUS",
                             "MUL",   "DIV",    "EOF",  "ERROR"};

typedef struct {
  TokenType type;
  char text[256];
  int line;
  int val; // 仅用于 T_INT_VAL
} Token;

// 保留字表（按字典序排列方便折半查找）
typedef struct {
  const char *word;
  TokenType type;
} ReservedWord;

ReservedWord reserved_words[] = {
    {"int", T_INT_TYPE}, {"main", T_MAIN}, {"var", T_VAR}, {"void", T_VOID}};
int reserved_count = 4;

/* --- 全局状态 --- */

FILE *fin;
int current_line = 1;
int ch;
Token current_token;
int has_error = 0;

/* --- 错误处理 --- */

void error(int line, const char *msg) {
  fprintf(stdout, "第%d行: %s\n", line, msg);
  has_error = 1;
}

/* --- 词法分析实现 --- */

// 折半查找保留字
TokenType lookup_reserved(const char *s) {
  int low = 0, high = reserved_count - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    int cmp = strcmp(s, reserved_words[mid].word);
    if (cmp == 0)
      return reserved_words[mid].type;
    if (cmp < 0)
      high = mid - 1;
    else
      low = mid + 1;
  }
  return T_ID;
}

void get_next_char() {
  ch = fgetc(fin);
  if (ch == '\n')
    current_line++;
}

Token get_token() {
  Token t;
  t.text[0] = '\0';

  while (isspace(ch) || ch == '/') {
    if (isspace(ch)) {
      get_next_char();
    } else if (ch == '/') {
      get_next_char();
      if (ch == '*') {
        // 注释 /* ... */
        get_next_char();
        while (1) {
          if (ch == EOF) {
            error(current_line, "未闭合的注释");
            t.type = T_EOF;
            return t;
          }
          if (ch == '*') {
            get_next_char();
            if (ch == '/') {
              get_next_char();
              break;
            }
          } else {
            get_next_char();
          }
        }
      } else {
        // 暂时只支持 /* */ 注释，如果是单个 /
        // 视作除号（由后面逻辑处理或返回错误）
        ungetc(ch, fin);
        if (ch == '\n')
          current_line--; // 撤销行号增加
        ch = '/';
        break;
      }
    }
  }

  t.line = current_line;

  if (ch == EOF) {
    t.type = T_EOF;
    return t;
  }

  if (isalpha(ch) || ch == '_') {
    int i = 0;
    while (isalnum(ch) || ch == '_') {
      if (i < 255)
        t.text[i++] = ch;
      get_next_char();
    }
    t.text[i] = '\0';
    t.type = lookup_reserved(t.text);
  } else if (isdigit(ch)) {
    int i = 0;
    int val = 0;
    while (isdigit(ch)) {
      if (i < 255)
        t.text[i++] = ch;
      val = val * 10 + (ch - '0');
      get_next_char();
    }
    t.text[i] = '\0';
    t.type = T_INT_VAL;
    t.val = val;
  } else {
    t.text[0] = ch;
    t.text[1] = '\0';
    switch (ch) {
    case '(':
      t.type = T_LPAREN;
      get_next_char();
      break;
    case ')':
      t.type = T_RPAREN;
      get_next_char();
      break;
    case '{':
      t.type = T_LBRACE;
      get_next_char();
      break;
    case '}':
      t.type = T_RBRACE;
      get_next_char();
      break;
    case ':':
      t.type = T_COLON;
      get_next_char();
      break;
    case '=':
      t.type = T_ASSIGN;
      get_next_char();
      break;
    case ';':
      t.type = T_SEMI;
      get_next_char();
      break;
    case '+':
      t.type = T_PLUS;
      get_next_char();
      break;
    case '-':
      t.type = T_MINUS;
      get_next_char();
      break;
    case '*':
      t.type = T_MUL;
      get_next_char();
      break;
    case '/':
      t.type = T_DIV;
      get_next_char();
      break;
    default:
      t.type = T_ERROR;
      char msg[64];
      sprintf(msg, "非法单词 \"%c\".", ch);
      error(current_line, msg);
      get_next_char();
      break;
    }
  }
  return t;
}

/* --- 语法设计与树结构 --- */

typedef enum {
  N_PROGRAM,
  N_MAIN_DECL,
  N_BLOCK,
  N_DECL_LIST,
  N_DECL_STAT,
  N_STAT_LIST,
  N_ASSIGN_STAT,
  N_EXPR,
  N_ID,
  N_TYPE,
  N_INT
} NodeType;

typedef struct ASTNode {
  NodeType type;
  const char *name;
  char text[256];
  int line;
  int val;
  struct ASTNode *children[10];
  int child_count;
} ASTNode;

ASTNode *create_node(NodeType type, const char *name, int line) {
  ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
  node->type = type;
  node->name = name;
  node->line = line;
  node->child_count = 0;
  node->text[0] = '\0';
  for (int i = 0; i < 10; i++)
    node->children[i] = NULL;
  return node;
}

void add_child(ASTNode *parent, ASTNode *child) {
  if (child && parent->child_count < 10) {
    parent->children[parent->child_count++] = child;
  }
}

/* --- 语法分析函数声明 --- */

ASTNode *parse_program();
ASTNode *parse_main_declaration();
ASTNode *parse_function_body();
ASTNode *parse_declaration_list();
ASTNode *parse_declaration_stat();
ASTNode *parse_statement_list();
ASTNode *parse_statement();

void match(TokenType expected) {
  if (current_token.type == expected) {
    current_token = get_token();
  } else {
    char msg[128];
    sprintf(msg, "缺少 %s。", token_names[expected]);
    error(current_token.line, msg);
    // 简单的错误跳过处理
    current_token = get_token();
  }
}

ASTNode *parse_program() {
  ASTNode *node = create_node(N_PROGRAM, "Program", current_token.line);
  add_child(node, parse_main_declaration());
  return node;
}

ASTNode *parse_main_declaration() {
  int line = current_token.line;
  ASTNode *node = create_node(N_MAIN_DECL, "main_declaration", line);

  if (current_token.type == T_VOID) {
    match(T_VOID);
  }

  if (current_token.type == T_MAIN) {
    ASTNode *id = create_node(N_ID, "ID", current_token.line);
    strcpy(id->text, "main");
    add_child(node, id);
    match(T_MAIN);
  } else {
    error(current_token.line, "缺少 ID。");
  }

  match(T_LPAREN);
  match(T_RPAREN);

  add_child(node, parse_function_body());
  return node;
}

ASTNode *parse_function_body() {
  int line = current_token.line;
  ASTNode *node = create_node(N_BLOCK, "function_body", line);
  match(T_LBRACE);

  add_child(node, parse_declaration_list());
  add_child(node, parse_statement_list());

  match(T_RBRACE);
  return node;
}

ASTNode *parse_declaration_list() {
  if (current_token.type == T_VAR || current_token.type == T_INT_TYPE) {
    ASTNode *node =
        create_node(N_DECL_LIST, "declaration_list", current_token.line);
    add_child(node, parse_declaration_stat());
    add_child(node, parse_declaration_list());
    return node;
  }
  return NULL; // epsilon
}

ASTNode *parse_declaration_stat() {
  int line = current_token.line;
  ASTNode *node = create_node(N_DECL_STAT, "declaration_stat", line);

  if (current_token.type == T_VAR) {
    match(T_VAR);
    if (current_token.type == T_ID) {
      ASTNode *id = create_node(N_ID, "ID", current_token.line);
      strcpy(id->text, current_token.text);
      add_child(node, id);
      match(T_ID);
    } else {
      error(current_token.line, "缺少ID。");
    }
    match(T_COLON);
    if (current_token.type == T_INT_TYPE) {
      ASTNode *type = create_node(N_TYPE, "TYPE", current_token.line);
      strcpy(type->text, "int");
      add_child(node, type);
      match(T_INT_TYPE);
    }
  } else if (current_token.type == T_INT_TYPE) {
    ASTNode *type = create_node(N_TYPE, "TYPE", current_token.line);
    strcpy(type->text, "int");
    add_child(node, type);
    match(T_INT_TYPE);

    if (current_token.type == T_ID) {
      ASTNode *id = create_node(N_ID, "ID", current_token.line);
      strcpy(id->text, current_token.text);
      add_child(node, id);
      match(T_ID);
    } else {
      error(current_token.line, "缺少ID。");
    }
    match(T_SEMI);
  }
  return node;
}

ASTNode *parse_statement_list() {
  if (current_token.type == T_ID) {
    ASTNode *node =
        create_node(N_STAT_LIST, "statement_list", current_token.line);
    add_child(node, parse_statement());
    add_child(node, parse_statement_list());
    return node;
  }
  return NULL; // epsilon
}

ASTNode *parse_statement() {
  // 简单实现赋值
  ASTNode *node =
      create_node(N_ASSIGN_STAT, "assignment_stat", current_token.line);
  ASTNode *id = create_node(N_ID, "ID", current_token.line);
  strcpy(id->text, current_token.text);
  add_child(node, id);
  match(T_ID);
  match(T_ASSIGN);

  // 这里简化处理表达式，直接取一个数字或ID
  if (current_token.type == T_INT_VAL) {
    ASTNode *val = create_node(N_INT, "INT", current_token.line);
    val->val = current_token.val;
    add_child(node, val);
    match(T_INT_VAL);
  } else if (current_token.type == T_ID) {
    ASTNode *sid = create_node(N_ID, "ID", current_token.line);
    strcpy(sid->text, current_token.text);
    add_child(node, sid);
    match(T_ID);
  }

  match(T_SEMI);
  return node;
}

/* --- 打印语法树 --- */

void print_ast(ASTNode *node, int indent) {
  if (!node)
    return;

  for (int i = 0; i < indent; i++)
    printf("  ");

  if (node->type == N_ID) {
    printf("ID: %s\n", node->text);
  } else if (node->type == N_TYPE) {
    printf("TYPE: %s\n", node->text);
  } else if (node->type == N_INT) {
    printf("INT: %d\n", node->val);
  } else {
    printf("%s (%d)\n", node->name, node->line);
  }

  for (int i = 0; i < node->child_count; i++) {
    print_ast(node->children[i], indent + 1);
  }
}

/* --- 语义分析与代码生成（占位） --- */

/* --- 语义分析与代码生成 --- */

typedef struct Symbol {
  char name[256];
  char type[32];
  struct Symbol *next;
} Symbol;

Symbol *symbol_table = NULL;

void add_symbol(const char *name, const char *type, int line) {
  Symbol *curr = symbol_table;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      char msg[512];
      sprintf(msg, "变量 \"%s\" 重复定义。", name);
      error(line, msg);
      return;
    }
    curr = curr->next;
  }
  Symbol *s = (Symbol *)malloc(sizeof(Symbol));
  strcpy(s->name, name);
  strcpy(s->type, type);
  s->next = symbol_table;
  symbol_table = s;
}

int check_symbol(const char *name, int line) {
  Symbol *curr = symbol_table;
  while (curr) {
    if (strcmp(curr->name, name) == 0)
      return 1;
    curr = curr->next;
  }
  char msg[512];
  sprintf(msg, "变量 \"%s\" 未定义。", name);
  error(line, msg);
  return 0;
}

void traverse_for_semantics(ASTNode *node) {
  if (!node)
    return;

  if (node->type == N_DECL_STAT) {
    char id_name[256] = "";
    char type_name[32] = "int";
    for (int i = 0; i < node->child_count; i++) {
      if (node->children[i]->type == N_ID)
        strcpy(id_name, node->children[i]->text);
      if (node->children[i]->type == N_TYPE)
        strcpy(type_name, node->children[i]->text);
    }
    if (strlen(id_name) > 0)
      add_symbol(id_name, type_name, node->line);
  } else if (node->type == N_ASSIGN_STAT) {
    check_symbol(node->children[0]->text, node->line);
    // 如果右侧也是ID
    if (node->child_count > 1 && node->children[1]->type == N_ID) {
      check_symbol(node->children[1]->text, node->line);
    }
  }

  for (int i = 0; i < node->child_count; i++) {
    traverse_for_semantics(node->children[i]);
  }
}

void generate_code_recursive(ASTNode *node) {
  if (!node)
    return;

  if (node->type == N_ASSIGN_STAT) {
    const char *lhs = node->children[0]->text;
    if (node->children[1]->type == N_INT) {
      printf("(ASSIGN, %d, _, %s)\n", node->children[1]->val, lhs);
    } else if (node->children[1]->type == N_ID) {
      printf("(ASSIGN, %s, _, %s)\n", node->children[1]->text, lhs);
    }
  }

  for (int i = 0; i < node->child_count; i++) {
    generate_code_recursive(node->children[i]);
  }
}

void semantic_analysis(ASTNode *root) {
  printf("\n--- 语义分析与错误报告 ---\n");
  traverse_for_semantics(root);
  if (!has_error) {
    printf("无语义错误。\n");
    printf("\n--- 中间代码生成 (四元组) ---\n");
    generate_code_recursive(root);
  }
}

/* --- 主程序 --- */

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("用法: %s <输入文件>\n", argv[0]);
    return 1;
  }

  fin = fopen(argv[1], "r");
  if (!fin) {
    perror("无法打开文件");
    return 1;
  }

  get_next_char();
  current_token = get_token();

  ASTNode *root = parse_program();

  if (!has_error) {
    print_ast(root, 0);
    semantic_analysis(root);
  }

  if (fin != stdin)
    fclose(fin);
  return 0;
}
