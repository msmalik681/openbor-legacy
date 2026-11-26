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
#include <process.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#define GETPID() _getpid()
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path,0755)
#define RMDIR(path) rmdir(path)
#define GETPID() getpid()
#define PATH_SEP '/'
#endif

typedef struct { char **items; size_t count; } Paths;
typedef struct { char *source, *relative, *target; int is_dir; } Entry;
typedef struct { Entry *items; size_t count; } Plan;

static char *copy_n(const char*s,size_t n){char*p=(char*)malloc(n+1);if(p){memcpy(p,s,n);p[n]=0;}return p;}
static char *copy_s(const char*s){return copy_n(s,strlen(s));}
static int equal_ci(const char*a,const char*b){while(*a&&*b){if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return 0;++a;++b;}return *a==*b;}
static int extension_is(const char*p,const char*ext){const char*d=strrchr(p,'.'),*s1=strrchr(p,'/'),*s2=strrchr(p,'\\'),*s=s1;if(s2&&(!s||s2>s))s=s2;return d&&(!s||d>s)&&equal_ci(d,ext);}
static char *join_path(const char*a,const char*b){size_t na=strlen(a),nb=strlen(b);char*p=(char*)malloc(na+nb+2);if(!p)return NULL;memcpy(p,a,na);if(na&&a[na-1]!='/'&&a[na-1]!='\\')p[na++]=PATH_SEP;memcpy(p+na,b,nb+1);return p;}
static char *lower_path(const char*s){char*p=copy_s(s);size_t i;if(p)for(i=0;p[i];i++){if(p[i]=='\\')p[i]='/';else p[i]=(char)tolower((unsigned char)p[i]);}return p;}
static char *mapped_relative(const char*s,int is_dir){char*p=lower_path(s);char*d;size_t n;if(!p)return NULL;if(!is_dir&&extension_is(p,".c")){d=strrchr(p,'.');n=(size_t)(d-p);p[n]=0;{char*q=(char*)realloc(p,n+5);if(!q){free(p);return NULL;}p=q;memcpy(p+n,".lua",5);}}return p;}
static int mkdir_one(const char*p){if(MKDIR(p)==0||errno==EEXIST)return 1;return 0;}
static int mkdirs(const char*path){char*p=copy_s(path);size_t i,n;int ok=1;if(!p)return 0;n=strlen(p);for(i=1;i<=n;i++)if(p[i]=='/'||p[i]=='\\'||p[i]==0){char c=p[i];p[i]=0;if(*p&&!mkdir_one(p)){ok=0;break;}p[i]=c;}free(p);return ok;}
static char *trim_trailing_separators(const char*path){char*p=copy_s(path);size_t n;if(!p)return NULL;n=strlen(p);while(n>1&&(p[n-1]=='/'||p[n-1]=='\\'))p[--n]=0;return p;}
static char *parent_dir(const char*path){char*p=copy_s(path),*a,*b,*q;if(!p)return NULL;a=strrchr(p,'/');b=strrchr(p,'\\');q=a;if(b&&(!q||b>q))q=b;if(!q){free(p);return copy_s(".");}if(q==p)q[1]=0;else *q=0;return p;}
static char *read_file(const char*path,size_t*size){FILE*f=fopen(path,"rb");char*d;long n;if(!f)return NULL;if(fseek(f,0,SEEK_END)||((n=ftell(f))<0)||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}d=(char*)malloc((size_t)n+1);if(!d){fclose(f);return NULL;}if(fread(d,1,(size_t)n,f)!=(size_t)n){free(d);fclose(f);return NULL;}d[n]=0;fclose(f);if(size)*size=(size_t)n;return d;}
static int write_file(const char*path,const char*data,size_t n){FILE*f=fopen(path,"wb");int ok;if(!f)return 0;ok=fwrite(data,1,n,f)==n;if(fclose(f)!=0)ok=0;return ok;}
static int copy_file(const char*src,const char*dst){FILE*a=fopen(src,"rb"),*b;char buf[16384];size_t n;int ok=1;if(!a)return 0;b=fopen(dst,"wb");if(!b){fclose(a);return 0;}while((n=fread(buf,1,sizeof(buf),a))>0)if(fwrite(buf,1,n,b)!=n){ok=0;break;}if(ferror(a))ok=0;if(fclose(a)!=0)ok=0;if(fclose(b)!=0)ok=0;return ok;}
static int add_entry(Plan*p,const char*src,const char*rel,int is_dir){Entry*e;char*t=mapped_relative(rel,is_dir);size_t i;if(!t)return 0;for(i=0;i<p->count;i++)if(!strcmp(p->items[i].target,t)){fprintf(stderr,"PATH COLLISION: '%s' and '%s' both map to '%s'\n",p->items[i].relative,rel,t);free(t);return -1;}e=(Entry*)realloc(p->items,(p->count+1)*sizeof(*e));if(!e){free(t);return 0;}p->items=e;e=&p->items[p->count++];e->source=copy_s(src);e->relative=copy_s(rel);e->target=t;e->is_dir=is_dir;return e->source&&e->relative;}
static void free_plan(Plan*p){size_t i;for(i=0;i<p->count;i++){free(p->items[i].source);free(p->items[i].relative);free(p->items[i].target);}free(p->items);memset(p,0,sizeof(*p));}
static int scan_tree(const char*root,const char*dir,const char*rel,Plan*p){DIR*d=opendir(dir);struct dirent*de;if(!d)return 0;while((de=readdir(d))!=NULL){char*src,*r;struct stat st;int added;if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;src=join_path(dir,de->d_name);r=rel&&*rel?join_path(rel,de->d_name):copy_s(de->d_name);if(!src||!r){free(src);free(r);closedir(d);return 0;}if(stat(src,&st)!=0){fprintf(stderr,"Cannot inspect %s\n",src);free(src);free(r);closedir(d);return 0;}if(S_ISDIR(st.st_mode)){added=add_entry(p,src,r,1);if(added<=0||!scan_tree(root,src,r,p)){free(src);free(r);closedir(d);return 0;}}else{added=add_entry(p,src,r,0);if(added<=0){free(src);free(r);closedir(d);return 0;}}free(src);free(r);}closedir(d);(void)root;return 1;}
static int convert_file_to(const char*src,const char*dst){char*data,*converted=NULL,error[512]={0};size_t n;int ok;if(extension_is(src,".c")){data=read_file(src,&n);if(!data){fprintf(stderr,"Cannot read %s\n",src);return 0;}converted=convert_c_source(data,error,sizeof(error));free(data);if(!converted){fprintf(stderr,"Conversion failed for %s: %s\n",src,error[0]?error:"unknown error");return 0;}ok=write_file(dst,converted,strlen(converted));free(converted);return ok;}if(extension_is(src,".txt")){data=read_file(src,&n);if(!data){fprintf(stderr,"Cannot read %s\n",src);return 0;}converted=convert_animation_text(data,error,sizeof(error));free(data);if(!converted){fprintf(stderr,"Conversion failed for %s: %s\n",src,error[0]?error:"unknown error");return 0;}ok=write_file(dst,converted,strlen(converted));free(converted);return ok;}return copy_file(src,dst);}
static int build_stage(const char*stage,Plan*p){size_t i;char*path;if(!mkdirs(stage)){fprintf(stderr,"Cannot create staging directory %s\n",stage);return 0;}for(i=0;i<p->count;i++)if(p->items[i].is_dir){path=join_path(stage,p->items[i].target);if(!path||!mkdirs(path)){fprintf(stderr,"Cannot create directory for %s\n",p->items[i].relative);free(path);return 0;}free(path);}for(i=0;i<p->count;i++)if(!p->items[i].is_dir){path=join_path(stage,p->items[i].target);if(!path){return 0;}{char*parent=parent_dir(path);if(!parent||!mkdirs(parent)){fprintf(stderr,"Cannot create parent directory for %s\n",path);free(parent);free(path);return 0;}free(parent);}if(!convert_file_to(p->items[i].source,path)){fprintf(stderr,"Failed writing staged output %s\n",path);free(path);return 0;}printf("Prepared %s -> %s\n",p->items[i].source,path);free(path);}return 1;}
static int remove_tree(const char*path){DIR*d=opendir(path);struct dirent*de;if(!d)return remove(path)==0||errno==ENOENT;while((de=readdir(d))!=NULL){char*p;struct stat st;if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;p=join_path(path,de->d_name);if(!p){closedir(d);return 0;}if(stat(p,&st)==0&&S_ISDIR(st.st_mode)){if(!remove_tree(p)){free(p);closedir(d);return 0;}}else if(remove(p)!=0){free(p);closedir(d);return 0;}free(p);}closedir(d);return RMDIR(path)==0;}
static int confirm_in_place(void){char answer[128];size_t i,n;printf("\nWARNING: In-place migration will rename paths to lowercase, convert .c files to .lua, rewrite .txt animation scripts, and remove the original .c files. A backup is kept only until the new tree is installed.\nType y/yes to continue: ");fflush(stdout);if(!fgets(answer,sizeof(answer),stdin))return 0;n=strlen(answer);while(n&&(answer[n-1]=='\n'||answer[n-1]=='\r'||isspace((unsigned char)answer[n-1])))answer[--n]=0;for(i=0;i<n;i++)answer[i]=(char)tolower((unsigned char)answer[i]);return !strcmp(answer,"y")||!strcmp(answer,"yes")||!strcmp(answer,"yeah")||!strcmp(answer,"yep")||!strcmp(answer,"yea");}
static int path_exists(const char*p){struct stat st;return stat(p,&st)==0;}
static int migrate_directory(const char*root,const char*outdir,int force){Plan plan={0};char*root_clean=NULL,*out_clean=NULL,*base=NULL,*stage=NULL,*backup=NULL;const char*active_root;int in_place=!outdir,ok=0;size_t n;root_clean=trim_trailing_separators(root);out_clean=outdir?trim_trailing_separators(outdir):NULL;if(!root_clean)goto done;active_root=in_place?root_clean:out_clean;if(!active_root)goto done;if(!scan_tree(active_root,active_root,"",&plan)){fprintf(stderr,"Failed to scan project tree.\n");goto done;}if(!plan.count){fprintf(stderr,"No files or directories found in %s\n",active_root);goto done;}if(in_place&&!confirm_in_place()){puts("Migration cancelled; no files changed.");goto done;}if(!in_place&&path_exists(active_root)&&!force){fprintf(stderr,"Output directory exists: %s (use -force to replace it)\n",active_root);goto done;}n=strlen(active_root)+64;stage=(char*)malloc(n);backup=(char*)malloc(n);if(!stage||!backup)goto done;snprintf(stage,n,"%s.openbor-smart-stage-%ld",active_root,(long)GETPID());snprintf(backup,n,"%s.openbor-smart-backup-%ld",active_root,(long)GETPID());if(path_exists(stage)||path_exists(backup)){fprintf(stderr,"Temporary migration path already exists; remove it before retrying.\n");goto done;}if(!build_stage(stage,&plan))goto done;if(path_exists(active_root)){if(rename(active_root,backup)!=0){fprintf(stderr,"Cannot move existing directory to backup: %s\n",strerror(errno));goto done;}}if(rename(stage,active_root)!=0){fprintf(stderr,"Cannot install staged project: %s\n",strerror(errno));if(path_exists(backup))rename(backup,active_root);goto done;}if(path_exists(backup)&&!remove_tree(backup))fprintf(stderr,"Warning: installed successfully but could not remove backup %s\n",backup);ok=1;printf("Project migration complete: %lu entries processed.\n",(unsigned long)plan.count);
done:if(stage&&path_exists(stage))remove_tree(stage);free(root_clean);free(out_clean);free(stage);free(backup);free_plan(&plan);return ok;}
static int migrate_single(const char*input,const char*outdir,int force){
 const char *a=strrchr(input,'/'),*b=strrchr(input,'\\'),*name=a;
 char *base=NULL,*lower=NULL,*target=NULL,*stage=NULL,*backup=NULL,*parent=NULL;
 int in_place=!outdir,ok=0;size_t n;
 if(b&&(!name||b>name))name=b;
 base=copy_s(name?name+1:input);if(!base)goto done;
 lower=lower_path(base);if(!lower)goto done;
 if(extension_is(lower,".c")){char*d=strrchr(lower,'.');*d=0;{char*q=(char*)realloc(lower,strlen(lower)+5);if(!q)goto done;lower=q;strcat(lower,".lua");}}
 if(in_place){parent=parent_dir(input);if(!parent)goto done;target=join_path(parent,lower);}
 else target=join_path(outdir,lower);
 if(!target)goto done;
 if(in_place&&!confirm_in_place()){puts("Migration cancelled; no files changed.");goto done;}
 if(!in_place&&!mkdirs(outdir))goto done;
 free(parent);parent=parent_dir(target);if(!parent||!mkdirs(parent))goto done;
 n=strlen(target)+64;stage=(char*)malloc(n);backup=(char*)malloc(n);if(!stage||!backup)goto done;
 snprintf(stage,n,"%s.openbor-smart-tmp-%ld",target,(long)GETPID());
 snprintf(backup,n,"%s.openbor-smart-backup-%ld",target,(long)GETPID());
 if(path_exists(stage)||path_exists(backup)){fprintf(stderr,"Temporary file already exists; remove it before retrying.\n");goto done;}
 if(!convert_file_to(input,stage)){fprintf(stderr,"Conversion failed; original left untouched.\n");goto done;}
 if(path_exists(target)){
  int same_source;
#ifdef _WIN32
  same_source=equal_ci(base,lower);
#else
  same_source=!strcmp(base,lower);
#endif
  if((in_place&&!same_source&&!force)||(!in_place&&!force)){fprintf(stderr,"Refusing to overwrite %s (use -force)\n",target);goto done;}
  if(rename(target,backup)!=0)goto done;
 }
 if(rename(stage,target)!=0){if(path_exists(backup))rename(backup,target);goto done;}
 {
  int same_source;
#ifdef _WIN32
  same_source=equal_ci(base,lower);
#else
  same_source=!strcmp(base,lower);
#endif
  if(in_place&&!same_source&&remove(input)!=0)fprintf(stderr,"Warning: output installed but could not remove original %s\n",input);
 }
 if(path_exists(backup))remove(backup);
 printf("Converted %s -> %s\n",input,target);ok=1;
done:
 if(stage&&path_exists(stage))remove(stage);
 free(base);free(lower);free(target);free(stage);free(backup);free(parent);return ok;
}
int main(int argc,char**argv){const char*input=NULL,*outdir=NULL;int force=0,i;struct stat st;for(i=1;i<argc;i++){if(!strcmp(argv[i],"-in")&&i+1<argc)input=argv[++i];else if(!strcmp(argv[i],"-out")&&i+1<argc)outdir=argv[++i];else if(!strcmp(argv[i],"-force"))force=1;else{fprintf(stderr,"Unknown or incomplete option: %s\n",argv[i]);return 2;}}if(!input){fprintf(stderr,"Usage: openbor-smart-c -in file.c|file.txt|project-directory [-out directory] [-force]\n");return 2;}if(stat(input,&st)!=0){fprintf(stderr,"Cannot access input: %s\n",input);return 1;}if(S_ISDIR(st.st_mode))return migrate_directory(input,outdir,force)?0:1;if(!extension_is(input,".c")&&!extension_is(input,".txt")){fprintf(stderr,"Input must be a .c file, .txt file, or project directory.\n");return 1;}return migrate_single(input,outdir,force)?0:1;}
