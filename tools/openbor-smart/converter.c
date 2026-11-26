#include "converter.h"
#include "parser.h"
#include "expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t length,capacity; int indent; } Writer;
static int append_n(Writer*w,const char*s,size_t n){
 if(w->length+n+1>w->capacity){size_t c=w->capacity?w->capacity:256;while(c<w->length+n+1)c*=2;
  {char*p=(char*)realloc(w->data,c);if(!p)return 0;w->data=p;w->capacity=c;}}
 memcpy(w->data+w->length,s,n);w->length+=n;w->data[w->length]=0;return 1;
}
static int append(Writer*w,const char*s){return append_n(w,s,strlen(s));}
static int line(Writer*w,const char*s){int i;for(i=0;i<w->indent;i++)if(!append(w,"\t"))return 0;return append(w,s)&&append(w,"\n");}
static void seterr(char*e,size_t n,const char*s){if(e&&n&&!e[0])snprintf(e,n,"%s",s);}
static char *copy_string(const char*s){size_t n=strlen(s);char*p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
static char *expr_lua(const char*s,char*error,size_t es){
 Expr*e=parse_c_expression(s,error,es);char*out;if(!e)return NULL;out=expression_to_lua(e,error,es);free_expression(e);return out;
}
static int has_continue(const Statement*s){
 size_t i,j;if(!s)return 0;
 switch(s->kind){
 case ST_CONTINUE:return 1;
 case ST_BLOCK:for(i=0;i<s->as.block.count;i++)if(has_continue(s->as.block.items[i]))return 1;break;
 case ST_IF:return has_continue(s->as.if_stmt.then_branch)||has_continue(s->as.if_stmt.else_branch);
 case ST_WHILE:return has_continue(s->as.while_stmt.body);
 case ST_DO_WHILE:return has_continue(s->as.do_while_stmt.body);
 case ST_FOR:return has_continue(s->as.for_stmt.body);
 case ST_SWITCH:for(i=0;i<s->as.switch_stmt.count;i++)for(j=0;j<s->as.switch_stmt.cases[i].count;j++)if(has_continue(s->as.switch_stmt.cases[i].statements[j]))return 1;break;
 default:break;
 }return 0;
}
static int emit_statement(Writer*w,const Statement*s,char*error,size_t es);
static int emit_block(Writer*w,const Statement*s,char*error,size_t es){
 size_t i;for(i=0;i<s->as.block.count;i++)if(!emit_statement(w,s->as.block.items[i],error,es))return 0;return 1;
}
static int emit_statement(Writer*w,const Statement*s,char*error,size_t es){
 size_t i,j;char*a=NULL,*b=NULL,*c=NULL;char buf[256];
 if(!s)return 0;
 switch(s->kind){
 case ST_BLOCK:
  if(!line(w,"do"))return 0;++w->indent;
  if(!emit_block(w,s,error,es))return 0;
  --w->indent;return line(w,"end");
 case ST_DECLARATION:
  for(i=0;i<s->as.declaration.count;i++){
   const Declarator*d=&s->as.declaration.items[i];
   if(d->pointer_depth){snprintf(buf,sizeof(buf),"pointer variable '%s' is not supported by Lua conversion",d->name);seterr(error,es,buf);return 0;}
   if(d->array_count){if(d->initializer&&*d->initializer){seterr(error,es,"array initializer lists are not supported");return 0;}
    snprintf(buf,sizeof(buf),"local %s = {}",d->name);if(!line(w,buf))return 0;continue;}
   a=d->initializer&&*d->initializer?expr_lua(d->initializer,error,es):NULL;
   if(d->initializer&&*d->initializer&&!a)return 0;
   if(a)snprintf(buf,sizeof(buf),"local %s = %s",d->name,a);else snprintf(buf,sizeof(buf),"local %s = nil",d->name);
   free(a);if(!line(w,buf))return 0;
  }return 1;
 case ST_RETURN:
  if(!s->as.expression.expression||!*s->as.expression.expression)return line(w,"return");
  a=expr_lua(s->as.expression.expression,error,es);if(!a)return 0;
  {Writer q={0};append(&q,"return ");append(&q,a);free(a);i=line(w,q.data);free(q.data);return (int)i;}
 case ST_EXPRESSION:
  if(!s->as.expression.expression||!*s->as.expression.expression)return 1;
  a=statement_expression_to_lua(s->as.expression.expression,error,es);if(!a)return 0;
  i=line(w,a);free(a);return (int)i;
 case ST_IF:
  a=expr_lua(s->as.if_stmt.condition,error,es);if(!a)return 0;
  {Writer q={0};append(&q,"if ");append(&q,a);append(&q," then");free(a);if(!line(w,q.data)){free(q.data);return 0;}free(q.data);}
  ++w->indent;if(!emit_statement(w,s->as.if_stmt.then_branch,error,es))return 0;--w->indent;
  if(s->as.if_stmt.else_branch){if(!line(w,"else"))return 0;++w->indent;
   if(!emit_statement(w,s->as.if_stmt.else_branch,error,es))return 0;--w->indent;}
  return line(w,"end");
 case ST_WHILE:
  if(has_continue(s->as.while_stmt.body)){seterr(error,es,"continue inside while-loop is not supported yet");return 0;}
  a=expr_lua(s->as.while_stmt.condition,error,es);if(!a)return 0;
  {Writer q={0};append(&q,"while ");append(&q,a);append(&q," do");free(a);if(!line(w,q.data)){free(q.data);return 0;}free(q.data);}
  ++w->indent;if(!emit_statement(w,s->as.while_stmt.body,error,es))return 0;--w->indent;return line(w,"end");
 case ST_DO_WHILE:
  if(has_continue(s->as.do_while_stmt.body)){seterr(error,es,"continue inside do-while is not supported yet");return 0;}
  a=expr_lua(s->as.do_while_stmt.condition,error,es);if(!a)return 0;
  if(!line(w,"repeat")){free(a);return 0;}++w->indent;
  if(!emit_statement(w,s->as.do_while_stmt.body,error,es)){free(a);return 0;}--w->indent;
  {Writer q={0};append(&q,"until not (");append(&q,a);append(&q,")");free(a);i=line(w,q.data);free(q.data);return (int)i;}
 case ST_FOR:
  if(has_continue(s->as.for_stmt.body)){seterr(error,es,"continue inside for-loop is not supported yet");return 0;}
  if(!line(w,"do"))return 0;++w->indent;
  if(s->as.for_stmt.initializer&&!emit_statement(w,s->as.for_stmt.initializer,error,es))return 0;
  a=s->as.for_stmt.condition&&*s->as.for_stmt.condition?expr_lua(s->as.for_stmt.condition,error,es):NULL;
  if(s->as.for_stmt.condition&&*s->as.for_stmt.condition&&!a)return 0;
  if(!a)a=copy_string("true");
  {Writer q={0};append(&q,"while ");append(&q,a);append(&q," do");free(a);if(!line(w,q.data)){free(q.data);return 0;}free(q.data);}
  ++w->indent;if(!emit_statement(w,s->as.for_stmt.body,error,es))return 0;
  if(s->as.for_stmt.post&&*s->as.for_stmt.post){a=statement_expression_to_lua(s->as.for_stmt.post,error,es);if(!a)return 0;i=line(w,a);free(a);if(!i)return 0;}
  --w->indent;if(!line(w,"end"))return 0;--w->indent;return line(w,"end");
 case ST_SWITCH:
  a=expr_lua(s->as.switch_stmt.expression,error,es);if(!a)return 0;
  if(!line(w,"repeat")){free(a);return 0;}++w->indent;
  for(i=0;i<s->as.switch_stmt.count;i++){
   const SwitchCase*item=&s->as.switch_stmt.cases[i];size_t body_count=item->count;
   if(item->is_default&&i+1!=s->as.switch_stmt.count){free(a);seterr(error,es,"default label must be last; C switch fallthrough is not supported");return 0;}
   if(!body_count){free(a);seterr(error,es,"empty switch case may fall through; explicit case bodies are required");return 0;}
   if(item->statements[body_count-1]->kind==ST_BREAK)--body_count;
   else if(item->statements[body_count-1]->kind!=ST_RETURN&&i+1!=s->as.switch_stmt.count){free(a);seterr(error,es,"switch case can fall through; add break or return");return 0;}
   if(item->is_default){if(i==0){if(!line(w,"if true then")){free(a);return 0;}}else if(!line(w,"else")){free(a);return 0;}}
   else {b=expr_lua(item->value,error,es);if(!b){free(a);return 0;}
    {Writer q={0};if(i)append(&q,"elseif ");else append(&q,"if ");append(&q,a);append(&q," == ");append(&q,b);append(&q," then");free(b);if(!line(w,q.data)){free(q.data);free(a);return 0;}free(q.data);}}
   ++w->indent;for(j=0;j<body_count;j++)if(!emit_statement(w,item->statements[j],error,es)){free(a);return 0;}--w->indent;
  }
  free(a);if(!line(w,"end"))return 0;--w->indent;return line(w,"until true");
 case ST_BREAK:return line(w,"break");
 case ST_CONTINUE:seterr(error,es,"continue requires loop-specific rewriting and is not supported");return 0;
 case ST_EMPTY:return 1;
 }
 (void)c;seterr(error,es,"unsupported statement");return 0;
}

char *convert_c_script_body(const char *source, char *error, size_t error_size) {
    const char *body_start, *body_end;
    char *wrapped, *converted, *body;
    size_t source_length, wrapped_length, body_length;

    if (error && error_size) error[0] = '\\0';
    if (!source) {
        seterr(error, error_size, "script body is null");
        return NULL;
    }

    source_length = strlen(source);
    wrapped_length = source_length + 40;
    wrapped = (char *)malloc(wrapped_length);
    if (!wrapped) {
        seterr(error, error_size, "out of memory");
        return NULL;
    }
    snprintf(wrapped, wrapped_length, "void __openbor_script_body__() {\\n%s\\n}", source);
    converted = convert_c_source(wrapped, error, error_size);
    free(wrapped);
    if (!converted) return NULL;

    body_start = strchr(converted, '\\n');
    body_end = NULL;
    {
        const char *p = converted;
        while ((p = strstr(p, "\\nend")) != NULL) {
            body_end = p;
            ++p;
        }
    }
    if (!body_start || !body_end || body_end < body_start) {
        free(converted);
        seterr(error, error_size, "could not extract converted script body");
        return NULL;
    }

    ++body_start;
    body_length = (size_t)(body_end - body_start);
    body = (char *)malloc(body_length + 1);
    if (!body) {
        free(converted);
        seterr(error, error_size, "out of memory");
        return NULL;
    }
    memcpy(body, body_start, body_length);
    body[body_length] = '\\0';
    free(converted);
    return body;
}

char *convert_c_source(const char*source,char*error,size_t error_size){
 FunctionList functions;Writer w={0};size_t i,j;
 if(error&&error_size)error[0]=0;
 if(!parse_c_source(source,&functions,error,error_size))return NULL;
 if(!functions.count){seterr(error,error_size,"no functions found");free_function_list(&functions);return NULL;}
 for(i=0;i<functions.count;i++){
  CFunction*f=&functions.items[i];Writer q={0};
  if(i)append(&w,"\n");
  append(&q,"function ");append(&q,f->name);append(&q,"(");
  for(j=0;j<f->parameter_count;j++){if(j)append(&q,", ");append(&q,f->parameters[j]);}
  append(&q,")");if(!line(&w,q.data)){free(q.data);goto fail;}free(q.data);
  ++w.indent;
  if(f->body && f->body->kind == ST_BLOCK) {
   if(!emit_block(&w,f->body,error,error_size))goto fail;
  } else if(!emit_statement(&w,f->body,error,error_size))goto fail;
  --w.indent;
  if(!line(&w,"end"))goto fail;
 }
 free_function_list(&functions);return w.data;
fail:
 free(w.data);free_function_list(&functions);return NULL;
}
