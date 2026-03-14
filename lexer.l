%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.hpp"
#include "parser.tab.hpp"

int column_number = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = column_number; \
    yylloc.last_column = column_number + yyleng - 1; \
    column_number += yyleng;
%}

%option noyywrap
%option yylineno

DIGIT           [0-9]
PIDENTIFIER     [_a-z]+
WHITESPACE      [ \t\r]
NEWLINE         \n

%%

"PROGRAM"       { return PROGRAM; }
"PROCEDURE"     { return PROCEDURE; }
"IS"            { return IS; }
"IN"            { return IN; }
"END"           { return END; }
"IF"            { return IF; }
"THEN"          { return THEN; }
"ELSE"          { return ELSE; }
"ENDIF"         { return ENDIF; }
"WHILE"         { return WHILE; }
"DO"            { return DO; }
"ENDWHILE"      { return ENDWHILE; }
"REPEAT"        { return REPEAT; }
"UNTIL"         { return UNTIL; }
"FOR"           { return FOR; }
"FROM"          { return FROM; }
"TO"            { return TO; }
"DOWNTO"        { return DOWNTO; }
"ENDFOR"        { return ENDFOR; }
"READ"          { return READ; }
"WRITE"         { return WRITE; }

":="            { return ASSIGN; }
"!="            { return NEQ; }
">="            { return GE; }
"<="            { return LE; }
"="             { return EQ; }
">"             { return GT; }
"<"             { return LT; }

"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULT; }
"/"             { return DIV; }
"%"             { return MOD; }

"T"             { return TYPE_T; }
"I"             { return TYPE_I; }
"O"             { return TYPE_O; }  

","             { return COMMA; }
";"             { return SEMICOLON; }
":"             { return COLON; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }

{PIDENTIFIER}   {
    yylval.str = strdup(yytext);
    return PIDENTIFIER;
}

{DIGIT}+        {
    yylval.str = strdup(yytext);
    return NUM;
}

"#".*           { /* komentarz */ }

{WHITESPACE}+   { /* ignoruj */ }

{NEWLINE}       { column_number = 1; }

.               {
    fprintf(stderr, "Błąd leksykalny w linii %d, kolumnie %d: nieoczekiwany znak '%c' (ASCII %d)\n", 
            yylineno, column_number, yytext[0], yytext[0]);
}

%%