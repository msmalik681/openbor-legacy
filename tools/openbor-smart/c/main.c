#include "converter.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path,0755)
#define PATH_SEP '/'
#endif

typedef struct { char **items; size_t count; } Paths;
static char *copy_n(const char*s,size_t n){char*p=(char*)malloc(n+1);if(p){memcpy(p,s,n);p[n]=0;}return p;}
static char *copy_s(const char*s){return copy_n(s,strlen(s));}
static int equal_ci(const char*a,const char*b){while(*a&&*b){if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return 0;++a;++b;}return *a==*b;}
static int has_c_extension(const char*path){const char*dot=strrchr(path,'.');return dot&&equal_ci(dot,".c");}
static char *join_path(const char*a,const char*b){
 size_t na=strlen(a),nb=strlen(b);char*p=(char*)malloc(na+nb+2);if(!p)return NULL;
 memcpy(p,a,na);if(na&&a[na-1]!= '/'&&a[na-1]!='\\')p[na++]=PATH_SEP;
 memcpy(p+na,b,nb+1);return p;
}
static char *replace_extension(const char*path){
 const char*dot=strrchr(path,'.');const char*slash1=strrchr(path,'/');const char*slash2=strrchr(path,'\\');
 if(slash1&&(!slash2||slash1>slash2)){}else if(slash2)slash1=slash2;
 if(!dot||(slash1&&dot<slash1))return join_path(path,".lua");
 return join_path(copy_n(path,(size_t)(dot-path)),".lua");
}
static char *base_name(const char*path){const char*a=strrchr(path,'/'),*b=strrchr(path,'\\'),*p=a;if(b&&(!p||b>p))p=b;return copy_s(p?p+1:path);}
static int mkdirs(const char*path){
 char *copy=copy_s(path);size_t i,n=strlen(copy);int ok=1;
 for(i=1;i<=n;i++)if(copy[i]=='/'||copy[i]=='\\'||copy[i]==0){char old=copy[i];copy[i]=0;if(*copy&&MKDIR(copy)!=0&&errno!=EEXIST)ok=0;copy[i]=old;}
 free(copy);return ok;
}
static char *read_file(const char*path,size_t*size){
 FILE*f=fopen(path,"rb");char*data;long n;if(!f)return NULL;
 if(fseek(f,0,SEEK_END)||((n=ftell(f))<0)||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
 data=(char*)malloc((size_t)n+1);if(!data){fclose(f);return NULL;}
 if(fread(data,1,(size_t)n,f)!=(size_t)n){free(data);fclose(f);return NULL;}
 data[n]=0;fclose(f);if(size)*size=(size_t)n;return data;
}
static int write_file(const char*path,const char*data){
 FILE*f=fopen(path,"wb");size_t n=strlen(data);int ok;if(!f)return 0;ok=fwrite(data,1,n,f)==n;if(fclose(f)!=0)ok=0;return ok;
}
static int add_path(Paths*p,const char*path){
 char**q=(char**)realloc(p->items,(p->count+1)*sizeof(*q));if(!q)return 0;p->items=q;p->items[p->count++]=copy_s(path);return 1;
}
static void free_paths(Paths*p){size_t i;for(i=0;i<p->count;i++)free(p->items[i]);free(p->items);}
static int scan_dir(const char*root,const char*dir,int recursive,Paths*out){
 DIR*d=opendir(dir);struct dirent*entry;if(!d)return 0;
 while((entry=readdir(d))!=NULL){
  char*path;struct stat st;
  if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
  path=join_path(dir,entry->d_name);if(!path){closedir(d);return 0;}
  if(stat(path,&st)==0&&S_ISDIR(st.st_mode)){
   if(recursive&&!scan_dir(root,path,recursive,out)){free(path);closedir(d);return 0;}
  }else if(has_c_extension(entry->d_name)){if(!add_path(out,path)){free(path);closedir(d);return 0;}}
  free(path);
 }
 closedir(d);(void)root;return 1;
}
static char *output_for(const char*input,const char*outdir,const char*root){
 char*relative=NULL,*base=NULL,*changed=NULL,*out=NULL;
 if(!outdir||!*outdir)return replace_extension(input);
 if(root&&*root){
  size_t n=strlen(root);
  if(!strncmp(input,root,n)){const char*p=input+n;while(*p=='/'||*p=='\\')++p;relative=copy_s(p);}
 }
 if(!relative){base=base_name(input);relative=base;}
 changed=replace_extension(relative);out=join_path(outdir,changed);
 free(relative);free(changed);return out;
}
static int convert_one(const char*input,const char*output,int force){
 size_t size;char*source=read_file(input,&size),*lua,error[512];(void)size;
 if(!source){fprintf(stderr,"FAILED: cannot read %s\n",input);return 0;}
 if(!force){FILE*f=fopen(output,"rb");if(f){fclose(f);fprintf(stderr,"FAILED: refusing to overwrite %s (use -force)\n",output);free(source);return 0;}}
 lua=convert_c_source(source,error,sizeof(error));free(source);
 if(!lua){fprintf(stderr,"FAILED: convert %s: %s\n",input,error[0]?error:"conversion failed");return 0;}
 {
  char*slash=copy_s(output);char*p=strrchr(slash,'/');char*q=strrchr(slash,'\\');if(q&&(!p||q>p))p=q;
  if(p){*p=0;if(!mkdirs(slash)){fprintf(stderr,"FAILED: create output directory %s\n",slash);free(slash);free(lua);return 0;}}
  free(slash);
 }
 if(!write_file(output,lua)){fprintf(stderr,"FAILED: write %s\n",output);free(lua);return 0;}
 printf("Converted %s -> %s\n",input,output);free(lua);return 1;
}
int main(int argc,char**argv){
 const char*input=NULL,*outdir=NULL;int recursive=0,force=0,i,failed=0,converted=0;struct stat st;
 for(i=1;i<argc;i++){
  if(!strcmp(argv[i],"-in")&&i+1<argc)input=argv[++i];
  else if(!strcmp(argv[i],"-out")&&i+1<argc)outdir=argv[++i];
  else if(!strcmp(argv[i],"-recursive"))recursive=1;
  else if(!strcmp(argv[i],"-force"))force=1;
  else {fprintf(stderr,"unknown or incomplete option: %s\n",argv[i]);return 2;}
 }
 if(!input){fprintf(stderr,"usage: openbor-smart-c -in path/to/script.c | -in path/to/scripts [-recursive] [-out directory] [-force]\n");return 2;}
 if(stat(input,&st)!=0){fprintf(stderr,"cannot access input: %s\n",input);return 1;}
 if(S_ISDIR(st.st_mode)){
  Paths files={0};size_t j;
  if(!scan_dir(input,input,recursive,&files)){fprintf(stderr,"cannot scan input directory: %s\n",input);free_paths(&files);return 1;}
  if(!files.count){fprintf(stderr,"no .c files found in %s\n",input);free_paths(&files);return 1;}
  for(j=0;j<files.count;j++){char*out=output_for(files.items[j],outdir,input);if(!out||!convert_one(files.items[j],out,force))++failed;else ++converted;free(out);}
  printf("Batch complete: %d converted, %d failed\n",converted,failed);free_paths(&files);return failed?1:0;
 }
 if(!has_c_extension(input)){fprintf(stderr,"input must be a .c file or a directory containing .c files\n");return 1;}
 {
  char*out;if(outdir){char*base=base_name(input);char*lua=replace_extension(base);out=join_path(outdir,lua);free(base);free(lua);}
  else out=replace_extension(input);
  if(!out){fprintf(stderr,"out of memory\n");return 1;}
  i=convert_one(input,out,force);free(out);return i?0:1;
 }
}
