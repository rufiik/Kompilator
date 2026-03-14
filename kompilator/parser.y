%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "ast.hpp"
void yyerror(const char* s);
int yylex(void);

extern int yylineno;

extern ASTNode* program_root;  
class ProcedureNode;
%}

%union {
    char* str;
    ASTNode* node;
    std::string* str_ptr;
    std::vector<ProcedureNode*>* proc_vec;
}

%token <str> PIDENTIFIER
%token <str> NUM

%token PROGRAM PROCEDURE IS IN END
%token IF THEN ELSE ENDIF
%token WHILE DO ENDWHILE
%token REPEAT UNTIL
%token FOR FROM TO DOWNTO ENDFOR
%token READ WRITE

%token ASSIGN
%token PLUS MINUS MULT DIV MOD
%token EQ NEQ GT LT GE LE

%token COMMA SEMICOLON COLON
%token LPAREN RPAREN LBRACKET RBRACKET

%token TYPE_T TYPE_I TYPE_O

%type <node> program_all main commands command
%type <node> proc_head proc_call args_decl args
%type <node> expression condition value identifier
%type <node> declarations 
%type <proc_vec> procedures
%type <str_ptr> type

%start program_all

%%

program_all:
    procedures main
    {
        ProgramAllNode* prog_all = new ProgramAllNode(@1.first_line);
        
        if ($1) {
            std::vector<ProcedureNode*>* proc_vec = (std::vector<ProcedureNode*>*)$1;
            for (auto proc : *proc_vec) {
                prog_all->addProcedure(proc);
            }
            delete proc_vec;
        }
        
        prog_all->setMainProgram((ProgramNode*)$2);
        program_root = prog_all;
    }
    ;

procedures:
    procedures PROCEDURE proc_head IS declarations IN commands END
    {
        std::vector<ProcedureNode*>* proc_vec = $1;
        
        ProcedureNode* proc = new ProcedureNode(
            (ProcedureHeaderNode*)$3,  
            (DeclarationsNode*)$5,     
            (BlockNode*)$7,           
            @2.first_line
        );
        
        proc_vec->push_back(proc);
        $$ = proc_vec;
    }
    | procedures PROCEDURE proc_head IS IN commands END
    {
        std::vector<ProcedureNode*>* proc_vec = $1;
        
        DeclarationsNode* empty_decls = new DeclarationsNode(@4.first_line);
        
        ProcedureNode* proc = new ProcedureNode(
            (ProcedureHeaderNode*)$3,  
            empty_decls,               
            (BlockNode*)$6,           
            @2.first_line
        );
        
        proc_vec->push_back(proc);
        $$ = proc_vec;
    }
    |   /* puste */
    {
        $$ = new std::vector<ProcedureNode*>();
    }
    ;
main:
    PROGRAM IS declarations IN commands END
    {
        $$ = new ProgramNode(
            $3,  
            (BlockNode*)$5,  
            @1.first_line
        );
    }
    | PROGRAM IS IN commands END
    {
        DeclarationsNode* empty_decls = new DeclarationsNode(@3.first_line);
        $$ = new ProgramNode(
            empty_decls,
            (BlockNode*)$4,
            @1.first_line
        );
    }
    ;
commands:
    commands command
    {
        BlockNode* block = dynamic_cast<BlockNode*>($1);
        block->addStatement($2);
        $$ = block;
    }
    | command
    {
        BlockNode* block = new BlockNode(@$.first_line);
        block->addStatement($1);
        $$ = block;
    }

    ;

command:
    identifier ASSIGN expression SEMICOLON
    {
        $$ = new AssignmentNode($1, $3, @2.first_line);
    }
    | IF condition THEN commands ELSE commands ENDIF
    {
        $$ = new IfNode($2, $4, $6, @1.first_line);
    }
    | IF condition THEN commands ENDIF
    {
        $$ = new IfNode($2, $4, nullptr, @1.first_line);
    }
    | WHILE condition DO commands ENDWHILE
    {
        $$ = new WhileNode($2, $4, @1.first_line);
    }
    | REPEAT commands UNTIL condition SEMICOLON
    {
        $$ = new RepeatNode($2, $4, @1.first_line);
    }
    | FOR PIDENTIFIER FROM value TO value DO commands ENDFOR
    {
        $$ = new ForNode($2, $4, $6, $8, false, @1.first_line);
        free($2);
    }
    | FOR PIDENTIFIER FROM value DOWNTO value DO commands ENDFOR
    {
        $$ = new ForNode($2, $4, $6, $8, true, @1.first_line);
        free($2);
    }
    | proc_call SEMICOLON
    {
        $$ = $1;
    }
    | READ identifier SEMICOLON
    {
        $$ = new ReadNode($2, @1.first_line);
    }
    | WRITE value SEMICOLON
    {
        $$ = new WriteNode($2, @1.first_line);
    }
    ;

proc_head:
    PIDENTIFIER LPAREN args_decl RPAREN
    {
        ProcedureHeaderNode* header = new ProcedureHeaderNode(
            $1, 
            (ParamsListNode*)$3, 
            @1.first_line
        );
        free($1);
        $$ = header;
    }
    ;
proc_call:
    PIDENTIFIER LPAREN args RPAREN
    {
        ArgsListNode* args_list = (ArgsListNode*)$3;
        ProcedureCallNode* call = new ProcedureCallNode($1, args_list, @1.first_line);
        free($1);
        $$ = call;
    }
    ;
declarations:
    declarations COMMA PIDENTIFIER
    {
        DeclarationsNode* decls = (DeclarationsNode*)$1;
        decls->addDeclaration(new VariableDeclNode($3, @3.first_line));
        free($3);
        $$ = decls;
    }
    | declarations COMMA PIDENTIFIER LBRACKET NUM COLON NUM RBRACKET
    {
        DeclarationsNode* decls = (DeclarationsNode*)$1;
        uint64_t start = std::stoull($5);
        uint64_t end = std::stoull($7);
        decls->addDeclaration(new ArrayDeclNode($3, start, end, @3.first_line));
        free($3); free($5); free($7);
        $$ = decls;
    }
    |  PIDENTIFIER
    {
        DeclarationsNode* decls = new DeclarationsNode(@$.first_line);
        decls->addDeclaration(new VariableDeclNode($1, @1.first_line));
        free($1);
        $$ = decls;
    }
    | PIDENTIFIER LBRACKET NUM COLON NUM RBRACKET
    {
        DeclarationsNode* decls = new DeclarationsNode(@$.first_line);
        uint64_t start = std::stoull($3);
        uint64_t end = std::stoull($5);
        decls->addDeclaration(new ArrayDeclNode($1, start, end, @1.first_line));
        free($1); free($3); free($5);
        $$ = decls;
    }
    ;
args_decl:
    args_decl COMMA type PIDENTIFIER
    {
        ParamsListNode* params = (ParamsListNode*)$1;
        
        std::string* type_str = $3;
        bool is_array = (*type_str == "T");
        ParamDeclNode::ParamType ptype = stringToParamType(*type_str);
        
        ParamDeclNode* param = new ParamDeclNode(ptype, $4, is_array, @4.first_line);
        params->addParam(param);
        free($4);
        delete type_str;
        $$ = params;
    }
    | type PIDENTIFIER
    {
        ParamsListNode* params = new ParamsListNode(@1.first_line);
        std::string* type_str = $1;
        bool is_array = (*type_str == "T");
        ParamDeclNode::ParamType ptype = stringToParamType(*type_str);
        
        ParamDeclNode* param = new ParamDeclNode(ptype, $2, is_array, @2.first_line);
        params->addParam(param);
        free($2);
        delete type_str;
        $$ = params;
    }
    ;
type:
    TYPE_T
    {
        $$ = new std::string("T");
    }
    | TYPE_I
    {
        $$ = new std::string("I");
    }
    | TYPE_O
    {
        $$ = new std::string("0");
    }
    |    /* puste */
    {
        $$ = new std::string("");  
    }
    ;
args:
    args COMMA PIDENTIFIER
    {
        ArgsListNode* args_list = (ArgsListNode*)$1;
        VariableNode* arg = new VariableNode($3, @3.first_line);
        args_list->addArg(arg);
        free($3);
        $$ = args_list;
    }
    | PIDENTIFIER
    {
        ArgsListNode* args_list = new ArgsListNode(@$.first_line);
        VariableNode* arg = new VariableNode($1, @1.first_line);
        args_list->addArg(arg);
        free($1);
        $$ = args_list;
    }
    ;
expression:
    value
    {
        $$ = $1;
    }
    | value PLUS value
    {
        $$ = new BinaryOpNode(BinaryOpNode::OpType::ADD, $1, $3, @2.first_line);
    }
    | value MINUS value
    {
        $$ = new BinaryOpNode(BinaryOpNode::OpType::SUB, $1, $3, @2.first_line);
    }
    | value MULT value
    {
        $$ = new BinaryOpNode(BinaryOpNode::OpType::MUL, $1, $3, @2.first_line);
    }
    | value DIV value
    {
        $$ = new BinaryOpNode(BinaryOpNode::OpType::DIV, $1, $3, @2.first_line);
    }
    | value MOD value
    {
        $$ = new BinaryOpNode(BinaryOpNode::OpType::MOD, $1, $3, @2.first_line);
    }
    ;

condition:
    value EQ value
    {
        $$ = new ConditionNode("=", $1, $3, @2.first_line);
    }
    | value NEQ value
    {
        $$ = new ConditionNode("!=", $1, $3, @2.first_line);
    }
    | value GT value
    {
        $$ = new ConditionNode(">", $1, $3, @2.first_line);
    }
    | value LT value
    {
        $$ = new ConditionNode("<", $1, $3, @2.first_line);
    }
    | value GE value
    {
        $$ = new ConditionNode(">=", $1, $3, @2.first_line);
    }
    | value LE value
    {
        $$ = new ConditionNode("<=", $1, $3, @2.first_line);
    }
    ;
value:
    NUM
    {
        NumberNode* num_node = new NumberNode($1, @1.first_line);
        free($1);
        $$ = num_node;
    }
    | identifier
    {
        $$ = $1;
    }
    ;
identifier:
    PIDENTIFIER
    {
        $$ = new VariableNode($1, @1.first_line);
        free($1);
    }
    | PIDENTIFIER LBRACKET PIDENTIFIER RBRACKET
    {
        VariableNode* index_var = new VariableNode($3, @3.first_line);
        ArrayAccessNode* array_access = new ArrayAccessNode($1, index_var, @1.first_line);
        free($1); free($3);
        $$ = array_access;
    }
    | PIDENTIFIER LBRACKET NUM RBRACKET
    {
        NumberNode* index_num = new NumberNode($3, @3.first_line);
        ArrayAccessNode* array_access = new ArrayAccessNode($1, index_num, @1.first_line);
        free($1); free($3);
        $$ = array_access;
    }
    ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "Błąd składniowy w linii %d: %s\n", yylineno, s);
}