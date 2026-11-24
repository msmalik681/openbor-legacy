#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *source;
    Token *tokens;
    size_t count, pos;
    char *error;
    size_t error_size;
} Parser;

static char *copy_text(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n); p[n] = '\0'; return p;
}

static Token *current(Parser *p) { return p->pos < p->count ? &p->tokens[p->pos] : NULL; }
static const char *cur(Parser *p) { Token *t = current(p); return t ? t->text : ""; }
static int at_end(Parser *p) { return p->pos >= p->count; }
static int accept(Parser *p, const char *s) {
    if (strcmp(cur(p), s)) return 0;
    ++p->pos; return 1;
}
static void fail(Parser *p, const char *message) {
    if (!p->error || !p->error_size || p->error[0]) return;
    if (current(p)) snprintf(p->error, p->error_size, "line %u, column %u: %s",
        current(p)->line, current(p)->column, message);
    else snprintf(p->error, p->error_size, "end of input: %s", message);
}
static int type_word(const char *s) {
    static const char *const words[] = {
        "void","char","short","int","long","float","double","signed","unsigned",
        "const","volatile","static","extern","register","struct","union","enum",NULL
    };
    int i; for (i=0; words[i]; ++i) if (!strcmp(s, words[i])) return 1;
    return 0;
}
static char *join_words(char **words, size_t count) {
    size_t i, n = 1; char *out, *at;
    for (i=0;i<count;i++) n += strlen(words[i]) + (i ? 1 : 0);
    out=(char *)malloc(n); if(!out) return NULL; at=out;
    for(i=0;i<count;i++){ if(i)*at++=' '; strcpy(at,words[i]); at+=strlen(words[i]); }
    *at='\0'; return out;
}
static char *parse_type(Parser *p) {
    char *parts[64]; size_t n=0;
    while (!at_end(p) && type_word(cur(p)) && n<64) {
        const char *word=cur(p); parts[n++]=copy_text(word,strlen(word)); ++p->pos;
        if ((!strcmp(word,"struct") || !strcmp(word,"union") || !strcmp(word,"enum"))
            && current(p) && current(p)->kind==TOK_IDENTIFIER && n<64) {
            parts[n++]=copy_text(cur(p),strlen(cur(p))); ++p->pos;
        }
    }
    if (!n) { fail(p,"expected C type"); return NULL; }
    {
        char *result=join_words(parts,n); size_t i;
        for(i=0;i<n;i++) free(parts[i]);
        return result;
    }
}
static char *slice_tokens(Parser *p, size_t from, size_t to) {
    size_t start,end;
    if (from>=to) return copy_text("",0);
    start=p->tokens[from].offset;
    end=p->tokens[to-1].offset+p->tokens[to-1].length;
    while(end>start && (p->source[end-1]==' ' || p->source[end-1]=='\t' ||
          p->source[end-1]=='\r' || p->source[end-1]=='\n')) --end;
    while(start<end && (p->source[start]==' ' || p->source[start]=='\t' ||
          p->source[start]=='\r' || p->source[start]=='\n')) ++start;
    return copy_text(p->source+start,end-start);
}
static char *collect_until(Parser *p, const char *a, const char *b) {
    size_t start=p->pos; int round=0,square=0,curly=0;
    while(!at_end(p)) {
        const char *s=cur(p);
        if(!round&&!square&&!curly && (!strcmp(s,a) || (b && !strcmp(s,b))))
            return slice_tokens(p,start,p->pos);
        if(!strcmp(s,"(")) ++round; else if(!strcmp(s,")")) --round;
        else if(!strcmp(s,"[")) ++square; else if(!strcmp(s,"]")) --square;
        else if(!strcmp(s,"{")) ++curly; else if(!strcmp(s,"}")) --curly;
        if(round<0||square<0||curly<0){fail(p,"unmatched closing delimiter");return NULL;}
        ++p->pos;
    }
    fail(p,"unexpected end of input"); return NULL;
}
static Statement *new_stmt(StatementKind kind) {
    Statement *s=(Statement *)calloc(1,sizeof(*s)); if(s)s->kind=kind; return s;
}
static int add_stmt(Statement ***items,size_t *count,Statement *s) {
    Statement **p=(Statement **)realloc(*items,(*count+1)*sizeof(**items));
    if(!p)return 0; *items=p; p[(*count)++]=s; return 1;
}
static int add_decl(Declaration *d,Declarator item) {
    Declarator *p=(Declarator *)realloc(d->items,(d->count+1)*sizeof(*p));
    if(!p)return 0; d->items=p; p[d->count++]=item; return 1;
}
static int add_array(Declarator *d,char *size) {
    char **p=(char **)realloc(d->array_sizes,(d->array_count+1)*sizeof(*p));
    if(!p)return 0; d->array_sizes=p; p[d->array_count++]=size; return 1;
}
static Statement *parse_statement(Parser *p);
static Statement *parse_block(Parser *p) {
    Statement *s=new_stmt(ST_BLOCK);
    if(!s)return NULL;
    while(!at_end(p) && strcmp(cur(p),"}") && (!p->error || !p->error[0])) {
        Statement *child=parse_statement(p);
        if(!child || !add_stmt(&s->as.block.items,&s->as.block.count,child)) return s;
    }
    if(!accept(p,"}")) fail(p,"unterminated block");
    return s;
}
static int parse_arrays(Parser *p,Declarator *d) {
    while(accept(p,"[")) {
        char *size=collect_until(p,"]",NULL);
        if(!size || !accept(p,"]") || !add_array(d,size)) return 0;
    }
    return 1;
}
static Statement *parse_declaration(Parser *p) {
    Statement *s=new_stmt(ST_DECLARATION); Declaration *d=&s->as.declaration;
    d->type_name=parse_type(p);
    if(!d->type_name)return s;
    for(;;) {
        Declarator item; memset(&item,0,sizeof(item));
        while(accept(p,"*")) ++item.pointer_depth;
        if(!current(p)||current(p)->kind!=TOK_IDENTIFIER){fail(p,"expected variable name");return s;}
        item.name=copy_text(cur(p),strlen(cur(p))); ++p->pos;
        if(!parse_arrays(p,&item)) return s;
        if(accept(p,"=")) item.initializer=collect_until(p,",",";");
        if(!add_decl(d,item)) return s;
        if(accept(p,";")) return s;
        if(!accept(p,",")){fail(p,"expected ',' or ';' in declaration");return s;}
    }
}
static char *parenthesized(Parser *p,const char *what) {
    char msg[128]; char *value;
    snprintf(msg,sizeof(msg),"expected '(' after %s",what);
    if(!accept(p,"(")){fail(p,msg);return NULL;}
    value=collect_until(p,")",NULL);
    snprintf(msg,sizeof(msg),"expected ')' after %s condition",what);
    if(!value||!accept(p,")")){fail(p,msg);free(value);return NULL;}
    if(!*value){fail(p,"empty condition");free(value);return NULL;}
    return value;
}
static Statement *parse_statement(Parser *p) {
    Statement *s; char *expr;
    if(accept(p,";")) return new_stmt(ST_EMPTY);
    if(accept(p,"{")) return parse_block(p);
    if(accept(p,"return")) {
        s=new_stmt(ST_RETURN); expr=collect_until(p,";",NULL);
        if(!expr||!accept(p,";")) fail(p,"expected ';' after return");
        s->as.expression.expression=expr; return s;
    }
    if(accept(p,"break")) {
        s=new_stmt(ST_BREAK); if(!accept(p,";"))fail(p,"expected ';' after break"); return s;
    }
    if(accept(p,"continue")) {
        s=new_stmt(ST_CONTINUE); if(!accept(p,";"))fail(p,"expected ';' after continue"); return s;
    }
    if(accept(p,"if")) {
        char *condition=parenthesized(p,"if"); Statement *then_s=parse_statement(p);
        s=new_stmt(ST_IF); s->as.if_stmt.condition=condition;s->as.if_stmt.then_branch=then_s;
        if(accept(p,"else"))s->as.if_stmt.else_branch=parse_statement(p);
        return s;
    }
    if(accept(p,"while")) {
        char *condition=parenthesized(p,"while"); s=new_stmt(ST_WHILE);
        s->as.while_stmt.condition=condition;s->as.while_stmt.body=parse_statement(p);return s;
    }
    if(accept(p,"do")) {
        Statement *body=parse_statement(p); char *condition;
        if(!accept(p,"while"))fail(p,"expected 'while' after do body");
        condition=parenthesized(p,"do-while");
        if(!accept(p,";"))fail(p,"expected ';' after do-while");
        s=new_stmt(ST_DO_WHILE);s->as.do_while_stmt.body=body;s->as.do_while_stmt.condition=condition;return s;
    }
    if(accept(p,"for")) {
        char *init; s=new_stmt(ST_FOR);
        if(!accept(p,"(")){fail(p,"expected '(' after for");return s;}
        if(!strcmp(cur(p),";")){++p->pos;}
        else if(type_word(cur(p)))s->as.for_stmt.initializer=parse_declaration(p);
        else {init=collect_until(p,";",NULL);if(!accept(p,";"))fail(p,"expected ';' after for initializer");
            if(init&&*init){Statement *x=new_stmt(ST_EXPRESSION);x->as.expression.expression=init;s->as.for_stmt.initializer=x;}else free(init);}
        s->as.for_stmt.condition=collect_until(p,";",NULL);
        if(!accept(p,";"))fail(p,"expected ';' after for condition");
        s->as.for_stmt.post=collect_until(p,")",NULL);
        if(!accept(p,")"))fail(p,"expected ')' after for clauses");
        s->as.for_stmt.body=parse_statement(p);return s;
    }
    if(accept(p,"switch")) {
        s=new_stmt(ST_SWITCH);s->as.switch_stmt.expression=parenthesized(p,"switch");
        if(!accept(p,"{")){fail(p,"expected '{' after switch");return s;}
        while(!at_end(p)&&strcmp(cur(p),"}")&&(!p->error||!p->error[0])) {
            SwitchCase item;memset(&item,0,sizeof(item));
            if(accept(p,"case"))item.value=collect_until(p,":",NULL);
            else if(accept(p,"default"))item.is_default=1;
            else {fail(p,"expected case or default label");break;}
            if(!accept(p,":")){fail(p,"expected ':' after switch label");break;}
            while(!at_end(p)&&strcmp(cur(p),"}")&&strcmp(cur(p),"case")&&strcmp(cur(p),"default")&&(!p->error||!p->error[0])) {
                Statement *child=parse_statement(p);
                Statement **q=(Statement **)realloc(item.statements,(item.count+1)*sizeof(*q));
                if(!q)break;item.statements=q;item.statements[item.count++]=child;
            }
            {SwitchCase *q=(SwitchCase *)realloc(s->as.switch_stmt.cases,(s->as.switch_stmt.count+1)*sizeof(*q));
             if(!q)break;s->as.switch_stmt.cases=q;q[s->as.switch_stmt.count++]=item;}
        }
        if(!accept(p,"}"))fail(p,"expected '}' after switch");return s;
    }
    if(type_word(cur(p)))return parse_declaration(p);
    expr=collect_until(p,";",NULL);
    if(!expr||!accept(p,";")){fail(p,"expected ';' after expression");free(expr);return NULL;}
    s=new_stmt(ST_EXPRESSION);s->as.expression.expression=expr;return s;
}
static int add_function(FunctionList *list,CFunction fn) {
    CFunction *p=(CFunction *)realloc(list->items,(list->count+1)*sizeof(*p));
    if(!p)return 0;list->items=p;p[list->count++]=fn;return 1;
}
int parse_c_source(const char *source,FunctionList *out,char *error,size_t error_size) {
    TokenList all; Parser p; size_t i; memset(out,0,sizeof(*out));
    if(error&&error_size)error[0]='\0';
    if(!lex_source(source,&all,error,error_size))return 0;
    p.source=source;p.tokens=(Token *)malloc(all.count*sizeof(Token));p.count=0;p.pos=0;p.error=error;p.error_size=error_size;
    if(all.count&&!p.tokens){free_tokens(&all);return 0;}
    for(i=0;i<all.count;i++)if(all.items[i].kind!=TOK_WHITESPACE&&all.items[i].kind!=TOK_COMMENT)p.tokens[p.count++]=all.items[i];
    for(i=0;i<all.count;i++)if(all.items[i].kind==TOK_WHITESPACE||all.items[i].kind==TOK_COMMENT)free(all.items[i].text);
    free(all.items);
    while(!at_end(&p)&&(!error||!error[0])) {
        CFunction fn; size_t param_count=0; memset(&fn,0,sizeof(fn));
        Token *start=current(&p);fn.line=start?start->line:1;
        fn.return_type=parse_type(&p);
        if(!fn.return_type)break;
        while(accept(&p,"*")){} /* pointer return types are retained as type spelling later */
        if(!current(&p)||current(&p)->kind!=TOK_IDENTIFIER){fail(&p,"expected function name");break;}
        fn.name=copy_text(cur(&p),strlen(cur(&p)));++p.pos;
        if(!accept(&p,"(")){fail(&p,"expected '(' after function name");break;}
        if(accept(&p,")")){}
        else if(!strcmp(cur(&p),"void")&&!strcmp(p.pos+1<p.count?p.tokens[p.pos+1].text:"",")"))p.pos+=2;
        else for(;;) {
            char *type=parse_type(&p);int pointers=0;char *name;
            while(accept(&p,"*"))++pointers;
            if(!current(&p)||current(&p)->kind!=TOK_IDENTIFIER){free(type);fail(&p,"expected parameter name");break;}
            name=copy_text(cur(&p),strlen(cur(&p)));++p.pos;
            while(accept(&p,"[")){char *unused=collect_until(&p,"]",NULL);free(unused);accept(&p,"]");}
            free(type);(void)pointers;
            {char **q=(char **)realloc(fn.parameters,(param_count+1)*sizeof(*q));if(!q)break;fn.parameters=q;fn.parameters[param_count++]=name;}
            if(accept(&p,")"))break;
            if(!accept(&p,",")){fail(&p,"expected ',' or ')' in parameter list");break;}
        }
        fn.parameter_count=param_count;
        if(!accept(&p,"{")){fail(&p,"expected function body");break;}
        fn.body=parse_block(&p);
        if(!fn.body||!add_function(out,fn)){fail(&p,"out of memory");break;}
    }
    free(p.tokens);
    return !error||!error[0];
}
static void free_statement(Statement *s) {
    size_t i,j;if(!s)return;
    switch(s->kind){
    case ST_BLOCK:for(i=0;i<s->as.block.count;i++)free_statement(s->as.block.items[i]);free(s->as.block.items);break;
    case ST_DECLARATION:
        free(s->as.declaration.type_name);
        for(i=0;i<s->as.declaration.count;i++){Declarator*d=&s->as.declaration.items[i];free(d->name);free(d->initializer);
            for(j=0;j<d->array_count;j++)free(d->array_sizes[j]);free(d->array_sizes);}
        free(s->as.declaration.items);break;
    case ST_RETURN:case ST_EXPRESSION:free(s->as.expression.expression);break;
    case ST_IF:free(s->as.if_stmt.condition);free_statement(s->as.if_stmt.then_branch);free_statement(s->as.if_stmt.else_branch);break;
    case ST_WHILE:free(s->as.while_stmt.condition);free_statement(s->as.while_stmt.body);break;
    case ST_DO_WHILE:free(s->as.do_while_stmt.condition);free_statement(s->as.do_while_stmt.body);break;
    case ST_FOR:free_statement(s->as.for_stmt.initializer);free(s->as.for_stmt.condition);free(s->as.for_stmt.post);free_statement(s->as.for_stmt.body);break;
    case ST_SWITCH:
        free(s->as.switch_stmt.expression);
        for(i=0;i<s->as.switch_stmt.count;i++){SwitchCase*c=&s->as.switch_stmt.cases[i];free(c->value);
            for(j=0;j<c->count;j++)free_statement(c->statements[j]);free(c->statements);}
        free(s->as.switch_stmt.cases);break;
    default:break;
    }free(s);
}
void free_function_list(FunctionList *functions) {
    size_t i,j;if(!functions)return;
    for(i=0;i<functions->count;i++){CFunction*f=&functions->items[i];free(f->return_type);free(f->name);
        for(j=0;j<f->parameter_count;j++)free(f->parameters[j]);free(f->parameters);free_statement(f->body);}
    free(functions->items);memset(functions,0,sizeof(*functions));
}
