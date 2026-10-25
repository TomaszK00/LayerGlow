// Build against the same pinned ArduinoJson headers as the firmware.
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <iostream>
#include <ArduinoJson.h>
constexpr int HEX=16;
class String:public std::string{
 public:
 using std::string::string;
 String(const std::string& s):std::string(s){}
 String(unsigned n,int base){std::ostringstream out;if(base==16)out<<std::hex;out<<n;assign(out.str());}
 bool isEmpty()const{return empty();}
 size_t length()const{return size();}
 size_t write(uint8_t c){push_back(c);return 1;}
 size_t write(const uint8_t* p,size_t n){append((const char*)p,n);return n;}
 friend String operator+(const String& a,const String& b){return String(static_cast<const std::string&>(a)+b.c_str());}
 friend String operator+(const String& a,const char* b){return a+String(b);}
 friend String operator+(const char* a,const String& b){return String(a)+b;}
};
namespace ArduinoJson{
 template<>struct Converter<String>{
  static void toJson(const String& s,JsonVariant dst){dst.set(s.c_str());}
  static String fromJson(JsonVariantConst src){const char* s=src.as<const char*>();return s?String(s):String();}
  static bool checkJson(JsonVariantConst src){return src.is<const char*>();}
 };
}
struct IPAddress{bool fromString(const String& v){int a,b,c,d;char extra;return std::sscanf(v.c_str(),"%d.%d.%d.%d%c",&a,&b,&c,&d,&extra)==4&&a>=0&&a<256&&b>=0&&b<256&&c>=0&&c<256&&d>=0&&d<256;}};
struct Preferences{
 static inline std::map<std::string,std::map<std::string,std::string>> db;
 static inline bool fail=false;
 std::string ns;
 bool begin(const char* name,bool){ns=name;return !fail;}void end(){}
 bool isKey(const char* k){return db[ns].count(k);}
 size_t getBytesLength(const char* k){return db[ns][k].size();}
 size_t getBytes(const char* k,void* p,size_t n){n=std::min(n,db[ns][k].size());std::memcpy(p,db[ns][k].data(),n);return n;}
 size_t putBytes(const char* k,const void* p,size_t n){if(fail)return 0;db[ns][k]=std::string((const char*)p,n);return n;}
};

struct{int disconnects=0;void disconnect(){disconnects++;}}bambuMqtt;
String printerType="moonraker",printerIP="192.168.1.9",moonIP=printerIP,bambuIP,octoIP,prusaIP,bambuSerial,bambuAccessCode,octoKey,prusaKey,ssid="Home";
uint16_t printerPort=7125,moonPort=7125,octoPort=80,prusaPort=80;
bool printerEnabled=true,printerConnected=false;String printerState,printerFile,lastError;float printerProgress=0;
uint32_t printerConfigRevision=1,lastPrinterReadAt=0,lastBambuAttempt=0,lastPoll=0,lastBambuReport=0;
void updateLEDs(){}
// PROFILE_FUNCTIONS
int main(){
 PrinterProfile initial;initial.id="00000001";initial.name="Workshop";initial.type="bambu";initial.ip="192.168.1.11";initial.port=8883;initial.serial="SERIAL01";initial.secret="ONE";
 PrinterProfile rows[MAX_PRINTER_PROFILES];rows[0]=initial;rows[1]=initial;rows[1].id="00000002";rows[1].ip="192.168.1.12";rows[1].secret="TWO";
 assert(commitPrinterProfiles(rows,2,0,true));applySavedPrinter();assert(bambuAccessCode=="ONE");
 Preferences::fail=true;assert(!commitPrinterProfiles(rows,2,1,true));assert(activePrinter==0&&bambuAccessCode=="ONE");Preferences::fail=false;
 assert(commitPrinterProfiles(rows,2,1,true));applySavedPrinter();assert(bambuAccessCode=="TWO");
 // Reboot: globals have been reset, then the real JSON loader restores the selected secret and enabled flag.
 printerProfileCount=0;activePrinter=-1;printerEnabled=false;bambuAccessCode="";loadPrinterProfiles();
 assert(printerProfileCount==2&&activePrinter==1&&printerEnabled&&bambuAccessCode=="TWO");
 JsonDocument publicInfo;addSavedPrinters(publicInfo);String publicJson;serializeJson(publicInfo,publicJson);
 assert(publicJson.find("ONE")==std::string::npos&&publicJson.find("TWO")==std::string::npos);
 assert(publicInfo["savedPrinters"][1]["hasSecret"].as<bool>());
 // Disconnect survives reboot, while the saved printer is retained.
 assert(commitPrinterProfiles(printerProfiles,2,1,false));loadPrinterProfiles();assert(!printerEnabled&&activePrinter==1&&printerProfileCount==2);
 // Remove active: no automatic switch to another printer, including after reboot.
 assert(commitPrinterProfiles(rows,1,-1,false));loadPrinterProfiles();assert(printerProfileCount==1&&activePrinter==-1&&!printerEnabled);
 assert(!commitPrinterProfiles(rows,9,0,true));assert(!commitPrinterProfiles(rows,1,1,true));
 rows[1]=rows[0];assert(!commitPrinterProfiles(rows,2,0,true));
 // All 8 maximal profiles fit in the blob storage rather than the 4 KiB NVS string path.
 for(int i=0;i<8;i++){rows[i]=initial;rows[i].id="0000000"+String(i+1,HEX);rows[i].name=String(std::string(48,'n'));rows[i].network=String(std::string(32,'s'));rows[i].type="octoprint";rows[i].port=80;rows[i].secret=String(std::string(128,'k'));rows[i].serial="";}
 assert(commitPrinterProfiles(rows,8,7,true));loadPrinterProfiles();assert(printerProfileCount==8&&activePrinter==7&&octoKey==String(std::string(128,'k')));
 // Corrupt persisted JSON never imports legacy settings or overwrites the damaged record silently.
 Preferences::db["lgprinters"]["profiles"]="broken";loadPrinterProfiles();assert(!printerProfileStorageValid&&!printerEnabled);assert(!commitPrinterProfiles(rows,1,0,true));
 printerProfileStorageValid=true;Preferences::db.clear();printerProfileCount=0;activePrinter=-1;printerType="moonraker";printerIP=moonIP;printerEnabled=true;loadPrinterProfiles();assert(printerProfileCount==0&&!printerEnabled);
 Preferences::db.clear();Preferences::db["k2led"]["ip"]="configured";printerIP=moonIP;printerEnabled=true;loadPrinterProfiles();assert(printerProfileCount==1&&activePrinter==0&&printerType=="moonraker");
 std::cout<<"printer profile persistence scenarios passed\n";
}
