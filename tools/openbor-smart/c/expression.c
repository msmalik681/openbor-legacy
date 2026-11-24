#include "expression.h"
#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { Token *t; size_t n,pos; char *error; size_t error_size; } EParser;
static char *dup_n(const char*s,size_t n){char*p=(char*)malloc(n+1);if(p){memcpy(p,s,n);p[n]=0;}return p;}
static Expr *node(ExprKind k){Expr*e=(Expr*)calloc(1,sizeof(*e));if(e)e->kind=k;return e;}
static const char *cur(EParser*p){return p->pos<p->n?p->t[p->pos].text:"";}
static int accept(EParser*p,const char*s){if(strcmp(cur(p),s))return 0;++p->pos;return 1;}
static void err(EParser*p,const char*s){if(!p->error||!p->error_size||p->error[0])return;
 if(p->pos<p->n)snprintf(p->error,p->error_size,"line %u, column %u: %s",p->t[p->pos].line,p->t[p->pos].column,s);
 else snprintf(p->error,p->error_size,"end of expression: %s",s);}
static int type_word(const char*s){
 static const char*const a[]={"void","char","short","int","long","float","double","signed","unsigned","const","volatile","static","extern","register","struct","union","enum",NULL};
 int i;for(i=0;a[i];i++)if(!strcmp(a[i],s))return 1;return 0;
}
static int precedence(const char*op,int*right){
 *right=0;
 if(!strcmp(op,","))return 1;
 if(!strcmp(op,"=")||!strcmp(op,"+=")||!strcmp(op,"-=")||!strcmp(op,"*=")||!strcmp(op,"/=")||!strcmp(op,"%=")||!strcmp(op,"<<=")||!strcmp(op,">>=")||!strcmp(op,"&=")||!strcmp(op,"^=")||!strcmp(op,"|=")){*right=1;return 2;}
 if(!strcmp(op,"||"))return 4;if(!strcmp(op,"&&"))return 5;if(!strcmp(op,"|"))return 6;if(!strcmp(op,"^"))return 7;if(!strcmp(op,"&"))return 8;
 if(!strcmp(op,"==")||!strcmp(op,"!="))return 9;
 if(!strcmp(op,"<")||!strcmp(op,"<=")||!strcmp(op,">")||!strcmp(op,">="))return 10;
 if(!strcmp(op,"<<")||!strcmp(op,">>"))return 11;
 if(!strcmp(op,"+")||!strcmp(op,"-"))return 12;
 if(!strcmp(op,"*")||!strcmp(op,"/")||!strcmp(op,"%"))return 13;return 0;
}
static int prefix(const char*s){return !strcmp(s,"+")||!strcmp(s,"-")||!strcmp(s,"!")||!strcmp(s,"~")||!strcmp(s,"*")||!strcmp(s,"&")||!strcmp(s,"++")||!strcmp(s,"--");}
static Expr *parse_expr(EParser*p,int min);
static int append_expr(Expr*e,Expr*x){Expr**q=(Expr**)realloc(e->items,(e->count+1)*sizeof(*q));if(!q)return 0;e->items=q;e->items[e->count++]=x;return 1;}
static int read_type(EParser*p,size_t start,size_t*end,char**out){
 size_t i=start,n=0,cap=8;char**parts=(char**)malloc(cap*sizeof(*parts));size_t bytes=1,j;
 if(i>=p->n||!type_word(p->t[i].text)){free(parts);return 0;}
 while(i<p->n&&type_word(p->t[i].text)){
  if(n==cap){cap*=2;parts=(char**)realloc(parts,cap*sizeof(*parts));}
  parts[n++]=p->t[i++].text;
  if((!strcmp(parts[n-1],"struct")||!strcmp(parts[n-1],"union")||!strcmp(parts[n-1],"enum"))&&i<p->n&&p->t[i].kind==TOK_IDENTIFIER){if(n==cap){cap*=2;parts=(char**)realloc(parts,cap*sizeof(*parts));}parts[n++]=p->t[i++].text;}
 }
 while(i<p->n&&!strcmp(p->t[i].text,"*")){if(n==cap){cap*=2;parts=(char**)realloc(parts,cap*sizeof(*parts));}parts[n++]="*";++i;}
 for(j=0;j<n;j++)bytes+=strlen(parts[j])+(j?1:0);
 *out=(char*)malloc(bytes);if(!*out){free(parts);return 0;}(*out)[0]=0;
 for(j=0;j<n;j++){if(j)strcat(*out," ");strcat(*out,parts[j]);}
 *end=i;free(parts);return 1;
}
static Expr *parse_postfix(EParser*p,Expr*e){
 for(;;){
  if(accept(p,"(")){
   Expr*x=node(EXPR_CALL);if(!x)return e;x->left=e;
   if(!accept(p,")"))for(;;){Expr*a=parse_expr(p,2);if(!a||!append_expr(x,a))return x;if(accept(p,")"))break;if(!accept(p,",")){err(p,"expected ',' or ')' in call");break;}}
   e=x;
  }else if(accept(p,"[")){
   Expr*x=node(EXPR_INDEX);x->left=e;x->right=parse_expr(p,1);if(!accept(p,"]"))err(p,"expected ']' after array index");e=x;
  }else if(!strcmp(cur(p),".")||!strcmp(cur(p),"->")){
   int ptr=!strcmp(cur(p),"->");++p->pos;
   if(p->pos>=p->n||p->t[p->pos].kind!=TOK_IDENTIFIER){err(p,"expected member name");return e;}
   {Expr*x=node(EXPR_MEMBER);x->left=e;x->text=dup_n(cur(p),strlen(cur(p)));x->flag=ptr;++p->pos;e=x;}
  }else if(!strcmp(cur(p),"++")||!strcmp(cur(p),"--")){
   Expr*x=node(EXPR_UNARY);x->text=dup_n(cur(p),strlen(cur(p)));x->left=e;x->flag=1;++p->pos;e=x;
  }else break;
 }
 return e;
}
static Expr *parse_prefix(EParser*p){
 Expr*e;Token*t;
 if(p->pos>=p->n){err(p,"expected expression");return NULL;}t=&p->t[p->pos];
 if(t->kind==TOK_IDENTIFIER){
  ++p->pos;
  if(!strcmp(t->text,"sizeof")){
   if(accept(p,"(")){
    size_t end;char*typ=NULL;
    if(read_type(p,p->pos,&end,&typ)&&end<p->n&&!strcmp(p->t[end].text,")")){
     e=node(EXPR_SIZEOF_TYPE);e->text=typ;p->pos=end+1;return e;
    }
    free(typ);e=node(EXPR_UNARY);e->text=dup_n("sizeof",6);e->left=parse_expr(p,1);
    if(!accept(p,")"))err(p,"expected ')' after sizeof");return e;
   }
   e=node(EXPR_UNARY);e->text=dup_n("sizeof",6);e->left=parse_expr(p,14);return e;
  }
  e=node(EXPR_IDENTIFIER);e->text=dup_n(t->text,strlen(t->text));return e;
 }
 if(t->kind==TOK_NUMBER||t->kind==TOK_STRING||t->kind==TOK_CHARACTER){
  ++p->pos;e=node(EXPR_LITERAL);e->text=dup_n(t->text,strlen(t->text));e->flag=(int)t->kind;return e;
 }
 if(prefix(t->text)){
  ++p->pos;e=node(EXPR_UNARY);e->text=dup_n(t->text,strlen(t->text));e->left=parse_expr(p,14);return e;
 }
 if(accept(p,"(")){
  size_t end;char*typ=NULL;
  if(read_type(p,p->pos,&end,&typ)&&end<p->n&&!strcmp(p->t[end].text,")")){
   p->pos=end+1;e=node(EXPR_CAST);e->text=typ;e->left=parse_expr(p,14);return e;
  }
  free(typ);e=parse_expr(p,1);if(!accept(p,")"))err(p,"expected ')' in grouped expression");return e;
 }
 err(p,"expected expression");return NULL;
}
static Expr *parse_expr(EParser*p,int min){
 Expr*left=parse_prefix(p);int right,prec;
 if(!left)return NULL;left=parse_postfix(p,left);
 for(;;){
  if(!strcmp(cur(p),"?")){
   if(3<min)break;++p->pos;
   {Expr*x=node(EXPR_CONDITIONAL);x->left=left;x->right=parse_expr(p,1);
    if(!accept(p,":"))err(p,"expected ':' in conditional expression");
    x->third=parse_expr(p,3);left=x;}continue;
  }
  prec=precedence(cur(p),&right);if(!prec||prec<min)break;
  {char*op=dup_n(cur(p),strlen(cur(p)));Expr*r; ++p->pos;r=parse_expr(p,right?prec:prec+1);
   if(!strcmp(op,",")){Expr*x;if(left->kind==EXPR_COMMA)x=left;else{x=node(EXPR_COMMA);append_expr(x,left);}append_expr(x,r);left=x;}
   else {Expr*x=node(EXPR_BINARY);x->text=op;x->left=left;x->right=r;left=x;continue;}
   free(op);}
 }
 return left;
}
Expr *parse_c_expression(const char*source,char*error,size_t error_size){
 TokenList all;EParser p;Expr*e;size_t i;
 if(error&&error_size)error[0]=0;if(!lex_source(source,&all,error,error_size))return NULL;
 memset(&p,0,sizeof(p));p.t=(Token*)malloc(all.count*sizeof(Token));p.error=error;p.error_size=error_size;
 for(i=0;i<all.count;i++)if(all.items[i].kind!=TOK_WHITESPACE&&all.items[i].kind!=TOK_COMMENT)p.t[p.n++]=all.items[i];
 for(i=0;i<all.count;i++)if(all.items[i].kind==TOK_WHITESPACE||all.items[i].kind==TOK_COMMENT)free(all.items[i].text);
 free(all.items);e=parse_expr(&p,1);
 if(e&&p.pos<p.n)err(&p,"unexpected token after expression");
 for(i=0;i<p.n;i++)free(p.t[i].text);free(p.t);
 if(error&&error_size&&error[0]){free_expression(e);return NULL;}return e;
}
void free_expression(Expr*e){size_t i;if(!e)return;free(e->text);free_expression(e->left);free_expression(e->right);free_expression(e->third);for(i=0;i<e->count;i++)free_expression(e->items[i]);free(e->items);free(e);}

typedef struct{char*s;size_t n,cap;} Buffer;
static int putn(Buffer*b,const char*s,size_t n){if(b->n+n+1>b->cap){size_t c=b->cap?b->cap:64;while(c<b->n+n+1)c*=2;b->s=(char*)realloc(b->s,c);if(!b->s)return 0;b->cap=c;}memcpy(b->s+b->n,s,n);b->n+=n;b->s[b->n]=0;return 1;}
static int putsb(Buffer*b,const char*s){return putn(b,s,strlen(s));}
static char *finish(Buffer*b){if(!b->s)return dup_n("",0);return b->s;}
static char *emit(const Expr*e,char*error,size_t es);
static char *emit2(const char*a,const char*op,const char*b){Buffer x={0};putsb(&x,a);putsb(&x,op);putsb(&x,b);return finish(&x);}
static void set_error(char*e,size_t n,const char*s){if(e&&n&&!e[0])snprintf(e,n,"%s",s);}
static int is_string(const Expr*e){return e&&e->kind==EXPR_LITERAL&&e->flag==TOK_STRING;}
static char *number_text(const char*s){
 size_t n=strlen(s),i;char*lower=dup_n(s,n);int hex;
 for(i=0;i<n;i++)lower[i]=(char)tolower((unsigned char)lower[i]);
 hex=n>1&&lower[0]=='0'&&lower[1]=='x';
 if(hex){
  if(strchr(lower,'p')&&n&&lower[n-1]=='f')lower[--n]=0;
  while(n&& (lower[n-1]=='u'||lower[n-1]=='l'))lower[--n]=0;
 }else{
  while(n&&strchr("ulf",lower[n-1]))lower[--n]=0;
 }
 if(!hex&&n>1&&lower[0]=='0'&&lower[1]>='0'&&lower[1]<='7'&&!strpbrk(lower,".e")){
  char*end;long v=strtol(lower,&end,8);if(*end==0){char tmp[64];snprintf(tmp,sizeof(tmp),"%ld",v);free(lower);return dup_n(tmp,strlen(tmp));}
 }
 return lower;
}
static char *emit(const Expr*e,char*error,size_t es){
 char *a=NULL,*b=NULL,*c=NULL,*out=NULL;Buffer x={0};size_t i;
 if(!e)return NULL;
 switch(e->kind){
 case EXPR_IDENTIFIER:
  if(!strcmp(e->text,"NULL"))return dup_n("nil",3);
  return dup_n(e->text,strlen(e->text));
 case EXPR_LITERAL:
  if(e->flag==TOK_NUMBER)return number_text(e->text);
  if(e->flag==TOK_CHARACTER){size_t n=strlen(e->text);if(n>=2){Buffer q={0};putsb(&q,"\"");putn(&q,e->text+1,n-2);putsb(&q,"\"");return finish(&q);}}
  return dup_n(e->text,strlen(e->text));
 case EXPR_CAST:
  if(strchr(e->text,'*')){set_error(error,es,"pointer casts are not supported");return NULL;}
  return emit(e->left,error,es);
 case EXPR_SIZEOF_TYPE:set_error(error,es,"sizeof(type) has no direct Lua equivalent");return NULL;
 case EXPR_UNARY:
  if(e->flag){set_error(error,es,"postfix increment inside an expression is not supported");return NULL;}
  if(!strcmp(e->text,"sizeof")){set_error(error,es,"sizeof(expression) has no direct Lua equivalent");return NULL;}
  a=emit(e->left,error,es);if(!a)return NULL;
  if(!strcmp(e->text,"!"))out=emit2("(not ",a,")");
  else if(!strcmp(e->text,"+")||!strcmp(e->text,"-")||!strcmp(e->text,"~")){Buffer q={0};putsb(&q,"(");putsb(&q,e->text);putsb(&q,a);putsb(&q,")");out=finish(&q);}
  else set_error(error,es,"unsupported C unary operator");
  free(a);return out;
 case EXPR_BINARY:
  a=emit(e->left,error,es);b=emit(e->right,error,es);if(!a||!b){free(a);free(b);return NULL;}
  if(!strcmp(e->text,"+")&&(is_string(e->left)||is_string(e->right))){
   if(!is_string(e->left)){char*t=emit2("tostring(",a,")");free(a);a=t;}
   if(!is_string(e->right)){char*t=emit2("tostring(",b,")");free(b);b=t;}
   out=emit2("(",a," .. ");{char*t=emit2(out,b,")");free(out);out=t;}
  }else{
   const char*op=e->text;if(!strcmp(op,"&&"))op="and";else if(!strcmp(op,"||"))op="or";else if(!strcmp(op,"!="))op="~=";
   if(strchr(op,'=')&&strcmp(op,"==")&&strcmp(op,"~=")&&strcmp(op,"<=")&&strcmp(op,">="))set_error(error,es,"compound assignment only supported as a statement");
   else {putsb(&x,"(");putsb(&x,a);putsb(&x," ");putsb(&x,op);putsb(&x," ");putsb(&x,b);putsb(&x,")");out=finish(&x);}
  }free(a);free(b);return out;
 case EXPR_CONDITIONAL:
  a=emit(e->left,error,es);b=emit(e->right,error,es);c=emit(e->third,error,es);
  if(a&&b&&c){putsb(&x,"(function() if ");putsb(&x,a);putsb(&x," then return ");putsb(&x,b);putsb(&x," else return ");putsb(&x,c);putsb(&x," end end)()");out=finish(&x);}
  free(a);free(b);free(c);return out;
 case EXPR_CALL:
  if(e->left&&e->left->kind==EXPR_IDENTIFIER&&!strcmp(e->left->text,"NULL")&&!e->count)return dup_n("nil",3);
  a=emit(e->left,error,es);if(!a)return NULL;putsb(&x,a);putsb(&x,"(");free(a);
  for(i=0;i<e->count;i++){b=emit(e->items[i],error,es);if(!b){free(x.s);return NULL;}if(i)putsb(&x,", ");putsb(&x,b);free(b);}
  putsb(&x,")");return finish(&x);
 case EXPR_INDEX:
  a=emit(e->left,error,es);b=emit(e->right,error,es);if(a&&b){putsb(&x,a);putsb(&x,"[(");putsb(&x,b);putsb(&x,") + 1]");out=finish(&x);}free(a);free(b);return out;
 case EXPR_MEMBER:
  a=emit(e->left,error,es);if(a){out=emit2(a,".",e->text);free(a);}return out;
 case EXPR_COMMA:set_error(error,es,"comma operator is not supported in Lua expressions");return NULL;
 }
 return NULL;
}
char *expression_to_lua(const Expr*expr,char*error,size_t error_size){if(error&&error_size)error[0]=0;return emit(expr,error,error_size);}
static char *lvalue(const Expr*e,char*error,size_t es){
 char*a,*b,*out;Buffer x={0};
 if(e->kind==EXPR_IDENTIFIER)return dup_n(e->text,strlen(e->text));
 if(e->kind==EXPR_MEMBER){a=emit(e->left,error,es);if(!a)return NULL;out=emit2(a,".",e->text);free(a);return out;}
 if(e->kind==EXPR_INDEX){a=emit(e->left,error,es);b=emit(e->right,error,es);if(!a||!b){free(a);free(b);return NULL;}putsb(&x,a);putsb(&x,"[(");putsb(&x,b);putsb(&x,") + 1]");free(a);free(b);return finish(&x);}
 set_error(error,es,"assignment target is not a supported variable");return NULL;
}
char *statement_expression_to_lua(const char*source,char*error,size_t error_size){
 Expr*e=parse_c_expression(source,error,error_size);char*out=NULL,*a=NULL,*b=NULL;Buffer x={0};const char*op=NULL;
 if(!e)return NULL;
 if(e->kind==EXPR_UNARY&&(!strcmp(e->text,"++")||!strcmp(e->text,"--"))){
  a=lvalue(e->left,error,error_size);if(a){putsb(&x,a);putsb(&x," = ");putsb(&x,a);putsb(&x,!strcmp(e->text,"++")?" + 1":" - 1");out=finish(&x);}free(a);
 }else if(e->kind==EXPR_BINARY&&e->text&&strchr(e->text,'=')&&strcmp(e->text,"==")&&strcmp(e->text,"!=")&&strcmp(e->text,"<=")&&strcmp(e->text,">=")){
  a=lvalue(e->left,error,error_size);b=emit(e->right,error,error_size);
  if(a&&b){op=e->text;if(!strcmp(op,"=")){putsb(&x,a);putsb(&x," = ");putsb(&x,b);}else{size_t n=strlen(op);char oper[4];if(n>1){memcpy(oper,op,n-1);oper[n-1]=0;}else oper[0]=0;putsb(&x,a);putsb(&x," = ");putsb(&x,a);putsb(&x," ");putsb(&x,oper);putsb(&x," (");putsb(&x,b);putsb(&x,")");}out=finish(&x);}
  free(a);free(b);
 }else out=emit(e,error,error_size);
 free_expression(e);return out;
}
