#include "lexer.h"
#include "expression.h"
#include "converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures=0;
static void check(int condition,const char*name){if(!condition){fprintf(stderr,"FAIL: %s\n",name);++failures;}else printf("PASS: %s\n",name);}
int main(void){
 TokenList tokens;char error[512];char*lua;Expr*expr;
 const char*sample="void test() {\n // note\n float x = 1.25e-2;\n self->health += 1;\n}";
 if(lex_source(sample,&tokens,error,sizeof(error))){
  size_t i,n=0;char*rebuilt=(char*)malloc(strlen(sample)+1);
  for(i=0;i<tokens.count;i++){memcpy(rebuilt+n,tokens.items[i].text,tokens.items[i].length);n+=tokens.items[i].length;}
  rebuilt[n]=0;check(!strcmp(rebuilt,sample),"lexer preserves source text");
  free(rebuilt);free_tokens(&tokens);
 }else check(0,"lexer tokenizes valid source");
 expr=parse_c_expression("a + b * c << 2 == d && ready || fallback",error,sizeof(error));
 check(expr!=NULL,"expression parser handles precedence");free_expression(expr);
 expr=parse_c_expression("spawn(name, getentityproperty(self, \"health\"), clamp(x, 0, max))",error,sizeof(error));
 check(expr!=NULL,"expression parser handles nested calls");free_expression(expr);
 lua=convert_c_source("void test(int player) { int health = 100; if (health > 0 && player != 0) { health -= 10; } else { health = 0; } for (int i = 0; i < 3; i++) { health = health + i; } return; }",error,sizeof(error));
 check(lua!=NULL,"source converter handles functions and control flow");
 if(lua){
  check(strstr(lua,"function test(player)")!=NULL,"function signature emitted");
  check(strstr(lua,"function test(player)\n\tdo\n")==NULL,"function body does not add an unnecessary scope");
  check(strstr(lua,"local health = 100")!=NULL,"local declaration emitted");
  check(strstr(lua,"if ((health > 0) and (player ~= 0)) then")!=NULL,"logical expression translated");
  check(strstr(lua,"health = health - (10)")!=NULL,"compound assignment translated");
  check(strstr(lua,"while (i < 3) do")!=NULL,"for loop translated");
  check(strstr(lua,"i = i + 1")!=NULL,"post expression translated");
  check(strstr(lua,"__openbor_smart_truth")==NULL,"no generated truthiness helper");
  free(lua);
 }
 lua=convert_c_source("void test() { int values[4]; values[0] = 10; values[2] = values[0] + 1; }",error,sizeof(error));
 check(lua!=NULL,"array source converts");
 if(lua){check(strstr(lua,"values[(0) + 1] = 10")!=NULL,"C array indexes shifted to Lua");free(lua);}
 lua=convert_c_script_body("void self = getlocalvar(\"self\");\nif (frame == 0) {\n    changeentityproperty(self, \"animation\", openborconstant(\"ANI_IDLE\"));\n}",error,sizeof(error));
 check(lua!=NULL,"embedded animation script body converts");
 if(lua){check(strstr(lua,"local self = getlocalvar(\"self\")")!=NULL,"embedded script declarations become Lua locals");
  check(strstr(lua,"if (frame == 0) then")!=NULL,"embedded script frame condition is preserved");free(lua);}
 lua=convert_animation_text("anim idle\n\t@script\n\tint value = 2;\n\tif (frame == 0) { value++; }\n\t@end_script\n\tframe data/test\n",error,sizeof(error));
 check(lua!=NULL,"animation text with embedded script converts");
 if(lua){check(strstr(lua,"-- OPENBOR_SMART_LUA")!=NULL,"converted script marker emitted");
  check(strstr(lua,"function") == NULL,"embedded body is emitted without a nested function");
  check(strstr(lua,"frame data/test")!=NULL,"animation text outside script is preserved");free(lua);}
 lua=convert_c_source("void test() { int value = sizeof(int); }",error,sizeof(error));
 check(lua==NULL,"unsupported sizeof reports an error");free(lua);

 lua=convert_c_source("int choose(int value) { do { value--; } while (value > 10); return value > 0 ? value : 0; }",error,sizeof(error));
 check(lua!=NULL,"do-while and ternary expressions convert");
 if(lua){check(strstr(lua,"until not ((value > 10))")!=NULL,"do-while condition preserved");
  check(strstr(lua,"if (value > 0) then return value else return 0 end")!=NULL,"ternary expression emitted");free(lua);}
 lua=convert_c_source("void test(int state) { switch (state) { case 1: activate(); break; case 2: return; default: reset(); break; } }",error,sizeof(error));
 check(lua!=NULL,"switch statements convert");
 if(lua){check(strstr(lua,"elseif state == 2 then")!=NULL,"switch cases emitted");check(strstr(lua,"until true")!=NULL,"switch exit emitted");free(lua);}
 lua=convert_c_source("void update() { int rush = 4; void self = getplayerproperty(0, \"entity\"); drawstring(21, 80, 1, \"Bonus_\"+25*rush); if (self == NULL()) { drawstring(1, 1, 1, \"none\"); } }",error,sizeof(error));
 check(lua!=NULL,"legacy string concatenation and NULL convert");
 if(lua){check(strstr(lua,"\"Bonus_\" .. tostring((25 * rush))")!=NULL,"legacy string concatenation translated");check(strstr(lua,"self == nil")!=NULL,"NULL() translated to nil");free(lua);}
 lua=convert_c_source("void test() { int *pointer; }",error,sizeof(error));
 check(lua==NULL,"unsupported pointer declarations are rejected");
 lua=convert_c_source("void test(int state) { switch (state) { case 1: activate(); case 2: reset(); break; } }",error,sizeof(error));
 check(lua==NULL,"switch fallthrough is rejected");
 if(failures){fprintf(stderr,"%d test(s) failed\n",failures);return 1;}
 puts("All C converter tests passed.");return 0;
}
