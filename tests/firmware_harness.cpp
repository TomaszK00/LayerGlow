#include <algorithm>
#include <any>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>

class String:public std::string {
 public:
  using std::string::string;
  String(const std::string& s):std::string(s){}
  String(int n):std::string(std::to_string(n)){}
  bool isEmpty()const{return empty();}
  void toLowerCase(){for(auto& c:*this)c=std::tolower(static_cast<unsigned char>(c));}
  bool endsWith(const String& suffix)const{return size()>=suffix.size()&&compare(size()-suffix.size(),suffix.size(),suffix)==0;}
  void remove(size_t pos){erase(pos);}
  void replace(const String& from,const String& to){size_t p=0;while((p=find(from,p))!=npos){std::string::replace(p,from.size(),to);p+=to.size();}}
  friend String operator+(const String& a,const String& b){return String(static_cast<const std::string&>(a)+static_cast<const std::string&>(b));}
  friend String operator+(const String& a,const char* b){return a+String(b);}
  friend String operator+(const char* a,const String& b){return String(a)+b;}
  friend String operator+(const String& a,int b){return a+String(b);}
};
struct Preferences {
 std::map<std::string,std::any> data; bool fail=false,beginFail=false;int writes=0;
 bool begin(const char*,bool){return !beginFail;} void end(){}
 bool isKey(const char* k){return data.count(k);}
 template<class T>T get(const char* k,T fallback){auto i=data.find(k);return i==data.end()?fallback:std::any_cast<T>(i->second);}
 String getString(const char* k,const String& d=""){return get<String>(k,d);}
 int getInt(const char* k,int d=0){return get<int>(k,d);}
 uint8_t getUChar(const char* k,uint8_t d=0){return get<uint8_t>(k,d);}
 uint16_t getUShort(const char* k,uint16_t d=0){return get<uint16_t>(k,d);}
 bool getBool(const char* k,bool d=false){return get<bool>(k,d);}
 template<class T>size_t put(const char* k,T value){writes++;if(fail)return 0;data[k]=value;return sizeof(T);}
 size_t putString(const char* k,const String& value){auto n=put(k,value);return n?value.size():0;}
 size_t putInt(const char* k,int v){return put(k,v);}size_t putUChar(const char* k,uint8_t v){return put(k,v);}
 size_t putUShort(const char* k,uint16_t v){return put(k,v);}size_t putBool(const char* k,bool v){return put(k,v);}
 bool remove(const char* k){if(fail)return false;data.erase(k);return true;}
} prefs;
struct IP {String value;String toString()const{return value;}};
constexpr int WL_CONNECTED=3;
struct FakeWifi {
 int connection=WL_CONNECTED,clients=0,reconnects=0;bool apClosed=false;
 int status(){return connection;}int softAPgetStationNum(){return clients;}
 bool softAPdisconnect(bool){apClosed=true;return true;}
 void scanDelete(){}void disconnect(bool){connection=0;}void reconnect(){reconnects++;}
 IP localIP(){return {"192.168.1.8"};}IP softAPIP(){return {"192.168.4.1"};}String macAddress(){return "01:02:03:04:05:06";}
} WiFi;
struct FakeMqtt {
 bool online=false,connectOk=false,loopOk=true,publishOk=true,refreshReport=false;unsigned buffer=256;int disconnected=0;int code=-1;
 bool connected(){return online;}void disconnect(){online=false;disconnected++;}
 unsigned getBufferSize(){return buffer;}bool setBufferSize(unsigned n){buffer=n;return true;}
 void setServer(const char*,int){}bool connect(const char*,const char*,const char*){online=connectOk;return online;}
 int state(){return code;}bool subscribe(const char*){return true;}
 bool loop();bool publish(const char*,const char*){return publishOk;}
} bambuMqtt;
uint32_t clockMs=1000;uint32_t millis(){return clockMs;}
uint32_t printerConfigRevision=1;
uint32_t lastBambuReport=0,lastBambuRequest=0,lastBambuAttempt=0;
bool FakeMqtt::loop(){if(refreshReport){clockMs+=5;lastBambuReport=clockMs;}return loopOk;}
String printerType="bambu",printerIP="192.168.1.9",bambuIP="192.168.1.10",moonIP,octoIP,prusaIP,bambuSerial="12345678",bambuAccessCode="12345678",octoKey,prusaKey;
uint16_t printerPort=7125,moonPort=7125,octoPort=80,prusaPort=80;
bool printerConnected=false,printerEnabled=true,simEnabled=false,otaUploadInProgress=false;
String printerState="standby",printerFile,lastError;float printerProgress=0;int repaints=0;void updateLEDs(){repaints++;}
constexpr uint8_t MAX_WIFI_PROFILES=8;struct WifiProfile{String name,password;};WifiProfile wifiProfiles[8];
uint8_t wifiProfileCount=0,activeWifi=0;String ssid,wifiPassword,webHost="layerglow";
bool wifiScanActive=false,wifiChangePending=false,apMode=false,mdnsNeedsRestart=false;
uint32_t wifiScanStarted=0,wifiChangeAt=0,wifiAttemptStarted=0,wifiConnectedSince=0,wifiLostSince=0,lastApAttempt=0;
void configTime(int,int,const char*,const char*){}void maintainMdns(uint32_t){}
int profileAttempts=0;void connectWifiProfile(uint8_t index,uint32_t now){activeWifi=index;wifiAttemptStarted=now;profileAttempts++;}
bool startRecoveryAP(){apMode=true;return true;}
struct {template<class T>void println(T){}} Serial;
String rowColors[6],rowExtraColors[6][11];uint8_t rowBrightness[6]={},rowColorCount[6]={},rowStyle[6]={};
bool rowPulse[6]={},rowEnabled[6]={};uint16_t rowPeriod[6]={};int activeLedCount=24,intervalMs=500;
constexpr int HTTP_POST=1,UPLOAD_FILE_START=0,UPLOAD_FILE_WRITE=1,UPLOAD_FILE_END=2,UPLOAD_FILE_ABORTED=3;
struct HTTPUpload{int status=0;String filename;uint8_t buf[32]={};size_t currentSize=0;} upload;
struct FakeServer{
 String host="layerglow.local",origin="http://layerglow.local",message;int code=0;std::function<void()> complete,chunk;
 String hostHeader(){return host;}String header(const char*){return origin;}
 void send(int c,const char*,const String& m){code=c;message=m;}HTTPUpload& upload(){return ::upload;}
 void on(const char*,int,std::function<void()> c,std::function<void()> h){complete=c;chunk=h;}
} server;
constexpr int UPDATE_SIZE_UNKNOWN=0,U_FLASH=0;
struct {bool error=false,started=false,finished=false;int begins=0;bool hasError(){return error;}String errorString(){return "write failed";}
 bool begin(int,int){begins++;started=true;return true;}size_t write(uint8_t*,size_t n){return n;}
 bool end(bool){finished=true;return true;}void abort(){started=false;finished=false;}
} Update;
bool otaImageReady=false;size_t otaBytesWritten=0;String updateError;
struct {int restarts=0;void restart(){restarts++;}} ESP;void delay(int){}
struct FakeQueue{std::any item;};using QueueHandle_t=FakeQueue*;constexpr int pdTRUE=1;
template<class T>int xQueueReceive(QueueHandle_t queue,T* result,int){if(!queue->item.has_value())return 0;*result=std::any_cast<T>(queue->item);queue->item.reset();return pdTRUE;}
template<class T>int xQueueSend(QueueHandle_t queue,const T* value,int){queue->item=*value;return pdTRUE;}
size_t strlcpy(char* dest,const char* src,size_t size){size_t n=strlen(src);if(size){std::memcpy(dest,src,std::min(n,size-1));dest[std::min(n,size-1)]=0;}return n;}
uint32_t lastPoll=0,lastPrinterReadAt=0;
// FIRMWARE_FUNCTIONS
int main(){
 // NVS: failure must surface; empty password is a valid successfully stored value.
 Preferences storage;storage.fail=true;assert(!storeString(storage,"x","#123456"));assert(!storeInt(storage,"n",12));
 storage.fail=false;assert(storeString(storage,"x",""));int writes=storage.writes;assert(storeString(storage,"x",""));assert(storage.writes==writes);
 assert(storeBool(storage,"on",false));assert(storePort(storage,"port",7125));
 prefs.beginFail=true;assert(!saveLighting());assert(!saveWifiSettings());assert(!savePrinterSettings());prefs.beginFail=false;
 prefs.fail=true;assert(!saveLighting());assert(!saveWifiSettings());assert(!savePrinterSettings());prefs.fail=false;
 assert(saveLighting());assert(saveWifiSettings());assert(savePrinterSettings());
 // Wi-Fi: recovery phone slows retry but no longer blocks it forever.
 WiFi.connection=0;WiFi.clients=1;apMode=true;wifiProfileCount=2;wifiLostSince=1;wifiAttemptStarted=1000;
 maintainNetwork(30999);assert(profileAttempts==0);maintainNetwork(31000);assert(profileAttempts==1&&activeWifi==1);assert(!WiFi.apClosed);
 wifiProfileCount=1;wifiAttemptStarted=31000;maintainNetwork(61000);assert(WiFi.reconnects==1);
 wifiScanActive=true;wifiScanStarted=61000;maintainNetwork(62000);assert(WiFi.reconnects==1);wifiScanActive=false;
 // Bambu: Wi-Fi loss clears real reading and repaints once, preserving active simulation.
 printerConnected=true;printerState="printing";printerProgress=.5;printerFile="job.gcode";bambuMqtt.online=true;
 maintainBambu(1000);assert(!printerConnected&&printerState=="standby"&&printerFile.isEmpty()&&printerProgress==0&&repaints==1);
 maintainBambu(1000);assert(repaints==1);
 simEnabled=true;printerConnected=true;printerState="printing";maintainBambu(1000);assert(repaints==1);simEnabled=false;
 // A callback receives a report after the caller captured now: no unsigned timeout.
 WiFi.connection=WL_CONNECTED;bambuMqtt.online=true;bambuMqtt.refreshReport=true;clockMs=1000;lastBambuRequest=1000;
 maintainBambu(1000);assert(bambuMqtt.online&&lastBambuReport==1005);bambuMqtt.refreshReport=false;
 printerConnected=true;printerState="printing";bambuMqtt.loopOk=false;maintainBambu(1010);assert(!printerConnected&&printerState=="standby");bambuMqtt.loopOk=true;
 // Queued HTTP results cannot resurrect a printer after configuration changes.
 FakeQueue requests,results;httpPrinterRequests=&requests;httpPrinterResults=&results;
 printerType="moonraker";printerEnabled=true;printerConnected=false;
 HttpPrinterResult reading={};reading.revision=printerConfigRevision-1;reading.connected=true;strlcpy(reading.state,"printing",sizeof(reading.state));
 results.item=reading;maintainHttpPrinter(2000);assert(!printerConnected);
 reading.revision=printerConfigRevision;results.item=reading;maintainHttpPrinter(2000);assert(printerConnected&&printerState=="printing");
 printerEnabled=false;printerConnected=false;results.item=reading;maintainHttpPrinter(2000);assert(!printerConnected);printerEnabled=true;
 // Origin/Host guard rejects external pages and DNS rebinding hosts.
 assert(allowMutation());server.origin="http://evil.example";assert(!allowMutation()&&server.code==403);
 server.origin="";assert(allowMutation());server.host="evil.example";assert(!allowMutation());server.host="192.168.4.1";assert(allowMutation());
 // OTA: no file/aborted/empty upload cannot restart; uppercase .BIN can finish.
 server.host="layerglow.local";server.origin="http://layerglow.local";registerOta();server.complete();assert(ESP.restarts==0&&server.code==500);
 upload.status=UPLOAD_FILE_START;upload.filename="test.BIN";server.chunk();upload.status=UPLOAD_FILE_WRITE;upload.currentSize=16;server.chunk();upload.status=UPLOAD_FILE_END;server.chunk();server.complete();assert(ESP.restarts==1&&server.code==200);
 server.complete();assert(ESP.restarts==1&&server.code==500);
 upload.status=UPLOAD_FILE_START;server.chunk();upload.status=UPLOAD_FILE_ABORTED;server.chunk();server.complete();assert(ESP.restarts==1&&!otaUploadInProgress);
 upload.status=UPLOAD_FILE_START;server.chunk();upload.status=UPLOAD_FILE_END;server.chunk();server.complete();assert(ESP.restarts==1);
 int begins=Update.begins;server.origin="http://evil.example";upload.status=UPLOAD_FILE_START;server.chunk();upload.status=UPLOAD_FILE_WRITE;server.chunk();server.complete();assert(Update.begins==begins&&ESP.restarts==1);
 std::cout<<"firmware regression scenarios passed\\n";
}
