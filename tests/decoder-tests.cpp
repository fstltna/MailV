#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
typedef unsigned short UniChar; typedef std::vector<unsigned char>* RefArg; typedef RefArg Ref;
const int kEndOfCharString=0, kEndOfUnicodeString=0, kNSErrNotABinaryObject=0;
static bool IsBinary(RefArg){return true;} static void ThrowBadTypeWithFrameData(int,RefArg){}
static unsigned Length(RefArg r){return r->size();} static void SetLength(RefArg r,unsigned n){r->resize(n);}
#define WITH_LOCKED_BINARY(r,p) { void* p = r->data();
#define END_WITH_LOCKED_BINARY(r) }
#include "harness_funcs.inc"
static std::string b64(const char*in){std::vector<unsigned char> v(in,in+strlen(in));DecodeBase64(0,&v);return std::string(v.begin(),v.end());}
static std::string qp(const char*in){std::vector<unsigned char> v(in,in+strlen(in));v.push_back(0xEE);v.pop_back();DecodeQuotedPrintable(0,&v);std::string s(v.begin(),v.end());return s.substr(0,strlen(s.c_str()));}
int main(){
 int fail=0; auto chk=[&](const char*name,std::string got,std::string want){bool ok=got==want;fail+=!ok;printf("%s %s: [%s]\n",ok?"PASS":"FAIL",name,got.c_str());};
 chk("b64 basic",b64("SGVsbG8gd29ybGQ="),"Hello world");
 chk("b64 pad2",b64("SGk="),"Hi"); chk("b64 pad1? 'Hel'",b64("SGVs"),"Hel"); chk("b64 1byte",b64("SA=="),"H");
 chk("b64 CRLF inside group",b64("SGVs\r\nbG8g\r\nd29y\r\nbGQ=\r\n"),"Hello world");
 chk("b64 high bytes ignored",b64("SGVs\xC3\xA9" "bG8="),"Hello");
 chk("b64 tiny",b64("S"),""); chk("b64 empty",b64(""),"");
 chk("qp basic",qp("caf=C3=A9 ok"),"caf\xC3\xA9 ok");
 chk("qp soft CRLF",qp("ab=\r\ncd"),"abcd"); chk("qp soft LF",qp("ab=\ncd"),"abcd");
 chk("qp no escapes",qp("plain text"),"plain text");
 chk("qp stray =",qp("2+2= 4"),"2+2= 4"); chk("qp bad esc",qp("x=ZZy"),"x=ZZy");
 // utf8
 auto u8=[&](const char*in){std::vector<UniChar> o(strlen(in)+1);long n=ConvertToUnicodeFunc_UTF8(in,o.data(),0,strlen(in));std::string s;for(long i=0;i<n;i++){char b[8];if(o[i]<128)s+=(char)o[i];else{snprintf(b,8,"<%04X>",o[i]);s+=b;}}return s;};
 chk("utf8 2byte",u8("caf\xC3\xA9"),"caf<00E9>"); chk("utf8 3byte",u8("\xE2\x82\xAC""5"),"<20AC>5");
 chk("utf8 emoji",u8("hi \xF0\x9F\x98\x80!"),"hi ?!"); chk("utf8 stray cont",u8("a\x80z"),"a?z");
 chk("utf8 truncated",u8("a\xE2\x82"),"a?"); chk("utf8 strip LF",u8("a\nb"),"ab");
 printf("%d failures\n",fail); return fail;}
