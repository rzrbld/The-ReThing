// Native first-run setup. Asset identities are embedded; extraction tools are bundled.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/wait.h>
#include "install_manifest.h"
namespace fs=std::filesystem;
using std::string;
static fs::path root;
static const char *iso_sha="1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61";
static void fail(const string &s){throw std::runtime_error(s);}
static string read(const fs::path &p){std::ifstream f(p,std::ios::binary);if(!f)fail("Cannot read "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
static void atomic_write(const fs::path &p,const string &s){
 fs::create_directories(p.parent_path());auto t=p;t+=".new";
 int fd=open(t.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0644);if(fd<0)fail("Cannot write "+t.string());
 size_t n=0;while(n<s.size()){ssize_t w=write(fd,s.data()+n,s.size()-n);if(w<=0){close(fd);fail("Write failed "+t.string());}n+=w;}
 if(fsync(fd)){close(fd);fail("Sync failed");}close(fd);fs::rename(t,p);
 int d=open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY);if(d>=0){fsync(d);close(d);}
}
static void progress(int n,const string &s){atomic_write(root/"tmp/install-status.txt",std::to_string(n)+"\n"+s+"\n");fprintf(stderr,"[setup] %s\n",s.c_str());}
// Streaming SHA-256; constant memory regardless of ISO size.
static uint32_t rotr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static string digest(const fs::path &p){
 static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 auto block=[&](const unsigned char *b){uint32_t w[64];for(int i=0;i<16;i++)w[i]=(uint32_t(b[4*i])<<24)|(uint32_t(b[4*i+1])<<16)|(uint32_t(b[4*i+2])<<8)|b[4*i+3];for(int i=16;i<64;i++){auto a=w[i-15],c=w[i-2];w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(c,17)^rotr(c,19)^(c>>10));}uint32_t a=h[0],b0=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];for(int i=0;i<64;i++){uint32_t t=v+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i],u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b0)^(a&c)^(b0&c));v=g;g=f;f=e;e=d+t;d=c;c=b0;b0=a;a=t+u;}uint32_t out[]={a,b0,c,d,e,f,g,v};for(int i=0;i<8;i++)h[i]+=out[i];};
 std::ifstream f(p,std::ios::binary);if(!f)fail("Cannot hash "+p.string());uint64_t total=0;std::array<unsigned char,65536> buf{};size_t tail=0;
 while(f){f.read((char*)buf.data(),buf.size());size_t n=f.gcount();total+=n;size_t i=0;for(;i+64<=n;i+=64)block(buf.data()+i);tail=n-i;if(tail)memmove(buf.data(),buf.data()+i,tail);if(n<buf.size())break;}
 if(f.bad())fail("Read failure "+p.string());unsigned char last[128]={};memcpy(last,buf.data(),tail);last[tail]=128;size_t bytes=tail<56?64:128;uint64_t bits=total*8;for(int i=0;i<8;i++)last[bytes-1-i]=bits>>(8*i);block(last);if(bytes==128)block(last+64);
 char out[65];for(int i=0;i<8;i++)snprintf(out+8*i,9,"%08x",h[i]);return out;
}
static string lower(string s){for(char &c:s)if(c>='A'&&c<='Z')c+=32;return s;}
static fs::path resolve(const fs::path &base,const fs::path &relative){
 fs::path p=base;for(auto &part:relative){int found=0;fs::path match;if(fs::is_directory(p))for(auto &e:fs::directory_iterator(p))if(lower(e.path().filename().string())==lower(part.string())){match=e.path();found++;}if(found!=1)fail("Missing/ambiguous file: "+relative.string());p=match;}return p;
}
static void safe_trees(){
 for(auto name:{"tmp","config","userdata","cache","saves","source","extracted-iso","bin","data"}){
 auto p=root/name;if(fs::is_symlink(p))fail("Symlink in "+p.string());if(fs::exists(p))for(auto &e:fs::recursive_directory_iterator(p))if(e.is_symlink())fail("Symlink in "+e.path().string());}
}
static void verify(const fs::path &base,const Asset *a,size_t n,bool hashes){for(size_t i=0;i<n;i++){auto p=resolve(base,a[i].path);if(!fs::is_regular_file(p)||fs::file_size(p)!=a[i].size||(hashes&&digest(p)!=a[i].sha))fail("Checksum/size mismatch: "+string(a[i].path));}}
static void tool(const string &name,std::vector<string> args){
 fs::path t=root/"tools";std::vector<string> cmd={(t/"lib/ld-linux-aarch64.so.1").string(),"--library-path",(t/"lib").string(),(t/(name=="7z"?"p7zip/7z":"bin/"+name)).string()};
 // Development-only ARM emulation executes the exact bundled tools.
 if(const char *q=getenv("THING_SETUP_QEMU"))cmd.insert(cmd.begin(),q);
 cmd.insert(cmd.end(),args.begin(),args.end());pid_t pid=fork();if(pid<0)fail("Cannot start extraction tool");if(!pid){unsetenv("LD_LIBRARY_PATH");setenv("TMPDIR",(root/"tmp").c_str(),1);setenv("LC_ALL","C",1);int fd=open((root/"tmp/install-tools.log").c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);if(fd>=0){dup2(fd,1);dup2(fd,2);close(fd);}std::vector<char*> av;for(auto &s:cmd)av.push_back(s.data());av.push_back(nullptr);execv(av[0],av.data());_exit(127);}int st;while(waitpid(pid,&st,0)<0)if(errno!=EINTR)fail("Tool wait failed");if(!WIFEXITED(st)||WEXITSTATUS(st))fail(name+" failed; see tmp/install-tools.log");
}
static constexpr size_t payload_count=sizeof(payload_assets)/sizeof(Asset),menu_count=sizeof(menu_assets)/sizeof(Asset);
static void menu(const fs::path &base){
 bool ready=true;try{verify(base,menu_assets,menu_count,true);}catch(...){ready=false;}if(ready)return;
 auto stage=root/"tmp/native-menu";fs::remove_all(stage);fs::create_directories(stage);
 tool("7z",{"x","-y",resolve(base,"data/mainui.pak").string(),"-o"+stage.string()});
 // Only compiled manifest entries can be copied into the game tree.
 for(auto &a:menu_assets){auto rel=fs::path(a.path).lexically_relative("data");auto src=resolve(stage,rel);if(digest(src)!=a.sha)fail("MainUI checksum mismatch");fs::path dst;try{dst=resolve(base,a.path);}catch(...){dst=base/a.path;}if(fs::exists(dst)){if(digest(dst)!=a.sha)fail("Differing loose MainUI file: "+dst.string());}}
 for(auto &a:menu_assets){fs::path dst;try{dst=resolve(base,a.path);}catch(...){dst=base/a.path;}if(!fs::exists(dst)){fs::create_directories(dst.parent_path());fs::copy_file(resolve(stage,fs::path(a.path).lexically_relative("data")),dst);}}
 fs::remove_all(stage);
}
static void receipt(){atomic_write(root/"config/install-native-v1.txt",string(iso_sha)+"\n");}
static void recover(){
 auto journal=root/"tmp/native-publishing.txt",work=root/"tmp/native-install-work";
 if(!fs::exists(journal))return;auto flags=read(journal);if(flags.size()<2)fail("Invalid installation journal");int i=0;
 for(auto n:{"bin","data"}){auto backup=work/(string("previous-")+n);if(fs::exists(backup)){fs::remove_all(root/n);fs::rename(backup,root/n);}else if(flags[i]=='0')fs::remove_all(root/n);i++;}
 fs::remove(root/"config/install-native-v1.txt");fs::remove(journal);
}
static void install(){
 safe_trees();for(auto n:{"tmp","config","userdata","cache","saves","source"})fs::create_directories(root/n);
 int lock=open((root/"tmp/install.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0644);if(lock<0||flock(lock,LOCK_EX|LOCK_NB))fail("Another installer is already running"); // process owns lock until exit
 // Recover the former Python installer's transaction as well.
 if(fs::exists(root/"tmp/install-publishing.json")){
  auto text=read(root/"tmp/install-publishing.json");for(auto n:{"bin","data"}){auto backup=root/"tmp/install-work"/(string("previous-")+n);auto key=text.find(string("\"")+n+"\"");auto colon=text.find(':',key);auto value=text.find_first_not_of(" \t\r\n",colon+1);if(key==string::npos||colon==string::npos||value==string::npos)fail("Invalid legacy installation journal");if(fs::exists(backup)){fs::remove_all(root/n);fs::rename(backup,root/n);}else if(text.compare(value,5,"false")==0)fs::remove_all(root/n);}
  fs::remove(root/"config/install-v12.json");fs::remove(root/"tmp/install-publishing.json");
 }
 recover();auto marker=root/"config/install-native-v1.txt";
 if(fs::exists(marker)&&read(marker)==string(iso_sha)+"\n")try{verify(root,payload_assets,payload_count,false);verify(root,menu_assets,menu_count,false);progress(100,"READY");return;}catch(...){}
 if(fs::is_directory(root/"bin")&&fs::is_directory(root/"data")){
  progress(-1,"VERIFYING EXISTING GAME FILES");try{verify(root,payload_assets,payload_count,true);menu(root);receipt();progress(100,"READY");return;}catch(const std::exception &e){fprintf(stderr,"Existing install: %s\n",e.what());if(!fs::exists(root/"source/the_thing.iso"))throw;}
 }
 auto iso=root/"source/the_thing.iso";if(!fs::is_regular_file(iso))fail("Missing source/the_thing.iso");progress(-1,"VERIFYING ORIGINAL ISO");if(digest(iso)!=iso_sha)fail("Wrong ISO checksum");if(fs::space(root).available<1500ULL*1024*1024)fail("Need 1500 MB free for installation");
 auto work=root/"tmp/native-install-work";fs::remove_all(work);fs::create_directories(work);auto tree=work/"iso",payload=work/"payload",stage=work/"game";
 progress(5,"EXTRACTING ISO");tool("7z",{"x","-y",iso.string(),"-o"+tree.string()});progress(25,"INSTALLING ORIGINAL GAME");tool("unshield",{"-d",payload.string(),"x",(tree/"data1.cab").string()});
 fs::create_directories(stage);fs::copy(payload/"US_bin/bin",stage/"bin",fs::copy_options::recursive);fs::create_directories(stage/"data");for(auto n:{"levels","English_data","FMV","Default_language_resources","Global_data"})fs::copy(payload/n/"data",stage/"data",fs::copy_options::recursive|fs::copy_options::overwrite_existing);
 for(int v=1;v<=2;v++){
  progress(v==1?50:65,v==1?"APPLYING PATCH 1.1":"APPLYING PATCH 1.2");string version=v==1?"1.1":"1.2";auto patch=work/("patch"+version);tool("7z",{"x","-y",(tree/"UPDATE"/("Version "+version+" Eng")/(v==1?"thething_patch_10_11.exe":"thething_patch_11_12.exe")).string(),"-o"+patch.string()});
  const char *deltas[]={"thething.exe","english.pak","resources.pak","mainui.pak","level5a.dll","level6.dll"};const char *targets[]={"bin/thething.exe","data/english.pak","data/resources.pak","data/mainui.pak","data/Levels/level5a.dll","data/Levels/level6.dll"};
  for(int i=0;i<(v==1?6:1);i++){auto old=stage/targets[i],fresh=old;fresh+=".patched";tool("xdelta",{"patch",(patch/"Disk1"/(v==2?"TheThing.exe":deltas[i])).string(),old.string(),fresh.string()});fs::rename(fresh,old);}
 }
 auto exe=tree/"UPDATE/Version 1.2 Eng/CRACK/TheThing.exe";if(digest(exe)!=payload_assets[0].sha)fail("Executable checksum mismatch");fs::copy_file(exe,stage/"bin/thething.exe",fs::copy_options::overwrite_existing);
 progress(80,"VERIFYING PATCHED GAME");verify(stage,payload_assets,payload_count,true);progress(90,"PREPARING MENU RESOURCES");menu(stage);progress(95,"FINISHING INSTALLATION");
 string flags;for(auto n:{"bin","data"})flags+=fs::exists(root/n)?'1':'0';atomic_write(root/"tmp/native-publishing.txt",flags);
 for(auto n:{"bin","data"}){if(fs::exists(root/n))fs::rename(root/n,work/(string("previous-")+n));fs::rename(stage/n,root/n);}
 fs::remove_all(root/"extracted-iso");fs::rename(tree,root/"extracted-iso");receipt();fs::remove(root/"tmp/native-publishing.txt");fs::remove_all(work);progress(100,"READY");
}
static string canonical(string s){string out;for(char c:s){if(c=='\\'&&!out.empty()&&out.back()=='\\')continue;out+=c;}out=lower(out);if(out.rfind("hkcu\\",0)==0)out="hkey_current_user"+out.substr(4);return out;}
static void shadows(){
 safe_trees();string choice="0";auto cfg=root/"config/launcher.cfg";if(fs::exists(cfg)){std::istringstream f(read(cfg));string l;while(std::getline(f,l))if(l.rfind("shadow_quality=",0)==0){choice=l.substr(15);if(!choice.empty()&&choice.back()=='\r')choice.pop_back();}}
 if(auto v=getenv("THING_SHADOW_QUALITY"))choice=v;if(choice!="0"&&choice!="1")fail("Invalid shadow quality");
 auto p=root/"config/registry.txt";string old=fs::exists(p)?read(p):"",out;bool found=false;string key="HKEY_CURRENT_USER\\SOFTWARE\\Computer Artworks\\The Thing\\1.0\\CircleShadowLOD";
 size_t pos=0;while(pos<old.size()){auto end=old.find('\n',pos);if(end==string::npos)end=old.size();string line=old.substr(pos,end-pos);auto eq=line.find('=');if(eq!=string::npos&&canonical(line.substr(0,eq))==canonical(key)){out+=line.substr(0,eq)+"=sz:"+(choice=="0"?"1":"0.2")+(line.back()=='\r'?"\r":"");found=true;}else out+=line;if(end<old.size())out+='\n';pos=end+1;}
 if(!found){if(!out.empty()&&out.back()!='\n')out+='\n';out+=key+"=sz:"+(choice=="0"?"1":"0.2")+"\n";}if(out!=old)atomic_write(p,out);fprintf(stderr,"[shadows] %s\n",choice=="0"?"SIMPLE CircleShadowLOD=1":"ORIGINAL CircleShadowLOD=0.2");
}
extern "C" int thing_native_setup(const char *directory,int shadow_only){
 try{root=fs::absolute(directory);if(shadow_only)shadows();else install();return 0;}catch(const std::exception &e){fprintf(stderr,"[setup-error] %s\n",e.what());if(!shadow_only)try{progress(-1,string(e.what()).substr(0,100));}catch(...){}return 1;}
}
