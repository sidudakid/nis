#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ----------------- Tokens -----------------
typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_INT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_PRINT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_ASSIGN,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

// ----------------- Lexer -----------------
typedef struct {
    const char *src;
    int pos;
    int line;
    int column;
} Lexer;

char peek(Lexer *lexer) { return lexer->src[lexer->pos]; }
char advance(Lexer *lexer) {
    char c = lexer->src[lexer->pos++];
    if(c=='\n'){ lexer->line++; lexer->column=1; } 
    else lexer->column++;
    return c;
}

int isAtEnd(Lexer *lexer){ return lexer->src[lexer->pos]=='\0'; }
int isDigit(char c){ return c>='0' && c<='9'; }
int isAlpha(char c){ return (c>='a' && c<='z')||(c>='A' && c<='Z')||c=='_'; }
int isAlphaNum(char c){ return isAlpha(c)||isDigit(c); }

Token makeToken(TokenType type,const char *start,int length,int line,int column){
    Token t;
    t.type = type;
    t.lexeme = malloc(length+1);
    strncpy(t.lexeme,start,length);
    t.lexeme[length]='\0';
    t.line=line;
    t.column=column;
    return t;
}

void skipWhitespace(Lexer *lexer){
    while(!isAtEnd(lexer)){
        char c=peek(lexer);
        if(c==' '||c=='\t'||c=='\r') advance(lexer);
        else if(c=='\n') advance(lexer);
        else break;
    }
}

Token number(Lexer *lexer){
    int start=lexer->pos-1,line=lexer->line,column=lexer->column-1;
    while(isDigit(peek(lexer))) advance(lexer);
    return makeToken(TOKEN_NUMBER,lexer->src+start,lexer->pos-start,line,column);
}

Token identifier(Lexer *lexer){
    int start=lexer->pos-1,line=lexer->line,column=lexer->column-1;
    while(isAlphaNum(peek(lexer))) advance(lexer);
    int len=lexer->pos-start;
    char *text=strndup(lexer->src+start,len);
    TokenType type=TOKEN_IDENTIFIER;
    if(strcmp(text,"int")==0) type=TOKEN_INT;
    else if(strcmp(text,"if")==0) type=TOKEN_IF;
    else if(strcmp(text,"else")==0) type=TOKEN_ELSE;
    else if(strcmp(text,"while")==0) type=TOKEN_WHILE;
    else if(strcmp(text,"print")==0) type=TOKEN_PRINT;
    free(text);
    return makeToken(type,lexer->src+start,len,line,column);
}

// ----------------- Token Vector -----------------
typedef struct { Token *data; int size; int capacity; } TokenVector;
void initVector(TokenVector *vec){ vec->size=0; vec->capacity=8; vec->data=malloc(sizeof(Token)*vec->capacity);}
void pushVector(TokenVector *vec,Token t){ if(vec->size==vec->capacity){vec->capacity*=2; vec->data=realloc(vec->data,sizeof(Token)*vec->capacity);} vec->data[vec->size++]=t;}
void freeVector(TokenVector *vec){ for(int i=0;i<vec->size;i++) free(vec->data[i].lexeme); free(vec->data); }

// ----------------- Lexer Core -----------------
void lexSource(Lexer *lexer,TokenVector *tokens){
    while(!isAtEnd(lexer)){
        skipWhitespace(lexer);
        if(isAtEnd(lexer)) break;
        char c=advance(lexer);
        Token t;
        if(isAlpha(c)) t=identifier(lexer);
        else if(isDigit(c)) t=number(lexer);
        else {
            int line=lexer->line,col=lexer->column-1;
            switch(c){
                case '+': t=makeToken(TOKEN_PLUS,"+",1,line,col); break;
                case '-': t=makeToken(TOKEN_MINUS,"-",1,line,col); break;
                case '*': t=makeToken(TOKEN_STAR,"*",1,line,col); break;
                case '/': t=makeToken(TOKEN_SLASH,"/",1,line,col); break;
                case '=': t=makeToken(TOKEN_ASSIGN,"=",1,line,col); break;
                case ';': t=makeToken(TOKEN_SEMICOLON,";",1,line,col); break;
                case '(': t=makeToken(TOKEN_LPAREN,"(",1,line,col); break;
                case ')': t=makeToken(TOKEN_RPAREN,")",1,line,col); break;
                case '{': t=makeToken(TOKEN_LBRACE,"{",1,line,col); break;
                case '}': t=makeToken(TOKEN_RBRACE,"}",1,line,col); break;
                default: t=makeToken(TOKEN_IDENTIFIER,&c,1,line,col);
            }
        }
        pushVector(tokens,t);
    }
    pushVector(tokens,makeToken(TOKEN_EOF,"",0,lexer->line,lexer->column));
}

// ----------------- Variables -----------------
typedef struct { char *name; } Variable;
typedef struct { Variable *data; int size; int capacity; } VarTable;
void initVarTable(VarTable *table){ table->size=0; table->capacity=8; table->data=malloc(sizeof(Variable)*table->capacity);}
void pushVarTable(VarTable *table,Variable v){ if(table->size==table->capacity){ table->capacity*=2; table->data=realloc(table->data,sizeof(Variable)*table->capacity);} table->data[table->size++]=v;}
Variable* findVar(VarTable *table,const char *name){ for(int i=0;i<table->size;i++) if(strcmp(table->data[i].name,name)==0) return &table->data[i]; return NULL;}

// ----------------- Parser & Code Generator -----------------
void parseExpression(TokenVector *tokens,int *pos,VarTable *vars){
    Token first=tokens->data[(*pos)++];
    if(first.type==TOKEN_NUMBER) printf("mov rax,%s\n",first.lexeme);
    else{ Variable*v=findVar(vars,first.lexeme); printf("mov rax,[%s]\n",v->name);}
    while(tokens->data[*pos].type==TOKEN_PLUS||tokens->data[*pos].type==TOKEN_MINUS){
        TokenType op=tokens->data[*pos].type;
        (*pos)++;
        Token next=tokens->data[(*pos)++];
        if(next.type==TOKEN_NUMBER){ if(op==TOKEN_PLUS) printf("add rax,%s\n",next.lexeme); else printf("sub rax,%s\n",next.lexeme);}
        else{ Variable*v=findVar(vars,next.lexeme); if(op==TOKEN_PLUS) printf("add rax,[%s]\n",v->name); else printf("sub rax,[%s]\n",v->name);}
    }
}

void parseVarDeclaration(TokenVector *tokens,int *pos,VarTable *vars){
    (*pos)++; // skip int
    Token varName=tokens->data[(*pos)++];
    (*pos)++; // skip '='
    printf("%s: dq 0\n",varName.lexeme);
    parseExpression(tokens,pos,vars);
    printf("mov [%s],rax\n\n",varName.lexeme);
    (*pos)++; // skip ';'
    Variable v; v.name=strdup(varName.lexeme); pushVarTable(vars,v);
}

void parsePrint(TokenVector *tokens,int *pos,VarTable *vars){
    (*pos)++; // skip print
    (*pos)++; // skip '('
    Token var=tokens->data[(*pos)++];
    Variable*v=findVar(vars,var.lexeme);
    printf("mov rax,[%s]\n",v->name);
    // print using syscall write
    printf("mov rdi,1\nmov rsi,rax\nmov rdx,8\nmov rax,1\nsyscall\n");
    (*pos)++; // skip ')'
    (*pos)++; // skip ';'
}

void generateAssembly(TokenVector *tokens){
    printf("section .data\n");
    VarTable vars; initVarTable(&vars);
    int pos=0;
    while(tokens->data[pos].type!=TOKEN_EOF){
        if(tokens->data[pos].type==TOKEN_INT) parseVarDeclaration(tokens,&pos,&vars);
        else if(tokens->data[pos].type==TOKEN_PRINT) parsePrint(tokens,&pos,&vars);
        else pos++;
    }
    printf("section .text\nglobal _start\n_start:\n");
    printf("mov rax,60\nxor rdi,rdi\nsyscall\n");
}

// ----------------- Main -----------------
int main(int argc,char*argv[]){
    if(argc!=2){ printf("Usage: nis <file>\n"); return 1; }
    FILE *f=fopen(argv[1],"r");
    if(!f){ printf("Cannot open file\n"); return 1; }
    fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
    char*source=malloc(size+1); fread(source,1,size,f); source[size]='\0'; fclose(f);
    Lexer lexer={source,0,1,1};
    TokenVector tokens; initVector(&tokens);
    lexSource(&lexer,&tokens);
    generateAssembly(&tokens);
    freeVector(&tokens); free(source);
    return 0;
}
