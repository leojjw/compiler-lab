%code requires {
  #include <memory>
  #include <string>
  #include <vector>
  #include "../ast.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "../ast.h"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::vector<BaseAST*> *vec_val;
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
}

%token INT VOID CONST RETURN IF ELSE WHILE BREAK CONTINUE LANDOP LOROP
%token <str_val> IDENT RELOP EQOP
%token <int_val> INT_CONST

%type <ast_val> FuncDef FuncFParam Block BlockItem Decl ConstDecl ConstDef ConstInitVal VarDecl VarDef InitVal Stmt OpenStmt MatchedStmt OtherStmt Exp LArrayVal UnaryExp PrimaryExp ConstExp MulExp AddExp RelExp EqExp LAndExp LOrExp
%type <int_val> Number
%type <str_val> LVal UnaryOp
%type <vec_val> DeclsAndDefs FuncFParams BlockItems ConstDefs ConstExps ConstInitVals VarDefs InitVals Exps ArrayExps

%%

CompUnit
  : DeclsAndDefs Decl {
    auto comp_unit = make_unique<CompUnitAST>();
    (*$1).push_back($2);
    comp_unit->declsAndDefs = unique_ptr<std::vector<BaseAST*>>($1);
    ast = move(comp_unit);
  }
  | DeclsAndDefs FuncDef {
    auto comp_unit = make_unique<CompUnitAST>();
    (*$1).push_back($2);
    comp_unit->declsAndDefs = unique_ptr<std::vector<BaseAST*>>($1);
    ast = move(comp_unit);
  }
  ;

DeclsAndDefs
  : DeclsAndDefs Decl {
    (*$1).push_back($2);
    $$ = $1;
  }
  | DeclsAndDefs FuncDef {
    (*$1).push_back($2);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

FuncDef
  : INT IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = string("int");
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    ast->params = false;
    $$ = ast;
  }
  | INT IDENT '(' FuncFParam FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = string("int");
    ast->ident = *unique_ptr<string>($2);
    ast->funcFParam = unique_ptr<BaseAST>($4);
    ast->funcFParams = unique_ptr<std::vector<BaseAST*>>($5);
    ast->block = unique_ptr<BaseAST>($7);
    ast->params = true;
    $$ = ast;
  }
  | VOID IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = string("void");
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    ast->params = false;
    $$ = ast;
  }
  | VOID IDENT '(' FuncFParam FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->func_type = string("void");
    ast->ident = *unique_ptr<string>($2);
    ast->funcFParam = unique_ptr<BaseAST>($4);
    ast->funcFParams = unique_ptr<std::vector<BaseAST*>>($5);
    ast->block = unique_ptr<BaseAST>($7);
    ast->params = true;
    $$ = ast;
  }
  ;

FuncFParam
  : INT IDENT {
    auto ast = new FuncFParamAST();
    ast->bType = string("int");
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  | INT IDENT '[' ']' ConstExps {
    auto ast = new FuncFArrayParamAST();
    ast->bType = string("int");
    ast->ident = *unique_ptr<string>($2);
    ast->constExps = unique_ptr<std::vector<BaseAST*>>($5);
    $$ = ast;
  }
  ;

FuncFParams
  : FuncFParams ',' FuncFParam {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;
  
Decl
  : ConstDecl {
    auto ast = new DeclAST();
    ast->decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | VarDecl {
    auto ast = new DeclAST();
    ast->decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST INT ConstDef ConstDefs ';' {
    auto ast = new ConstDeclAST();
    ast->bType = string("int");
    ast->constDef = unique_ptr<BaseAST>($3);
    ast->constDefs = unique_ptr<std::vector<BaseAST*>>($4);
    $$ = ast;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constInitVal = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' ConstExps '=' ConstInitVal {
    auto ast = new VarArrayDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constExp = unique_ptr<BaseAST>($3);
    ast->constExps = unique_ptr<std::vector<BaseAST*>>($5);
    ast->initVal = unique_ptr<BaseAST>($7);
    ast->init = true;
    $$ = ast;
  }
  ;

ConstExps
  : ConstExps '[' ConstExp ']' {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

ConstDefs
  : ConstDefs ',' ConstDef {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

ConstInitVal
  : ConstExp {
    auto ast = new ConstInitValAST();
    ast->constExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}' {
    auto ast = new InitArrayValAST();
    ast->init = false;
    $$ = ast;
  }
  | '{'ConstInitVal ConstInitVals '}' {
    auto ast = new InitArrayValAST();
    ast->initVal = unique_ptr<BaseAST>($2);
    ast->initVals = unique_ptr<std::vector<BaseAST*>>($3);
    ast->init = true;
    $$ = ast;
  }
  ;

ConstInitVals
  : ConstInitVals ',' ConstInitVal {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

VarDecl
  : INT VarDef VarDefs ';' {
    auto ast = new VarDeclAST();
    ast->bType = string("int");
    ast->varDef = unique_ptr<BaseAST>($2);
    ast->varDefs = unique_ptr<std::vector<BaseAST*>>($3);
    $$ = ast;
  }
  ;

VarDef
  : IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->initVal = unique_ptr<BaseAST>($3);
    ast->init = true;
    $$ = ast;
  }
  | IDENT {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init = false;
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' ConstExps '=' InitVal {
    auto ast = new VarArrayDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constExp = unique_ptr<BaseAST>($3);
    ast->constExps = unique_ptr<std::vector<BaseAST*>>($5);
    ast->initVal = unique_ptr<BaseAST>($7);
    ast->init = true;
    $$ = ast;
  }
  | IDENT '[' ConstExp ']' ConstExps {
    auto ast = new VarArrayDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constExp = unique_ptr<BaseAST>($3);
    ast->constExps = unique_ptr<std::vector<BaseAST*>>($5);
    ast->init = false;
    $$ = ast;
  }
  ;

VarDefs
  : VarDefs ',' VarDef {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

InitVal
  : Exp {
    auto ast = new InitValAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}' {
    auto ast = new InitArrayValAST();
    ast->init = false;
    $$ = ast;
  }
  | '{' InitVal InitVals '}' {
    auto ast = new InitArrayValAST();
    ast->initVal = unique_ptr<BaseAST>($2);
    ast->initVals = unique_ptr<std::vector<BaseAST*>>($3);
    ast->init = true;
    $$ = ast;
  }
  ;

InitVals
  : InitVals ',' InitVal {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;


Block
  : '{' BlockItems '}' {
    auto ast = new BlockAST();
    ast->blockItems = unique_ptr<std::vector<BaseAST*>>($2);
    $$ = ast;
  }
  ;

BlockItem
  : Decl {
    $$ = $1;
  }
  | Stmt {
    $$ = $1;
  }
  ;

BlockItems
  : BlockItems BlockItem {
    (*$1).push_back($2);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

Stmt
  : OpenStmt {
    $$ = $1;
  }
  | MatchedStmt {
    $$ = $1;
  }
  ;

OpenStmt
  : IF '(' Exp ')' Stmt {
    auto ast = new IfElseAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->ifStmt = unique_ptr<BaseAST>($5);
    ast->isElse = false;
    $$ = ast;
  }
  | IF '(' Exp ')' MatchedStmt ELSE OpenStmt {
    auto ast = new IfElseAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->ifStmt = unique_ptr<BaseAST>($5);
    ast->elseStmt = unique_ptr<BaseAST>($7);
    ast->isElse = true;
    $$ = ast;
  }
  ;

MatchedStmt
  : OtherStmt {
    $$ = $1;
  }
  | IF '(' Exp ')' MatchedStmt ELSE MatchedStmt {
    auto ast = new IfElseAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->ifStmt = unique_ptr<BaseAST>($5);
    ast->elseStmt = unique_ptr<BaseAST>($7);
    ast->isElse = true;
    $$ = ast;
  }
  ;

OtherStmt
  : LVal '=' Exp ';' {
    auto ast = new Stmt1AST();
    ast->lVal = *unique_ptr<string>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | LArrayVal '=' Exp ';' {
    auto ast = new Stmt2AST();
    ast->lVal = unique_ptr<BaseAST>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | ';' {
    auto ast = new Stmt3AST();
    ast->isExp = false;
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new Stmt3AST();
    ast->exp = unique_ptr<BaseAST>($1);
    ast->isExp = true;
    $$ = ast;
  }
  | Block {
    auto ast = new Stmt4AST();
    ast->block = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new Stmt5AST();
    ast->isExp = false;
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new Stmt5AST();
    ast->exp = unique_ptr<BaseAST>($2);
    ast->isExp = true;
    $$ = ast;
  }
  | WHILE '(' Exp ')' Stmt {
    auto ast = new Stmt6AST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new Stmt7AST();
    ast->isBreak = true;
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new Stmt7AST();
    ast->isBreak = false;
    $$ = ast;
  }
  ;

Exp
  : UnaryExp {
    auto ast = new ExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp {
    auto ast = new ExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp {
    auto ast = new ExpAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

Exps
  : Exps ',' Exp {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

LVal
  : IDENT {
    $$ = $1;
  }
  ;

LArrayVal
  : IDENT '[' Exp ']' ArrayExps {
    auto ast = new LArrayValAST();
    ast->ident = *unique_ptr<string>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    ast->exps = unique_ptr<std::vector<BaseAST*>>($5);
    $$ = ast;
  }
  ;

ArrayExps
  : ArrayExps '[' Exp ']' {
    (*$1).push_back($3);
    $$ = $1;
  }
  | /* empty */ {
    $$ = new std::vector<BaseAST*>;
  }
  ;

PrimaryExp
  : '(' Exp ')' {
    auto ast = new PrimaryExp1AST();
    ast->exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | Number {
    auto ast = new PrimaryExp2AST();
    ast->number = $1;
    $$ = ast;
  }
  | LVal {
    auto ast = new PrimaryExp3AST();
    ast->lVal = *unique_ptr<string>($1);
    $$ = ast;
  }
  | LArrayVal {
    auto ast = new PrimaryExp4AST();
    ast->lVal = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

Number
  : INT_CONST {
    $$ = $1;
  }
  ;

ConstExp
  : Exp {
    $$ = $1;
  }

UnaryExp
  : PrimaryExp {
    auto ast = new UnaryExp1AST();
    ast->primaryExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | IDENT '(' ')' {
    auto ast = new UnaryExp2AST();
    ast->ident = *unique_ptr<string>($1);
    ast->params = false;
    $$ = ast;
  }
  | IDENT '(' Exp Exps ')' {
    auto ast = new UnaryExp2AST();
    ast->ident = *unique_ptr<string>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    ast->exps = unique_ptr<std::vector<BaseAST*>>($4);
    ast->params = true;
    $$ = ast;
  }
  | UnaryOp UnaryExp {
    auto ast = new UnaryExp3AST();
    ast->unaryOp = *unique_ptr<string>($1);
    ast->unaryExp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  ;

UnaryOp
  : '+' {
    $$ = new string("+");
  }
  | '-' {
    $$ = new string("-");
  }
  | '!' {
    $$ = new string("!");
  }
  ;

MulExp
  : UnaryExp {
    auto ast = new MulExp1AST();
    ast->unaryExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp '*' UnaryExp {
    auto ast = new MulExp2AST();
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->op = string("*");
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    auto ast = new MulExp2AST();
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->op = string("/");
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    auto ast = new MulExp2AST();
    ast->mulExp = unique_ptr<BaseAST>($1);
    ast->op = string("%");
    ast->unaryExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

AddExp
  : MulExp {
    auto ast = new AddExp1AST();
    ast->mulExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    auto ast = new AddExp2AST();
    ast->addExp = unique_ptr<BaseAST>($1);
    ast->op = string("+");
    ast->mulExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    auto ast = new AddExp2AST();
    ast->addExp = unique_ptr<BaseAST>($1);
    ast->op = string("-");
    ast->mulExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

RelExp
  : AddExp {
    auto ast = new RelExp1AST();
    ast->addExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp RELOP AddExp {
    auto ast = new RelExp2AST();
    ast->relExp = unique_ptr<BaseAST>($1);
    ast->op = *unique_ptr<string>($2);
    ast->addExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    auto ast = new EqExp1AST();
    ast->relExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EQOP RelExp {
    auto ast = new EqExp2AST();
    ast->eqExp = unique_ptr<BaseAST>($1);
    ast->op = *unique_ptr<string>($2);
    ast->relExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;
  
LAndExp
  : EqExp {
    auto ast = new LAndExp1AST();
    ast->eqExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp LANDOP EqExp {
    auto ast = new LAndExp2AST();
    ast->lAndExp = unique_ptr<BaseAST>($1);
    ast->op = string("&&");
    ast->eqExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;
  
LOrExp
  : LAndExp {
    auto ast = new LOrExp1AST();
    ast->lAndExp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp LOROP LAndExp {
    auto ast = new LOrExp2AST();
    ast->lOrExp = unique_ptr<BaseAST>($1);
    ast->op = string("||");
    ast->lAndExp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

%%

void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}