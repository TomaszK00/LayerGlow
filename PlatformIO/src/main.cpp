#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "src/PubSubClient.h"
#include <time.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// LayerGlow v2.53 – FatGiantCat
// Set only after confirming the physical strip wiring. Never connect 24 V to ESP32/LEDs.
#define LED_PIN 4
#define LED_COUNT 24 // default; configured total is 5..300 addressable RGBW LEDs
#define MAX_LED_COUNT 300
#define MAX_LED_PAIRS ((MAX_LED_COUNT+1)/2)
#define STATUS_LED_PIN 8 // ESP32-C3 SuperMini onboard blue LED (active LOW)
#define LED_ENABLED true
const char* AP_NAME = "LayerGlow-Setup";
// Only used during migration before the owner pairs this device physically.
const char* INITIAL_AP_PASSWORD = "layerglowsetup";
constexpr uint8_t PAIR_BUTTON_PIN = 9; // BOOT on the reference ESP32-C3 SuperMini
String adminSalt,adminHash,apPassword;
uint8_t authFailures=0;
uint32_t authRetryAt=0,physicalResetAt=0;
bool restartAfterPairing=false;
uint32_t pairButtonReleasedAt=0;

// The corresponding private key is never included in firmware or the repository.
const char OTA_PUBLIC_KEY[] = R"PEM(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuroo+vEDZ6V3SPUZoCck
lRscKK1fi/uPDpWOzIDyhO8VOQJuPE6hl+Tz1GvJ7xJw4NyNKSO+2Iq3Oaqi0n9N
+ruChYiQUwOEpQqzRNpdXakSNx/Mq+g4dQLgGWiO1eFOe0DcHqRd4FS/Z6ZKqnYw
hKg2exDb74YqfOm3KdEhs5vtE7nlFT86uG5S5CA+vy8DzrWs+PDE8un7ygJBn0LJ
GGEwirgkDfUI6vekJgWzmE2ot10fLlTV2CKOKn5ZgXHmczqJXZEZstdby/bPQZN0
hll9+vfKNu3qj6+V15GzspEY0UUO3pbbGtHEKJVEjAWo77FBT5okioQklv3i5Lqc
twIDAQAB
-----END PUBLIC KEY-----
)PEM";
mbedtls_sha256_context otaHash;
bool otaHashStarted=false,otaSignatureValid=false;
uint8_t otaSignature[256];
size_t otaSignatureLength=0;

WebServer server(80);
Preferences prefs;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
String ssid, wifiPassword, printerIP, webHost="layerglow", mdnsHostActive="";
constexpr uint8_t MAX_WIFI_PROFILES=8;
struct WifiProfile { String name, password, staticIP, gateway, subnet, dns; bool useStatic=false; };
WifiProfile wifiProfiles[MAX_WIFI_PROFILES];
uint8_t wifiProfileCount=0, activeWifi=0;
bool migrateLegacyWifi=false;
uint32_t wifiAttemptStarted=0;
bool mdnsStarted=false, mdnsNeedsRestart=false;
uint32_t lastMdnsAttempt=0,mdnsChangeAt=0;
String printerType="moonraker", moonIP, bambuIP, octoIP, prusaIP, bambuSerial, bambuAccessCode, octoKey, prusaKey;
constexpr uint8_t MAX_PRINTER_PROFILES=8;
struct PrinterProfile { String id,name,type,ip,mac,serial,secret,network; uint16_t port=7125; };
PrinterProfile printerProfiles[MAX_PRINTER_PROFILES];
uint8_t printerProfileCount=0;
int activePrinter=-1;
bool printerProfileStorageValid=true;
WiFiClientSecure bambuTLS;
PubSubClient bambuMqtt(bambuTLS);
uint32_t lastBambuAttempt=0,lastBambuReport=0,lastBambuRequest=0;
uint16_t printerPort = 7125, moonPort=7125, octoPort=80, prusaPort=80;
const char* const DEFAULT_ROW_COLORS[6] = {"#00c8f0", "#ff9e35", "#ffce45", "#2fe182", "#ff445c", "#ffffff"};
const char* const stateNames[6] = {"printing", "heating", "paused", "complete", "error", "standby"};
String rowColors[6] = {"#00c8f0", "#ff9e35", "#ffce45", "#2fe182", "#ff445c", "#ffffff"};
const char* const DEFAULT_EXTRA_COLORS[11] = {"#ff9e35", "#ffce45", "#2fe182", "#9b5cff", "#ff445c", "#2878ff", "#ffffff", "#ff3366", "#3edbd1", "#f0df56", "#704dff"};
uint8_t rowColorCount[6] = {1,1,1,1,1,1};
String rowExtraColors[6][11];
uint8_t rowBrightness[6] = {65,65,65,65,65,65};
bool rowPulse[6] = {false,false,false,false,false,false};
bool rowEnabled[6] = {true,true,true,true,true,true};
uint8_t rowStyle[6] = {0,0,0,0,0,0}; // 0 whole bar, 1 last pair, 2 sweeping light
uint16_t rowPeriod[6] = {1400,1400,1400,1400,1400,1400};
int intervalMs = 500;
int activeLedCount = 24;
uint32_t lastPulseFrame = 0;
String printerState = "unknown", printerFile = "";
float printerProgress = 0;
bool printerConnected = false, printerEnabled = true;
uint32_t lastPrinterReadAt = 0;
String lastError = "Waiting for first poll";
uint32_t lastPoll = 0, simCycleStartedAt = 0;
uint32_t printerConfigRevision=1;
constexpr uint32_t SIM_STEP_MS = 500, SIM_HOLD_MS = 1000;
constexpr uint32_t SIM_CYCLE_MS = 20 * SIM_STEP_MS + SIM_HOLD_MS;
bool apMode = false, simEnabled = false;
int simStatus = 0, simProgress = 0;
String rememberedColours[8];
uint8_t rememberedColourCount = 0;
uint32_t bootIdentifier=0;
bool otaImageReady=false;
size_t otaBytesWritten=0;
String updateError = "";
bool updateAvailable = false;

uint32_t wifiLostSince = 0, lastWifiAttempt = 0, wifiConnectedSince = 0, lastApAttempt = 0;
uint8_t wifiRetryCount = 0;
bool wifiChangePending = false, otaUploadInProgress = false;
uint32_t wifiChangeAt = 0;
bool wifiScanActive = false, lightingBackupAvailable = false;
bool wifiScanLaunched = false;
uint32_t wifiScanStarted = 0, wifiScanNextTry = 0, wifiScanResumeAt = 0;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LayerGlow by FatGiantCat</title><meta name="theme-color" content="#143b2b"><link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%220%200%2064%2064%22%20width%3D%22256%22%20height%3D%22256%22%3E%0A%3Crect%20width%3D%2264%22%20height%3D%2264%22%20rx%3D%2214%22%20fill%3D%22%230f1419%22%2F%3E%0A%3Cdefs%3E%0A%3ClinearGradient%20id%3D%22g%22%20gradientUnits%3D%22userSpaceOnUse%22%20x1%3D%220%22%20y1%3D%2211%22%20x2%3D%220%22%20y2%3D%2254%22%3E%3Cstop%20offset%3D%220%22%20stop-color%3D%22%2322d3ee%22%2F%3E%3Cstop%20offset%3D%221%22%20stop-color%3D%22%234ade80%22%2F%3E%3C%2FlinearGradient%3E%0A%3Cfilter%20id%3D%22glow%22%20x%3D%22-50%25%22%20y%3D%22-50%25%22%20width%3D%22200%25%22%20height%3D%22200%25%22%3E%3CfeGaussianBlur%20stdDeviation%3D%222.5%22%2F%3E%3C%2Ffilter%3E%0A%3C%2Fdefs%3E%0A%3Cg%20fill%3D%22url(%23g)%22%3E%0A%3Cg%20filter%3D%22url(%23glow)%22%20opacity%3D%220.85%22%3E%0A%3Crect%20x%3D%2218%22%20y%3D%2211%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2218.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2226%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2233.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2241%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2248.5%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%0A%3C%2Fg%3E%0A%3Crect%20x%3D%2218%22%20y%3D%2211%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2218.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2226%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2233.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2241%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2248.5%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%0A%3C%2Fg%3E%0A%3C%2Fsvg%3E">
<script>
try {
  const mode=localStorage.getItem('layerglowTheme');
  document.documentElement.dataset.theme=mode==='dark'||(mode!=='light'&&window.matchMedia('(prefers-color-scheme: dark)').matches)?'dark':'light';
} catch(e) {document.documentElement.dataset.theme='light'}
</script><style>
:root{font-family:Inter,system-ui,-apple-system,Segoe UI,sans-serif;color-scheme:light;--bg:#f5f7f6;--panel:#fff;--field:#fff;--border:#dce3df;--text:#181d1a;--muted:#65736b;--green:#20b461}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text)}main{max-width:1100px;margin:auto;padding:24px 18px 44px}header,.headbar{display:flex;justify-content:space-between;align-items:center;gap:14px;flex-wrap:wrap}header{margin-bottom:24px;padding:12px 0;border-bottom:1px solid var(--border)}h1{font-size:clamp(22px,3vw,30px);margin:0;font-weight:700}h1 span,.brand{color:var(--green)}h2{font-size:17px;margin:0 0 17px}p{margin:9px 0}.muted,small{font-size:13px;color:var(--muted)}.card{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:22px;margin-bottom:17px}.cols{display:grid;grid-template-columns:1fr 1fr;gap:17px}.chips{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}.chip{background:#edf5f0;padding:6px 11px;border-radius:6px;font-size:13px}.chip.ok{color:#137a43}.chip.error{color:#b53737}label{display:block;margin:13px 0 6px;color:#26362c;font-size:13px}input,select,button{font:inherit}input:not([type=color]):not([type=range]):not([type=checkbox]):not([type=file]),select{display:block;width:100%;max-width:340px;background:var(--field);border:1px solid #b8c7be;color:#18241c;border-radius:7px;padding:8px 10px;min-height:39px}input[type=number]{max-width:130px!important}#ip{max-width:205px}#ssid,#pass{max-width:300px}#wifiList{max-width:310px}input[type=color]{width:60px;height:37px;border:1px solid #96a79b;border-radius:7px;cursor:pointer;background:var(--field);padding:2px}input[type=range]{width:100%;accent-color:var(--green)}.fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:4px 18px}.colorrow{display:grid;grid-template-columns:minmax(110px,1fr) 60px 80px;gap:8px;align-items:center;margin:8px 0}.colorrow label{margin:0}.colorcode{font:12px monospace;color:var(--muted)}button{border:1px solid var(--green);border-radius:7px;background:var(--green);color:#fff;font-weight:700;padding:9px 14px;cursor:pointer}button.alt{background:#edf5f0;color:#245536;border-color:#bfd4c6}button.ghost{background:transparent;color:#356344;border-color:#a9c7b2}button.active{border-color:var(--green);color:#147a42;background:#e3f6e9}button:disabled{opacity:.5}[hidden]{display:none!important}footer a{color:inherit;text-decoration:underline}.langs{display:flex;gap:5px;flex-wrap:wrap}.langs button{padding:6px 9px;font-size:13px}.bar{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:7px;margin:18px 0 9px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:3px}.led{height:28px;border-radius:4px;background:#e3ebe5;transition:background .12s}.pct{font-size:34px;font-weight:750}.controls{display:flex;flex-wrap:wrap;gap:8px;margin-top:14px}.notice{min-height:20px;color:#16864a;font-size:13px}.switch{display:flex;align-items:center;gap:9px;margin:12px 0}.switch label{margin:0}.switch input{width:18px;height:18px;accent-color:var(--green)}.switches{display:flex;gap:20px;flex-wrap:wrap}.checks{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:0 10px;border:1px solid var(--border);border-radius:7px;margin:15px 0;padding:8px 12px}.checks legend{font-size:13px;color:var(--muted);padding:0 5px}.checks .switch{margin:5px 0}hr{border:0;border-top:1px solid var(--border);margin:19px 0}.spaced{margin-top:15px}.scanrow{display:flex;gap:8px;align-items:end}.scanrow select{flex:1}.warning{color:#8e5b19}footer{text-align:center;color:var(--muted);font-size:13px;margin-top:24px}input[type=file]{display:block;max-width:100%;margin:12px 0}@media(max-width:750px){.cols{grid-template-columns:1fr}main{padding:12px}.card{padding:16px}.fields{grid-template-columns:1fr}.bar{gap:3px}.led{height:22px}.colorrow{grid-template-columns:minmax(105px,1fr) 60px 75px}}
.app-header{display:grid;grid-template-columns:1fr auto 1fr;align-items:center}.app-header .brand-title{text-align:center}.brand-logo{display:block;width:185px;max-width:100%;height:auto;margin:auto}.app-header .langs{justify-self:end}.app-header .network-open{justify-self:start}.led-card{width:100%}.led-head,.led-row{display:grid;grid-template-columns:minmax(130px,1.2fr) 78px 110px 90px 125px minmax(160px,1.2fr);gap:10px;align-items:center}.led-head{color:var(--muted);font-size:12px;font-weight:700;padding:7px 10px}.led-row{border-top:1px solid var(--border);padding:10px}.led-row:first-child{border-top:0}.led-row label{margin:0;font-weight:600}.led-row input[type=number],.led-row select{max-width:100%!important}.led-row .switch{margin:0}.led-row .field-label{display:none}.led-card .global-fields{display:flex;gap:18px;flex-wrap:wrap;margin-bottom:12px}.led-card .global-fields>div{width:160px}.led-card .global-fields input,.led-card .global-fields select{max-width:160px!important}dialog{width:min(560px,calc(100% - 24px));max-height:calc(100vh - 36px);overflow:auto;border:1px solid var(--border);border-radius:13px;padding:22px;background:#fff;color:var(--text);box-shadow:0 20px 60px #15291b44}dialog::backdrop{background:#14261a99}.dialog-head{display:flex;align-items:center;justify-content:space-between;gap:10px}.toasts{position:fixed;right:16px;bottom:16px;display:grid;gap:9px;z-index:100;max-width:min(380px,calc(100vw - 32px))}.toast{background:#fff;border:1px solid var(--border);border-left:4px solid var(--green);box-shadow:0 7px 22px #172d1d2b;border-radius:8px;padding:11px 14px;font-size:13px}.toast.error{border-left-color:#dd5555}.toast button{float:right;padding:0 3px;margin-left:12px;border:0;background:none;color:var(--muted)}@media(max-width:850px){.app-header{grid-template-columns:1fr auto}.app-header .brand-title{grid-column:1/-1;grid-row:1}.app-header .network-open{grid-row:2}.app-header .langs{grid-row:2}.led-head{display:none}.led-row{grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.led-row .row-title{grid-column:1/-1}.led-row .field-label{display:block;font-size:12px;color:var(--muted);font-weight:400;margin:0 0 3px}.led-row>div{min-width:0}}@media(max-width:500px){.led-row{grid-template-columns:1fr 1fr}.led-card .global-fields>div{width:calc(50% - 9px)}.langs button{padding:5px}.toasts{right:10px;bottom:10px}}
.header-actions{display:flex;gap:7px;justify-self:start;flex-wrap:wrap}.app-header .header-actions{justify-self:start}.app-header{background:#fff;border-bottom:1px solid #e4e4e4;min-height:62px}.app-header .menu-button{background:transparent;color:#292929;border:0;border-bottom:2px solid transparent;border-radius:0;box-shadow:none;padding:20px 8px 17px;font-size:13px;font-weight:650;min-height:0}.app-header .menu-button:hover,.app-header .menu-button:focus-visible{background:transparent;color:var(--green);border-bottom-color:var(--green)}.app-header .langs .menu-button.active{background:transparent;color:#137a43;border-bottom-color:var(--green);box-shadow:none}.app-header .langs .menu-button{padding:20px 8px 17px}.preview-card{padding-bottom:12px}.preview-card .bar{margin:10px 0 6px}.preview-card hr{margin:12px 0}.preview-card .spaced{margin-top:9px}.preview-card .switch{margin:5px 0}.preview-card .fields label{margin-top:7px}.preview-card #simMessage:empty{display:none}#updateDialog .controls{align-items:center;margin-top:5px}#updateDialog .controls button{white-space:normal}@media(max-width:850px){.app-header .header-actions{grid-row:2;grid-column:1}.app-header .langs{grid-row:2;grid-column:2}}@media(max-width:530px){.app-header{grid-template-columns:1fr}.app-header .brand-title,.app-header .header-actions,.app-header .langs{grid-column:1;justify-self:center}.app-header .brand-title{grid-row:1}.app-header .header-actions{grid-row:2}.app-header .langs{grid-row:3}}#faqDialog h3{font-size:15px;margin:16px 0 5px}.led-row{border-radius:9px}@media(max-width:650px){main{padding:8px 10px 22px}.card{padding:13px;margin-bottom:10px}.app-header{padding:7px 0 9px;margin-bottom:12px;gap:0}.app-header .menu-button,.app-header .langs .menu-button{padding:7px 6px;font-size:12px}.app-header .header-actions,.app-header .langs{gap:3px}.app-header .brand-title{line-height:1.1}.app-header .brand-title .muted{font-size:11px;margin-top:3px}.preview-card .fields{grid-template-columns:minmax(120px,.85fr) minmax(0,1.15fr);gap:10px}.preview-card .fields>div{min-width:0}.preview-card .fields select{max-width:100%}.preview-card .switches{gap:5px 15px}.preview-card .switch label{font-size:12px}.preview-card .bar{gap:3px}.led-card .led-row{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:7px;padding:10px;margin:8px 0;border:1px solid var(--border);background:#fbfdfb}.led-row .row-title{grid-column:1/-1;padding-bottom:5px;border-bottom:1px solid var(--border);font-size:14px}.led-row>div:nth-child(2),.led-row>div:nth-child(3),.led-row>div:nth-child(4){grid-column:span 2}.led-row>div:nth-child(5){grid-column:span 2}.led-row>div:nth-child(6){grid-column:span 4}.led-row .field-label{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:11px;margin-bottom:3px}.led-row input[type=number],.led-row select{width:100%;min-width:0;max-width:none!important;padding:6px;height:36px;min-height:36px;font-size:13px}.led-row input[type=color]{height:36px;width:100%;max-width:66px}.led-row>div:nth-child(4){display:flex;align-items:center;justify-content:center;gap:7px;padding-top:15px}.led-row>div:nth-child(4) .field-label{margin:0;white-space:normal}.led-row>div:nth-child(5) .field-label,.led-row>div:nth-child(6) .field-label{white-space:normal;min-height:26px;line-height:1.15;display:flex;align-items:end}.led-row>div:nth-child(4) input{width:19px;height:19px;flex:none}.led-card .global-fields{margin:5px 0}.led-card .global-fields>div{width:100%;max-width:200px}.led-card hr{margin:12px 0}.led-card .notice:empty{display:none}.led-card .muted{margin:8px 0}.pct{font-size:29px}.led{height:22px}dialog{padding:16px}#updateDialog .controls{gap:7px}}@media(max-width:370px){.app-header .menu-button,.app-header .langs .menu-button{padding:6px 4px;font-size:11px}.led-row{gap:5px}.led-row .field-label{font-size:10px}.preview-card .fields{grid-template-columns:minmax(104px,.8fr) minmax(0,1.2fr)}}.app-header #openUpdate.update-ready,.app-header #openUpdate.update-ready:hover,.app-header #openUpdate.update-ready:focus-visible{background:#bf1e2e;color:#fff;border-color:#bf1e2e;box-shadow:0 1px 5px #92182735}.led-row .row-title{display:flex;align-items:center;justify-content:space-between;gap:5px;flex-wrap:wrap}.led-row .enabled-toggle{display:inline-flex;align-items:center;gap:5px;font-size:11px;font-weight:500;white-space:nowrap;color:#326246}.led-row .enabled-toggle input{width:17px;height:17px;margin:0;accent-color:var(--green)}@media(max-width:650px){.led-card .led-row .row-title{flex-wrap:nowrap}.led-row .enabled-toggle{font-size:12px}.led-row .enabled-toggle input{width:19px;height:19px}}.led-head,.led-row{grid-template-columns:minmax(120px,1fr) minmax(170px,1.5fr) 90px 90px 110px minmax(150px,1.2fr)}.palette-controls{display:grid;gap:6px}.palette-count{display:flex;align-items:center;gap:5px}.palette-count label{font-size:11px;font-weight:400}.led-row .palette-count select{width:55px;min-height:30px;padding:3px 6px;font-size:12px}.palette-swatches{display:flex;gap:4px;flex-wrap:wrap}.led-row .palette-swatches input[type=color]{width:30px;height:32px;min-width:0;padding:1px}.bar{grid-template-columns:repeat(auto-fit,minmax(24px,1fr));gap:4px}.pair{min-width:0}.pair .led{min-width:0}@media(max-width:1000px){.led-head{display:none}.led-row{grid-template-columns:repeat(6,minmax(0,1fr))}.led-row .row-title{grid-column:1/-1}.led-row>div:nth-child(2){grid-column:span 4}.led-row>div:nth-child(3){grid-column:span 2}.led-row>div:nth-child(4){grid-column:span 1}.led-row>div:nth-child(5){grid-column:span 2}.led-row>div:nth-child(6){grid-column:span 3}.led-row .field-label{display:block;font-size:11px;color:var(--muted)}.led-row>div:nth-child(4){padding-top:0;display:block}.led-row .palette-swatches input[type=color]{width:32px;height:32px}.led-row .palette-count select{height:30px;min-height:30px}.led-row>div:nth-child(5) .field-label,.led-row>div:nth-child(6) .field-label{min-height:0}.preview-card .bar{gap:3px}.led-card .global-fields>div{max-width:220px;width:220px}}.led-head>span{overflow-wrap:anywhere}.led-row>div:nth-child(4) .field-label{white-space:normal;text-overflow:clip;overflow:visible}@media(max-width:1000px){.led-card .led-row{grid-template-columns:repeat(8,minmax(0,1fr))}.led-row>div:nth-child(2){grid-column:span 6}.led-row>div:nth-child(3){grid-column:span 2}.led-row>div:nth-child(4){grid-column:span 2}.led-row>div:nth-child(5){grid-column:span 2}.led-row>div:nth-child(6){grid-column:span 4}}

.app-header{position:sticky;top:0;z-index:40;margin-bottom:12px;transition:transform .2s ease;box-shadow:0 2px 10px #14261a13}.preview-card{position:sticky;top:var(--header-stick-height,78px);z-index:30;box-shadow:0 7px 20px #172d1d16}.preview-card h2{margin-bottom:12px}.preview-controls{display:flex;gap:10px 22px;align-items:center;flex-wrap:wrap}.preview-controls .switch:last-child{margin-left:auto}.preview-details{display:grid;grid-template-columns:1fr 1fr 1.45fr;gap:16px;align-items:end}.preview-details label{margin:0 0 6px}.preview-status .chips{margin:0 0 9px}.preview-status .headbar{justify-content:flex-start;gap:14px}.preview-test-state select{max-width:none;width:100%}.preview-progress{min-width:0}.preview-progress input{margin-bottom:6px}.preview-card .bar{margin:13px 0 0}.preview-card .notice:empty{display:none}.preview-card #file{max-width:150px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.mobile-languages{display:none!important}.language-options{display:grid;gap:10px;margin-top:16px}.language-options button{text-align:left}
.palette-controls{min-width:0}.palette-toolbar{display:flex;align-items:flex-start;gap:7px;min-width:0}.palette-swatches{display:grid;grid-template-columns:repeat(6,30px);gap:5px;align-content:start}.led-row .palette-swatches input[type=color]{width:30px;height:32px}.palette-actions{display:flex;gap:4px;flex-shrink:0}.palette-actions button{width:29px;height:32px;padding:0;font-size:21px;line-height:1}.led-row .palette-actions button:disabled{opacity:.35}.led-head,.led-row{grid-template-columns:minmax(110px,1fr) minmax(275px,1.8fr) 84px 70px 103px minmax(135px,1fr)}
@media(max-width:850px){.app-header .langs{display:none}.app-header .mobile-languages{display:inline-flex!important}.app-header{grid-template-columns:1fr auto}.app-header .brand-title{grid-row:1;grid-column:2;justify-self:end;text-align:right}.app-header .header-actions{grid-row:1;grid-column:1}.app-header .menu-button{padding:12px 6px}.preview-card{top:0}.app-header.scroll-hidden{transform:translateY(calc(-100% - 2px))}.app-header:not(.scroll-hidden)+.preview-card{top:var(--header-stick-height,68px)}.preview-card.preview-compact .preview-controls,.preview-card.preview-compact .preview-test-state,.preview-card.preview-compact .preview-progress,.preview-card.preview-compact #simMessage{display:none}.preview-card.preview-compact .preview-details{display:block}.preview-card.preview-compact .preview-status{display:flex;align-items:center;justify-content:space-between;gap:6px}.preview-card.preview-compact .preview-status .chips{margin:0}.preview-card.preview-compact .bar{margin-top:6px}.preview-card.preview-compact h2{font-size:14px;margin:0 0 4px}.preview-card.preview-compact .pct{font-size:22px}.preview-card.preview-compact #ledcount{display:none}.preview-details{grid-template-columns:1fr 1fr;gap:8px}.preview-progress{grid-column:1/-1}.preview-controls{gap:1px 15px}.preview-controls .switch:last-child{margin-left:0}}
@media(max-width:650px){.app-header{display:flex;align-items:center;justify-content:space-between;gap:4px;padding:0 2px 3px;margin-bottom:8px}.app-header .brand-title{display:none}.app-header .header-actions{display:flex;justify-content:space-between;flex:1;gap:1px;flex-wrap:nowrap}.app-header .menu-button{padding:10px 3px;font-size:11px;white-space:nowrap}.preview-card{padding:10px 12px;max-height:min(55vh,410px);overflow:auto}.preview-card h2{font-size:15px;margin-bottom:5px}.preview-controls .switch label{font-size:11px}.preview-card .bar{gap:2px;overflow:hidden}.preview-card .led{height:15px}.preview-card .pct{font-size:25px}.led-card .led-row{grid-template-columns:repeat(8,minmax(0,1fr))}.led-row>div:nth-child(2){grid-column:1/-1}.led-row>div:nth-child(3){grid-column:span 2}.led-row>div:nth-child(4){grid-column:span 2}.led-row>div:nth-child(5){grid-column:span 2}.led-row>div:nth-child(6){grid-column:span 2}.palette-swatches{grid-template-columns:repeat(6,29px)}.led-row .palette-swatches input[type=color]{width:29px}}
@media(max-width:390px){.palette-swatches{grid-template-columns:repeat(6,27px);gap:4px}.led-row .palette-swatches input[type=color]{width:27px}.palette-actions button{width:26px}.app-header .menu-button{font-size:10px}}

.discovery-progress{display:flex;align-items:center;gap:12px;margin:8px 0}.discovery-progress[hidden]{display:none}.discovery-progress progress{flex:1;min-width:0;height:12px;accent-color:#198347}.discovery-progress span{font-variant-numeric:tabular-nums;font-weight:700;min-width:3em;text-align:right}.connection-dashboard{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:14px 0 2px}.connection-panel{background:#f7faf8;border:1px solid var(--border);border-radius:9px;padding:12px;min-width:0}.connection-panel h3{font-size:14px;margin:0 0 8px}.connection-panel h3:not(:first-child){margin-top:16px}.connection-panel dl{display:grid;grid-template-columns:minmax(105px,1fr) minmax(0,1.1fr);gap:5px 8px;margin:0;font-size:12px}.connection-panel dt{color:var(--muted)}.connection-panel dd{margin:0;overflow-wrap:anywhere;font-weight:600}.connection-panel .link-ok{color:#137a43}.connection-panel .link-off{color:#a64040}#connectionInfoMessage:empty{display:none}@media(max-width:530px){.connection-dashboard{grid-template-columns:1fr}.connection-panel{padding:11px}.connection-panel dl{grid-template-columns:minmax(105px,1fr) minmax(0,1.25fr)}}
#linkFriendly{color:#147a42;text-decoration:underline;overflow-wrap:anywhere}#webHost{max-width:300px}.saved-network{display:flex;align-items:center;justify-content:space-between;gap:12px;border:1px solid var(--border);border-radius:8px;padding:9px 11px;margin:7px 0}.saved-network strong{overflow-wrap:anywhere}.saved-network small{display:block}.saved-network .controls{margin:0;flex-shrink:0}.saved-network .controls button{font-size:12px;padding:6px 9px}@media(max-width:520px){.saved-network{align-items:flex-start;flex-direction:column}.saved-network .controls{width:100%}}
.led-head,.led-row{grid-template-columns:minmax(120px,1fr) minmax(170px,1.5fr) minmax(110px,1fr) minmax(130px,1fr) minmax(160px,1.2fr)}
.brightness-control details,.pulse-control details{border:1px solid var(--border);border-radius:8px;background:#fff;padding:7px 9px}
.brightness-control summary,.pulse-control summary{display:flex;justify-content:space-between;align-items:center;gap:6px;cursor:pointer;min-height:30px;list-style:none}
.brightness-control summary::-webkit-details-marker,.pulse-control summary::-webkit-details-marker{display:none}
.brightness-control summary strong,.pulse-control summary strong{font-size:12px;color:var(--green);white-space:nowrap}
.brightness-control input[type=range],.pulse-control input[type=range]{width:100%;max-width:none;margin:8px 0;accent-color:var(--green)}
.pulse-control .pulse-enabled{display:flex;align-items:center;gap:7px;padding:8px 0;font-weight:500}
.pulse-control .pulse-enabled input[type=checkbox]{width:18px;height:18px;accent-color:var(--green)}
.pulse-control .range-line{display:flex;align-items:center;gap:8px}
.pulse-control output{min-width:53px;text-align:right;font-size:12px;font-variant-numeric:tabular-nums}
.effect-control select{display:block;width:100%;min-width:0}
.effect-name{display:none;overflow-wrap:anywhere;font-size:12px;color:var(--green);margin-bottom:5px}
@media(max-width:850px){
 dialog:not(#languageDialog){position:fixed;inset:0;margin:0;width:100vw;max-width:none;height:100vh;height:100dvh;max-height:none;border:0;border-radius:0;box-shadow:none;padding:0 16px 24px;box-sizing:border-box;overscroll-behavior:contain}
 dialog:not(#languageDialog)::backdrop{background:#fff}
 dialog:not(#languageDialog) .dialog-head{position:sticky;top:0;z-index:5;background:#fff;border-bottom:1px solid var(--border);padding:10px 0;margin-bottom:12px}
 dialog:not(#languageDialog) .dialog-head button{min-height:44px}
 body:has(dialog:not(#languageDialog)[open]){overflow:hidden}
 #networkDialog .connection-dashboard{grid-template-columns:1fr}
 .led-card .led-row{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}
 .led-card .led-row>div:nth-child(1),.led-card .led-row>div:nth-child(2),.led-card .led-row>div:nth-child(5){grid-column:1/-1}
 .led-card .led-row>div:nth-child(3),.led-card .led-row>div:nth-child(4){grid-column:span 1}
 .led-card .led-row .pulse-control{display:block;padding:0}
 .led-card .led-row .pulse-control input[type=range]{width:100%;height:auto;min-height:20px}
 .led-card .led-row .pulse-control input[type=checkbox]{width:18px;height:18px;min-height:0}
 .led-card .led-row .brightness-control input[type=range]{width:100%;height:auto;min-height:20px}
 .led-row .effect-control .field-label{display:block;white-space:normal;min-height:0}
 .effect-name{display:block}
 .led-row .effect-control select{width:100%;max-width:none!important;min-width:0;min-height:44px;height:auto;font-size:14px}
 .preview-controls .switch:last-child{margin-left:0}
}
@media(max-width:380px){
 .preview-controls{gap:3px 8px}
 .preview-controls .switch label{font-size:10px}
 .led-card .led-row{grid-template-columns:1fr}
 .led-card .led-row>div:nth-child(3),.led-card .led-row>div:nth-child(4){grid-column:1/-1}
}
/* Mobile layout: keep the preview close to the settings and give every control room to breathe. */
@media (max-width: 850px) {
  body {font-size: 14px; line-height: 1.45}
  main {max-width: 700px; padding: 0 12px 28px}
  .card {padding: 16px; margin-bottom: 12px; border-radius: 14px}
  h2 {font-size: 16px; line-height: 1.3; margin-bottom: 12px}
  .muted, small {line-height: 1.5}
  button, input, select {touch-action: manipulation}
  button:focus-visible, input:focus-visible, select:focus-visible, summary:focus-visible {
    outline: 3px solid #176c41; outline-offset: 2px;
  }

  .app-header {display: block; padding: 8px 0; margin-bottom: 10px; min-height: 0;
    border-bottom: 0; box-shadow: 0 3px 12px #172d1d15}
  .app-header .header-actions {display: grid; grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 6px; width: 100%; flex-wrap: nowrap}
  .app-header .brand-title {display: none}
  .app-header .menu-button {display: flex; align-items: center; justify-content: center;
    width: 100%; min-width: 0; min-height: 44px; padding: 6px 7px;
    font-size: 12px; line-height: 1.2; white-space: normal; text-align: center;
    color: #244332; background: #f7faf8; border: 1px solid #d6e5da;
    border-radius: 9px; box-shadow: none}
  .app-header .menu-button:hover, .app-header .menu-button:focus-visible {
    background: #ebf6ee; border-color: #92bea1;
  }
  .app-header #openLang {font-size: 21px}
  .app-header #openUpdate.update-ready {color: #fff; background: #b93237; border-color: #b93237}

  .preview-card {top: 0; padding: 14px; max-height: min(48dvh, 380px); overflow: auto;
    overscroll-behavior: contain; box-shadow: 0 6px 18px #172d1d18}
  .preview-card h2 {margin-bottom: 8px}
  .preview-controls {display: grid; grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 6px 10px; align-items: start}
  .preview-controls .switch {min-width: 0; min-height: 38px; margin: 0;
    align-items: center; gap: 6px}
  .preview-controls .switch:last-child {grid-column: 1 / -1; margin-left: 0}
  .preview-controls .switch input {width: 20px; height: 20px; flex: 0 0 20px}
  .preview-controls .switch label {font-size: 12px; line-height: 1.3; overflow-wrap: anywhere}
  .preview-details {grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px 12px;
    align-items: end}
  .preview-status {grid-column: 1 / -1}
  .preview-progress {grid-column: auto}
  .preview-details select {width: 100%; max-width: none; min-height: 44px}
  .preview-details label {font-size: 12px; font-weight: 600}
  .preview-card .bar {margin: 10px 0 0; gap: 3px}
  .preview-card .led {height: 19px}
  .preview-card .pct {font-size: 27px}
  .preview-card.preview-compact {max-height: 128px; overflow: hidden}
  .preview-card.preview-compact .preview-status {min-width: 0}
  .preview-card.preview-compact .chips {min-width: 0}
  .preview-card.preview-compact #displaystate {overflow: hidden; text-overflow: ellipsis;
    white-space: nowrap; max-width: 48vw}

  .led-card .led-row {display: grid; grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 11px; align-items: start; padding: 14px; margin: 10px 0;
    border: 1px solid #dbe6de; border-radius: 12px; background: #fff;
    box-shadow: 0 2px 7px #172d1d0b}
  .led-card .led-row > div {min-width: 0}
  .led-card .led-row > .row-title, .led-card .led-row > .palette-controls,
  .led-card .led-row > .effect-control {grid-column: 1 / -1}
  .led-card .led-row > .brightness-control,
  .led-card .led-row > .pulse-control {grid-column: span 1}
  .led-card .led-row > .brightness-control:has(details[open]),
  .led-card .led-row > .pulse-control:has(details[open]) {grid-column: 1 / -1}
  .led-card .led-row .row-title {display: flex; align-items: center;
    justify-content: space-between; gap: 10px; flex-wrap: wrap;
    padding: 0 0 9px; border-bottom: 1px solid #e4ebe6}
  .led-card .led-row .row-title > label:first-child {font-size: 15px;
    line-height: 1.3; font-weight: 750}
  .led-card .led-row .enabled-toggle {min-height: 36px; padding: 5px 9px;
    font-size: 12px; background: #eef7f1; border-radius: 8px}
  .led-card .led-row .enabled-toggle input {width: 20px; height: 20px}
  .led-card .palette-toolbar {display: grid; grid-template-columns: minmax(0, 1fr) auto;
    align-items: center; gap: 9px}
  .led-card .palette-swatches {display: grid; grid-template-columns: repeat(6, minmax(0, 1fr));
    gap: 5px; width: 100%}
  .led-card .palette-swatches input[type=color] {width: 100%; min-width: 0;
    height: 38px; border-radius: 8px; cursor: pointer}
  .led-card .palette-actions {display: flex; gap: 5px}
  .led-card .palette-actions button {width: 38px; height: 38px; font-size: 23px}
  .led-card .brightness-control details, .led-card .pulse-control details {
    padding: 0; overflow: hidden; border-color: #d6e4d9; border-radius: 9px;
    background: #f8fbf9}
  .led-card .brightness-control summary, .led-card .pulse-control summary {
    min-height: 58px; padding: 8px 10px; flex-wrap: wrap;
    align-content: center; gap: 2px 8px}
  .led-card .brightness-control summary span, .led-card .pulse-control summary span {
    font-size: 12px; line-height: 1.25; color: #31473a}
  .led-card .brightness-control summary strong, .led-card .pulse-control summary strong {
    font-size: 13px; color: #126a3c; margin-left: auto}
  .led-card .brightness-control details[open], .led-card .pulse-control details[open] {
    background: #fff; border-color: #a9d1b4}
  .led-card .led-row .brightness-control input[type=range] {width: calc(100% - 24px);
    margin: 10px 12px 14px; min-height: 28px}
  .led-card .pulse-enabled {min-height: 44px; padding: 7px 12px;
    border-top: 1px solid #e8eee9; font-size: 13px}
  .led-card .pulse-control .range-line {padding: 0 12px 10px; gap: 10px}
  .led-card .pulse-control .range-line input[type=range] {min-height: 28px; margin: 0}
  .led-card .pulse-control output {font-size: 13px; min-width: 58px}
  .led-row .effect-control .field-label {font-size: 12px; font-weight: 650;
    color: #31473a; margin: 0 0 3px}
  .led-row .effect-name {display: block; font-size: 12px; line-height: 1.35;
    color: #126a3c; font-weight: 650; margin-bottom: 5px}
  .led-row .effect-control select {width: 100%; max-width: none !important;
    min-height: 46px; padding: 9px 12px; font-size: 14px}
  .led-card .global-fields > div {width: min(100%, 220px); max-width: 220px}
  .led-card .global-fields input {min-height: 44px}
  .led-card .muted {font-size: 12px; line-height: 1.5; margin: 9px 0}
  .led-card .controls button {min-height: 44px}

  dialog:not(#languageDialog) {padding: 0 16px 24px; line-height: 1.45}
  dialog:not(#languageDialog) > :not(.dialog-head) {max-width: 680px; margin-left: auto; margin-right: auto}
  dialog:not(#languageDialog) .dialog-head {margin: 0 -16px 16px; padding: 12px 16px;
    box-shadow: 0 3px 10px #172d1d12}
  dialog:not(#languageDialog) .dialog-head h2 {margin: 0; font-size: 18px}
  dialog:not(#languageDialog) h3 {line-height: 1.35}
  dialog:not(#languageDialog) form input:not([type=checkbox]):not([type=range]):not([type=color]),
  dialog:not(#languageDialog) form select {min-height: 46px; max-width: 100%}
  dialog:not(#languageDialog) .controls button, dialog:not(#languageDialog) form button {
    min-height: 44px; white-space: normal; line-height: 1.25}
  dialog:not(#languageDialog) .connection-panel {padding: 14px}
  dialog:not(#languageDialog) .connection-panel dl {gap: 8px 10px; line-height: 1.4}
  dialog:not(#languageDialog) .saved-network .controls button {min-height: 42px}
  #languageDialog {width: min(420px, calc(100% - 24px)); padding: 18px;
    border-radius: 14px}
  #languageDialog .language-options button {min-height: 46px}
}

@media (max-width: 390px) {
  .led-card .palette-toolbar {grid-template-columns: 1fr}
  .led-card .palette-actions {justify-content: flex-end}
  .preview-details {grid-template-columns: 1fr}
  .preview-test-state, .preview-progress {grid-column: 1}
  .preview-card .bar {gap: 2px}
}

@media (prefers-reduced-motion: reduce) {
  .app-header, .led, .preview-card {transition: none}
}

.preview-state-heading {display: flex; align-items: baseline; gap: 10px; min-width: 0; margin-bottom: 6px}
.preview-state-heading label {flex: 0 0 auto; margin: 0}
.mobile-preview-summary {display: flex; align-items: baseline; gap: 7px; min-width: 0;
  font-size: 13px; color: var(--muted); white-space: nowrap}
.mobile-preview-summary strong {font-size: inherit; font-weight: 650; color: inherit}
.preview-details {grid-template-columns: minmax(0,1fr) minmax(180px,.55fr)}
.preview-card .preview-status {display: none}

@media (max-width: 850px) {
  .app-header {padding: 4px 0 6px}
  .app-header .header-actions {display: flex; align-items: center;
    justify-content: space-between; gap: 2px; flex-wrap: nowrap}
  .app-header .menu-button {display: inline-flex; width: auto; flex: 0 1 auto;
    min-width: 0; min-height: 42px; padding: 5px 2px;
    font-size: clamp(10px, 2.8vw, 13px); font-weight: 680; line-height: 1.2;
    color: #234632; background: transparent; border: 0;
    border-bottom: 2px solid transparent; border-radius: 0;
    white-space: nowrap; box-shadow: none}
  .app-header .menu-button:hover, .app-header .menu-button:focus-visible {
    color: #116739; background: transparent; border-bottom-color: #18864d}
  .app-header #openLang {font-size: 19px; flex: 0 0 auto; padding: 5px 3px}
  .app-header #openUpdate.update-ready,
  .app-header #openUpdate.update-ready:hover,
  .app-header #openUpdate.update-ready:focus-visible {
    color: #b72e39; background: transparent; border: 0;
    border-bottom: 2px solid #b72e39; box-shadow: none}

  .preview-state-heading {display: flex; align-items: baseline; gap: 6px; min-width: 0; margin-bottom: 5px}
  .preview-state-heading label {flex: 0 0 auto; margin: 0; font-size: clamp(10px,2.7vw,12px)}
  .mobile-preview-summary {display: flex; flex: 1; align-items: baseline; min-width: 0;
    gap: 4px; overflow-x: auto; white-space: nowrap; scrollbar-width: none;
    font-size: clamp(9px,2.55vw,12px); line-height: 1.35; color: var(--muted)}
  .mobile-preview-summary::-webkit-scrollbar {display: none}
  .mobile-preview-summary strong {font-size: inherit; font-weight: 650; color: inherit}
  #mobileState, #mobileLedCount {white-space: nowrap}
  .preview-card .preview-status, .preview-card .preview-progress {display: none}
  .preview-card .preview-details {display: block}
  .preview-card .preview-test-state {max-width: 100%}
  .preview-card.preview-compact {max-height: 110px}
  .preview-card.preview-compact .preview-test-state {display: block}
  .preview-card.preview-compact .preview-test-state select {display: none}
  .preview-card.preview-compact .preview-state-heading {margin-bottom: 0}

  .led-card .led-row > .brightness-control:has(details[open]),
  .led-card .led-row > .pulse-control:has(details[open]) {grid-column: span 1}
  .led-card .pulse-control .range-line {display: grid; grid-template-columns: minmax(0, 1fr);
    gap: 0; padding: 0 10px 7px}
  .led-card .pulse-control .range-line input[type=range] {width: 100%; min-width: 0}
  .led-card .pulse-control output {width: 100%; text-align: right; margin: 0}
  .led-card .pulse-enabled {padding: 6px 10px}
  .led-row .effect-control .field-label {display: inline; margin: 0 6px 0 0}
  .led-row .effect-name {display: inline; margin: 0; line-height: 1.4}
  .led-row .effect-control select {margin-top: 6px}
}

@media (max-width: 370px) {
  .app-header .menu-button {font-size: 10px; padding-inline: 1px}
  .app-header #openLang {font-size: 18px}
  .led-card .brightness-control summary, .led-card .pulse-control summary {
    padding: 7px; min-height: 60px}
}

/* Keep both collapsed controls in one row, including phones below 380 CSS pixels. */
@media (max-width: 850px) {
  .led-card .led-row > div.brightness-control,
  .led-card .led-row > div.pulse-control {grid-column: span 1}
  .led-card .led-row > div.brightness-control:has(details[open]),
  .led-card .led-row > div.pulse-control:has(details[open]) {grid-column: span 1}
}

@media (min-width: 1040px) {
  .led-head, .led-row {
    grid-template-columns: minmax(145px, 1.05fr) minmax(284px, 1.85fr)
      minmax(130px, .95fr) minmax(130px, .95fr) minmax(200px, 1.25fr);
    gap: 12px;
  }
}
@media (min-width: 1001px) and (max-width: 1039px) {
  .led-row .palette-toolbar {flex-wrap: wrap}
  .led-row .palette-actions {margin-left: auto}
}
@media (min-width: 851px) {
  .led-row .row-title > label:first-child {font-size: 15px}
}

@media (min-width:651px){#faqDialog{width:min(780px,calc(100% - 24px))}}
.faq-guide{margin-top:18px;border-top:1px solid var(--border);padding-top:16px}
.faq-guide>label{display:block;font-weight:700;margin-bottom:7px}
#faqSearch{width:100%;max-width:none}
.faq-section{margin:18px 0 8px}
.faq-section h3{font-size:16px!important;margin:0 0 9px!important;color:#17442e}
.faq-item{border:1px solid var(--border);border-radius:9px;margin:7px 0;background:#fbfdfb}
.faq-item summary{cursor:pointer;padding:12px 14px;font-weight:650;line-height:1.35}
.faq-item summary:focus-visible{outline:2px solid var(--green);outline-offset:2px}
.faq-item p{margin:0;padding:0 14px 14px;line-height:1.55;overflow-wrap:anywhere}
#faqEmpty{margin:16px 0}

/* Footer theme control */
.theme-settings{display:inline-flex;align-items:center;justify-content:center;gap:10px;padding:6px 10px;border:1px solid var(--border);border-radius:9px;background:var(--panel);white-space:nowrap}
.theme-settings #themeLabel{color:var(--text);font-weight:650}
.theme-settings .theme-option{display:inline-flex;align-items:center;gap:5px;margin:0;color:var(--text);font-weight:600;cursor:pointer}
.theme-settings .theme-choice{width:19px;height:19px;margin:0;flex:none;accent-color:var(--green);cursor:pointer}
.theme-settings .theme-choice:focus-visible{outline:3px solid var(--green);outline-offset:2px}
@media(max-width:390px){.theme-settings{gap:7px;padding:5px 7px;font-size:12px}.theme-settings .theme-option{gap:3px}.theme-settings .theme-choice{width:18px;height:18px}}
footer a{display:block;margin-top:10px}
#faqDialog .faq-guide{margin-top:0;border-top:0;padding-top:0}
.faq-led-pattern{display:inline-block;min-width:64px;text-align:center;margin-right:9px;padding:4px 6px;border-radius:6px;background:#e4f5e9;color:#17673d;font-size:15px;letter-spacing:1px;vertical-align:middle;white-space:nowrap}
@media(max-width:390px){.faq-led-pattern{min-width:52px;margin-right:5px;font-size:13px}}
/* Dark appearance, including dialogs, mobile controls and feedback. */
html[data-theme=dark]{color-scheme:dark;--bg:#0f1712;--panel:#1b2920;--field:#15221a;--border:#3b5544;--text:#e7f1e9;--muted:#b4c9b8;--green:#45cb81}
html[data-theme=dark] label{color:var(--text)}
html[data-theme=dark] input:not([type=color]):not([type=range]):not([type=checkbox]):not([type=file]),
html[data-theme=dark] select{background:var(--field);border-color:#52745d;color:var(--text)}
html[data-theme=dark] input::placeholder{color:#adc4b2}
html[data-theme=dark] input[type=color]{background:var(--field);border-color:#64826b}
html[data-theme=dark] button:not(.alt):not(.ghost):not(.menu-button){background:#247d4d;border-color:#318956;color:#fff}
html[data-theme=dark] button.alt{background:#294533;color:#e6f5e9;border-color:#53765a}
html[data-theme=dark] button.ghost{background:transparent;color:#b8edc8;border-color:#668a6c}
html[data-theme=dark] button.active{background:#274d33;color:#b7f4c9;border-color:#55ab70}
html[data-theme=dark] .chip,
html[data-theme=dark] .connection-panel,
html[data-theme=dark] .mobile-preview-summary,
html[data-theme=dark] .faq-item,
html[data-theme=dark] .led-card .brightness-control details,
html[data-theme=dark] .led-card .pulse-control details{background:#24392b;border-color:var(--border);color:var(--text)}
html[data-theme=dark] .led-card .led-row,
html[data-theme=dark] .app-header,
html[data-theme=dark] dialog,
html[data-theme=dark] .toast{background:var(--panel);color:var(--text);border-color:var(--border)}
html[data-theme=dark] .led-card .led-row .row-title,
html[data-theme=dark] .led-card .pulse-enabled{border-color:var(--border)}
html[data-theme=dark] .led-card .led-row .enabled-toggle{background:#2a4633;color:var(--text)}
html[data-theme=dark] .led-card .brightness-control details[open],
html[data-theme=dark] .led-card .pulse-control details[open]{background:#1c3023;border-color:#64956e}
html[data-theme=dark] dialog:not(#languageDialog) .dialog-head{background:var(--panel);border-color:var(--border)}
html[data-theme=dark] dialog:not(#languageDialog)::backdrop{background:#0f1712}
html[data-theme=dark] .app-header .menu-button,
html[data-theme=dark] .app-header .langs .menu-button{color:#d9eddf}
html[data-theme=dark] .app-header .menu-button:hover,
html[data-theme=dark] .app-header .menu-button:focus-visible,
html[data-theme=dark] .app-header .langs .menu-button.active{color:#8ceab1;border-bottom-color:#8ceab1}
html[data-theme=dark] .led{background:#314638}
html[data-theme=dark] .chip.ok,
html[data-theme=dark] .connection-panel .link-ok,
html[data-theme=dark] #linkFriendly,
html[data-theme=dark] .notice,
html[data-theme=dark] .led-row .effect-name,
html[data-theme=dark] .led-card .brightness-control summary strong,
html[data-theme=dark] .led-card .pulse-control summary strong,
html[data-theme=dark] #mobileLedCount,
html[data-theme=dark] .mobile-preview-summary strong{color:#9be9b8}
html[data-theme=dark] .chip.error,
html[data-theme=dark] .connection-panel .link-off{color:#ff9494}
html[data-theme=dark] .warning{color:#ffd192}
html[data-theme=dark] .toast button{background:none!important;border:0;color:var(--muted)}
html[data-theme=dark] .led-row .enabled-toggle,
html[data-theme=dark] .led-card .brightness-control summary span,
html[data-theme=dark] .led-card .pulse-control summary span,
html[data-theme=dark] .led-row .effect-control .field-label,
html[data-theme=dark] .mobile-preview-summary > :first-child,
html[data-theme=dark] #mobileState{color:#d4e8d8}
html[data-theme=dark] #faqDialog .faq-section h3{color:#a8e7bd}
html[data-theme=dark] .faq-led-pattern{background:#2c563a;color:#d5ffe1}
html[data-theme=dark] button:focus-visible,
html[data-theme=dark] input:focus-visible,
html[data-theme=dark] select:focus-visible,
html[data-theme=dark] summary:focus-visible{outline-color:#a4eec0}
@media(max-width:850px){
  html[data-theme=dark] .app-header #openUpdate.update-ready,
  html[data-theme=dark] .app-header #openUpdate.update-ready:hover,
  html[data-theme=dark] .app-header #openUpdate.update-ready:focus-visible{color:#ff8790;border-bottom-color:#ff8790;background:transparent}
}

/* On phones, palette buttons use the free slots after up to twelve colours. */
@media (max-width: 850px) {
  .led-card .palette-toolbar {display: grid; grid-template-columns: repeat(6, minmax(0, 1fr)); gap: 5px; align-items: center}
  .led-card .palette-swatches {display: contents}
  .led-card .palette-swatches input[type=color] {width: 100%; min-width: 0; height: 38px}
  .led-card .palette-actions {grid-column: 5 / span 2; grid-row: 1; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 5px; width: 100%}
  .led-card .palette-toolbar[data-action-row=second] .palette-actions {grid-row: 2}
  .led-card .palette-toolbar[data-action-row=third] .palette-actions {grid-row: 3}
  .led-card .palette-actions button {width: 100%; min-width: 0; height: 38px}
  .led-card .effect-heading {display: flex; align-items: baseline; flex-wrap: wrap; gap: 4px 8px; margin-bottom: 5px}
  .led-card .effect-heading .field-label, .led-card .effect-heading .effect-name {display: inline; margin: 0; line-height: 1.35}
  .led-card .effect-control select {margin-top: 0}
}
#bar{width:100%}
#bar[data-dense=true]{gap:2px}
#bar[data-dense=true] .pair{gap:1px}
#bar[data-dense=true] .led{border-radius:2px}
.preview-card #previewScale{display:block;margin:3px 0 0;font-size:11px;color:var(--muted)}
.preview-card.preview-compact #previewScale{display:none}
@media(max-width:850px){.preview-controls .switch:last-child{grid-column:auto}}
.preview-details{display:block}
.printer-connection-head{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px}.printer-connection-head h3{margin:0}.printer-connection-head button{min-height:44px}
.saved-printer{border:1px solid var(--border);border-radius:12px;padding:12px;margin:10px 0;min-width:0}.saved-printer.active{border-color:var(--green)}.saved-printer strong,.saved-printer p{overflow-wrap:anywhere}.saved-printer p{margin:6px 0}.saved-printer .controls{flex-wrap:wrap}.saved-printer .controls button{min-height:40px}
#testStatus.live-reading{border-color:var(--green);color:var(--green);font-weight:650;background:#edf7f0}
#testStatus .live-printer-option{color:#17613e;font-weight:700}
html[data-theme=dark] #testStatus.live-reading{background:#1c3023;color:#9be9b8;border-color:#64956e}
html[data-theme=dark] #testStatus .live-printer-option{color:#9be9b8;background:#1c3023}
@media(min-width:851px){.preview-test-state select{max-width:360px}}
.ip-mode{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin:14px 0 6px;border:1px solid var(--border);border-radius:10px;padding:10px}.ip-mode label{display:flex;align-items:center;gap:6px;margin:0}.ip-mode input{width:auto}#staticFields[hidden]{display:none}#staticFields{padding:8px 12px 12px;border:1px solid var(--border);border-radius:10px;margin-bottom:12px}
</style></head><body><main>
<header class="app-header"><div class="header-actions"><button type="button" class="menu-button" id="openNetwork" data-i="networkSettings">Network</button><button type="button" class="menu-button" id="openPrinter" data-i="printerSettings">Printer</button><button type="button" class="menu-button" id="openUpdate" data-i="updateMenu">Update</button><button type="button" class="menu-button" id="openFAQ" data-i="faqMenu">FAQ</button><button type="button" class="menu-button mobile-languages" id="openLang" data-aria-i="languages" aria-label="Languages">🇬🇧</button></div><div class="brand-title"><img class="brand-logo" src="data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%220%200%20400%2096%22%20width%3D%22800%22%20height%3D%22192%22%3E%0A%3Crect%20width%3D%22400%22%20height%3D%2296%22%20rx%3D%2220%22%20fill%3D%22%230f1419%22%2F%3E%0A%3Cg%20transform%3D%22translate(14%2C16)%22%3E%0A%3Crect%20width%3D%2264%22%20height%3D%2264%22%20rx%3D%2214%22%20fill%3D%22%23161d25%22%2F%3E%0A%3Cdefs%3E%0A%3ClinearGradient%20id%3D%22g%22%20gradientUnits%3D%22userSpaceOnUse%22%20x1%3D%220%22%20y1%3D%2211%22%20x2%3D%220%22%20y2%3D%2254%22%3E%3Cstop%20offset%3D%220%22%20stop-color%3D%22%2322d3ee%22%2F%3E%3Cstop%20offset%3D%221%22%20stop-color%3D%22%234ade80%22%2F%3E%3C%2FlinearGradient%3E%0A%3Cfilter%20id%3D%22glow%22%20x%3D%22-50%25%22%20y%3D%22-50%25%22%20width%3D%22200%25%22%20height%3D%22200%25%22%3E%3CfeGaussianBlur%20stdDeviation%3D%222.5%22%2F%3E%3C%2Ffilter%3E%0A%3C%2Fdefs%3E%0A%3Cg%20fill%3D%22url(%23g)%22%3E%0A%3Cg%20filter%3D%22url(%23glow)%22%20opacity%3D%220.85%22%3E%0A%3Crect%20x%3D%2218%22%20y%3D%2211%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2218.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2226%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2233.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2241%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2248.5%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%0A%3C%2Fg%3E%0A%3Crect%20x%3D%2218%22%20y%3D%2211%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2218.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2226%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2233.5%22%20width%3D%2212%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2241%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%3Crect%20x%3D%2218%22%20y%3D%2248.5%22%20width%3D%2230%22%20height%3D%225%22%20rx%3D%222.5%22%2F%3E%0A%3C%2Fg%3E%0A%3C%2Fg%3E%0A%3Ctext%20x%3D%2296%22%20y%3D%2262%22%20font-family%3D%22'Segoe%20UI'%2C%20'Helvetica%20Neue'%2C%20Arial%2C%20sans-serif%22%20font-size%3D%2240%22%20font-weight%3D%22700%22%20letter-spacing%3D%220.5%22%3E%3Ctspan%20fill%3D%22%23e6edf3%22%3ELayer%3C%2Ftspan%3E%3Ctspan%20fill%3D%22%234ade80%22%3EGlow%3C%2Ftspan%3E%3C%2Ftext%3E%0A%3C%2Fsvg%3E" alt="LayerGlow"><div class="muted">by FatGiantCat</div></div><nav class="langs" aria-label="Language"><button type="button" class="menu-button" id="en" onclick="setLang('en')">🇬🇧 EN</button><button type="button" class="menu-button" id="pl" onclick="setLang('pl')">🇵🇱 PL</button><button type="button" class="menu-button" id="es" onclick="setLang('es')">🇪🇸 ES</button><button type="button" class="menu-button" id="de" onclick="setLang('de')">🇩🇪 DE</button></nav></header>
<section class="card preview-card" id="previewCard"><h2 data-i="preview">LED preview and test</h2>
<div class="preview-details"><div class="preview-status"><div class="chips"><span id="displaystate" class="chip">…</span><span id="file" class="chip"></span></div><div class="headbar"><span class="pct" id="percent">0%</span><span class="muted" id="ledcount">0 / 12</span></div></div><div class="preview-test-state"><div class="preview-state-heading"><label for="testStatus" data-i="testStatus">Test state</label><div class="mobile-preview-summary" aria-live="polite"><span id="previewProgressLabel" data-i="testProgress">Test progress</span><strong id="mobilePct">0%</strong><span id="mobileLedCount">0/24 LED</span><span id="mobileState">Idle</span></div></div><select id="testStatus" onchange="testStatusChanged()"><option value="printer" class="live-printer-option" data-i="printerReading">● Live printer data</option><option value="printing" data-i="printing">Printing</option><option value="heating" data-i="heating">Heating</option><option value="paused" data-i="paused">Paused</option><option value="complete" data-i="complete">Completed</option><option value="error" data-i="error">Error</option><option value="standby" data-i="standby">Idle</option></select></div></div>
<div class="bar" id="bar"></div><small id="previewScale" class="muted" hidden></small><p id="simMessage" class="notice"></p></section>
<section class="card led-card"><h2 data-i="lighting">Colours and LED settings</h2><form id="lightForm"><div class="led-head"><span data-i="function">Function</span><span data-i="colour">Colour</span><span data-i="brightness">Brightness</span><span data-i="pulseSetting">Pulse</span><span data-i="effect">Effect</span></div><div id="ledRows"></div><hr><div class="global-fields"><div><label for="ledCount" data-i="ledCount">Physical LEDs</label><input name="ledCount" id="ledCount" type="number" min="5" max="300" step="1" value="24" required inputmode="numeric"></div></div><p class="muted" data-i="stripCountHint">Total connected LEDs: 5–300. Two 12-LED strips in one chain = 24. Longer strips need suitable power and wiring.</p><p class="muted" data-i="colourEffectHint">Select at least two colours for palette animations. Rainbow uses its own spectrum. These effects run without Pulse; Cycle controls their speed.</p><p class="muted" data-i="paletteHint">Choose 1–12 colours per stage. Colours form equal zones along the strip; printing reveals them with progress.</p><p class="muted" data-i="powerHint">Check LED power wiring before increasing brightness.</p><div class="controls"><button type="button" class="ghost" id="resetColors" data-i="resetLights">Restore default LED settings</button><button type="button" class="alt" id="backupLights" data-i="backupLights">Save my LED backup</button><button type="button" class="alt" id="restoreLights" data-i="restoreLights" disabled>Restore my LED backup</button></div><p id="lightMessage" class="notice"></p></form></section>

<footer><div class="theme-settings" role="group" aria-labelledby="themeLabel"><span id="themeLabel">Theme</span><label class="theme-option"><input type="checkbox" class="theme-choice" value="auto" checked><span>Auto</span></label><label class="theme-option"><input type="checkbox" class="theme-choice" value="light"><span>Light</span></label><label class="theme-option"><input type="checkbox" class="theme-choice" value="dark"><span>Dark</span></label></div><a href="https://github.com/TomaszK00/LayerGlow" target="_blank" rel="noopener noreferrer"><span data-i="appName">LayerGlow</span> · v2.53 · by FatGiantCat · GitHub</a></footer>
<dialog id="networkDialog" aria-labelledby="networkTitle"><div class="dialog-head"><h2 id="networkTitle" data-i="networkSettings">Network</h2><button type="button" class="ghost" id="closeNetwork" data-i="close">Close</button></div><div class="connection-dashboard" aria-live="polite"><div class="connection-panel"><h3 data-i="homeConnection">Home Wi-Fi / LayerGlow</h3><dl><dt data-i="connectionState">Status</dt><dd id="linkWifi">—</dd><dt data-i="wifiName">Wi-Fi name (SSID)</dt><dd id="linkSSID">—</dd><dt data-i="deviceIP">LayerGlow IP</dt><dd id="linkLocalIP">—</dd><dt data-i="friendlyAddress">Easy address</dt><dd><a id="linkFriendly" href="#">http://layerglow.local/</a></dd><dt data-i="gatewayIP">Router IP</dt><dd id="linkGateway">—</dd></dl></div><div class="connection-panel"><h3 data-i="setupConnection">Setup AP</h3><dl><dt data-i="connectionState">Status</dt><dd id="linkAP">—</dd><dt data-i="wifiName">Wi-Fi name (SSID)</dt><dd id="linkAPSSID">—</dd><dt data-i="deviceIP">LayerGlow IP</dt><dd id="linkAPIP">—</dd><dt data-i="apClients">Connected devices</dt><dd id="linkClients">—</dd><dt data-i="browserAddress">Current panel address</dt><dd id="linkBrowser">—</dd></dl></div></div><p class="muted" id="connectionInfoMessage"></p><form id="wifiForm"><h3 data-i="wifiSection">Wi-Fi</h3><label for="wifiList" data-i="scanWifi">Available 2.4 GHz Wi-Fi</label><div class="scanrow"><select id="wifiList" onchange="if(this.value){document.getElementById('ssid').value=this.value;fillWifiIPForm()}"><option data-i="scanFirst">Scan first</option></select><button type="button" class="alt" id="scanWifiButton" onclick="scanWifi()" data-i="scan">Scan</button></div><label for="ssid" data-i="wifiName">Wi-Fi name (SSID)</label><input id="ssid" name="ssid" maxlength="32"><label for="webHost" data-i="deviceName">Device name (.local)</label><input id="webHost" name="host" maxlength="38" spellcheck="false" autocomplete="off" placeholder="layerglow"><label for="pass" data-i="wifiPass">Wi-Fi password</label><input id="pass" name="pass" type="password" autocomplete="new-password"><fieldset class="ip-mode"><legend data-i="ipMode">IP address</legend><label><input type="radio" name="ipMode" value="dhcp" checked><span data-i="ipDhcp">Automatic (DHCP)</span></label><label><input type="radio" name="ipMode" value="static"><span data-i="ipStatic">Static IP</span></label></fieldset><div id="staticFields" hidden><p class="muted" data-i="ipSuggestion">Current DHCP values are suggested. Check them before saving a static address.</p><label for="staticIP" data-i="ipAddress">LayerGlow IP</label><input id="staticIP" name="staticIP" inputmode="decimal" autocomplete="off"><label for="staticGateway" data-i="ipGateway">Router IP</label><input id="staticGateway" name="staticGateway" inputmode="decimal" autocomplete="off"><label for="staticSubnet" data-i="ipSubnet">Subnet mask</label><input id="staticSubnet" name="staticSubnet" inputmode="decimal" autocomplete="off"><label for="staticDns" data-i="ipDns">DNS server</label><input id="staticDns" name="staticDns" inputmode="decimal" autocomplete="off"></div><div class="controls"><button data-i="saveNetwork" type="submit">Save &amp; Connect Wi-Fi</button></div><p class="muted" data-i="wifiChangeHint">To add a network, enter or select its name, enter the password and save. Use Connect below for a saved network. If none works, join LayerGlow-Setup and open 192.168.4.1.</p><p id="wifiMessage" class="notice"></p></form><section aria-labelledby="savedWifiTitle"><h3 id="savedWifiTitle" data-i="savedWifiTitle">Saved Wi-Fi networks</h3><p class="muted" data-i="savedWifiHint">Up to 8 networks. The device automatically tries saved networks when you change location. Passwords are never shown here.</p><div id="savedWifiList"></div><p id="savedWifiMessage" class="notice" aria-live="polite"></p></section></dialog><dialog id="printerDialog" aria-labelledby="printerTitle"><div class="dialog-head"><h2 id="printerTitle" data-i="printerSettings">Printer</h2><button type="button" class="ghost" id="closePrinter" data-i="close">Close</button></div><div class="connection-panel printer-connection" aria-live="polite"><div class="printer-connection-head"><h3 data-i="printerConnection">Printer</h3><button type="button" class="ghost" id="disconnectPrinter" data-i="disconnectPrinter">Disconnect printer</button></div><dl><dt data-i="connectionState">Status</dt><dd id="linkPrinter">—</dd><dt data-i="printerType">Printer type</dt><dd id="linkPrinterType">—</dd><dt data-i="printerAddress">Printer IP : port</dt><dd id="linkPrinterAddress">—</dd><dt data-i="connectionReason">Details</dt><dd id="linkPrinterError">—</dd></dl><dl><dt data-i="printerLiveState">Printer state</dt><dd id="printerReadState">—</dd><dt data-i="printerLiveProgress">Print progress</dt><dd id="printerReadProgress">—</dd><dt data-i="printerLiveFile">Print file</dt><dd id="printerReadFile">—</dd><dt data-i="printerLiveAge">Last read</dt><dd id="printerReadAge">—</dd></dl></div><hr><section aria-labelledby="savedPrintersTitle"><h3 id="savedPrintersTitle" data-i="savedPrintersTitle">Saved printers</h3><div id="savedPrinterList"></div><p id="savedPrinterMessage" class="notice" aria-live="polite"></p></section><hr><form id="printerForm"><input type="hidden" id="profileId" name="profileId"><input type="hidden" id="printerName" name="printerName"><div class="controls"><button type="button" class="alt" id="findPrinters" data-i="findPrinters">Find printers</button></div><label for="foundPrinters" data-i="foundPrinters">Printers on this network</label><select id="foundPrinters"><option value="" data-i="discoveryIdle">Press Search to find printers</option></select><div class="discovery-progress" id="discoveryProgress" hidden><progress id="discoveryBar" max="100" value="0" aria-label="Printer discovery progress"></progress><span id="discoveryPercent">0%</span></div><p class="muted" id="discoveryMessage" aria-live="polite"></p><p class="muted" data-i="discoveryHint">Selecting a printer fills in known details. Enter its LAN code or API key if required, then save the printer.</p><label for="printerType" data-i="printerType">Printer type</label><select id="printerType" name="printerType" onchange="printerTypeChanged()"><option value="moonraker" data-i="crealityK2">Creality / Klipper (Moonraker)</option><option value="bambu" data-i="bambuA1">Bambu Lab (LAN / MQTT)</option><option value="octoprint" data-i="octoType">OctoPrint (many brands)</option><option value="prusalink" data-i="prusaType">PrusaLink (Prusa)</option></select><label for="ip" data-i="printerIP">Printer IP address</label><input id="ip" name="ip" required><div id="portFields"><label for="port" data-i="printerPort">Printer API port</label><input id="port" name="port" type="number" min="1" max="65535" required></div><div id="bambuFields" style="display:none"><label for="bambuSerial" data-i="bambuSerial">Bambu serial number</label><input id="bambuSerial" name="bambuSerial" maxlength="40" autocomplete="off"><label for="bambuCode" data-i="bambuCode">LAN access code (blank keeps saved)</label><input id="bambuCode" name="bambuCode" type="password" maxlength="64" autocomplete="new-password"><p class="muted" data-i="bambuHint">Enable LAN Only and Developer Mode. Enter IP, serial number and LAN access code. Status is read locally.</p></div><div id="apiKeyFields" style="display:none"><label for="apiKey" data-i="apiKey">API key (blank keeps saved)</label><input id="apiKey" name="apiKey" type="password" maxlength="128" autocomplete="new-password"><p class="muted" data-i="apiHint">Enter the local API key for the selected printer service.</p></div><div class="controls"><button data-i="savePrinter" type="submit">Save printer</button></div><p id="printerMessage" class="notice"></p></form></dialog><dialog id="updateDialog" aria-labelledby="updateTitle"><div class="dialog-head"><h2 id="updateTitle" data-i="updateTitle">Firmware update · OTA</h2><button type="button" class="ghost" id="closeUpdate" data-i="close">Close</button></div><p class="muted" data-i="updateHint">Select the compiled LayerGlow .bin file. Update takes about 2 minutes. Keep power connected and do not close this page.</p><form id="fwform"><input required id="bin" type="file" accept=".bin,application/octet-stream"><label for="sig" data-i="signatureFile">Matching signature (.sig)</label><input required id="sig" type="file" accept=".sig,text/plain"><div class="controls"><button type="submit" data-i="upload">Upload and restart</button><button type="button" class="alt" id="checkGitHub" data-i="checkUpdates">Check GitHub for updates</button><button type="button" id="installGitHub" data-i="installUpdate" hidden>Install update</button></div></form><p id="githubMessage" class="muted" aria-live="polite"></p><p id="fwMessage" class="notice"></p><p class="muted warning" data-i="security">Administrator login is required. This local HTTP panel is still unencrypted; use only on a trusted network.</p></dialog><dialog id="faqDialog" aria-labelledby="faqTitle"><div class="dialog-head"><h2 id="faqTitle" data-i="faqMenu">FAQ</h2><button type="button" class="ghost" id="closeFAQ" data-i="close">Close</button></div><div class="faq-guide"><label id="faqSearchLabel" for="faqSearch">Search FAQ</label><input id="faqSearch" type="search" autocomplete="off"><p id="faqEmpty" class="muted" hidden></p><div id="faqQuestions"></div></div></dialog><dialog id="languageDialog" aria-labelledby="languagesTitle"><div class="dialog-head"><h2 id="languagesTitle" data-i="languages">Languages</h2><button type="button" class="ghost" id="closeLang" data-i="close">Close</button></div><div class="language-options"><button type="button" class="alt" onclick="setLang('en');document.getElementById('languageDialog').close()">🇬🇧 English</button><button type="button" class="alt" onclick="setLang('pl');document.getElementById('languageDialog').close()">🇵🇱 Polski</button><button type="button" class="alt" onclick="setLang('es');document.getElementById('languageDialog').close()">🇪🇸 Español</button><button type="button" class="alt" onclick="setLang('de');document.getElementById('languageDialog').close()">🇩🇪 Deutsch</button></div></dialog><datalist id="layerglow-colour-presets"><option value="#ff0000"></option><option value="#00ffff"></option><option value="#0000ff"></option><option value="#00ff00"></option><option value="#ff00ff"></option><option value="#ffff00"></option><option value="#000000"></option><option id="savedCustomColour" value="#ffffff"></option></datalist></main><div class="toasts" id="toasts" aria-live="polite"></div><script>
const T={
en:{ipMode:"IP address",ipDhcp:"Automatic (DHCP)",ipStatic:"Static IP",ipAddress:"LayerGlow IP",ipGateway:"Router IP",ipSubnet:"Subnet mask",ipDns:"DNS server",ipSuggestion:"Current DHCP values are suggested. Confirm that this address is free before saving.",staticConfirm:"Use these static IP settings for this Wi-Fi network?",savedWifiAvailable:"Available now",savedWifiNotSeen:"Not detected in last scan (may be hidden)",savedWifiUnknown:"Scan to check availability",signatureFile:'Matching signature (.sig)',signatureRequired:'Select the matching .sig file.',printerRemoved:"Saved printer removed",savedPrintersTitle:"Saved printers",savedPrintersHint:"Up to 8 printers. Connect selects one for live readings; saving keeps its settings after restart.",savedPrintersNone:"No saved printers yet.",printerName:"Printer name",printerMAC:"MAC address (optional)",printerMACHint:"Enter the MAC from your router if known. It is not read automatically.",notProvided:"Not provided",editPrinter:"Edit",deletePrinterConfirm:"Delete this saved printer?",printerSelectedWaiting:"Selected · waiting for data",printerStoredDisconnected:"Disconnected",printerSavedNetwork:"Wi-Fi when saved",printerSerialLabel:"Serial number",invalidSavedPrinter:"Invalid printer settings. Check name, IP, port, MAC and credentials.",savedPrinterMissing:"Saved printer no longer exists. Refresh the list.",savedPrinterLimit:"8 printers are already saved. Remove one before adding another.",savedPrintersError:"Could not load saved printers.",macLabel:"MAC",selectedPrinter:"Selected printer",settingsSaveError:'Could not save settings. Changes may be temporary; try again.',settingsRestoreError:'Could not restore LED settings.',rememberedColourSaveError:'Could not save the colour to LayerGlow.',pulseSetting:'Pulse',pulseEnabled:'Enabled',pulseOn:'On',networkSettings:'Network',printerSettings:'Printer',disconnectPrinter:'Disconnect printer',printerDisabled:'Printer disconnected',printerLiveState:'Printer state',printerLiveProgress:'Print progress',printerLiveFile:'Print file',printerLiveAge:'Last read',printerStatusHint:'Printer status continues updating during an LED test.',secondsAgo:'s ago',rescanPrinters:'Search again',selectThenSave:'Press Save printer below to connect to the selected printer.',savedWifiTitle:'Saved Wi-Fi networks',savedWifiHint:'Up to 8 networks. The device automatically tries saved networks when you change location. Passwords are never shown here.',savedWifiNone:'No saved networks yet',savedWifiConnected:'Connected',savedWifiTrying:'Selected',savedWifiConnect:'Connect',savedWifiRemove:'Remove',savedWifiConfirm:'Remove this saved Wi-Fi network?',savedWifiRemoved:'Network removed',savedWifiFull:'Up to 8 saved networks are allowed.',savedWifiInvalid:'Invalid saved network',deviceName:'Device name (.local)',friendlyAddress:'Easy address',nameHint:'For example, layerglow opens at http://layerglow.local/. Use letters, digits and hyphens. If .local does not work on your network, use the LayerGlow IP shown above.',namePending:'Waiting for mDNS / Wi-Fi',nameReady:'Available on home Wi-Fi',nameInvalid:'Use 1–32 letters, digits or hyphens; no spaces or leading/trailing hyphen.',openNewAddress:'Open the new address',homeConnection:'Home Wi-Fi / LayerGlow',setupConnection:'Setup AP',printerConnection:'Printer connection',connectionState:'Status',deviceIP:'LayerGlow IP',gatewayIP:'Router IP',subnetMask:'Subnet mask',dnsServer:'DNS server',wifiSignal:'Signal',wifiChannel:'Wi-Fi channel',deviceMAC:'LayerGlow MAC',apClients:'Connected devices',browserAddress:'Current panel address',printerAddress:'Printer IP : port',connectionReason:'Details',connectionOn:'Connected',connectionOff:'Disconnected',connectionActive:'Active',connectionInactive:'Inactive',connectionTest:'Test mode',connectionNoError:'No error',connectionUnavailable:'Connection details unavailable',connectionWaiting:'Waiting for printer response',languages:'Languages',addColour:'Add colour',removeColour:'Remove colour',findPrinters:'Find printers',foundPrinters:'Printers on this network',discoveryIdle:'Press Search to find printers',discoveryHint:'Select a printer to fill in its details. Enter its LAN code or API key if required, then save.',discoverySearching:'Searching local network… this can take a minute or two',discoveryNone:'No compatible printer found. You can still enter its address manually.',discoveryUnavailable:'Connect the LayerGlow to your home Wi-Fi to find printers.',discoveryError:'Printer search failed. Enter the address manually.',discoverySelect:'Select a printer',discoveryReady:'Found printers: ',discoveryCredentials:'Enter the LAN code or API key on the printer before saving.',appName:"LayerGlow",colourFlow:"Colour flow",colourFade:"Colour fade",rainbowFlow:"Rainbow flow",colourMarch:"Moving colour bands",colourEffectHint:"Choose at least two colours for palette animations. Rainbow uses its own spectrum. These effects run without Pulse; Cycle controls their speed.",colourCount:"Colours",stripCountHint:"Total connected LEDs: 5–300. Two 12-LED strips in one chain = 24. Longer strips need suitable power and wiring.",paletteHint:"Choose 1–12 colours per stage. Colours form equal zones along the strip; printing reveals them with progress.",wifiSection:'Wi-Fi',saveNetwork:'Save & Connect Wi-Fi',printerSection:'Printer',savePrinter:'Save printer',printerSaveHint:'Saved in LayerGlow and kept after a restart. Wi-Fi settings are saved separately.',printerSaved:'Printer settings saved',printerPort:'Printer API port',apiKey:'API key (blank keeps saved)',apiHint:'Enter the local API key for the selected printer service.',octoType:'OctoPrint (many brands)',prusaType:'PrusaLink (Prusa)',bambuDenied:"Bambu rejected the LAN access code",printerType:"Printer type",crealityK2:"Creality / Klipper (Moonraker)",bambuA1:"Bambu Lab (LAN / MQTT)",bambuSerial:"Printer serial number",bambuCode:"LAN access code (blank keeps saved)",bambuHint:"On the A1 enable LAN Only and Developer Mode. Enter its IP, serial number and LAN access code. Status is read locally.",invalidBambu:"Invalid Bambu serial or access code",bambuOffline:"Bambu connection unavailable",bambuWait:"Waiting for Bambu status",scanNone:"No 2.4 GHz networks found. You can enter the SSID manually.",backupLights:"Save my LED backup",restoreLights:"Restore my LED backup",backupSaved:"LED backup saved in the device",backupRestored:"LED backup restored",comet:'Comet',wipe:'Moving fill',alternating:'Alternating pairs',randomPairs:'Random pairs',randomHint:'Pairs light up and turn off at random intervals.',ledOn:'On',faqMenu:'FAQ',blueLedTitle:'Blue LED on LayerGlow',blueLedIntro:'The signal repeats with a pause between cycles:',blueLedNoWifi:'One flash: no Wi-Fi connection',blueLedWifi:'Two flashes: connected to Wi-Fi',blueLedUpdate:'Two long flashes, pause, one short flash: newer firmware on GitHub',faqUpdateHint:'The panel checks GitHub when opened and every 30 minutes while it is open. You can also check manually in Update.',githubMissing:'No OTA .bin file found in the Software folder on GitHub.',githubNetwork:'Could not reach GitHub. Check Internet access.',githubFormat:'GitHub returned an invalid update listing.',updateMenu:'Update',networkSaved:'Network settings saved',networkRestart:'Saved. Switching to the new Wi-Fi. If it fails, connect to LayerGlow-Setup.',invalidMoonraker:'Invalid Moonraker URL',missingFields:'Missing printer fields',invalidNetwork:'Invalid IP, port or SSID',invalidLedCount:'Invalid LED count',invalidLedRow:'Invalid LED settings',noSpace:'Not enough OTA space. Flash once via USB with the min_spiffs partition layout.',led24:'24 LEDs · 12 pairs',led12:'12 LEDs · 6 pairs',moonPort:'Moonraker port',setupOffline:'No printer connection (setup AP)',checkUpdates:'Check GitHub for updates',installUpdate:'Install update',checkingUpdates:'Checking GitHub…',noUpdate:'Firmware is up to date',newVersion:'New version available:',githubUnavailable:'Could not check GitHub. Check Internet access and the Software/update.json file.',downloadFirmware:'Downloading firmware from GitHub…',invalidFirmware:'Firmware download does not match the update manifest.',githubConfirm:'Install version {version}? This can take about 2 minutes. Keep power connected and this page open.',centreOut:'Centre → edges → centre',edgesIn:'Edges → centre → edges',wave:'Flowing wave',restarting:'Uploading and restarting…',waiting:'Waiting for LayerGlow to return…',retry:'Could not confirm the restart. Check the device and reload the page.',deviceOffline:'Connection to LayerGlow lost',function:'Function',colour:'Colour',pulsing:'Pulse?',effect:'Effect',allBar:'Whole bar',lastPair:'Last pair',scanner:'Scanner (Knight Rider)',close:'Close',preview:'LED preview and test',testStatus:'Test state',printerReading:'● Live printer data',testProgress:'Test progress',printProgress:'Print progress',printing:'Printing',heating:'Heating',paused:'Paused',complete:'Completed',error:'Error',standby:'Idle',run:'▶ Run test',stop:'■ Stop',reset:'Reset',simHint:'The test also controls connected LEDs.',networkSettings:'Network',scanWifi:'Available 2.4 GHz Wi-Fi',scanFirst:'Scan first',scan:'Scan',wifiName:'Wi-Fi name (SSID)',wifiPass:'Wi-Fi password',printerIP:'Printer IP address',lighting:'Colours and LED settings',ledCount:'Physical LEDs',brightness:'Brightness',stepTime:'Test step (ms)',pulseMode:'Pulsing mode',pulseOff:'Off',pulseAll:'All lit LEDs',pulseLast:'Last lit pair during printing',pulsePeriod:'Cycle / speed (ms)',powerHint:'Check LED power wiring before increasing brightness.',resetColors:'Restore original colours',saving:'Saving…',wifiChangeHint:'To add a network, enter or select its name, enter the password and save. Use Connect below for a saved network. If none works, join LayerGlow-Setup and open 192.168.4.1.',pulseStages:'Pulse in these states (off by default)',pulseLastPair:'Pulse only the last lit pair while printing',resetLights:'Restore default LED settings',saveLights:'Save LED settings',updateTitle:'Firmware update · OTA',updateHint:'Select the compiled LayerGlow .bin file. Update takes about 2 minutes. Keep power connected and do not close this page.',upload:'Upload and restart',security:'Administrator login is required. This local HTTP panel is still unencrypted; use only on a trusted network.',credits:'Designed by',progressColor:'Print progress',colour_printing:'Printing',colour_heating:'Heating',colour_paused:'Paused',colour_complete:'Finished',colour_error:'Error',colour_standby:'Idle',saved:'Settings saved',scanError:'Scan failed',updateDone:'Firmware uploaded; device restarting…',updateFailed:'Upload failed',offline:'Printer offline',connected:'Printer connected',setup:'Setup access point',home:'Local Wi-Fi',test:'TEST',uploadConfirm:'Update can take about 2 minutes. Keep power connected and do not close this page. Upload firmware now?',chooseBin:'Select a .bin file',scanning:'Scanning…'},
pl:{ipMode:"Adres IP",ipDhcp:"Automatyczny (DHCP)",ipStatic:"Stały adres IP",ipAddress:"Adres IP LayerGlow",ipGateway:"Adres routera",ipSubnet:"Maska podsieci",ipDns:"Serwer DNS",ipSuggestion:"Podpowiadamy obecne wartości DHCP. Przed zapisem sprawdź, czy ten adres jest wolny.",staticConfirm:"Zastosować te ustawienia stałego IP do tej sieci Wi-Fi?",savedWifiAvailable:"Dostępna teraz",savedWifiNotSeen:"Niewykryta w ostatnim skanie (może być ukryta)",savedWifiUnknown:"Skanuj, aby sprawdzić dostępność",signatureFile:'Pasujący podpis (.sig)',signatureRequired:'Wybierz plik .sig odpowiadający firmware.',printerRemoved:"Usunięto zapisaną drukarkę",savedPrintersTitle:"Zapisane drukarki",savedPrintersHint:"Do 8 drukarek. Połącz wybiera drukarkę do odczytu na żywo; zapisane ustawienia pozostają po restarcie.",savedPrintersNone:"Nie ma jeszcze zapisanych drukarek.",printerName:"Nazwa drukarki",printerMAC:"Adres MAC (opcjonalnie)",printerMACHint:"Jeśli znasz MAC z routera, wpisz go tutaj. Nie jest odczytywany automatycznie.",notProvided:"Nie podano",editPrinter:"Edytuj",deletePrinterConfirm:"Usunąć tę zapisaną drukarkę?",printerSelectedWaiting:"Wybrana · oczekiwanie na dane",printerStoredDisconnected:"Rozłączona",printerSavedNetwork:"Wi-Fi przy zapisie",printerSerialLabel:"Numer seryjny",invalidSavedPrinter:"Nieprawidłowe ustawienia drukarki. Sprawdź nazwę, IP, port, MAC i dane dostępu.",savedPrinterMissing:"Zapisana drukarka już nie istnieje. Odśwież listę.",savedPrinterLimit:"Zapisano już 8 drukarek. Usuń jedną przed dodaniem kolejnej.",savedPrintersError:"Nie udało się wczytać zapisanych drukarek.",macLabel:"MAC",selectedPrinter:"Wybrana drukarka",settingsSaveError:'Nie udało się zapisać ustawień. Zmiany mogą być tymczasowe; spróbuj ponownie.',settingsRestoreError:'Nie udało się przywrócić ustawień LED.',rememberedColourSaveError:'Nie udało się zapisać koloru w LayerGlow.',pulseSetting:'Puls',pulseEnabled:'Włączone',pulseOn:'Włączony',networkSettings:'Sieć',printerSettings:'Drukarka',disconnectPrinter:'Rozłącz drukarkę',printerDisabled:'Drukarka rozłączona',printerLiveState:'Stan drukarki',printerLiveProgress:'Postęp druku',printerLiveFile:'Plik wydruku',printerLiveAge:'Ostatni odczyt',printerStatusHint:'Stan pochodzi z drukarki także wtedy, gdy działa test LED.',secondsAgo:'s temu',rescanPrinters:'Szukaj ponownie',selectThenSave:'Aby połączyć wybraną drukarkę, użyj przycisku Zapisz drukarkę poniżej.',savedWifiTitle:'Zapisane sieci Wi-Fi',savedWifiHint:'Maksymalnie 8 sieci. Po zmianie lokalizacji urządzenie samo próbuje połączyć się z zapamiętanymi sieciami. Hasła nie są tutaj pokazywane.',savedWifiNone:'Nie ma zapisanych sieci',savedWifiConnected:'Połączono',savedWifiTrying:'Wybrana',savedWifiConnect:'Połącz',savedWifiRemove:'Usuń',savedWifiConfirm:'Usunąć tę zapisaną sieć Wi-Fi?',savedWifiRemoved:'Sieć usunięta',savedWifiFull:'Można zapisać najwyżej 8 sieci.',savedWifiInvalid:'Nieprawidłowa zapisana sieć',deviceName:'Nazwa urządzenia (.local)',friendlyAddress:'Prosty adres',nameHint:'Na przykład layerglow otwiera się przez http://layerglow.local/. Użyj liter, cyfr i myślników. Jeśli .local nie działa w Twojej sieci, skorzystaj z adresu IP LayerGlow powyżej.',namePending:'Czekam na mDNS / Wi-Fi',nameReady:'Dostępny w domowej sieci Wi-Fi',nameInvalid:'Użyj 1–32 liter, cyfr lub myślników; bez spacji i myślnika na początku lub końcu.',openNewAddress:'Otwórz nowy adres',homeConnection:'Domowe Wi-Fi / LayerGlow',setupConnection:'Sieć awaryjna AP',printerConnection:'Połączenie z drukarką',connectionState:'Stan',deviceIP:'Adres IP LayerGlow',gatewayIP:'Adres IP routera',subnetMask:'Maska podsieci',dnsServer:'Serwer DNS',wifiSignal:'Sygnał',wifiChannel:'Kanał Wi-Fi',deviceMAC:'Adres MAC LayerGlow',apClients:'Połączone urządzenia',browserAddress:'Obecny adres panelu',printerAddress:'Adres IP : port drukarki',connectionReason:'Szczegóły',connectionOn:'Połączono',connectionOff:'Rozłączono',connectionActive:'Aktywna',connectionInactive:'Wyłączona',connectionTest:'Tryb testowy',connectionNoError:'Brak błędu',connectionUnavailable:'Dane połączenia niedostępne',connectionWaiting:'Czekam na odpowiedź drukarki',languages:'Języki',addColour:'Dodaj kolor',removeColour:'Usuń kolor',findPrinters:'Znajdź drukarki',foundPrinters:'Drukarki w tej sieci',discoveryIdle:'Wyszukiwanie rozpocznie się po otwarciu okna',discoveryHint:'Wybierz drukarkę, aby uzupełnić dane. Wpisz kod LAN lub klucz API, jeśli jest wymagany, i zapisz.',discoverySearching:'Szukam w sieci lokalnej… to może potrwać minutę lub dwie',discoveryNone:'Nie znaleziono zgodnej drukarki. Nadal możesz wpisać adres ręcznie.',discoveryUnavailable:'Aby wykryć drukarki, połącz LayerGlow z domowym Wi-Fi.',discoveryError:'Wyszukiwanie nie powiodło się. Wpisz adres ręcznie.',discoverySelect:'Wybierz drukarkę',discoveryReady:'Znalezione drukarki: ',discoveryCredentials:'Przed zapisaniem wpisz kod LAN lub klucz API drukarki.',appName:"LayerGlow",colourFlow:"Pływające kolory",colourFade:"Przenikanie kolorów",rainbowFlow:"Płynąca tęcza",colourMarch:"Wędrujące strefy kolorów",colourEffectHint:"Do animacji palety wybierz co najmniej dwa kolory. Tęcza używa własnego widma. Te efekty działają bez Pulsowania; Cykl ustawia ich szybkość.",colourCount:"Kolory",stripCountHint:"Łączna liczba podłączonych LED: 5–300. Dwa paski po 12 LED w jednym łańcuchu = 24. Dłuższe paski wymagają odpowiedniego zasilania i przewodów.",paletteHint:"Wybierz 1–12 kolorów dla każdego etapu. Kolory tworzą równe strefy na pasku, odsłaniane wraz z postępem drukowania.",wifiSection:'Sieć Wi-Fi',saveNetwork:'Zapisz i połącz Wi-Fi',printerSection:'Drukarka',savePrinter:'Zapisz drukarkę',printerSaveHint:'Ustawienia są zapisywane w LayerGlow i pozostają po restarcie. Wi-Fi zapisujesz osobno.',printerSaved:'Zapisano ustawienia drukarki',printerPort:'Port API drukarki',apiKey:'Klucz API (puste pole zachowa zapisany)',apiHint:'Wpisz lokalny klucz API wybranej usługi drukarki.',octoType:'OctoPrint (różne marki)',prusaType:'PrusaLink (Prusa)',bambuDenied:"Bambu odrzuciła kod dostępu LAN",printerType:"Typ drukarki",crealityK2:"Creality / Klipper (Moonraker)",bambuA1:"Bambu Lab (LAN / MQTT)",bambuSerial:"Numer seryjny drukarki",bambuCode:"Kod dostępu LAN (puste pole zachowa zapisany)",bambuHint:"Na A1 włącz LAN Only i Developer Mode. Wpisz adres IP, numer seryjny i kod dostępu LAN. Status jest pobierany lokalnie.",invalidBambu:"Nieprawidłowy numer seryjny lub kod Bambu",bambuOffline:"Brak połączenia z Bambu",bambuWait:"Oczekiwanie na status Bambu",scanNone:"Nie znaleziono sieci 2,4 GHz. Możesz wpisać nazwę sieci ręcznie.",backupLights:"Zapisz moją kopię ustawień LED",restoreLights:"Przywróć moją kopię ustawień LED",backupSaved:"Zapisano kopię ustawień LED w urządzeniu",backupRestored:"Przywrócono kopię ustawień LED",comet:'Kometa',wipe:'Wędrujące wypełnienie',alternating:'Pary naprzemiennie',randomPairs:'Losowe pary',randomHint:'Pary zapalają się i gasną niezależnie w losowych, krótkich odstępach.',ledOn:'Świeci',faqMenu:'FAQ',blueLedTitle:'Niebieska dioda LayerGlow',blueLedIntro:'Sygnał powtarza się z przerwą między cyklami:',blueLedNoWifi:'Jeden błysk: brak połączenia z Wi-Fi',blueLedWifi:'Dwa błyski: połączono z Wi-Fi',blueLedUpdate:'Dwa długie błyski, przerwa, jeden krótki: nowa wersja na GitHubie',faqUpdateHint:'Panel sprawdza GitHub po otwarciu i co 30 minut, kiedy jest otwarty. Możesz też sprawdzić ręcznie w Aktualizacjach.',githubMissing:'W katalogu Software na GitHubie nie znaleziono pliku OTA .bin.',githubNetwork:'Nie udało się połączyć z GitHubem. Sprawdź dostęp do Internetu.',githubFormat:'GitHub zwrócił nieprawidłową listę aktualizacji.',updateMenu:'Aktualizacja',networkSaved:'Zapisano ustawienia sieci',networkRestart:'Zapisano. Łączenie z nową siecią Wi-Fi. W razie problemu użyj LayerGlow-Setup.',invalidMoonraker:'Nieprawidłowy adres Moonraker',missingFields:'Brakuje danych drukarki',invalidNetwork:'Nieprawidłowy adres IP, port lub SSID',invalidLedCount:'Nieprawidłowa liczba diod LED',invalidLedRow:'Nieprawidłowe ustawienia LED',noSpace:'Za mało miejsca na OTA. Wgraj raz przez USB z układem partycji min_spiffs.',led24:'24 diody LED · 12 par',led12:'12 diod LED · 6 par',moonPort:'Port Moonraker',setupOffline:'Brak połączenia z drukarką (sieć konfiguracyjna)',checkUpdates:'Sprawdź aktualizacje na GitHubie',installUpdate:'Zainstaluj aktualizację',checkingUpdates:'Sprawdzam GitHub…',noUpdate:'Oprogramowanie jest aktualne',newVersion:'Dostępna nowa wersja:',githubUnavailable:'Nie udało się sprawdzić GitHuba. Sprawdź Internet i plik Software/update.json.',downloadFirmware:'Pobieram firmware z GitHuba…',invalidFirmware:'Pobrany firmware nie zgadza się z manifestem aktualizacji.',githubConfirm:'Zainstalować wersję {version}? Może to potrwać około 2 minut. Nie odłączaj zasilania ani nie zamykaj strony.',centreOut:'Środek → boki → środek',edgesIn:'Boki → środek → boki',wave:'Płynąca fala',restarting:'Wgrywanie i uruchamianie ponownie…',waiting:'Czekam na ponowne połączenie z LayerGlow…',retry:'Nie mogę potwierdzić restartu. Sprawdź urządzenie i odśwież stronę.',deviceOffline:'Utracono połączenie z LayerGlow',function:'Funkcja',colour:'Kolor',pulsing:'Pulsowanie?',effect:'Efekt',allBar:'Cały pasek',lastPair:'Ostatnia para',scanner:'Wędrujące światło (Knight Rider)',close:'Zamknij',preview:'Podgląd i test LED',testStatus:'Stan testowy',printerReading:'● Dane na żywo z drukarki',testProgress:'Postęp testu',printProgress:'Postęp wydruku',printing:'Drukowanie',heating:'Nagrzewanie',paused:'Pauza',complete:'Zakończono',error:'Błąd',standby:'Bezczynność',run:'▶ Uruchom test',stop:'■ Zatrzymaj',reset:'Resetuj',simHint:'Test steruje także podłączonym paskiem LED.',networkSettings:'Sieć',scanWifi:'Dostępne Wi-Fi 2,4 GHz',scanFirst:'Najpierw skanuj',scan:'Skanuj',wifiName:'Nazwa Wi-Fi (SSID)',wifiPass:'Hasło Wi-Fi',printerIP:'Adres IP drukarki',lighting:'Kolory i ustawienia LED',ledCount:'Fizyczne diody LED',brightness:'Jasność',stepTime:'Krok testu (ms)',pulseMode:'Tryb pulsowania',pulseOff:'Wyłączone',pulseAll:'Wszystkie zapalone diody',pulseLast:'Ostatnia zapalona para podczas druku',pulsePeriod:'Cykl / szybkość (ms)',powerHint:'Sprawdź zasilanie LED przed zwiększeniem jasności.',resetColors:'Przywróć oryginalne kolory',saving:'Zapisywanie…',wifiChangeHint:'Aby dodać sieć, wybierz ją lub wpisz nazwę, podaj hasło i zapisz. Przycisk Połącz poniżej pozwala ręcznie wybrać zapisaną sieć. Jeśli żadna nie działa, połącz się z LayerGlow-Setup i otwórz 192.168.4.1.',pulseStages:'Pulsuj w tych stanach (domyślnie wyłączone)',pulseLastPair:'Pulsuj tylko ostatnią zapaloną parę podczas druku',resetLights:'Przywróć domyślne ustawienia LED',saveLights:'Zapisz ustawienia LED',updateTitle:'Aktualizacja oprogramowania · OTA',updateHint:'Wybierz plik .bin dla LayerGlow. Aktualizacja trwa około 2 minut. Nie odłączaj zasilania ani nie zamykaj tej strony.',upload:'Wgraj i uruchom ponownie',security:'Panel wymaga logowania, ale lokalny HTTP nadal nie szyfruje danych. Używaj tylko w zaufanej sieci.',credits:'Projekt:',progressColor:'Postęp druku',colour_printing:'Drukowanie',colour_heating:'Nagrzewanie',colour_paused:'Pauza',colour_complete:'Zakończono',colour_error:'Błąd',colour_standby:'Bezczynność',saved:'Zapisano ustawienia',scanError:'Błąd skanowania',updateDone:'Wgrano firmware; urządzenie uruchamia się ponownie…',updateFailed:'Błąd aktualizacji',offline:'Brak połączenia z drukarką',connected:'Połączono z drukarką',setup:'Sieć konfiguracyjna',home:'Domowe Wi-Fi',test:'TEST',uploadConfirm:'Aktualizacja może potrwać około 2 minut. Nie odłączaj zasilania ani nie zamykaj tej strony. Wgrać firmware?',chooseBin:'Wybierz plik .bin',scanning:'Skanowanie…'},
es:{ipMode:"Dirección IP",ipDhcp:"Automática (DHCP)",ipStatic:"IP fija",ipAddress:"IP de LayerGlow",ipGateway:"IP del router",ipSubnet:"Máscara de subred",ipDns:"Servidor DNS",ipSuggestion:"Se sugieren los valores DHCP actuales. Comprueba que la dirección esté libre antes de guardar.",staticConfirm:"¿Usar esta IP fija para esta red Wi-Fi?",savedWifiAvailable:"Disponible ahora",savedWifiNotSeen:"No detectada en el último escaneo (puede estar oculta)",savedWifiUnknown:"Escanea para comprobar disponibilidad",signatureFile:'Firma correspondiente (.sig)',signatureRequired:'Selecciona el archivo .sig correspondiente.',printerRemoved:"Impresora guardada eliminada",savedPrintersTitle:"Impresoras guardadas",savedPrintersHint:"Hasta 8 impresoras. Conectar selecciona una para datos en vivo; los ajustes se conservan al reiniciar.",savedPrintersNone:"Aún no hay impresoras guardadas.",printerName:"Nombre de la impresora",printerMAC:"Dirección MAC (opcional)",printerMACHint:"Introduce la MAC del router si la conoces. No se lee automáticamente.",notProvided:"Sin datos",editPrinter:"Editar",deletePrinterConfirm:"¿Eliminar esta impresora guardada?",printerSelectedWaiting:"Seleccionada · esperando datos",printerStoredDisconnected:"Desconectada",printerSavedNetwork:"Wi-Fi al guardar",printerSerialLabel:"Número de serie",invalidSavedPrinter:"Ajustes de impresora no válidos. Revisa nombre, IP, puerto, MAC y credenciales.",savedPrinterMissing:"La impresora guardada ya no existe. Actualiza la lista.",savedPrinterLimit:"Ya hay 8 impresoras guardadas. Elimina una para añadir otra.",savedPrintersError:"No se pudieron cargar las impresoras guardadas.",macLabel:"MAC",selectedPrinter:"Impresora seleccionada",settingsSaveError:'No se pudieron guardar los ajustes. Los cambios pueden ser temporales; inténtalo de nuevo.',settingsRestoreError:'No se pudieron restaurar los ajustes LED.',rememberedColourSaveError:'No se pudo guardar el color en LayerGlow.',pulseSetting:'Pulso',pulseEnabled:'Activado',pulseOn:'Activado',networkSettings:'Red',printerSettings:'Impresora',disconnectPrinter:'Desconectar impresora',printerDisabled:'Impresora desconectada',printerLiveState:'Estado de la impresora',printerLiveProgress:'Progreso de impresión',printerLiveFile:'Archivo de impresión',printerLiveAge:'Última lectura',printerStatusHint:'El estado procede de la impresora incluso durante la prueba LED.',secondsAgo:'s atrás',rescanPrinters:'Buscar de nuevo',selectThenSave:'Pulsa Guardar impresora para conectar la impresora seleccionada.',savedWifiTitle:'Redes Wi-Fi guardadas',savedWifiHint:'Hasta 8 redes. El dispositivo prueba automáticamente las redes guardadas al cambiar de ubicación. Las contraseñas no aparecen aquí.',savedWifiNone:'No hay redes guardadas',savedWifiConnected:'Conectada',savedWifiTrying:'Seleccionada',savedWifiConnect:'Conectar',savedWifiRemove:'Eliminar',savedWifiConfirm:'¿Eliminar esta red Wi-Fi guardada?',savedWifiRemoved:'Red eliminada',savedWifiFull:'Se pueden guardar hasta 8 redes.',savedWifiInvalid:'Red guardada no válida',deviceName:'Nombre del dispositivo (.local)',friendlyAddress:'Dirección fácil',nameHint:'Por ejemplo, layerglow abre http://layerglow.local/. Usa letras, números y guiones. Si .local no funciona en tu red, usa la IP del LayerGlow indicada arriba.',namePending:'Esperando mDNS / Wi-Fi',nameReady:'Disponible en la Wi-Fi doméstica',nameInvalid:'Usa 1–32 letras, números o guiones; sin espacios ni guión al principio o final.',openNewAddress:'Abrir la nueva dirección',homeConnection:'Wi-Fi doméstica / LayerGlow',setupConnection:'Red AP de configuración',printerConnection:'Conexión con la impresora',connectionState:'Estado',deviceIP:'IP del LayerGlow',gatewayIP:'IP del router',subnetMask:'Máscara de subred',dnsServer:'Servidor DNS',wifiSignal:'Señal',wifiChannel:'Canal Wi-Fi',deviceMAC:'MAC del LayerGlow',apClients:'Dispositivos conectados',browserAddress:'Dirección actual del panel',printerAddress:'IP : puerto de la impresora',connectionReason:'Detalles',connectionOn:'Conectado',connectionOff:'Desconectado',connectionActive:'Activo',connectionInactive:'Inactivo',connectionTest:'Modo prueba',connectionNoError:'Sin errores',connectionUnavailable:'Datos de conexión no disponibles',connectionWaiting:'Esperando respuesta de la impresora',languages:'Idiomas',addColour:'Añadir color',removeColour:'Quitar color',findPrinters:'Buscar impresoras',foundPrinters:'Impresoras en esta red',discoveryIdle:'La búsqueda comienza al abrir esta ventana',discoveryHint:'Selecciona una impresora para completar los datos. Introduce el código LAN o la clave API si hace falta y guarda.',discoverySearching:'Buscando en la red local… puede tardar uno o dos minutos',discoveryNone:'No se encontró una impresora compatible. Puedes introducir la dirección manualmente.',discoveryUnavailable:'Conecta el LayerGlow a la Wi-Fi doméstica para buscar impresoras.',discoveryError:'Falló la búsqueda. Introduce la dirección manualmente.',discoverySelect:'Selecciona una impresora',discoveryReady:'Impresoras encontradas: ',discoveryCredentials:'Introduce el código LAN o la clave API antes de guardar.',appName:"LayerGlow",colourFlow:"Flujo de colores",colourFade:"Transición de colores",rainbowFlow:"Arcoíris en movimiento",colourMarch:"Bandas de color móviles",colourEffectHint:"Elige al menos dos colores para animar la paleta. El arcoíris usa su propio espectro. No requieren pulsación; Ciclo controla la velocidad.",colourCount:"Colores",stripCountHint:"Total de LED conectados: 5–300. Dos tiras de 12 LED en una cadena = 24. Las tiras largas requieren alimentación y cableado adecuados.",paletteHint:"Elige 1–12 colores por etapa. Forman zonas iguales en la tira, que se iluminan con el progreso de impresión.",wifiSection:'Wi-Fi',saveNetwork:'Guardar y conectar Wi-Fi',printerSection:'Impresora',savePrinter:'Guardar impresora',printerSaveHint:'Se guarda en LayerGlow y se conserva al reiniciar. El Wi-Fi se guarda por separado.',printerSaved:'Ajustes de impresora guardados',printerPort:'Puerto API de impresora',apiKey:'Clave API (vacío conserva la guardada)',apiHint:'Introduce la clave API local del servicio de impresión.',octoType:'OctoPrint (varias marcas)',prusaType:'PrusaLink (Prusa)',bambuDenied:"Bambu rechazó el código de acceso LAN",printerType:"Tipo de impresora",crealityK2:"Creality / Klipper (Moonraker)",bambuA1:"Bambu Lab (LAN / MQTT)",bambuSerial:"Número de serie de A1",bambuCode:"Código de acceso LAN (vacío conserva el guardado)",bambuHint:"En la A1 activa LAN Only y Developer Mode. Introduce IP, número de serie y código LAN. El estado se obtiene localmente.",invalidBambu:"Número de serie o código Bambu incorrecto",bambuOffline:"Sin conexión con Bambu",bambuWait:"Esperando el estado de Bambu",scanNone:"No se encontraron redes de 2,4 GHz. Puedes escribir el SSID manualmente.",backupLights:"Guardar mi copia de LED",restoreLights:"Restaurar mi copia de LED",backupSaved:"Copia de LED guardada en el dispositivo",backupRestored:"Copia de LED restaurada",comet:'Cometa',wipe:'Relleno en movimiento',alternating:'Pares alternos',randomPairs:'Pares aleatorios',randomHint:'Las parejas se encienden y apagan en intervalos cortos y aleatorios.',ledOn:'Encendido',faqMenu:'Preguntas',blueLedTitle:'LED azul del LayerGlow',blueLedIntro:'La señal se repite con una pausa entre ciclos:',blueLedNoWifi:'Un destello: sin conexión Wi-Fi',blueLedWifi:'Dos destellos: conectado a Wi-Fi',blueLedUpdate:'Dos destellos largos, pausa, uno corto: firmware nuevo en GitHub',faqUpdateHint:'El panel consulta GitHub al abrirse y cada 30 minutos mientras está abierto. También puedes buscar manualmente en Actualizar.',githubMissing:'No se encontró ningún OTA .bin en Software de GitHub.',githubNetwork:'No se pudo conectar a GitHub. Comprueba Internet.',githubFormat:'GitHub devolvió una lista de actualizaciones no válida.',updateMenu:'Actualizar',networkSaved:'Ajustes de red guardados',networkRestart:'Guardado. Conectando a la nueva red Wi-Fi. Si falla, usa LayerGlow-Setup.',invalidMoonraker:'URL de Moonraker no válida',missingFields:'Faltan datos de la impresora',invalidNetwork:'IP, puerto o SSID no válidos',invalidLedCount:'Número de LED no válido',invalidLedRow:'Ajustes LED no válidos',noSpace:'No hay espacio OTA suficiente. Graba una vez por USB con la partición min_spiffs.',led24:'24 LED · 12 pares',led12:'12 LED · 6 pares',moonPort:'Puerto Moonraker',setupOffline:'Sin conexión con la impresora (red de configuración)',checkUpdates:'Buscar actualizaciones en GitHub',installUpdate:'Instalar actualización',checkingUpdates:'Consultando GitHub…',noUpdate:'El firmware está actualizado',newVersion:'Nueva versión disponible:',githubUnavailable:'No se pudo consultar GitHub. Comprueba Internet y Software/update.json.',downloadFirmware:'Descargando firmware de GitHub…',invalidFirmware:'El firmware descargado no coincide con el manifiesto.',githubConfirm:'¿Instalar la versión {version}? Puede tardar unos 2 minutos. Mantén la alimentación y la página abierta.',centreOut:'Centro → extremos → centro',edgesIn:'Extremos → centro → extremos',wave:'Ola en movimiento',restarting:'Subiendo y reiniciando…',waiting:'Esperando a que vuelva el LayerGlow…',retry:'No se pudo confirmar el reinicio. Comprueba el dispositivo y recarga la página.',deviceOffline:'Se perdió la conexión con LayerGlow',function:'Función',colour:'Color',pulsing:'¿Pulsar?',effect:'Efecto',allBar:'Toda la barra',lastPair:'Último par',scanner:'Luz móvil (Knight Rider)',close:'Cerrar',preview:'Vista previa y prueba LED',testStatus:'Estado de prueba',printerReading:'● Datos en vivo de la impresora',testProgress:'Progreso de prueba',printProgress:'Progreso de impresión',printing:'Imprimiendo',heating:'Calentando',paused:'En pausa',complete:'Finalizado',error:'Error',standby:'En espera',run:'▶ Iniciar prueba',stop:'■ Detener',reset:'Restablecer',simHint:'La prueba también controla los LED conectados.',networkSettings:'Red',scanWifi:'Redes Wi-Fi de 2,4 GHz',scanFirst:'Escanear primero',scan:'Escanear',wifiName:'Nombre Wi-Fi (SSID)',wifiPass:'Contraseña Wi-Fi',printerIP:'Dirección IP de la impresora',lighting:'Colores y ajustes LED',ledCount:'LED físicos',brightness:'Brillo',stepTime:'Paso de prueba (ms)',pulseMode:'Modo de pulsación',pulseOff:'Desactivado',pulseAll:'Todos los LED encendidos',pulseLast:'Último par encendido al imprimir',pulsePeriod:'Ciclo / velocidad (ms)',powerHint:'Comprueba la alimentación antes de aumentar el brillo.',resetColors:'Restaurar colores originales',saving:'Guardando…',wifiChangeHint:'Para añadir una red, selecciona o introduce su nombre y contraseña y guarda. Usa Conectar para elegir una red guardada. Si ninguna funciona, usa LayerGlow-Setup en 192.168.4.1.',pulseStages:'Pulsar en estos estados (desactivado por defecto)',pulseLastPair:'Pulsar solo el último par encendido al imprimir',resetLights:'Restaurar ajustes LED predeterminados',saveLights:'Guardar ajustes LED',updateTitle:'Actualizar firmware · OTA',updateHint:'Selecciona el .bin para LayerGlow. La actualización tarda unos 2 minutos. Mantén la alimentación y no cierres esta página.',upload:'Subir y reiniciar',security:'El panel requiere acceso de administrador, pero HTTP local sigue sin cifrar los datos. Úsalo solo en una red de confianza.',credits:'Diseñado por',progressColor:'Progreso de impresión',colour_printing:'Imprimiendo',colour_heating:'Calentando',colour_paused:'En pausa',colour_complete:'Finalizado',colour_error:'Error',colour_standby:'En espera',saved:'Ajustes guardados',scanError:'Error al escanear',updateDone:'Firmware cargado; reiniciando…',updateFailed:'Error al subir',offline:'Impresora desconectada',connected:'Impresora conectada',setup:'Punto de acceso',home:'Wi-Fi local',test:'PRUEBA',uploadConfirm:'La actualización puede tardar unos 2 minutos. Mantén la alimentación y no cierres esta página. ¿Subir firmware?',chooseBin:'Selecciona un archivo .bin',scanning:'Escaneando…'},
de:{ipMode:"IP-Adresse",ipDhcp:"Automatisch (DHCP)",ipStatic:"Feste IP",ipAddress:"LayerGlow-IP",ipGateway:"Router-IP",ipSubnet:"Subnetzmaske",ipDns:"DNS-Server",ipSuggestion:"Die aktuellen DHCP-Werte werden vorgeschlagen. Prüfe vor dem Speichern, ob die Adresse frei ist.",staticConfirm:"Diese feste IP für dieses WLAN verwenden?",savedWifiAvailable:"Jetzt verfügbar",savedWifiNotSeen:"Beim letzten Scan nicht gefunden (eventuell verborgen)",savedWifiUnknown:"Zum Prüfen scannen",signatureFile:'Passende Signatur (.sig)',signatureRequired:'Wähle die passende .sig-Datei aus.',printerRemoved:"Gespeicherter Drucker entfernt",savedPrintersTitle:"Gespeicherte Drucker",savedPrintersHint:"Bis zu 8 Drucker. Verbinden wählt einen für Live-Daten; Einstellungen bleiben nach einem Neustart erhalten.",savedPrintersNone:"Noch keine Drucker gespeichert.",printerName:"Druckername",printerMAC:"MAC-Adresse (optional)",printerMACHint:"Bekannte MAC aus dem Router eingeben. Sie wird nicht automatisch gelesen.",notProvided:"Nicht angegeben",editPrinter:"Bearbeiten",deletePrinterConfirm:"Diesen gespeicherten Drucker löschen?",printerSelectedWaiting:"Ausgewählt · warte auf Daten",printerStoredDisconnected:"Getrennt",printerSavedNetwork:"WLAN beim Speichern",printerSerialLabel:"Seriennummer",invalidSavedPrinter:"Ungültige Druckereinstellungen. Name, IP, Port, MAC und Zugangsdaten prüfen.",savedPrinterMissing:"Gespeicherter Drucker nicht mehr vorhanden. Liste aktualisieren.",savedPrinterLimit:"Bereits 8 Drucker gespeichert. Vor dem Hinzufügen einen löschen.",savedPrintersError:"Gespeicherte Drucker konnten nicht geladen werden.",macLabel:"MAC",selectedPrinter:"Ausgewählter Drucker",settingsSaveError:'Einstellungen konnten nicht gespeichert werden. Änderungen können vorübergehend sein; erneut versuchen.',settingsRestoreError:'LED-Einstellungen konnten nicht wiederhergestellt werden.',rememberedColourSaveError:'Die Farbe konnte nicht in LayerGlow gespeichert werden.',pulseSetting:'Pulsieren',pulseEnabled:'Aktiv',pulseOn:'Ein',networkSettings:'Netzwerk',printerSettings:'Drucker',disconnectPrinter:'Drucker trennen',printerDisabled:'Drucker getrennt',printerLiveState:'Druckerstatus',printerLiveProgress:'Druckfortschritt',printerLiveFile:'Druckdatei',printerLiveAge:'Letzter Abruf',printerStatusHint:'Der Druckerstatus wird auch während eines LED-Tests aktualisiert.',secondsAgo:'s zuvor',rescanPrinters:'Erneut suchen',selectThenSave:'Unten Drucker speichern wählen, um den ausgewählten Drucker zu verbinden.',savedWifiTitle:'Gespeicherte WLANs',savedWifiHint:'Bis zu 8 Netzwerke. Bei einem Standortwechsel versucht das Gerät automatisch, sich mit gespeicherten Netzwerken zu verbinden. Passwörter werden hier nicht angezeigt.',savedWifiNone:'Noch keine gespeicherten Netzwerke',savedWifiConnected:'Verbunden',savedWifiTrying:'Ausgewählt',savedWifiConnect:'Verbinden',savedWifiRemove:'Entfernen',savedWifiConfirm:'Dieses gespeicherte WLAN entfernen?',savedWifiRemoved:'Netzwerk entfernt',savedWifiFull:'Es können höchstens 8 Netzwerke gespeichert werden.',savedWifiInvalid:'Ungültiges gespeichertes Netzwerk',deviceName:'Gerätename (.local)',friendlyAddress:'Einfacher Adresse',nameHint:'Beispiel: layerglow öffnet http://layerglow.local/. Buchstaben, Zahlen und Bindestriche verwenden. Falls .local im Netz nicht funktioniert, die LayerGlow-IP oben nutzen.',namePending:'Warte auf mDNS / WLAN',nameReady:'Im Heim-WLAN verfügbar',nameInvalid:'1–32 Buchstaben, Zahlen oder Bindestriche; keine Leerzeichen und kein Bindestrich am Anfang/Ende.',openNewAddress:'Neue Adresse öffnen',homeConnection:'Heim-WLAN / LayerGlow',setupConnection:'Einrichtungs-AP',printerConnection:'Druckerverbindung',connectionState:'Status',deviceIP:'LayerGlow-IP',gatewayIP:'Router-IP',subnetMask:'Subnetzmaske',dnsServer:'DNS-Server',wifiSignal:'Signal',wifiChannel:'WLAN-Kanal',deviceMAC:'LayerGlow-MAC',apClients:'Verbundene Geräte',browserAddress:'Aktuelle Panel-Adresse',printerAddress:'Drucker-IP : Port',connectionReason:'Details',connectionOn:'Verbunden',connectionOff:'Getrennt',connectionActive:'Aktiv',connectionInactive:'Inaktiv',connectionTest:'Testmodus',connectionNoError:'Kein Fehler',connectionUnavailable:'Verbindungsdaten nicht verfügbar',connectionWaiting:'Warte auf Antwort des Druckers',languages:'Sprachen',addColour:'Farbe hinzufügen',removeColour:'Farbe entfernen',findPrinters:'Drucker suchen',foundPrinters:'Drucker in diesem Netz',discoveryIdle:'Suchen drücken, um Drucker zu finden',discoveryHint:'Drucker auswählen, um bekannte Daten einzutragen. Falls nötig, LAN-Code oder API-Schlüssel eingeben und speichern.',discoverySearching:'Suche im lokalen Netz… dies kann ein bis zwei Minuten dauern',discoveryNone:'Kein kompatibler Drucker gefunden. Adresse kann manuell eingetragen werden.',discoveryUnavailable:'LayerGlow mit dem Heim-WLAN verbinden, um Drucker zu finden.',discoveryError:'Druckersuche fehlgeschlagen. Adresse manuell eingeben.',discoverySelect:'Drucker auswählen',discoveryReady:'Gefundene Drucker: ',discoveryCredentials:'Vor dem Speichern LAN-Code oder API-Schlüssel eingeben.',appName:"LayerGlow",colourFlow:"Fließende Farben",colourFade:"Farbübergang",rainbowFlow:"Fließender Regenbogen",colourMarch:"Wandernde Farbzonen",colourEffectHint:"Für Palettenanimationen mindestens zwei Farben wählen. Der Regenbogen nutzt sein eigenes Spektrum. Pulsieren ist optional; Zyklus regelt das Tempo.",colourCount:"Farben",stripCountHint:"Gesamtzahl angeschlossener LEDs: 5–300. Zwei Streifen mit je 12 LEDs in einer Kette = 24. Längere Streifen brauchen passende Stromversorgung und Kabel.",paletteHint:"Pro Status 1–12 Farben wählen. Sie bilden gleich große Zonen, die mit dem Druckfortschritt aufleuchten.",wifiSection:'WLAN',saveNetwork:'WLAN speichern und verbinden',printerSection:'Drucker',savePrinter:'Drucker speichern',printerSaveHint:'Wird in LayerGlow gespeichert und bleibt nach einem Neustart erhalten. WLAN wird separat gespeichert.',printerSaved:'Druckereinstellungen gespeichert',printerPort:'Drucker-API-Port',apiKey:'API-Schlüssel (leer behält gespeicherten)',apiHint:'Lokalen API-Schlüssel des Druckerdienstes eingeben.',octoType:'OctoPrint (viele Marken)',prusaType:'PrusaLink (Prusa)',bambuDenied:"Bambu hat den LAN-Zugangscode abgelehnt",printerType:"Druckertyp",crealityK2:"Creality / Klipper (Moonraker)",bambuA1:"Bambu Lab (LAN / MQTT)",bambuSerial:"Drucker-Seriennummer",bambuCode:"LAN-Zugangscode (leer behält gespeicherten Code)",bambuHint:"Auf dem A1 LAN Only und Developer Mode aktivieren. IP, Seriennummer und LAN-Code eingeben. Status wird lokal gelesen.",invalidBambu:"Ungültige Bambu-Seriennummer oder Zugangscode",bambuOffline:"Keine Bambu-Verbindung",bambuWait:"Warte auf Bambu-Status",scanNone:"Keine 2,4-GHz-Netze gefunden. SSID kann manuell eingegeben werden.",backupLights:"Meine LED-Sicherung speichern",restoreLights:"Meine LED-Sicherung laden",backupSaved:"LED-Sicherung im Gerät gespeichert",backupRestored:"LED-Sicherung wiederhergestellt",comet:'Komet',wipe:'Wandernde Füllung',alternating:'Wechselnde Paare',randomPairs:'Zufällige Paare',randomHint:'Die Paare leuchten unabhängig in kurzen, zufälligen Abständen auf und erlöschen.',ledOn:'An',faqMenu:'FAQ',blueLedTitle:'Blaue LED am LayerGlow',blueLedIntro:'Das Signal wiederholt sich mit einer Pause zwischen den Zyklen:',blueLedNoWifi:'Ein Blinkzeichen: keine WLAN-Verbindung',blueLedWifi:'Zwei Blinkzeichen: mit WLAN verbunden',blueLedUpdate:'Zwei lange Blinkzeichen, Pause, ein kurzes: neue Firmware auf GitHub',faqUpdateHint:'Das Panel prüft GitHub beim Öffnen und alle 30 Minuten, solange es geöffnet ist. Manuell geht es unter Update.',githubMissing:'Keine OTA-.bin-Datei im GitHub-Ordner Software gefunden.',githubNetwork:'GitHub nicht erreichbar. Internetverbindung prüfen.',githubFormat:'Ungültige Update-Liste von GitHub.',updateMenu:'Update',networkSaved:'Netzwerkeinstellungen gespeichert',networkRestart:'Gespeichert. Verbindung zum neuen WLAN läuft. Falls nötig, LayerGlow-Setup verwenden.',invalidMoonraker:'Ungültige Moonraker-URL',missingFields:'Live-Druckerdaten fehlen',invalidNetwork:'Ungültige IP, Port oder SSID',invalidLedCount:'Ungültige LED-Anzahl',invalidLedRow:'Ungültige LED-Einstellungen',noSpace:'Zu wenig OTA-Speicher. Einmal per USB mit min_spiffs-Partitionierung flashen.',led24:'24 LEDs · 12 Paare',led12:'12 LEDs · 6 Paare',moonPort:'Moonraker-Port',setupOffline:'Keine Druckerverbindung (Einrichtungsnetz)',checkUpdates:'Auf GitHub nach Updates suchen',installUpdate:'Update installieren',checkingUpdates:'GitHub wird geprüft…',noUpdate:'Firmware ist aktuell',newVersion:'Neue Version verfügbar:',githubUnavailable:'GitHub konnte nicht geprüft werden. Internet und Software/update.json prüfen.',downloadFirmware:'Firmware von GitHub wird geladen…',invalidFirmware:'Geladene Firmware stimmt nicht mit dem Update-Manifest überein.',githubConfirm:'Version {version} installieren? Dies kann etwa 2 Minuten dauern. Stromversorgung und Seite geöffnet lassen.',centreOut:'Mitte → außen → Mitte',edgesIn:'Außen → Mitte → außen',wave:'Fließende Welle',restarting:'Upload und Neustart…',waiting:'Warte auf LayerGlow…',retry:'Neustart nicht bestätigt. Gerät prüfen und Seite neu laden.',deviceOffline:'Verbindung zum LayerGlow verloren',function:'Funktion',colour:'Farbe',pulsing:'Pulsieren?',effect:'Effekt',allBar:'Ganzer Streifen',lastPair:'Letztes Paar',scanner:'Lauflicht (Knight Rider)',close:'Schließen',preview:'LED-Vorschau und Test',testStatus:'Teststatus',printerReading:'● Live-Druckerdaten',testProgress:'Testfortschritt',printProgress:'Druckfortschritt',printing:'Druckt',heating:'Heizt',paused:'Pausiert',complete:'Abgeschlossen',error:'Fehler',standby:'Bereit',run:'▶ Test starten',stop:'■ Stoppen',reset:'Zurücksetzen',simHint:'Der Test steuert auch die angeschlossenen LEDs.',networkSettings:'Netzwerk',scanWifi:'Verfügbare 2,4-GHz-WLANs',scanFirst:'Zuerst suchen',scan:'Suchen',wifiName:'WLAN-Name (SSID)',wifiPass:'WLAN-Passwort',printerIP:'IP-Adresse des Druckers',lighting:'Farben und LED-Einstellungen',ledCount:'Physische LEDs',brightness:'Helligkeit',stepTime:'Testschritt (ms)',pulseMode:'Pulsiermodus',pulseOff:'Aus',pulseAll:'Alle leuchtenden LEDs',pulseLast:'Letztes leuchtendes Paar beim Drucken',pulsePeriod:'Zyklus / Tempo (ms)',powerHint:'LED-Stromversorgung vor höherer Helligkeit prüfen.',resetColors:'Originalfarben wiederherstellen',saving:'Speichern…',wifiChangeHint:'WLAN hinzufügen: Namen auswählen oder eingeben, Passwort eingeben und speichern. Mit Verbinden unten ein gespeichertes Netzwerk wählen. Wenn keines funktioniert, LayerGlow-Setup verbinden und 192.168.4.1 öffnen.',pulseStages:'In diesen Zuständen pulsieren (standardmäßig aus)',pulseLastPair:'Beim Drucken nur das letzte leuchtende Paar pulsieren lassen',resetLights:'LED-Standardeinstellungen wiederherstellen',saveLights:'LED-Einstellungen speichern',updateTitle:'Firmware-Update · OTA',updateHint:'.bin-Datei für LayerGlow auswählen. Das Update dauert etwa 2 Minuten. Strom nicht trennen und diese Seite nicht schließen.',upload:'Hochladen und neu starten',security:'Das Panel erfordert eine Anmeldung; lokales HTTP bleibt jedoch unverschlüsselt. Nur in einem vertrauenswürdigen Netz verwenden.',credits:'Entwickelt von',progressColor:'Druckfortschritt',colour_printing:'Druckt',colour_heating:'Heizt',colour_paused:'Pausiert',colour_complete:'Fertig',colour_error:'Fehler',colour_standby:'Bereit',saved:'Einstellungen gespeichert',scanError:'Suche fehlgeschlagen',updateDone:'Firmware hochgeladen; Neustart…',updateFailed:'Upload fehlgeschlagen',offline:'Drucker offline',connected:'Drucker verbunden',setup:'Einrichtungsnetz',home:'Lokales WLAN',test:'TEST',uploadConfirm:'Das Update kann etwa 2 Minuten dauern. Strom nicht trennen und diese Seite nicht schließen. Firmware hochladen?',chooseBin:'.bin-Datei auswählen',scanning:'Suche läuft…'}};
const FAQ_CONTENT={"en":{"search":"Search FAQ","empty":"No matching questions. Try another word.","sections":[["Getting started",[["How do I open LayerGlow?","On your home Wi-Fi open http://layerglow.local/. If that address does not work, use the LayerGlow IP shown under Network or in your router. On the setup network open http://192.168.4.1/."],["What do the buttons at the top do?","Network manages Wi-Fi and the device address; Printer sets the printer connection; Update handles firmware; FAQ opens this guide. On a phone, tap the flag to choose a language."],["How are changes saved?","Use Save & Connect Wi-Fi for network details and Save printer for printer details. LED and colour changes save automatically shortly after an adjustment. A message in the lower right confirms a save or reports an error."]]],["Wi-Fi and network",[["How do I connect to home Wi-Fi for the first time?","Join LayerGlow-Setup (password: layerglowsetup), open http://192.168.4.1/, then in Network scan for a 2.4 GHz network or enter its name. Enter the password and tap Save & Connect Wi-Fi."],["Why is my network missing from the scan?","LayerGlow uses 2.4 GHz Wi-Fi. Check that your router broadcasts a 2.4 GHz network, then scan again. You can enter the network name manually if it is hidden or scanning fails."],["What happens when home Wi-Fi goes offline?","LayerGlow keeps trying saved networks and starts the LayerGlow-Setup recovery network after a short disconnection. Connect to it and open http://192.168.4.1/ to inspect or change the settings."],["Can I save and switch between several Wi-Fi networks?","Yes. You can save up to eight networks. LayerGlow tries saved networks when you change location. The saved-network list lets you connect to a selected network or delete it; passwords are not displayed."],["Can I change the layerglow.local address?","In Network, change Device name (.local) and save. Use 1–32 letters, digits or hyphens. The new .local address may take a moment to appear; the router's device name follows the custom name after a restart."],["Why does layerglow.local not open?","Your phone or computer must be on the same local network. Try the IP shown under Network or in the router; some networks do not resolve .local names. On LayerGlow-Setup use 192.168.4.1."]]],["Printer connection",[["Which printer connections can I choose?","The panel offers Creality/Klipper through Moonraker, Bambu Lab through local MQTT, OctoPrint and PrusaLink. Compatibility depends on the printer and its local-network settings; test your model."],["How do I find and select a printer?","Connect LayerGlow and the printer to the same local network. Open Printer, tap Find printers and watch the search percentage. Select a result, complete any missing details and tap Save printer. If no result appears, enter its IP and port manually."],["How do I set up a Creality K2?","Choose Creality / Klipper (Moonraker), enter the printer IP and Moonraker port (usually 7125), then tap Save printer. If discovery misses the K2, entering its address manually is supported."],["What does a Bambu Lab printer need?","Enable LAN Only and Developer Mode on the printer. Choose Bambu Lab and enter its IP, serial number and LAN access code, then save. The status is read over the local network."],["How do I switch or disconnect the printer?","In Printer, use Connect on a saved profile. Disconnect printer is next to the connection status; it stops reading but keeps the profile. To change an existing profile, remove it and add it again with new settings."],["Where can I check if printer data is arriving?","The top of Printer shows connection status, details, last read, printer state, progress and file. If there is no data, check Wi-Fi, the printer IP and port, and any LAN code or API key."],["How do I save more than one printer?","Open Printer, fill in the printer details or select a discovery result, then Save printer. Enter any required LAN code or API key. Up to 8 profiles remain after restart. Connect selects one; Remove deletes it. Disconnect keeps it saved. To change an existing printer, remove its profile and add it again with the new details."],["Where does a saved printer’s MAC come from?","MAC is not read automatically. Earlier saved MAC addresses remain visible; a new profile can be saved without one. The listed Wi-Fi is the network at save time."]]],["LEDs, colours and effects",[["How do I change the LEDs for one printing stage?","In Colours and LED settings, find the stage such as Printing, Heating, Paused, Completed, Error or Idle. Change its colours, brightness, pulse and effect in that row. The switch marked On disables that stage when turned off."],["How do I add or remove colours?","Tap + beside the colour swatches to add a colour, or − to remove the last one. Each stage supports 1–12 colours, displayed six per row. Tap a swatch to choose its colour; during printing the palette forms zones along the strip."],["Can I reuse a custom colour?","Tap a colour swatch to open the browser's original colour picker. Its suggested colours include seven standard shades and the last custom colour in the former white slot. Choose Custom to make another colour. LayerGlow saves that colour on the device and makes it available for the next swatch and after a restart. The exact picker layout depends on your browser."],["How do I set brightness and pulse?","Tap Brightness and adjust the 0–100% slider. Tap Pulse to enable it and adjust the 0–5000 ms period. The chosen values remain visible after the controls close."],["What do the effects do?","Choose Effect in a stage to select whole bar, last pair, scanner, centre-to-edges, wave, random pairs, comet, moving fill, alternating pairs or colour animations. Palette animations need at least two colours; rainbow supplies its own colours."],["How many LEDs can I configure?","Set Physical LEDs to the total number connected, from 5 to 300. For two chained 12-LED sections enter 24. Ensure the strip has suitable power and wiring before increasing its length or brightness."],["What is the difference between restore and backup?","Restore default LED settings resets the entire Colours and LED settings section to its built-in values. Save my LED backup stores your current LED settings on the device; Restore my LED backup brings back that saved copy."]]],["Preview and test",[["Do test states control the real LED strip?","Yes. Selecting any test state controls the preview and the connected LEDs. Choose Live printer data to return to live printer data. Live printer datas continue in the Printer panel during a test."],["How do I test progress and stages?","Choose a test state to control the preview and connected LEDs. Printing automatically advances 5% every 0.5 seconds, holds at 100% for 1 second, then starts again. Other states remain active until you choose another state. Live printer data returns to the real printer status and progress. The mobile preview stays visible while you scroll."]]],["Blue status LED",[["What does one short flash mean?","LayerGlow has no home Wi-Fi connection. Check the saved network, or join LayerGlow-Setup and open 192.168.4.1 to repair it.","•"],["What do two short flashes mean?","LayerGlow is connected to Wi-Fi. This signal does not confirm a printer connection; check the Printer panel for that status.","• •"],["What do two long flashes and one short flash mean?","The page detected newer firmware on GitHub. Open Update to review it. The browser checks GitHub when the panel is open or when you check manually.","━ ━ •"]]],["Updates and troubleshooting",[["How do I update from GitHub?","Open Update and tap Check GitHub for updates. If a newer compatible version is found, Install update appears and Update is highlighted. The browser needs Internet access to fetch the GitHub manifest and firmware."],["How do I upload an OTA file manually?","Open Update, select the LayerGlow *_OTA.bin file and its matching .bin.sig file, then tap Upload and restart. Confirm the warning, keep power connected and allow about two minutes. The page tries to reconnect after restart."],["What if the update says Not Enough Space?","The firmware must fit the installed OTA app partition. Use the LayerGlow OTA binary for your board; if its size exceeds the available slot, the partition layout may need changing with a compatible USB flash. Do not upload an OTA file as a full image at address 0x0."],["Why is the Update button red?","A newer version was detected on GitHub. Open Update to review it. After installation and reconnection the panel checks the current version again."],["What if the page does not return after an update?","Keep power connected while it restarts. Try layerglow.local or the current IP from the router; the IP can change. If home Wi-Fi is unavailable, join LayerGlow-Setup and open 192.168.4.1."]]]]},"pl":{"search":"Szukaj w FAQ","empty":"Nie znaleziono pytania. Spróbuj innego słowa.","sections":[["Pierwsze kroki",[["Jak otworzyć stronę LayerGlow?","W domowej sieci Wi-Fi otwórz http://layerglow.local/. Jeśli adres nie działa, użyj IP LayerGlow widocznego w zakładce Sieć lub w routerze. Po połączeniu z siecią konfiguracyjną otwórz http://192.168.4.1/."],["Do czego służą przyciski u góry?","Sieć służy do ustawienia Wi-Fi i adresu urządzenia, Drukarka do połączenia z drukarką, Aktualizacja do wgrywania oprogramowania, a FAQ otwiera tę pomoc. Na telefonie dotknij flagi, aby zmienić język."],["Jak zapisywane są zmiany?","Dla sieci użyj Zapisz i połącz Wi-Fi, a dla drukarki Zapisz drukarkę. Zmiany LED i kolorów zapisują się automatycznie chwilę po ustawieniu. Komunikat w prawym dolnym rogu potwierdza zapis lub pokazuje błąd."]]],["Wi-Fi i sieć",[["Jak pierwszy raz połączyć się z domowym Wi-Fi?","Połącz się z LayerGlow-Setup (hasło: layerglowsetup) i otwórz http://192.168.4.1/. W zakładce Sieć wyszukaj sieć 2,4 GHz lub wpisz jej nazwę, podaj hasło i naciśnij Zapisz i połącz Wi-Fi."],["Dlaczego moja sieć nie pojawia się na liście?","LayerGlow korzysta z Wi-Fi 2,4 GHz. Sprawdź, czy router udostępnia sieć 2,4 GHz, i ponów skanowanie. Gdy sieć jest ukryta albo skan nie działa, wpisz jej nazwę ręcznie."],["Co się stanie po utracie domowego Wi-Fi?","LayerGlow próbuje łączyć się z zapisanymi sieciami i po krótkiej przerwie uruchamia sieć awaryjną LayerGlow-Setup. Połącz się z nią i otwórz http://192.168.4.1/, aby sprawdzić lub zmienić ustawienia."],["Czy mogę zapisać kilka sieci Wi-Fi?","Tak, maksymalnie osiem. Po zmianie lokalizacji LayerGlow próbuje zapisanych sieci. Na liście możesz połączyć się z wybraną siecią lub ją usunąć. Hasła nie są tam wyświetlane."],["Czy mogę zmienić adres layerglow.local?","W zakładce Sieć zmień Nazwę urządzenia (.local) i zapisz. Użyj 1–32 liter, cyfr lub myślników. Nowy adres .local może zacząć działać po chwili; nazwa w routerze zmieni się po restarcie."],["Dlaczego layerglow.local się nie otwiera?","Telefon lub komputer musi być w tej samej sieci lokalnej. Spróbuj adresu IP z zakładki Sieć albo z routera; niektóre sieci nie obsługują nazw .local. W LayerGlow-Setup użyj 192.168.4.1."]]],["Połączenie z drukarką",[["Jakie drukarki mogę wybrać?","Panel oferuje Creality/Klipper przez Moonraker, Bambu Lab przez lokalne MQTT, OctoPrint i PrusaLink. Zgodność zależy od modelu i jego ustawień sieciowych; sprawdź działanie ze swoją drukarką."],["Jak wyszukać i wybrać drukarkę?","Podłącz LayerGlow i drukarkę do tej samej sieci lokalnej. W zakładce Drukarka naciśnij Znajdź drukarki i obserwuj procent wyszukiwania. Wybierz wynik, uzupełnij wymagane dane i naciśnij Zapisz drukarkę. Gdy jej nie znaleziono, wpisz IP i port ręcznie."],["Jak skonfigurować Creality K2?","Wybierz Creality / Klipper (Moonraker), wpisz adres IP drukarki i port Moonraker (zwykle 7125), a potem naciśnij Zapisz drukarkę. Jeśli wyszukiwanie nie znajdzie K2, możesz wpisać adres ręcznie."],["Co jest potrzebne do połączenia z Bambu Lab?","W drukarce włącz tryb LAN Only i Developer Mode. Wybierz Bambu Lab, wpisz IP, numer seryjny i kod dostępu LAN, a następnie zapisz. Status jest odczytywany przez sieć lokalną."],["Jak zmienić lub rozłączyć drukarkę?","W zakładce Drukarka użyj Połącz przy zapisanym profilu. Rozłącz drukarkę znajduje się przy statusie połączenia; zatrzymuje odczyt, ale zachowuje profil. Aby zmienić dane, usuń profil i dodaj go ponownie."],["Gdzie sprawdzić, czy LayerGlow odczytuje dane?","U góry zakładki Drukarka są status połączenia, szczegóły, czas ostatniego odczytu, stan, postęp i plik wydruku. Gdy brak danych, sprawdź Wi-Fi, IP, port oraz kod LAN lub klucz API."],["Jak zapisać więcej niż jedną drukarkę?","Otwórz Drukarka, wpisz dane drukarki albo wybierz wynik wyszukiwania, a następnie Zapisz drukarkę. Podaj wymagany kod LAN lub klucz API. Do 8 profili pozostaje po restarcie. Połącz wybiera jeden, Usuń kasuje profil, a Rozłącz zachowuje go na liście. Aby zmienić dane zapisanej drukarki, usuń jej profil i dodaj ją ponownie."],["Skąd wziąć adres MAC zapisanej drukarki?","Adres MAC nie jest odczytywany automatycznie. Poprzednio zapisany MAC pozostaje widoczny, a nowy profil można zapisać bez niego. Wi-Fi na liście oznacza sieć w chwili zapisu."]]],["LED, kolory i efekty",[["Jak zmienić LED dla wybranego etapu?","W sekcji Kolory i ustawienia LED znajdź etap, np. Drukowanie, Nagrzewanie, Pauza, Zakończono, Błąd lub Bezczynność. W jego wierszu ustaw kolory, jasność, puls i efekt. Wyłączenie przełącznika Świeci wygasza ten etap."],["Jak dodać lub usunąć kolory?","Naciśnij + obok kolorów, aby dodać kolor, lub −, aby usunąć ostatni. Każdy etap obsługuje 1–12 kolorów, po sześć w wierszu. Dotknij próbki, aby wybrać barwę; podczas drukowania paleta tworzy strefy na pasku."],["Czy mogę ponownie użyć własnego koloru?","Dotknij próbki, aby otworzyć wcześniejszy wybierak kolorów przeglądarki. Wśród proponowanych barw jest siedem standardowych kolorów oraz ostatni własny kolor w miejscu białego pola. Wybierz Niestandardowe, aby ustawić nową barwę. LayerGlow zapisuje ją w urządzeniu; będzie dostępna przy następnej próbce i po restarcie. Układ wybieraka zależy od przeglądarki."],["Jak ustawić jasność i puls?","Dotknij Jasność i ustaw suwak 0–100%. Dotknij Puls, włącz go i ustaw okres 0–5000 ms. Wybrane wartości pozostają widoczne po zamknięciu suwaków."],["Jakie są dostępne efekty?","Wybierz Efekt w danym etapie: cały pasek, ostatnia para, skaner, środek–boki, fala, losowe pary, kometa, wypełnianie, naprzemienne pary lub animacje kolorów. Animacje palety wymagają co najmniej dwóch kolorów; tęcza korzysta z własnych barw."],["Ile diod mogę ustawić?","W polu Fizyczne LED wpisz całkowitą liczbę połączonych diod, od 5 do 300. Dwie sekcje po 12 diod połączone szeregowo to 24. Przed zwiększaniem długości lub jasności zadbaj o odpowiednie zasilanie."],["Czym różni się przywrócenie od kopii zapasowej?","Przywróć domyślne ustawienia LED resetuje całą sekcję Kolory i ustawienia LED do wartości fabrycznych. Zapisz moją kopię LED zachowuje aktualne ustawienia w urządzeniu, a Przywróć moją kopię LED odtwarza ten zapis."]]],["Podgląd i test",[["Czy wybór stanu testowego steruje prawdziwym paskiem LED?","Tak. Każdy stan testowy steruje podglądem i podłączonym paskiem. Wybierz Dane na żywo z drukarki, aby wrócić do danych drukarki. Podczas testu odczyt nadal trwa i jest widoczny w zakładce Drukarka."],["Jak sprawdzić etapy i postęp?","Wybierz stan testowy, aby sterować podglądem i podłączonym paskiem LED. Drukowanie automatycznie zwiększa postęp o 5% co 0,5 sekundy, zatrzymuje się przy 100% na sekundę i zaczyna od nowa. Pozostałe stany są aktywne do zmiany wyboru. Dane na żywo z drukarki przywraca rzeczywisty stan i postęp. Na telefonie podgląd jest widoczny podczas przewijania."]]],["Niebieska dioda LayerGlow",[["Co oznacza jeden krótki błysk?","LayerGlow nie ma połączenia z domowym Wi-Fi. Sprawdź zapisaną sieć albo połącz się z LayerGlow-Setup i otwórz 192.168.4.1, aby poprawić ustawienia.","•"],["Co oznaczają dwa krótkie błyski?","LayerGlow jest połączony z Wi-Fi. Ten sygnał nie potwierdza połączenia z drukarką; jej stan sprawdzisz w zakładce Drukarka.","• •"],["Co oznaczają dwa długie i jeden krótki błysk?","Strona wykryła nowszy firmware na GitHub. Otwórz Aktualizacja, aby go sprawdzić. Przeglądarka sprawdza GitHub po otwarciu panelu lub na żądanie.","━ ━ •"]]],["Aktualizacje i problemy",[["Jak zaktualizować oprogramowanie z GitHub?","Otwórz Aktualizacja i naciśnij Sprawdź aktualizacje na GitHub. Gdy jest nowsza zgodna wersja, pojawi się Instaluj aktualizację, a przycisk Aktualizacja zostanie wyróżniony. Przeglądarka musi mieć Internet, aby pobrać manifest i firmware."],["Jak ręcznie wgrać plik OTA?","W zakładce Aktualizacja wybierz plik LayerGlow *_OTA.bin i pasujący plik .bin.sig, a następnie naciśnij Wgraj i uruchom ponownie. Potwierdź ostrzeżenie, nie odłączaj zasilania i poczekaj około dwóch minut. Strona spróbuje połączyć się ponownie."],["Co oznacza błąd Not Enough Space?","Firmware musi zmieścić się w partycji aplikacji OTA. Użyj obrazu LayerGlow przeznaczonego dla tej płytki; jeśli przekracza wolne miejsce, zmiana układu partycji może wymagać wgrania przez USB. Nie wgrywaj pliku OTA jak pełnego obrazu pod adres 0x0."],["Dlaczego przycisk Aktualizacja jest czerwony?","Wykryto nowszą wersję na GitHub. Otwórz Aktualizacja, aby ją sprawdzić. Po instalacji i ponownym połączeniu panel ponownie porówna wersje."],["Co zrobić, gdy strona nie wraca po aktualizacji?","Nie odłączaj zasilania podczas restartu. Spróbuj layerglow.local lub aktualnego IP z routera; IP mogło się zmienić. Gdy nie ma domowego Wi-Fi, połącz się z LayerGlow-Setup i otwórz 192.168.4.1."]]]]},"es":{"search":"Buscar en las preguntas","empty":"No hay preguntas que coincidan. Prueba otra palabra.","sections":[["Primeros pasos",[["¿Cómo abro LayerGlow?","En la Wi-Fi de casa abre http://layerglow.local/. Si no funciona, usa la IP de LayerGlow que aparece en Red o en el router. En la red de configuración abre http://192.168.4.1/."],["¿Para qué sirven los botones de arriba?","Red configura la Wi-Fi y la dirección del dispositivo; Impresora configura la conexión; Actualización instala el firmware; Preguntas abre esta guía. En el móvil, toca la bandera para cambiar de idioma."],["¿Cómo se guardan los cambios?","Usa Guardar y conectar Wi-Fi para la red y Guardar impresora para la impresora. Los cambios de LED y colores se guardan automáticamente poco después de ajustarlos. Un aviso abajo a la derecha confirma el guardado o muestra un error."]]],["Wi-Fi y red",[["¿Cómo conecto por primera vez a la Wi-Fi de casa?","Conéctate a LayerGlow-Setup (contraseña: layerglowsetup) y abre http://192.168.4.1/. En Red busca una red de 2,4 GHz o escribe su nombre, introduce la contraseña y pulsa Guardar y conectar Wi-Fi."],["¿Por qué no aparece mi red en la búsqueda?","LayerGlow usa Wi-Fi de 2,4 GHz. Comprueba que el router emita en 2,4 GHz y vuelve a buscar. Si la red está oculta o falla la búsqueda, escribe su nombre manualmente."],["¿Qué ocurre si se pierde la Wi-Fi de casa?","LayerGlow intenta conectarse a las redes guardadas y, tras una breve desconexión, abre la red de recuperación LayerGlow-Setup. Conéctate a ella y abre http://192.168.4.1/ para revisar o cambiar los ajustes."],["¿Puedo guardar varias redes Wi-Fi?","Sí, hasta ocho. LayerGlow prueba las redes guardadas al cambiar de lugar. En la lista puedes conectar con una red concreta o eliminarla; las contraseñas no se muestran."],["¿Puedo cambiar la dirección layerglow.local?","En Red cambia el Nombre del dispositivo (.local) y guarda. Usa entre 1 y 32 letras, números o guiones. La nueva dirección .local puede tardar un poco en aparecer; el nombre del router cambia tras reiniciar."],["¿Por qué no abre layerglow.local?","El móvil u ordenador debe estar en la misma red local. Usa la IP que aparece en Red o en el router; algunas redes no resuelven nombres .local. En LayerGlow-Setup usa 192.168.4.1."]]],["Conexión con la impresora",[["¿Qué tipos de impresora puedo elegir?","El panel ofrece Creality/Klipper por Moonraker, Bambu Lab por MQTT local, OctoPrint y PrusaLink. La compatibilidad depende del modelo y sus ajustes de red local; compruébala con tu impresora."],["¿Cómo busco y elijo una impresora?","Conecta LayerGlow y la impresora a la misma red local. En Impresora pulsa Buscar impresoras y observa el porcentaje. Elige un resultado, completa los datos necesarios y pulsa Guardar impresora. Si no aparece, introduce la IP y el puerto manualmente."],["¿Cómo configuro una Creality K2?","Elige Creality / Klipper (Moonraker), introduce la IP de la impresora y el puerto Moonraker (normalmente 7125), y pulsa Guardar impresora. Si la búsqueda no encuentra la K2, puedes introducir la dirección a mano."],["¿Qué necesita una impresora Bambu Lab?","Activa LAN Only y Developer Mode en la impresora. Elige Bambu Lab e introduce la IP, el número de serie y el código de acceso LAN; después guarda. El estado se lee por la red local."],["¿Cómo cambio o desconecto la impresora?","En Impresora pulsa Conectar en un perfil guardado. Desconectar impresora está junto al estado de conexión; detiene la lectura y conserva el perfil. Para cambiar los ajustes, elimina el perfil y añádelo otra vez."],["¿Dónde compruebo si llegan datos de la impresora?","En la parte superior de Impresora se muestran conexión, detalles, última lectura, estado, progreso y archivo. Si no hay datos, revisa Wi-Fi, IP, puerto y el código LAN o la clave API."],["¿Cómo guardo más de una impresora?","Abre Impresora, introduce los datos de la impresora o selecciona un resultado de búsqueda y pulsa Guardar impresora. Introduce el código LAN o la clave API si es necesario. Se conservan hasta 8 perfiles tras reiniciar. Conectar selecciona uno; Eliminar lo borra. Desconectar lo mantiene guardado. Para cambiar los datos de una impresora, elimina su perfil y vuelve a añadirla."],["¿De dónde se obtiene la MAC de una impresora?","La MAC no se lee automáticamente. Se muestran las MAC guardadas anteriormente y se puede añadir un perfil sin MAC. El Wi-Fi de la lista corresponde a la red al guardar."]]],["LED, colores y efectos",[["¿Cómo cambio los LED de una fase?","En Colores y ajustes LED busca la fase: Impresión, Calentamiento, Pausa, Terminado, Error o Reposo. En su fila cambia colores, brillo, pulso y efecto. Al apagar el interruptor Encendido se desactiva la iluminación de esa fase."],["¿Cómo añado o quito colores?","Pulsa + junto a las muestras para añadir un color o − para quitar el último. Cada fase admite entre 1 y 12 colores, seis por fila. Toca una muestra para elegir el color; durante la impresión la paleta forma zonas a lo largo de la tira."],["¿Puedo reutilizar un color personalizado?","Toca una muestra para abrir el selector original del navegador. Los colores sugeridos incluyen siete colores estándar y el último color personalizado en lugar del blanco. Usa Personalizado para definir otro color. LayerGlow lo guarda en el dispositivo para la siguiente muestra y después de reiniciar. El diseño exacto depende del navegador."],["¿Cómo ajusto brillo y pulso?","Toca Brillo y mueve el control de 0 a 100%. Toca Pulso para activarlo y ajustar el periodo de 0 a 5000 ms. Los valores quedan visibles al cerrar los controles."],["¿Qué efectos hay?","En Efecto puedes elegir tira completa, último par, escáner, centro y extremos, ola, pares aleatorios, cometa, relleno móvil, pares alternos y animaciones de color. Las animaciones de paleta requieren al menos dos colores; arcoíris utiliza sus propios colores."],["¿Cuántos LED puedo configurar?","En LED físicos indica el total conectado, entre 5 y 300. Dos segmentos de 12 LED conectados en serie son 24. Antes de aumentar la longitud o el brillo, asegura una alimentación y un cableado adecuados."],["¿Qué diferencia hay entre restaurar y la copia de seguridad?","Restaurar ajustes LED predeterminados devuelve toda la sección Colores y ajustes LED a sus valores originales. Guardar mi copia LED almacena tus ajustes actuales en el dispositivo; Restaurar mi copia LED recupera esa copia."]]],["Vista previa y prueba",[["¿Los estados de prueba controlan la tira LED real?","Sí. Cualquier estado de prueba controla la vista previa y los LED conectados. Elige Datos en vivo de la impresora para volver a los datos reales. La lectura continúa en el panel Impresora durante la prueba."],["¿Cómo pruebo fases y progreso?","Elige un estado de prueba para controlar la vista previa y los LED conectados. Impresión avanza automáticamente un 5% cada 0,5 segundos, mantiene el 100% durante 1 segundo y vuelve a empezar. Los demás estados permanecen activos hasta que elijas otro. Datos en vivo de la impresora muestra de nuevo el estado y el progreso reales. En el móvil, la vista previa sigue visible al desplazarte."]]],["LED azul de estado",[["¿Qué significa un destello corto?","LayerGlow no está conectado a la Wi-Fi doméstica. Revisa la red guardada o conéctate a LayerGlow-Setup y abre 192.168.4.1 para corregirla.","•"],["¿Qué significan dos destellos cortos?","LayerGlow está conectado a la Wi-Fi. No confirma la conexión con la impresora; comprueba su estado en Impresora.","• •"],["¿Qué significan dos destellos largos y uno corto?","La página detectó firmware nuevo en GitHub. Abre Actualización para revisarlo. El navegador consulta GitHub al abrir el panel o cuando lo solicitas.","━ ━ •"]]],["Actualizaciones y problemas",[["¿Cómo actualizo desde GitHub?","Abre Actualización y pulsa Buscar actualizaciones en GitHub. Si hay una versión nueva compatible, aparecerá Instalar actualización y se resaltará Actualización. El navegador necesita Internet para descargar el manifiesto y el firmware."],["¿Cómo subo un archivo OTA manualmente?","En Actualización elige el archivo LayerGlow *_OTA.bin y el archivo .bin.sig correspondiente, luego pulsa Subir y reiniciar. Confirma el aviso, mantén la alimentación y espera unos dos minutos. La página intentará reconectar tras el reinicio."],["¿Qué significa Not Enough Space?","El firmware debe caber en la partición de aplicación OTA. Usa el binario LayerGlow adecuado para tu placa; si supera el espacio disponible, cambiar las particiones puede requerir flashear por USB. No subas un OTA como imagen completa en la dirección 0x0."],["¿Por qué está rojo el botón Actualización?","Se ha detectado una versión nueva en GitHub. Abre Actualización para revisarla. Después de instalarla y reconectar, el panel vuelve a comparar las versiones."],["¿Qué hago si la página no vuelve tras actualizar?","Mantén la alimentación durante el reinicio. Prueba layerglow.local o la IP actual del router; la IP puede cambiar. Si no hay Wi-Fi doméstica, conéctate a LayerGlow-Setup y abre 192.168.4.1."]]]]},"de":{"search":"FAQ durchsuchen","empty":"Keine passenden Fragen. Anderes Suchwort versuchen.","sections":[["Erste Schritte",[["Wie öffne ich LayerGlow?","Im Heim-WLAN http://layerglow.local/ öffnen. Falls die Adresse nicht funktioniert, die LayerGlow-IP unter Netzwerk oder im Router verwenden. Im Einrichtungsnetz http://192.168.4.1/ öffnen."],["Wozu dienen die Schaltflächen oben?","Netzwerk verwaltet WLAN und Geräteadresse; Drucker richtet die Druckerverbindung ein; Update installiert Firmware; FAQ öffnet diese Hilfe. Auf dem Handy die Flagge antippen, um die Sprache zu wechseln."],["Wie werden Änderungen gespeichert?","Für das Netzwerk WLAN speichern und verbinden, für den Drucker Drucker speichern antippen. LED- und Farbänderungen werden kurz nach dem Einstellen automatisch gespeichert. Eine Meldung unten rechts bestätigt das Speichern oder zeigt einen Fehler."]]],["WLAN und Netzwerk",[["Wie verbinde ich LayerGlow erstmals mit dem Heim-WLAN?","Mit LayerGlow-Setup verbinden (Passwort: layerglowsetup) und http://192.168.4.1/ öffnen. Unter Netzwerk ein 2,4-GHz-Netz suchen oder seinen Namen eingeben, Passwort eintragen und WLAN speichern und verbinden antippen."],["Warum erscheint mein WLAN nicht beim Suchen?","LayerGlow nutzt 2,4-GHz-WLAN. Prüfen, ob der Router ein 2,4-GHz-Netz sendet, und erneut suchen. Bei verstecktem Netz oder fehlgeschlagener Suche den Namen von Hand eingeben."],["Was passiert, wenn das Heim-WLAN ausfällt?","LayerGlow versucht gespeicherte Netze und startet nach einer kurzen Unterbrechung das Einrichtungsnetz LayerGlow-Setup. Damit verbinden und unter http://192.168.4.1/ die Einstellungen prüfen oder ändern."],["Kann ich mehrere WLAN-Netze speichern?","Ja, bis zu acht. Beim Standortwechsel versucht LayerGlow gespeicherte Netze. In der Liste kann ein Netz verbunden oder gelöscht werden; Passwörter werden dort nicht angezeigt."],["Kann ich layerglow.local ändern?","Unter Netzwerk Gerätename (.local) ändern und speichern. Erlaubt sind 1–32 Buchstaben, Ziffern oder Bindestriche. Die neue .local-Adresse kann etwas später verfügbar sein; der Name im Router ändert sich nach einem Neustart."],["Warum öffnet layerglow.local nicht?","Handy oder Computer müssen im selben lokalen Netz sein. Die IP unter Netzwerk oder im Router ausprobieren; manche Netze lösen .local-Namen nicht auf. Im LayerGlow-Setup 192.168.4.1 verwenden."]]],["Druckerverbindung",[["Welche Drucker kann ich auswählen?","Das Panel bietet Creality/Klipper über Moonraker, Bambu Lab über lokales MQTT, OctoPrint und PrusaLink. Die Kompatibilität hängt vom Modell und dessen LAN-Einstellungen ab; mit dem eigenen Drucker prüfen."],["Wie suche und wähle ich einen Drucker?","LayerGlow und Drucker mit demselben lokalen Netz verbinden. Unter Drucker Drucker suchen antippen und den Prozentwert beobachten. Ein Ergebnis wählen, fehlende Angaben ergänzen und Drucker speichern antippen. Falls keiner gefunden wird, IP und Port von Hand eintragen."],["Wie richte ich einen Creality K2 ein?","Creality / Klipper (Moonraker) wählen, Drucker-IP und Moonraker-Port (meist 7125) eingeben, dann Drucker speichern. Wird der K2 nicht gefunden, kann seine Adresse manuell eingetragen werden."],["Was benötigt ein Bambu-Lab-Drucker?","Am Drucker LAN Only und Developer Mode aktivieren. Bambu Lab wählen, IP, Seriennummer und LAN-Zugangscode eintragen und speichern. Der Status wird über das lokale Netz gelesen."],["Wie wechsle oder trenne ich den Drucker?","Unter Drucker bei einem gespeicherten Profil Verbinden wählen. Drucker trennen steht neben dem Verbindungsstatus; das Profil bleibt erhalten. Für Änderungen das Profil entfernen und mit neuen Daten erneut hinzufügen."],["Wo sehe ich, ob Live-Druckerdaten ankommen?","Oben unter Drucker stehen Verbindungsstatus, Details, letzte Abfrage, Zustand, Fortschritt und Datei. Falls Daten fehlen, WLAN, IP, Port sowie LAN-Code oder API-Schlüssel prüfen."],["Wie speichere ich mehrere Drucker?","Drucker öffnen, die Druckerdaten eingeben oder ein Suchergebnis wählen, dann Drucker speichern. Falls nötig LAN-Code oder API-Schlüssel eingeben. Bis zu 8 Profile bleiben nach Neustart erhalten. Verbinden wählt eines, Entfernen löscht es, Trennen behält es in der Liste. Um gespeicherte Druckerdaten zu ändern, das Profil entfernen und mit den neuen Angaben erneut hinzufügen."],["Woher kommt die MAC-Adresse eines Druckers?","Die MAC wird nicht automatisch gelesen. Früher gespeicherte MAC-Adressen bleiben sichtbar; neue Profile können ohne MAC gespeichert werden. Das WLAN in der Liste ist das Netz beim Speichern."]]],["LEDs, Farben und Effekte",[["Wie ändere ich LEDs für einen Druckabschnitt?","Unter Farben und LED-Einstellungen den Abschnitt wählen: Drucken, Heizen, Pause, Fertig, Fehler oder Bereit. In seiner Zeile Farben, Helligkeit, Pulsieren und Effekt einstellen. Wird der Schalter An deaktiviert, bleiben die LEDs in diesem Abschnitt aus."],["Wie füge ich Farben hinzu oder entferne sie?","Mit + neben den Farbfeldern eine Farbe hinzufügen oder mit − die letzte entfernen. Je Abschnitt sind 1–12 Farben möglich, sechs pro Zeile. Ein Farbfeld antippen, um die Farbe zu wählen; beim Drucken bilden sie Zonen entlang des Streifens."],["Kann ich eine eigene Farbe erneut verwenden?","Tippe auf ein Farbfeld, um den ursprünglichen Farbwähler des Browsers zu öffnen. Die vorgeschlagenen Farben enthalten sieben Standardfarben und die letzte eigene Farbe anstelle von Weiß. Unter Benutzerdefiniert wählst du eine neue Farbe. LayerGlow speichert sie auf dem Gerät für das nächste Farbfeld und nach einem Neustart. Das genaue Aussehen hängt vom Browser ab."],["Wie stelle ich Helligkeit und Pulsieren ein?","Helligkeit antippen und den Regler auf 0–100 % stellen. Pulsieren antippen, aktivieren und die Dauer auf 0–5000 ms einstellen. Die gewählten Werte bleiben nach dem Schließen sichtbar."],["Welche Effekte gibt es?","Unter Effekt stehen ganzer Streifen, letztes Paar, Scanner, Mitte und Ränder, Welle, zufällige Paare, Komet, laufende Füllung, abwechselnde Paare und Farbanimationen zur Wahl. Palettenanimationen brauchen mindestens zwei Farben; Regenbogen verwendet eigene Farben."],["Wie viele LEDs kann ich einstellen?","Unter Physische LEDs die Gesamtzahl von 5 bis 300 eintragen. Zwei verkettete Abschnitte mit je 12 LEDs ergeben 24. Vor mehr LEDs oder höherer Helligkeit Stromversorgung und Verkabelung prüfen."],["Was ist der Unterschied zwischen Zurücksetzen und Sicherung?","LED-Standardeinstellungen wiederherstellen setzt den gesamten Abschnitt Farben und LED-Einstellungen zurück. Meine LED-Sicherung speichern bewahrt die aktuellen Einstellungen auf dem Gerät; Meine LED-Sicherung wiederherstellen lädt diese Kopie."]]],["Vorschau und Test",[["Steuern Testzustände den echten LED-Streifen?","Ja. Jeder Testzustand steuert Vorschau und angeschlossene LEDs. Mit Live-Druckerdaten kehrst du zu den echten Live-Druckerdaten zurück. Der Drucker wird während des Tests weiter ausgelesen."],["Wie teste ich Zustände und Fortschritt?","Wähle einen Testzustand, um Vorschau und angeschlossene LEDs zu steuern. Drucken erhöht den Fortschritt automatisch alle 0,5 Sekunden um 5 %, bleibt 1 Sekunde bei 100 % und beginnt erneut. Andere Zustände bleiben bis zur nächsten Auswahl aktiv. Live-Druckerdaten zeigt wieder den tatsächlichen Zustand und Fortschritt. Auf dem Handy bleibt die Vorschau beim Scrollen sichtbar."]]],["Blaue Status-LED",[["Was bedeutet ein kurzes Blinken?","LayerGlow ist nicht mit dem Heim-WLAN verbunden. Gespeichertes Netz prüfen oder mit LayerGlow-Setup verbinden und unter 192.168.4.1 die Einstellungen korrigieren.","•"],["Was bedeuten zwei kurze Blinkzeichen?","LayerGlow ist mit dem WLAN verbunden. Das bestätigt keine Druckerverbindung; deren Status steht im Drucker-Panel.","• •"],["Was bedeuten zwei lange und ein kurzes Blinkzeichen?","Die Seite hat neue Firmware auf GitHub erkannt. Unter Update kann sie geprüft werden. Der Browser prüft GitHub beim Öffnen des Panels oder auf Anforderung.","━ ━ •"]]],["Updates und Probleme",[["Wie aktualisiere ich über GitHub?","Update öffnen und Auf GitHub nach Updates suchen antippen. Bei einer neueren kompatiblen Version erscheint Update installieren und Update wird hervorgehoben. Der Browser benötigt Internet, um Manifest und Firmware von GitHub zu laden."],["Wie lade ich eine OTA-Datei manuell hoch?","Unter Update die LayerGlow-Datei *_OTA.bin und die passende .bin.sig-Datei wählen, dann Hochladen und neu starten antippen. Warnung bestätigen, Strom eingeschaltet lassen und etwa zwei Minuten warten. Die Seite versucht nach dem Neustart, sich erneut zu verbinden."],["Was bedeutet Not Enough Space?","Die Firmware muss in die OTA-App-Partition passen. Die passende LayerGlow-OTA-Datei verwenden; ist sie zu groß, kann eine Änderung der Partitionierung per USB nötig sein. Eine OTA-Datei nicht als vollständiges Abbild an Adresse 0x0 schreiben."],["Warum ist die Schaltfläche Update rot?","Auf GitHub wurde eine neuere Version erkannt. Unter Update kann sie geprüft werden. Nach Installation und erneuter Verbindung vergleicht das Panel die Versionen nochmals."],["Was, wenn die Seite nach dem Update nicht zurückkommt?","Während des Neustarts die Stromversorgung angeschlossen lassen. layerglow.local oder die aktuelle IP im Router ausprobieren; die IP kann sich ändern. Ohne Heim-WLAN mit LayerGlow-Setup verbinden und 192.168.4.1 öffnen."]]]]}};
function renderFAQ(){
  const data=FAQ_CONTENT[lang]||FAQ_CONTENT.en,container=document.getElementById('faqQuestions');
  const search=document.getElementById('faqSearch');
  document.getElementById('faqSearchLabel').textContent=data.search;
  search.placeholder=data.search;
  document.getElementById('faqEmpty').textContent=data.empty;
  container.replaceChildren();
  for(const [heading,entries] of data.sections){
    const section=document.createElement('section');section.className='faq-section';
    const title=document.createElement('h3');title.textContent=heading;section.append(title);
    for(const [question,answer,pattern] of entries){
      const item=document.createElement('details');item.className='faq-item';
      item.dataset.faqText=(heading+' '+question+' '+answer+' '+(pattern||'')).toLocaleLowerCase();
      const summary=document.createElement('summary');if(pattern){const badge=document.createElement('span');badge.className='faq-led-pattern';badge.textContent=pattern;summary.append(badge,document.createTextNode(' '+question))}else summary.textContent=question;
      const body=document.createElement('p');body.textContent=answer;
      item.append(summary,body);section.append(item);
    }
    container.append(section);
  }
  filterFAQ();
}
function filterFAQ(){
  const query=document.getElementById('faqSearch').value.trim().toLocaleLowerCase();
  let visible=0;
  for(const section of document.querySelectorAll('#faqQuestions .faq-section')){
    let count=0;
    for(const item of section.querySelectorAll('.faq-item')){
      item.hidden=!!query&&!item.dataset.faqText.includes(query);
      if(!item.hidden)count++;
    }
    section.hidden=count===0;visible+=count;
  }
  document.getElementById('faqEmpty').hidden=visible!==0;
}
document.getElementById('faqSearch').addEventListener('input',filterFAQ);
const THEME_TEXT={
  en:{label:'Theme',auto:'Automatic',autoShort:'Auto',light:'Light',dark:'Dark'},
  pl:{label:'Motyw',auto:'Automatycznie',autoShort:'Auto',light:'Jasny',dark:'Ciemny'},
  es:{label:'Tema',auto:'Automático',autoShort:'Auto',light:'Claro',dark:'Oscuro'},
  de:{label:'Design',auto:'Automatisch',autoShort:'Auto',light:'Hell',dark:'Dunkel'}
};
const themeChoices=[...document.querySelectorAll('.theme-choice')];
const themeMedia=window.matchMedia('(prefers-color-scheme: dark)');
const savedTheme=localStorage.getItem('layerglowTheme');
let themeMode=['auto','light','dark'].includes(savedTheme)?savedTheme:'auto';
function updateThemeLabels(){
  const labels=THEME_TEXT[lang]||THEME_TEXT.en;
  document.getElementById('themeLabel').textContent=labels.label;
  for(const choice of themeChoices){
    choice.parentElement.querySelector('span').textContent=choice.value==='auto'?labels.autoShort:labels[choice.value];
    choice.setAttribute('aria-label',labels[choice.value]);
    choice.title=labels[choice.value];
    choice.checked=choice.value===themeMode;
  }
}
function applyTheme(){
  const dark=themeMode==='dark'||(themeMode==='auto'&&themeMedia.matches);
  document.documentElement.dataset.theme=dark?'dark':'light';
  document.querySelector('meta[name="theme-color"]').content=dark?'#0f1712':'#f5f7f6';
}
for(const choice of themeChoices)choice.addEventListener('change',()=>{
  themeMode=choice.value;
  localStorage.setItem('layerglowTheme',themeMode);
  updateThemeLabels();
  applyTheme();
});
themeMedia.addEventListener('change',()=>{if(themeMode==='auto')applyTheme()});
applyTheme();
let lang='en',initialized=false,lastStatus={},simBusy=false,simQueued=null,lastNetworkInfo=null,networkInfoBusy=false;
function setLang(l){if(!T[l])l='en';lang=l;document.documentElement.lang=l;document.title=T[l].appName+' by FatGiantCat';document.getElementById('openLang').textContent=({en:'🇬🇧',pl:'🇵🇱',es:'🇪🇸',de:'🇩🇪'})[l];localStorage.setItem('k2lang',l);document.querySelectorAll('[data-i]').forEach(e=>{e.textContent=T[l][e.dataset.i]||T.en[e.dataset.i]||e.textContent});document.querySelectorAll('[data-aria-i]').forEach(e=>e.setAttribute('aria-label',T[l][e.dataset.ariaI]||T.en[e.dataset.ariaI]));document.querySelectorAll('.toast button').forEach(e=>e.setAttribute('aria-label',T[l].close));for(const id of Object.keys(T).filter(x=>['en','pl','es','de'].includes(x)))document.getElementById(id).classList.toggle('active',l===id);renderFAQ();updateThemeLabels();paintStatus(lastStatus);if(lastNetworkInfo)renderConnectionStatus(lastNetworkInfo);if(availableUpdate)document.getElementById('githubMessage').textContent=T[l].newVersion+' '+availableUpdate.version;rowIds.forEach(syncRowControls);if(initialized){lastConnectionKey=null;refresh()}}
const GITHUB_MANIFEST='https://raw.githubusercontent.com/TomaszK00/LayerGlow/main/Software/update.json';
const GITHUB_LIST='https://api.github.com/repos/TomaszK00/LayerGlow/contents/Software?ref=main';
let availableUpdate=null,refreshing=false,updatingFirmware=false;
const rowIds=['progress','heating','paused','complete','error','standby'];
const rowLabels=['printing','heating','paused','complete','error','standby'];
const rowContainer=document.getElementById('ledRows');
for(const [i,id] of rowIds.entries()){const row=document.createElement('div');row.className='led-row';const effects='<option value="0" data-i="allBar">Whole bar</option><option value="1" data-i="lastPair">Last pair</option><option value="2" data-i="scanner">Scanner (Knight Rider)</option><option value="3" data-i="centreOut">Centre → edges → centre</option><option value="5" data-i="wave">Flowing wave</option><option value="6" data-i="randomPairs">Random pairs</option><option value="7" data-i="comet">Comet</option><option value="8" data-i="wipe">Moving fill</option><option value="9" data-i="alternating">Alternating pairs</option><option value="10" data-i="colourFlow">Colour flow</option><option value="11" data-i="colourFade">Colour fade</option><option value="12" data-i="rainbowFlow">Rainbow flow</option><option value="13" data-i="colourMarch">Moving colour bands</option>'; row.innerHTML=`<div class="row-title"><label data-i="${rowLabels[i]}" for="c_${id}"></label><label class="enabled-toggle"><input type="checkbox" name="on_${id}" id="on_${id}" checked><span data-i="ledOn">On</span></label></div><div class="palette-controls"><input type="hidden" name="n_${id}" id="n_${id}" value="1"><div class="palette-toolbar" data-action-row="first"><div class="palette-swatches"><input type="color" name="c_${id}" id="c_${id}" list="layerglow-colour-presets" data-aria-i="colour">${Array.from({length:11},(_,idx)=>idx+1).map(n=>`<input type="color" name="c${n}_${id}" id="c${n}_${id}" list="layerglow-colour-presets" data-aria-i="colour" hidden>`).join('')}</div><div class="palette-actions"><button type="button" class="alt" id="add_${id}" data-aria-i="addColour" aria-label="Add colour" onclick="changePalette('${id}',1)">+</button><button type="button" class="alt" id="remove_${id}" data-aria-i="removeColour" aria-label="Remove colour" onclick="changePalette('${id}',-1)">−</button></div></div></div><div class="brightness-control"><details name="led-setting"><summary><span data-i="brightness">Brightness</span><strong id="bv_${id}">65%</strong></summary><input type="range" name="b_${id}" id="b_${id}" min="0" max="100" step="1" value="65" data-aria-i="brightness"></details></div><div class="pulse-control"><details name="led-setting"><summary><span data-i="pulseSetting">Pulse</span><strong id="pv_${id}">Off</strong></summary><label class="pulse-enabled"><input type="checkbox" name="pe_${id}" id="pe_${id}"><span data-i="pulseEnabled">Enabled</span></label><div class="range-line"><input type="range" name="pp_${id}" id="pp_${id}" min="0" max="5000" step="50" value="1400" data-aria-i="pulsePeriod"><output id="pms_${id}" for="pp_${id}">1400 ms</output></div></details></div><div class="effect-control"><div class="effect-heading"><span class="field-label" data-i="effect">Effect</span><span class="effect-name" id="effectCurrent_${id}"></span></div><select name="ps_${id}" id="ps_${id}" data-aria-i="effect">${effects}</select></div>`;rowContainer.append(row)}
// Keep one LED adjustment open, including browsers without details[name] support.
for(const control of rowContainer.querySelectorAll('details')){
  control.addEventListener('toggle',()=>{
    if(!control.open){clearTimeout(control.closeTimer);return}
    for(const other of rowContainer.querySelectorAll('details[open]')){
      if(other!==control){clearTimeout(other.closeTimer);other.open=false}
    }
  });
}
function syncRowControls(id){
  const bright=document.getElementById('b_'+id),period=document.getElementById('pp_'+id),pulse=document.getElementById('pe_'+id),effect=document.getElementById('ps_'+id);
  document.getElementById('bv_'+id).textContent=bright.value+'%';
  document.getElementById('pv_'+id).textContent=pulse.checked&&Number(period.value)>0?period.value+' ms':T[lang].pulseOff;
  document.getElementById('pms_'+id).textContent=period.value+' ms';
  document.getElementById('effectCurrent_'+id).textContent=effect.selectedOptions[0]?.textContent||'';
}
function updatePaletteControls(id){const count=Number(document.getElementById('n_'+id).value);document.getElementById('add_'+id).closest('.palette-toolbar').dataset.actionRow=count<=4?'first':count<=10?'second':'third';for(let n=1;n<12;n++)document.getElementById('c'+n+'_'+id).hidden=n>=count;document.getElementById('add_'+id).disabled=count>=12;document.getElementById('remove_'+id).disabled=count<=1}
function changePalette(id,delta){const field=document.getElementById('n_'+id);field.value=String(Math.max(1,Math.min(12,Number(field.value)+delta)));updatePaletteControls(id);paintStatus(lastStatus);scheduleLightSave()}
const BUILTIN_COLOURS=new Set(['#ff0000','#00ffff','#0000ff','#00ff00','#ff00ff','#ffff00','#000000','#ffffff']);
let savedCustomColour='#ffffff',customColourSaveChain=Promise.resolve(),customColourSavePending=0;
function showSavedCustomColour(colours){
  const custom=Array.isArray(colours)?colours.find(c=>/^#[0-9a-f]{6}$/i.test(c)&&!BUILTIN_COLOURS.has(c.toLowerCase())):null;
  if(custom)savedCustomColour=custom.toLowerCase();
  document.getElementById('savedCustomColour').value=savedCustomColour;
}
function rememberCustomColour(value){
  const colour=String(value).toLowerCase();
  if(!/^#[0-9a-f]{6}$/.test(colour)||BUILTIN_COLOURS.has(colour))return;
  savedCustomColour=colour;
  document.getElementById('savedCustomColour').value=colour;
  customColourSavePending++;
  customColourSaveChain=customColourSaveChain.catch(()=>{}).then(async()=>{
    try{await api('/api/colours/recent',{method:'POST',body:new URLSearchParams({colour})})}
    catch(_){toast(T[lang].rememberedColourSaveError,true)}
    finally{customColourSavePending--}
  });
}
function paletteFor(id,r){const count=Number(document.getElementById('n_'+id)?.value||r.colourCount||1);return Array.from({length:count},(_,n)=>document.getElementById((n?'c'+n:'c')+'_'+id)?.value||r.colours?.[n]||r.color);}
function paletteSlot(pixel,total,count){return Math.min(count-1,Math.floor(pixel*count/total));}
function blendColour(a,b,f){const x=parseInt(a.slice(1),16),y=parseInt(b.slice(1),16);return '#'+[16,8,0].map(shift=>Math.floor((((x>>shift)&255)*(65536-f)+((y>>shift)&255)*f+32768)/65536).toString(16).padStart(2,'0')).join('')}
function samplePalette(palette,position,smooth=true){const scaled=position*palette.length,slot=Math.floor(scaled/65536),fraction=scaled%65536;return smooth?blendColour(palette[slot],palette[(slot+1)%palette.length],fraction):palette[slot]}
function effectColour(palette,pixel,total,style,tick){const position=(Math.floor(pixel*65536/total)+65536-tick)%65536;if(style===10)return samplePalette(palette,position);if(style===11)return samplePalette(palette,tick);if(style===12)return samplePalette(['#ff0000','#ffff00','#00ff00','#00ffff','#0000ff','#ff00ff'],position);if(style===13)return samplePalette(palette,position,false);return palette[paletteSlot(pixel,total,palette.length)]}
function applyRows(rows){for(const id of rowIds){const r=rows[id];document.getElementById('c_'+id).value=r.color;document.getElementById('n_'+id).value=r.colourCount||1;for(let n=1;n<12;n++)document.getElementById('c'+n+'_'+id).value=r.colours?.[n]||['#ff9e35','#ffce45','#2fe182','#9b5cff','#ff445c','#2878ff','#ffffff','#ff3366','#3edbd1','#f0df56','#704dff'][n-1];updatePaletteControls(id);document.getElementById('on_'+id).checked=r.enabled!==false;document.getElementById('b_'+id).value=r.brightness;document.getElementById('pe_'+id).checked=r.pulse;document.getElementById('pp_'+id).value=r.period;document.getElementById('ps_'+id).value=r.style;syncRowControls(id)}}
function updateUpdateButton(s=lastStatus){document.getElementById('openUpdate').classList.toggle('update-ready',Boolean(availableUpdate||s?.updateAvailable))}
function toast(message,error=false,sticky=false,key=''){if(!message)return;if(key)document.querySelector(`[data-toast-key="${key}"]`)?.remove();const box=document.createElement('div');box.className='toast'+(error?' error':'');if(key)box.dataset.toastKey=key;if(sticky)box.dataset.sticky='true';const close=document.createElement('button');close.type='button';close.textContent='×';close.setAttribute('aria-label',T[lang].close);close.onclick=()=>box.remove();box.append(close,document.createTextNode(message));document.getElementById('toasts').prepend(box);const removable=[...document.querySelectorAll('.toast:not([data-sticky="true"])')];while(document.querySelectorAll('.toast').length>4&&removable.length)removable.pop().remove();if(!sticky)setTimeout(()=>box.remove(),6500)}

document.getElementById('openNetwork').onclick=async()=>{document.getElementById('networkDialog').showModal();await refreshNetworkInfo();fillWifiIPForm();scanWifi()};
document.getElementById('openPrinter').onclick=()=>{document.getElementById('printerDialog').showModal();refreshNetworkInfo()};
document.getElementById('closePrinter').onclick=()=>document.getElementById('printerDialog').close();
document.getElementById('closeNetwork').onclick=()=>document.getElementById('networkDialog').close();
document.getElementById('openUpdate').onclick=()=>document.getElementById('updateDialog').showModal();
document.getElementById('closeUpdate').onclick=()=>document.getElementById('updateDialog').close();
document.getElementById('openFAQ').onclick=()=>document.getElementById('faqDialog').showModal();
document.getElementById('closeFAQ').onclick=()=>document.getElementById('faqDialog').close();
document.getElementById('openLang').onclick=()=>document.getElementById('languageDialog').showModal();
document.getElementById('closeLang').onclick=()=>document.getElementById('languageDialog').close();
for(const dialog of document.querySelectorAll('dialog')){
  dialog.addEventListener('click',e=>{
    if(window.matchMedia('(max-width:850px)').matches&&dialog.id!=='languageDialog')return;
    const r=dialog.getBoundingClientRect();
    if(e.target===dialog&&(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom))dialog.close();
  });
  dialog.addEventListener('cancel',e=>{if(window.matchMedia('(max-width:850px)').matches&&dialog.id!=='languageDialog')e.preventDefault()});
}
const appHeader=document.querySelector('.app-header'),previewCard=document.getElementById('previewCard');
function updateStickyLayout(){document.documentElement.style.setProperty('--header-stick-height',Math.ceil(appHeader.getBoundingClientRect().height+12)+'px')}
new ResizeObserver(updateStickyLayout).observe(appHeader);updateStickyLayout();
const lightingHeading=document.querySelector('.led-card > h2');
function updateMobileScrollLayout(){
  if(window.innerWidth<=850){
    previewCard.classList.toggle('preview-compact',window.scrollY>previewCard.offsetTop+70);
    // Use the header-visible preview edge for a stable threshold while the header moves.
    const previewEdge=appHeader.offsetHeight+12+previewCard.getBoundingClientRect().height;
    appHeader.classList.toggle('scroll-hidden',lightingHeading.getBoundingClientRect().top<=previewEdge);
  }else{
    appHeader.classList.remove('scroll-hidden');
    previewCard.classList.remove('preview-compact');
  }
}
window.addEventListener('scroll',updateMobileScrollLayout,{passive:true});
window.addEventListener('resize',()=>{updateStickyLayout();updateMobileScrollLayout()});
updateMobileScrollLayout();

const MAX_PREVIEW_LEDS=48;
function buildBar(total){
  const bar=document.getElementById('bar'),shown=Math.min(total,MAX_PREVIEW_LEDS);
  if(Number(bar.dataset.leds)===total)return shown;
  bar.replaceChildren();
  bar.dataset.leds=String(total);
  bar.dataset.previewLeds=String(shown);
  bar.dataset.dense=String(shown>24);
  bar.style.gridTemplateColumns='repeat('+Math.ceil(shown/2)+',minmax(0,1fr))';
  for(let i=0;i<shown;i+=2){
    const pair=document.createElement('div');pair.className='pair';
    for(let j=i;j<Math.min(i+2,shown);j++){
      const led=document.createElement('div');led.className='led';led.dataset.pixel=String(j);pair.append(led);
    }
    bar.append(pair);
  }
  return shown;
}

function renderConnectionStatus(info){const t=T[lang],put=(id,value,mode='')=>{const el=document.getElementById(id);el.textContent=value===undefined||value===null||value===''?'—':String(value);el.classList.toggle('link-ok',mode==='ok');el.classList.toggle('link-off',mode==='off')};const wifi=info.wifi||{},ap=info.ap||{},printer=info.printer||{};
put('linkWifi',wifi.connected?t.connectionOn:t.connectionOff,wifi.connected?'ok':'off');put('linkSSID',wifi.ssid);put('linkLocalIP',wifi.ip);const friendly=document.getElementById('linkFriendly');friendly.textContent='http://'+(info.webHost||'layerglow')+'.local/';friendly.href=friendly.textContent;put('linkGateway',wifi.gateway);
put('linkAP',ap.active?t.connectionActive:t.connectionInactive,ap.active?'ok':'off');put('linkAPSSID',ap.active?ap.ssid:'—');put('linkAPIP',ap.ip);put('linkClients',ap.active?ap.clients:'—');put('linkBrowser',window.location.host);
put('linkPrinter',printer.connected?t.connectionOn:t.connectionOff,printer.connected?'ok':'off');put('linkPrinterType',({moonraker:t.crealityK2,bambu:t.bambuA1,octoprint:t.octoType,prusalink:t.prusaType})[printer.type]||printer.type);put('linkPrinterAddress',printer.ip?printer.ip+':'+printer.port:'—');put('linkPrinterError',!printer.enabled?'—':printer.error?localError(printer.error):t.connectionNoError);put('printerReadState',printer.enabled?(printer.connected?(t[printer.state]||printer.state):t.connectionWaiting):'—');put('printerReadProgress',printer.connected?Math.round((printer.progress||0)*100)+'%':'—');put('printerReadFile',printer.connected?printer.filename:'—');put('printerReadAge',printer.connected&&printer.lastReadAgeSeconds>=0?printer.lastReadAgeSeconds+' '+t.secondsAgo:'—');document.getElementById('disconnectPrinter').disabled=!printer.enabled;renderSavedNetworks(info);renderSavedPrinters(info)}
let lastWifiScanNames=null;
function updateStaticFields(){const enabled=document.querySelector('input[name=ipMode]:checked').value==='static';document.getElementById('staticFields').hidden=!enabled;for(const id of ['staticIP','staticGateway','staticSubnet','staticDns'])document.getElementById(id).required=enabled}
function fillWifiIPForm(){const name=document.getElementById('ssid').value.trim(),profile=(lastNetworkInfo?.savedNetworks||[]).find(n=>n.ssid===name),wifi=lastNetworkInfo?.wifi||{},suggest=wifi.connected&&wifi.ssid===name;document.querySelector('input[name=ipMode][value='+((profile?.ipMode)==='static'?'static':'dhcp')+']').checked=true;for(const [id,key] of [['staticIP','ip'],['staticGateway','gateway'],['staticSubnet','subnet'],['staticDns','dns']])document.getElementById(id).value=profile?.ipMode==='static'?(profile[id==='staticIP'?'staticIP':key]||''):(suggest?(wifi[key]||''):'');updateStaticFields()}
document.getElementById('ssid').addEventListener('change',fillWifiIPForm);document.querySelectorAll('input[name=ipMode]').forEach(r=>r.addEventListener('change',updateStaticFields));
function renderSavedNetworks(info){const box=document.getElementById('savedWifiList'),list=info.savedNetworks||[],t=T[lang];box.replaceChildren();if(!list.length){const p=document.createElement('p');p.className='muted';p.textContent=t.savedWifiNone;box.append(p);return}for(const network of list){const row=document.createElement('div');row.className='saved-network';const label=document.createElement('div'),name=document.createElement('strong'),state=document.createElement('small'),controls=document.createElement('div');name.textContent=network.ssid;const availability=network.connected||lastWifiScanNames?.has(network.ssid)?t.savedWifiAvailable:lastWifiScanNames?t.savedWifiNotSeen:t.savedWifiUnknown;state.textContent=[network.connected?t.savedWifiConnected:network.active?t.savedWifiTrying:'',availability,network.ipMode==='static'?t.ipStatic:t.ipDhcp].filter(Boolean).join(' · ');label.append(name,state);controls.className='controls';const connect=document.createElement('button');connect.type='button';connect.className='alt';connect.textContent=t.savedWifiConnect;connect.disabled=network.connected;connect.onclick=()=>savedWifiAction('select',network.index);const remove=document.createElement('button');remove.type='button';remove.className='ghost';remove.textContent=t.savedWifiRemove;remove.onclick=()=>{if(confirm(t.savedWifiConfirm+' '+network.ssid))savedWifiAction('delete',network.index)};controls.append(connect,remove);row.append(label,controls);box.append(row)}}
async function savedWifiAction(action,index){const message=document.getElementById('savedWifiMessage');try{const response=await fetch('/api/wifi/'+action,{method:'POST',body:new URLSearchParams({index:String(index)})});if(!response.ok)throw Error(await response.text());message.textContent=action==='select'?T[lang].networkRestart:T[lang].savedWifiRemoved;await refreshNetworkInfo()}catch(e){message.textContent=localError(e.message);toast(message.textContent,true)}}

let savedPrinters=[],printerProfileBusy=false;
function fillPrinterForm(){
  document.getElementById('profileId').value='';document.getElementById('printerName').value='';
  document.getElementById('printerType').value='moonraker';printerTypeChanged(true);
  document.getElementById('ip').value='';document.getElementById('port').value=7125;
  document.getElementById('bambuSerial').value='';document.getElementById('bambuCode').value='';
  document.getElementById('apiKey').value='';requiredPrinterCredentials();
}
function renderSavedPrinters(info){
  savedPrinters=info.savedPrinters||[];const box=document.getElementById('savedPrinterList'),t=T[lang];box.replaceChildren();
  if(info.printerProfileStorageOK===false)document.getElementById('savedPrinterMessage').textContent=t.savedPrintersError;
  if(!savedPrinters.length){const p=document.createElement('p');p.className='muted';p.textContent=t.savedPrintersNone;box.append(p)}
  const typeLabel=p=>({moonraker:t.crealityK2,bambu:t.bambuA1,octoprint:t.octoType,prusalink:t.prusaType})[p.type]||p.type;
  for(const p of savedPrinters){
    const row=document.createElement('div');row.className='saved-printer'+(p.active?' active':'');row.dataset.profileId=p.id;
    const title=document.createElement('strong');title.textContent=p.name;row.append(title);
    const detail=text=>{const el=document.createElement('p');el.className='muted';el.textContent=text;row.append(el)};
    detail(typeLabel(p)+' · '+p.ip+':'+p.port);
    if(p.mac)detail(t.macLabel+': '+p.mac);
    if(p.serial)detail(t.printerSerialLabel+': '+p.serial);
    if(p.network)detail(t.printerSavedNetwork+': '+p.network);
    if(p.active&&p.enabled)detail(p.connected?t.connected:t.printerSelectedWaiting);
    const controls=document.createElement('div');controls.className='controls';
    for(const [text,action] of [[t.savedWifiConnect,'select'],[t.savedWifiRemove,'delete']]){
      const b=document.createElement('button');b.type='button';b.className=action==='select'?'alt':'ghost';b.textContent=text;b.dataset.action=action;
      b.disabled=printerProfileBusy||(action==='select'&&p.active&&p.enabled);
      b.onclick=()=>{if(action!=='delete'||confirm(t.deletePrinterConfirm+' '+p.name))savedPrinterAction(action,p.id)};
      controls.append(b);
    }row.append(controls);box.append(row);
  }
}
async function refreshSavedPrinters(){const info=await api('/api/printers/saved',{cache:'no-store'});renderSavedPrinters(info)}
async function savedPrinterAction(action,id){
  if(printerProfileBusy)return;printerProfileBusy=true;
  try{
    const response=await fetch('/api/printers/'+action,{method:'POST',body:new URLSearchParams({id}),signal:AbortSignal.timeout(10000)});if(!response.ok)throw Error(await response.text());
    await refreshSavedPrinters();
    await refresh();await refreshNetworkInfo();toast(action==='delete'?T[lang].printerRemoved:T[lang].printerSaved);
  }catch(e){toast(localError(e.message),true)}finally{printerProfileBusy=false;if(lastNetworkInfo)renderSavedPrinters(lastNetworkInfo)}
}

async function refreshNetworkInfo(){if(networkInfoBusy)return;networkInfoBusy=true;try{const info=await api('/api/network/info',{cache:'no-store',signal:AbortSignal.timeout(3500)});lastNetworkInfo=info;renderConnectionStatus(info);document.getElementById('connectionInfoMessage').textContent=''}catch(_){document.getElementById('connectionInfoMessage').textContent=T[lang].connectionUnavailable}finally{networkInfoBusy=false}}
setInterval(()=>{if(document.getElementById('networkDialog').open||document.getElementById('printerDialog').open)refreshNetworkInfo()},3000);
async function api(path,options={}){const r=await fetch(path,{...options,signal:options.signal||AbortSignal.timeout(10000)});if(!r.ok)throw Error(await r.text());return r.json()}
const PREVIEW_SCALE_TEXT={
  en:(shown,total)=>shown+' preview lights represent '+total+' physical LEDs.',
  pl:(shown,total)=>shown+' pól podglądu reprezentuje '+total+' fizycznych diod LED.',
  es:(shown,total)=>shown+' puntos de vista previa representan '+total+' LED físicos.',
  de:(shown,total)=>shown+' Vorschau-Felder stehen für '+total+' physische LEDs.'
};
function paintStatus(s){document.getElementById('testStatus').classList.toggle('live-reading',document.getElementById('testStatus').value==='printer');if(!s||!s.rows)return;const live=!s.sim&&s.connected&&s.printerEnabled,state=s.sim?s.simState:(live?s.state:'standby'),id=state==='printing'?'progress':state,simPhase=s.sim&&state==='printing'&&Number.isFinite(s.simCycleElapsed)?(s.simCycleElapsed+Math.max(0,performance.now()-s.simSnapshotAt))%11000:null,prog=s.sim?(simPhase===null?s.simProgress:Math.min(100,Math.floor(simPhase/500)*5)):(live?s.progress*100:0),pairs=Math.ceil(s.ledCount/2),r=s.rows[id];if(!r)return;let count=state==='printing'?Math.ceil(prog*pairs/100):(state==='standby'&&!r.pulse&&Number(document.getElementById('ps_'+id)?.value??r.style)<6?0:pairs);const enabled=document.getElementById('on_'+id)?.checked??(r.enabled!==false);if(!enabled)count=0;const previewTotal=buildBar(s.ledCount),previewPairs=Math.ceil(previewTotal/2),shownCount=count?Math.min(previewPairs,Math.ceil(count*previewPairs/pairs)):0;const scaleHint=document.getElementById('previewScale');scaleHint.hidden=s.ledCount<=MAX_PREVIEW_LEDS;if(!scaleHint.hidden)scaleHint.textContent=PREVIEW_SCALE_TEXT[lang](previewTotal,s.ledCount);const palette=paletteFor(id,r),bright=Number(document.getElementById('b_'+id)?.value??r.brightness)/100,pulse=document.getElementById('pe_'+id)?.checked??r.pulse,period=Number(document.getElementById('pp_'+id)?.value??r.period),style=Number(document.getElementById('ps_'+id)?.value??r.style);document.getElementById('percent').textContent=Math.round(prog)+'%';document.getElementById('ledcount').textContent=Math.min(count*2,s.ledCount)+' / '+s.ledCount+' LED';document.getElementById('displaystate').textContent=live||s.sim?(s.sim?T[lang].test+' · ':'')+(T[lang][state]||state):(s.printerEnabled?T[lang].offline:'—');document.getElementById('mobilePct').textContent=live||s.sim?Math.round(prog)+'%':'—';document.getElementById('previewProgressLabel').textContent=s.sim?T[lang].testProgress:T[lang].printProgress;document.getElementById('mobileState').textContent=live||s.sim?(T[lang][state]||state):(s.printerEnabled?T[lang].offline:'—');document.getElementById('mobileLedCount').textContent=Math.min(count*2,s.ledCount)+'/'+s.ledCount+' LED';document.getElementById('file').textContent=live?s.filename||'':'';document.getElementById('file').style.display=live&&s.filename?'':'none';const randomNow=Date.now(),randomKey=id+'/'+shownCount+'/'+style+'/'+previewTotal;if(randomPreview.key!==randomKey){randomPreview.key=randomKey;randomPreview.on=[];randomPreview.next=[]}const randomBase=Math.max(55,Math.min(220,Math.round(period/12)));const cycle=Math.max(200,period||1400),phase=(randomNow%cycle)/cycle,wave=(1-Math.cos(2*Math.PI*phase))/2,scale=.25+.75*wave,travel=phase*2*Math.max(0,shownCount-1),head=travel<=shownCount-1?travel:2*(shownCount-1)-travel,centre=(shownCount-1)/2,radius=Math.max(.5,centre),outward=1-Math.abs(2*phase-1),front=outward*radius;[...document.querySelectorAll('.pair')].forEach((p,i)=>{if(style===6&&i<shownCount){if(randomPreview.next[i]===undefined)randomPreview.next[i]=randomNow+Math.random()*randomBase;if(randomNow>=randomPreview.next[i]){randomPreview.on[i]=!randomPreview.on[i];randomPreview.next[i]=randomNow+randomBase*(.5+Math.random()*1.5)}}[...p.children].forEach(led=>{const physicalPixel=Math.min(s.ledCount-1,Math.floor((Number(led.dataset.pixel)+.5)*s.ledCount/previewTotal));const color=effectColour(palette,physicalPixel,s.ledCount,style,Math.floor((randomNow%cycle)*65536/cycle));let lit=i<shownCount&&(style!==6||!!randomPreview.on[i]);let level=1;if(pulse&&lit&&period>0){if(style===2)level=Math.max(.08,1-Math.abs(i-head)/2.2);else if(style===3)level=Math.max(.08,1-Math.abs(Math.abs(i-centre)-front)/1.3);else if(style===5)level=.16+.84*(.5+.5*Math.cos(2*Math.PI*(i/Math.max(1,shownCount-1)-phase)));else if(style===0||style>=10||(style===1&&i===shownCount-1))level=scale}if(lit&&style===7){const trail=((phase*shownCount-i)%shownCount+shownCount)%shownCount;level=Math.max(0,1-trail/3.2);lit=level>.02}else if(lit&&style===8){const step=Math.floor(phase*2*shownCount);lit=step<shownCount?i<=step:i>step-shownCount}else if(lit&&style===9){lit=i%2===Math.floor(phase*2)}led.style.background=lit?color:'#e3ebe5';led.style.opacity=lit?Math.max(.05,bright*level):1;led.style.boxShadow=lit?'0 0 7px '+color+'40':'none'})})}

let discoveredPrinters=[],discoveryBusy=false;
function requiredPrinterCredentials(){const type=document.getElementById('printerType').value;document.getElementById('bambuCode').required=type==='bambu';document.getElementById('apiKey').required=type==='octoprint'||type==='prusalink'}

async function discoverPrinters(){if(discoveryBusy)return;discoveryBusy=true;const button=document.getElementById('findPrinters'),list=document.getElementById('foundPrinters'),msg=document.getElementById('discoveryMessage'),bar=document.getElementById('discoveryBar'),pct=document.getElementById('discoveryPercent');document.getElementById('discoveryProgress').hidden=false;bar.value=0;pct.textContent='0%';button.disabled=true;msg.textContent=T[lang].discoverySearching;try{let result=await api('/api/printers/discovery',{method:'POST',signal:AbortSignal.timeout(4000)});for(let i=0;result.searching&&i<180;i++){const done=result.phase===1&&result.total?Math.min(99,Math.floor(100*result.checked/result.total)):0;bar.value=done;pct.textContent=done+'%';msg.textContent=T[lang].discoverySearching;await new Promise(resolve=>setTimeout(resolve,800));result=await api('/api/printers/discovery',{signal:AbortSignal.timeout(4000)})}if(result.searching)throw Error('timeout');bar.value=100;pct.textContent='100%';discoveredPrinters=result.printers||[];list.replaceChildren(new Option(discoveredPrinters.length?T[lang].discoverySelect:T[lang].discoveryNone,''));discoveredPrinters.forEach((p,i)=>list.add(new Option((p.name||p.type)+' · '+p.ip+(p.port&&p.type!=='bambu'?':'+p.port:'')+' ('+p.type+')',String(i))));msg.textContent=discoveredPrinters.length?T[lang].discoveryReady+discoveredPrinters.length:T[lang].discoveryNone}catch(err){msg.textContent=err.message.includes('Wi-Fi')?T[lang].discoveryUnavailable:T[lang].discoveryError}finally{button.disabled=false;discoveryBusy=false;button.textContent=T[lang].rescanPrinters}}
document.getElementById('findPrinters').onclick=discoverPrinters;
document.getElementById('foundPrinters').onchange=e=>{const p=discoveredPrinters[Number(e.target.value)];if(!e.target.value||!p)return;fillPrinterForm();document.getElementById('printerName').value=p.name||'';document.getElementById('printerType').value=p.type;printerTypeChanged();document.getElementById('ip').value=p.ip;if(p.type!=='bambu')document.getElementById('port').value=p.port;if(p.type==='bambu')document.getElementById('bambuSerial').value=p.serial||'';requiredPrinterCredentials();document.getElementById('discoveryMessage').textContent=p.type==='bambu'||p.type==='octoprint'||p.type==='prusalink'?T[lang].discoveryCredentials:T[lang].discoveryHint+' '+T[lang].selectThenSave};
document.getElementById('ip').addEventListener('input',requiredPrinterCredentials);document.getElementById('bambuSerial').addEventListener('input',requiredPrinterCredentials);
const printerIPs={moonraker:'',bambu:'',octoprint:'',prusalink:''},savedPrinterIPs={moonraker:'',bambu:'',octoprint:'',prusalink:''},printerPorts={moonraker:7125,octoprint:80,prusalink:80};let printerTypeShown='moonraker';
function printerTypeChanged(initial=false){const type=document.getElementById('printerType').value;if(!initial){printerIPs[printerTypeShown]=document.getElementById('ip').value;if(printerTypeShown!=='bambu')printerPorts[printerTypeShown]=Number(document.getElementById('port').value)||80}printerTypeShown=type;document.getElementById('ip').value=printerIPs[type]||'';document.getElementById('portFields').style.display=type==='bambu'?'none':'';document.getElementById('port').required=type!=='bambu';document.getElementById('port').value=printerPorts[type]||80;document.getElementById('bambuFields').style.display=type==='bambu'?'':'none';document.getElementById('apiKeyFields').style.display=(type==='octoprint'||type==='prusalink')?'':'none';document.getElementById('bambuSerial').required=type==='bambu';document.getElementById('bambuCode').required=type==='bambu'&&!(lastStatus&&lastStatus.hasBambuCode);document.getElementById('apiKey').required=(type==='octoprint'&&!lastStatus?.hasOctoKey)||(type==='prusalink'&&!lastStatus?.hasPrusaKey);document.getElementById('apiKey').value='';requiredPrinterCredentials()}
const randomPreview={key:"",on:[],next:[]};
let lastConnectionKey=null;
async function refresh(){if(refreshing||updatingFirmware)return;refreshing=true;try{const s=await api('/api/status',{cache:'no-store',signal:AbortSignal.timeout(3500)});lastStatus=s;if(!customColourSavePending)showSavedCustomColour(s.recentColours);updateUpdateButton(s);if(!initialized){for(const [id,v] of Object.entries({ssid:s.ssid,ip:s.ip,port:s.port,ledCount:s.ledCount}))document.getElementById(id).value=v;applyRows(s.rows);document.getElementById('webHost').value=s.webHost||'layerglow';printerIPs.moonraker=s.moonIP||'';printerIPs.bambu=s.bambuIP||'';printerIPs.octoprint=s.octoIP||'';printerIPs.prusalink=s.prusaIP||'';Object.assign(savedPrinterIPs,printerIPs);printerPorts.moonraker=s.moonPort||7125;printerPorts.octoprint=s.octoPort||80;printerPorts.prusalink=s.prusaPort||80;printerTypeShown=s.printerType||'moonraker';document.getElementById('printerType').value=printerTypeShown;printerTypeChanged(true);fillPrinterForm();initialized=true}requiredPrinterCredentials();if(!simBusy)document.getElementById('testStatus').value=s.sim?s.simState:'printer';s.simSnapshotAt=performance.now();paintStatus(s);document.getElementById('restoreLights').disabled=!s.lightingBackup;const key=[s.printerEnabled,s.ap,s.connected,s.connected?'':s.error].join('|');if(key!==lastConnectionKey){lastConnectionKey=key;if(!s.printerEnabled)document.querySelector('[data-toast-key="connection"]')?.remove();else toast(s.connected?T[lang].connected:(s.ap?T[lang].setupOffline:T[lang].offline+(s.error&&s.error!=='No printer connection (setup AP)'?' · '+localError(s.error):'')),!s.connected,!s.connected,'connection')}}catch(e){if(lastConnectionKey!=='device-offline'){lastConnectionKey='device-offline';toast(T[lang].deviceOffline,true,true,'connection')}}finally{refreshing=false}}

function localError(message){const profileErrors={"Invalid saved printer settings":"invalidSavedPrinter","Saved printer not found":"savedPrinterMissing","Saved printer limit reached":"savedPrinterLimit","Could not load saved printers":"savedPrintersError"};if(profileErrors[message])return T[lang][profileErrors[message]];if(message==='Could not save settings')return T[lang].settingsSaveError;if(message==='Could not restore LED settings')return T[lang].settingsRestoreError;if(message==='Invalid device name')return T[lang].nameInvalid;if(message==='Saved Wi-Fi list is full')return T[lang].savedWifiFull;if(message==='Invalid saved network')return T[lang].savedWifiInvalid;if(message==='No printer connection (setup AP)')return T[lang].setupOffline;if(message==='Printer disconnected')return T[lang].printerDisabled;if(message==='Waiting for first poll')return T[lang].connectionWaiting;if(message==='Invalid Bambu serial or access code')return T[lang].invalidBambu;if(message==='Bambu access denied')return T[lang].bambuDenied;if(message==='Bambu MQTT offline'||message==='Bambu settings missing')return T[lang].bambuOffline;if(message==='Bambu status timeout')return T[lang].bambuWait;if(message==='Invalid Moonraker URL')return T[lang].invalidMoonraker;if(message==='Missing printer fields')return T[lang].missingFields;if(message==='Invalid IP, port or SSID')return T[lang].invalidNetwork;if(message==='Invalid LED count')return T[lang].invalidLedCount;if(message.startsWith('Invalid LED row:'))return T[lang].invalidLedRow;if(message.includes('Not Enough Space'))return T[lang].noSpace;if(message.startsWith('JSON:'))return 'JSON: '+message.slice(5);return message}
async function postForm(path,form){const r=await fetch(path,{method:'POST',body:new URLSearchParams(new FormData(form)),signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error(await r.text());return r.text()}
document.getElementById('wifiForm').onsubmit=async e=>{e.preventDefault();try{const host=document.getElementById('webHost').value.trim().toLowerCase().replace(/\.local\.?$/,'');document.getElementById('webHost').value=host;if(document.querySelector('input[name=ipMode]:checked').value==='static'&&!confirm(T[lang].staticConfirm))return;const msg=await postForm('/api/wifi',e.target),label=msg.startsWith('Switching')?T[lang].networkRestart:T[lang].networkSaved;const output=document.getElementById('wifiMessage');output.replaceChildren(document.createTextNode(label+' · '));const link=document.createElement('a');link.href='http://'+host+'.local/';link.textContent=T[lang].openNewAddress+' ('+host+'.local)';output.append(link);document.getElementById('pass').value='';refreshNetworkInfo();toast(label)}catch(err){document.getElementById('wifiMessage').textContent=localError(err.message);toast(localError(err.message),true)}};
document.getElementById('disconnectPrinter').onclick=async()=>{try{await fetch('/api/printer/disconnect',{method:'POST'}).then(r=>{if(!r.ok)throw Error('HTTP '+r.status)});await refresh();await refreshNetworkInfo();document.getElementById('printerMessage').textContent=''}catch(e){toast(localError(e.message),true)}};
document.getElementById('printerForm').onsubmit=async e=>{e.preventDefault();if(printerProfileBusy)return;printerProfileBusy=true;const button=e.target.querySelector('button[type=submit]');button.disabled=true;try{await postForm('/api/printer',e.target);await refreshSavedPrinters();fillPrinterForm();await refresh();await refreshNetworkInfo();document.getElementById('printerMessage').textContent=T[lang].printerSaved;toast(T[lang].printerSaved)}catch(err){document.getElementById('printerMessage').textContent=localError(err.message);toast(localError(err.message),true)}finally{printerProfileBusy=false;button.disabled=false;if(lastNetworkInfo)renderSavedPrinters(lastNetworkInfo)}};

let lightTimer,lightSaveChain=Promise.resolve(),resettingColors=false;
const lightForm=document.getElementById('lightForm'),lightMessage=document.getElementById('lightMessage');
function scheduleLightSave(){if(resettingColors)return;clearTimeout(lightTimer);lightTimer=setTimeout(saveLightSettings,450)}
function saveLightSettings(){if(resettingColors||!lightForm.checkValidity())return;clearTimeout(lightTimer);lightMessage.textContent=T[lang].saving;lightSaveChain=lightSaveChain.catch(()=>{}).then(async()=>{if(resettingColors)return;try{await postForm('/api/lighting',lightForm);lightMessage.textContent=T[lang].saved;toast(T[lang].saved);await refresh()}catch(err){lightMessage.textContent=localError(err.message);toast(localError(err.message),true)}});return lightSaveChain}
lightForm.addEventListener('input',e=>{
  const id=e.target.id.split('_').slice(1).join('_');
  if(rowIds.includes(id)&&['pp','b','pe','ps'].includes(e.target.id.split('_')[0])){
    if(e.target.id.startsWith('pp_'))document.getElementById('pe_'+id).checked=Number(e.target.value)>0;
    if(e.target.id.startsWith('pe_')&&e.target.checked&&Number(document.getElementById('pp_'+id).value)===0)document.getElementById('pp_'+id).value=1400;
    syncRowControls(id);
  }
  if(e.target.type!=='color')scheduleLightSave();
});
lightForm.addEventListener('change',e=>{
  const id=e.target.id.split('_').slice(1).join('_');
  if(rowIds.includes(id))syncRowControls(id);
  if(e.target.type==='color')rememberCustomColour(e.target.value);
  scheduleLightSave();
  if(window.matchMedia('(max-width:850px)').matches&&e.target.matches('.brightness-control input[type=range], .pulse-control input[type=range]')){
    const details=e.target.closest('details');
    clearTimeout(details.closeTimer);
    details.closeTimer=setTimeout(()=>{details.open=false},800);
  }
});
lightForm.onsubmit=e=>{e.preventDefault();saveLightSettings()};
document.getElementById('resetColors').onclick=async()=>{clearTimeout(lightTimer);resettingColors=true;const button=document.getElementById('resetColors');button.disabled=true;try{await lightSaveChain.catch(()=>{});const settings=await api('/api/lighting/defaults',{method:'POST'});document.getElementById('ledCount').value=settings.ledCount;applyRows(settings.rows);lastStatus={...lastStatus,...settings};paintStatus(lastStatus);lightMessage.textContent=T[lang].saved;toast(T[lang].saved)}catch(err){lightMessage.textContent=localError(err.message);toast(localError(err.message),true)}finally{button.disabled=false;resettingColors=false}};

document.getElementById('backupLights').onclick=async()=>{const button=document.getElementById('backupLights');button.disabled=true;clearTimeout(lightTimer);try{if(!lightForm.checkValidity()){lightForm.reportValidity();return}await lightSaveChain.catch(()=>{});await postForm('/api/lighting',lightForm);const result=await api('/api/lighting/backup',{method:'POST'});document.getElementById('restoreLights').disabled=!result.lightingBackup;lightMessage.textContent=T[lang].backupSaved;toast(T[lang].backupSaved)}catch(err){lightMessage.textContent=localError(err.message);toast(localError(err.message),true)}finally{button.disabled=false}};
document.getElementById('restoreLights').onclick=async()=>{clearTimeout(lightTimer);resettingColors=true;const button=document.getElementById('restoreLights');button.disabled=true;try{await lightSaveChain.catch(()=>{});const settings=await api('/api/lighting/restore',{method:'POST'});document.getElementById('ledCount').value=settings.ledCount;applyRows(settings.rows);lastStatus={...lastStatus,...settings};paintStatus(lastStatus);lightMessage.textContent=T[lang].backupRestored;toast(T[lang].backupRestored)}catch(err){lightMessage.textContent=localError(err.message);toast(localError(err.message),true)}finally{resettingColors=false;button.disabled=!(lastStatus&&lastStatus.lightingBackup)}};

async function scanWifi(){const list=document.getElementById('wifiList'),button=document.getElementById('scanWifiButton');button.disabled=true;list.replaceChildren(new Option(T[lang].scanning,''));try{let result;for(let i=0;i<24;i++){result=await api('/api/wifi/scan');if(!result.scanning)break;await new Promise(resolve=>setTimeout(resolve,650))}if(result.scanning)throw Error('scan timeout');lastWifiScanNames=new Set(result.networks.map(n=>n.ssid));if(lastNetworkInfo)renderSavedNetworks(lastNetworkInfo);list.replaceChildren(new Option(result.networks.length?'—':T[lang].scanNone,''));result.networks.forEach(n=>list.add(new Option(n.ssid+' ('+n.rssi+' dBm) '+(n.secure?'🔒':''),n.ssid)))}catch(e){lastWifiScanNames=null;if(lastNetworkInfo)renderSavedNetworks(lastNetworkInfo);list.replaceChildren(new Option(T[lang].scanError,''));toast(T[lang].scanError,true)}finally{button.disabled=false}}
async function sendSimulation(){simQueued=true;if(simBusy)return;simBusy=true;try{while(simQueued){simQueued=false;const selected=document.getElementById('testStatus').value;const p=new URLSearchParams({enabled:selected==='printer'?'0':'1',status:selected==='printer'?'standby':selected});await api('/api/simulation',{method:'POST',body:p});document.getElementById('simMessage').textContent='';await refresh()}}catch(e){document.getElementById('simMessage').textContent=e.message}finally{simBusy=false}}
function testStatusChanged(){sendSimulation()}
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function firmwareRestarted(before,after,expectedVersion=null){
  if(expectedVersion&&after.version!==expectedVersion)return false;
  if(before.bootId&&after.bootId)return before.bootId!==after.bootId;
  // Compatibility with older firmware that has no per-boot ID.
  return after.version!==before.version||Number.isFinite(before.uptimeSeconds)&&Number.isFinite(after.uptimeSeconds)&&after.uptimeSeconds<before.uptimeSeconds;
}
async function waitForRestart(before,expectedVersion=null){
  let consecutive=0;const began=Date.now();await sleep(1200);
  while(Date.now()-began<180000){
    try{
      const status=await api('/api/health?probe='+Date.now(),{cache:'no-store',signal:AbortSignal.timeout(5000)});
      if(firmwareRestarted(before,status,expectedVersion)){if(++consecutive>=2){location.replace('/?updated='+Date.now());return}}else consecutive=0;
    }catch(_){consecutive=0}
    await sleep(1800);
  }
  throw Error(T[lang].retry);
}
// SHA-256 works on the local HTTP panel too, where SubtleCrypto may be unavailable.
function sha256(bytes){
  const primes=[];for(let n=2;primes.length<64;n++){let prime=true;for(let d=2;d*d<=n;d++)if(n%d===0){prime=false;break}if(prime)primes.push(n)}
  const frac=x=>Math.floor((x-Math.floor(x))*4294967296)>>>0;
  const k=primes.map(x=>frac(Math.cbrt(x))),h=primes.slice(0,8).map(x=>frac(Math.sqrt(x)));
  const size=bytes.length,pad=((size+9+63)>>6)<<6,data=new Uint8Array(pad);data.set(bytes);data[size]=128;
  const bitLen=size*8;for(let i=0;i<8;i++)data[pad-1-i]=Math.floor(bitLen/2**(i*8))&255;
  const rr=(v,n)=>(v>>>n)|(v<<(32-n)),w=new Uint32Array(64);
  for(let offset=0;offset<pad;offset+=64){
    for(let i=0;i<16;i++){let j=offset+i*4;w[i]=(data[j]<<24)|(data[j+1]<<16)|(data[j+2]<<8)|data[j+3]}
    for(let i=16;i<64;i++){let a=w[i-15],b=w[i-2];w[i]=(w[i-16]+(rr(a,7)^rr(a,18)^(a>>>3))+w[i-7]+(rr(b,17)^rr(b,19)^(b>>>10)))|0}
    let [a,b,c,d,e,f,g,q]=h;
    for(let i=0;i<64;i++){let t1=(q+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i])|0,t2=((rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c)))|0;q=g;g=f;f=e;e=(d+t1)|0;d=c;c=b;b=a;a=(t1+t2)|0}
    [a,b,c,d,e,f,g,q].forEach((v,i)=>h[i]=(h[i]+v)|0)
  }
  return h.map(x=>(x>>>0).toString(16).padStart(8,'0')).join('')
}
const versionParts=v=>v.split('.').map(Number);
function newer(a,b){const x=versionParts(a),y=versionParts(b);for(let i=0;i<3;i++){if((x[i]||0)>(y[i]||0))return true;if((x[i]||0)<(y[i]||0))return false}return false}
function sha1Blob(bytes){
  const prefix=new TextEncoder().encode('blob '+bytes.length+'\0'),raw=new Uint8Array(prefix.length+bytes.length);
  raw.set(prefix);raw.set(bytes,prefix.length);
  const length=raw.length,padded=((length+9+63)>>6)<<6,data=new Uint8Array(padded);data.set(raw);data[length]=128;
  const bitLength=length*8;for(let i=0;i<8;i++)data[padded-1-i]=Math.floor(bitLength/2**(i*8))&255;
  const rol=(v,n)=>(v<<n)|(v>>>(32-n)),h=[0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0],w=new Uint32Array(80);
  for(let offset=0;offset<padded;offset+=64){
    for(let i=0;i<16;i++){const j=offset+i*4;w[i]=(data[j]<<24)|(data[j+1]<<16)|(data[j+2]<<8)|data[j+3]}
    for(let i=16;i<80;i++)w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    let [a,b,c,d,e]=h;
    for(let i=0;i<80;i++){
      const f=i<20?(b&c)|(~b&d):i<40?b^c^d:i<60?(b&c)|(b&d)|(c&d):b^c^d;
      const k=i<20?0x5a827999:i<40?0x6ed9eba1:i<60?0x8f1bbcdc:0xca62c1d6;
      const t=(rol(a,5)+f+e+k+w[i])|0;e=d;d=c;c=rol(b,30);b=a;a=t
    }
    [a,b,c,d,e].forEach((v,i)=>h[i]=(h[i]+v)|0)
  }
  return h.map(x=>(x>>>0).toString(16).padStart(8,'0')).join('')
}
async function fetchUpdateInfo(){
  const response=await fetch(GITHUB_MANIFEST+'?t='+Date.now(),{cache:'no-store',signal:AbortSignal.timeout(20000)});
  if(!response.ok)throw Error('HTTP '+response.status+' · '+T[lang].githubNetwork);
  const m=await response.json();
  if(!/^\d+\.\d+(?:\.\d+)?$/.test(m.version)||!/^LayerGlow_v[0-9_]+_OTA\.bin$/.test(m.file)||!Number.isInteger(m.size)||m.size<100000||m.size>1966080||!(/^[a-f0-9]{64}$/i.test(m.sha256))||!(/^[A-Za-z0-9+/]{342}==$/.test(m.signature||'')))throw Error(T[lang].githubFormat);
  return m
}
async function checkGitHub(silent=false){
  if(updatingFirmware||silent&&lastStatus?.ap)return;
  const msg=document.getElementById('githubMessage'),button=document.getElementById('checkGitHub'),install=document.getElementById('installGitHub');
  if(!silent)msg.textContent=T[lang].checkingUpdates;
  button.disabled=true;
  try{
    const m=await fetchUpdateInfo(),previous=availableUpdate?.version;
    availableUpdate=newer(m.version,lastStatus.version||'0.0')?m:null;updateUpdateButton();
    install.hidden=!availableUpdate;
    msg.textContent=availableUpdate?T[lang].newVersion+' '+m.version:T[lang].noUpdate;
    fetch('/api/update/available',{method:'POST',body:new URLSearchParams({available:availableUpdate?'1':'0'})}).catch(()=>{});
    if(availableUpdate&&previous!==m.version)toast(msg.textContent);
  }catch(e){availableUpdate=null;updateUpdateButton();install.hidden=true;msg.textContent=e instanceof TypeError?T[lang].githubNetwork:e.message||T[lang].githubUnavailable;if(!silent)toast(msg.textContent,true)}
  finally{button.disabled=false}
}
async function uploadFirmware(file,button,expectedVersion=null,signature=null){
  if(!signature||!(/^[A-Za-z0-9+/]{342}==$/.test(signature)))throw Error(T[lang].signatureRequired);
  button.disabled=true;updatingFirmware=true;
  const msg=document.getElementById('fwMessage'),began=Date.now();
  const ticker=setInterval(()=>{if(!msg.dataset.uploading)msg.textContent=T[lang].waiting+' '+Math.floor((Date.now()-began)/1000)+' s'},1000);
  try{
    const head=new Uint8Array(await file.slice(0,1).arrayBuffer());
    if(head[0]!==0xe9)throw Error(T[lang].invalidFirmware);
    const capacity=await api('/api/ota/capacity');
    const before=await api('/api/health',{cache:'no-store'});
    if(!capacity.appSlotBytes||file.size>capacity.appSlotBytes)throw Error(T[lang].noSpace+' ('+file.size+' / '+capacity.appSlotBytes+' bytes)');
    const f=new FormData();f.append('firmware',file,file.name);msg.dataset.uploading='1';msg.textContent=T[lang].restarting+' 0%';

    try{await new Promise((resolve,reject)=>{
      const xhr=new XMLHttpRequest();xhr.open('POST','/api/update');xhr.setRequestHeader('X-LayerGlow-Signature',signature);
      xhr.upload.onprogress=e=>{if(e.lengthComputable)msg.textContent=T[lang].restarting+' '+Math.round(e.loaded*100/e.total)+'%'};
      xhr.onload=()=>xhr.status===200?resolve():reject(Error(xhr.responseText||'HTTP '+xhr.status));
      xhr.timeout=180000;xhr.ontimeout=()=>reject(new TypeError('Upload timed out'));xhr.onerror=()=>reject(new TypeError('Device restarting'));xhr.send(f);
    })}catch(err){if(!(err instanceof TypeError))throw err}
    delete msg.dataset.uploading;
    await waitForRestart(before,expectedVersion);
  }catch(err){msg.textContent=T[lang].updateFailed+': '+localError(err.message);toast(msg.textContent,true)}
  finally{delete msg.dataset.uploading;clearInterval(ticker);button.disabled=false;updatingFirmware=false}
}
document.getElementById('checkGitHub').onclick=()=>checkGitHub();
document.getElementById('installGitHub').onclick=async e=>{
  const m=availableUpdate;if(!m||!confirm(T[lang].githubConfirm.replace('{version}',m.version)))return;
  const button=e.target,msg=document.getElementById('githubMessage');button.disabled=true;
  try{
    msg.textContent=T[lang].downloadFirmware;
    const url='https://raw.githubusercontent.com/TomaszK00/LayerGlow/main/Software/'+m.file;
    const response=await fetch(url+'?t='+Date.now(),{cache:'no-store',signal:AbortSignal.timeout(60000)});
    if(!response.ok)throw Error('HTTP '+response.status);
    const bytes=new Uint8Array(await response.arrayBuffer());
    if(bytes.length!==m.size||bytes[0]!==0xe9||(m.sha256?sha256(bytes)!==m.sha256.toLowerCase():sha1Blob(bytes)!==m.sha))throw Error(T[lang].invalidFirmware);
    await uploadFirmware(new File([bytes],m.file,{type:'application/octet-stream'}),button,m.version,m.signature);
  }catch(err){msg.textContent=err.message;toast(msg.textContent,true)}
  finally{button.disabled=false}
};
document.getElementById('fwform').onsubmit=async e=>{
  e.preventDefault();const file=document.getElementById('bin').files[0];
  if(!file||!file.name.toLowerCase().endsWith('.bin')){document.getElementById('fwMessage').textContent=T[lang].chooseBin;return}
  const sigFile=document.getElementById('sig').files[0];
  if(!sigFile||!sigFile.name.toLowerCase().endsWith('.sig')){document.getElementById('fwMessage').textContent=T[lang].signatureRequired;return}
  const signature=(await sigFile.text()).trim();
  if(confirm(T[lang].uploadConfirm))await uploadFirmware(file,e.target.querySelector('button'),null,signature);
};

setLang(T[localStorage.getItem('k2lang')]?localStorage.getItem('k2lang'):'en');refresh().then(()=>setTimeout(()=>checkGitHub(true),2500));setInterval(refresh,2000);setInterval(()=>checkGitHub(true),30*60*1000);setInterval(()=>paintStatus(lastStatus),120);
</script></body></html>
)HTML";

String normalizeColor(String c, const String& fallback) {
  if(c.length()!=7 || c[0]!='#')return fallback;
  for(int i=1;i<7;i++)if(!isxdigit((unsigned char)c[i]))return fallback;
  c.toLowerCase();return c;
}
void loadRememberedColours(){
  Preferences saved;
  if(!saved.begin("lgcolour",true))return;
  String data=saved.getString("recent","");
  saved.end();
  rememberedColourCount=0;
  int pos=0;
  while(pos<data.length()&&rememberedColourCount<8){
    int end=data.indexOf(',',pos);
    if(end<0)end=data.length();
    String colour=normalizeColor(data.substring(pos,end),"");
    if(!colour.isEmpty()){
      bool duplicate=false;
      for(int i=0;i<rememberedColourCount;i++)if(rememberedColours[i]==colour)duplicate=true;
      if(!duplicate)rememberedColours[rememberedColourCount++]=colour;
    }
    pos=end+1;
  }
}
bool rememberColour(String colour){
  colour=normalizeColor(colour,"");
  if(colour.isEmpty())return false;
  String next[8];
  int count=0;
  next[count++]=colour;
  for(int i=0;i<rememberedColourCount&&count<8;i++)if(rememberedColours[i]!=colour)next[count++]=rememberedColours[i];
  if(count==rememberedColourCount){
    bool unchanged=true;
    for(int i=0;i<count;i++)if(next[i]!=rememberedColours[i])unchanged=false;
    if(unchanged)return true;
  }
  String data;
  for(int i=0;i<count;i++){if(i)data+=',';data+=next[i];}
  Preferences saved;
  if(!saved.begin("lgcolour",false))return false;
  size_t written=saved.putString("recent",data);
  saved.end();
  if(!written)return false;
  rememberedColourCount=count;
  for(int i=0;i<count;i++)rememberedColours[i]=next[i];
  return true;
}
void seedRememberedColours(){
  if(rememberedColourCount)return;
  for(int i=0;i<6;i++){
    if(rowColors[i]!=DEFAULT_ROW_COLORS[i])rememberColour(rowColors[i]);
    for(int j=0;j<rowColorCount[i]-1;j++)
      if(rowExtraColors[i][j]!=DEFAULT_EXTRA_COLORS[j])rememberColour(rowExtraColors[i][j]);
  }
}
void addRememberedColours(JsonDocument& d){
  JsonArray list=d["recentColours"].to<JsonArray>();
  for(int i=0;i<rememberedColourCount;i++)list.add(rememberedColours[i]);
}
uint32_t color24(const String& h) {return strtoul(h.c_str()+1,nullptr,16);}
int strictCount(const String& value,int low,int high){
  if(value.isEmpty()||value.length()>6)return -1;
  for(char c:value)if(c<'0'||c>'9')return -1;
  int n=value.toInt();return n>=low&&n<=high?n:-1;
}
void loadPalette(Preferences& storage,int i){
  rowColorCount[i]=constrain((int)storage.getUChar((String("pn")+i).c_str(),1),1,12);
  for(int j=0;j<11;j++)rowExtraColors[i][j]=normalizeColor(storage.getString((String("pc")+i+j).c_str(),DEFAULT_EXTRA_COLORS[j]),DEFAULT_EXTRA_COLORS[j]);
}
bool savePalette(Preferences& storage,int i){
  bool ok=storage.putUChar((String("pn")+i).c_str(),rowColorCount[i])>0;
  for(int j=0;j<11;j++)ok=(storage.putString((String("pc")+i+j).c_str(),rowExtraColors[i][j])>0)&&ok;
  return ok;
}
int paletteSlot(int pixel,int total,int count){return min(count-1,pixel*count/total);}
uint32_t pixelColour(int row,int pixel){int slot=paletteSlot(pixel,activeLedCount,rowColorCount[row]);return color24(slot==0?rowColors[row]:rowExtraColors[row][slot-1]);}
uint32_t blendColour(uint32_t a,uint32_t b,uint32_t fraction){
  uint32_t colour=0;
  for(int shift=16;shift>=0;shift-=8){uint32_t channel=(((a>>shift)&255)*(65536-fraction)+((b>>shift)&255)*fraction+32768)/65536;colour|=channel<<shift;}
  return colour;
}
uint32_t samplePalette(int row,uint32_t position,bool smooth){
  uint32_t scaled=position*rowColorCount[row];int slot=scaled/65536;int next=(slot+1)%rowColorCount[row];
  uint32_t a=color24(slot==0?rowColors[row]:rowExtraColors[row][slot-1]);
  if(!smooth)return a;
  uint32_t b=color24(next==0?rowColors[row]:rowExtraColors[row][next-1]);
  return blendColour(a,b,scaled%65536);
}
uint32_t effectColour(int row,int pixel,int style,uint32_t tick){
  uint32_t position=((uint32_t)pixel*65536/activeLedCount+65536-tick)%65536;
  if(style==10)return samplePalette(row,position,true);
  if(style==11)return samplePalette(row,tick,true);
  if(style==12){const uint32_t rainbow[]={0xff0000,0xffff00,0x00ff00,0x00ffff,0x0000ff,0xff00ff};uint32_t scaled=position*6;int slot=scaled/65536;return blendColour(rainbow[slot],rainbow[(slot+1)%6],scaled%65536);}
  if(style==13)return samplePalette(row,position,false);
  return pixelColour(row,pixel);
}
void loadSettings(){
  prefs.begin("k2led",true);
  webHost=prefs.getString("host","layerglow");
  if(prefs.isKey("wcount")){
    wifiProfileCount=min((int)prefs.getUChar("wcount",0),(int)MAX_WIFI_PROFILES);
    activeWifi=wifiProfileCount?min((int)prefs.getUChar("wactive",0),(int)wifiProfileCount-1):0;
    for(int i=0;i<wifiProfileCount;i++){
      wifiProfiles[i].name=prefs.getString((String("ws")+i).c_str(),"");
      wifiProfiles[i].password=prefs.getString((String("wp")+i).c_str(),"");
      wifiProfiles[i].useStatic=prefs.getBool((String("wm")+i).c_str(),false);
      wifiProfiles[i].staticIP=prefs.getString((String("wi")+i).c_str(),"");
      wifiProfiles[i].gateway=prefs.getString((String("wg")+i).c_str(),"");
      wifiProfiles[i].subnet=prefs.getString((String("wn")+i).c_str(),"");
      wifiProfiles[i].dns=prefs.getString((String("wd")+i).c_str(),"");
    }
  }else{
    String oldSSID=prefs.getString("ssid","");
    if(oldSSID.length()){
      wifiProfiles[0].name=oldSSID;wifiProfiles[0].password=prefs.getString("pass","");
      wifiProfileCount=1;
    }
    migrateLegacyWifi=true;
  }
  if(wifiProfileCount){ssid=wifiProfiles[activeWifi].name;wifiPassword=wifiProfiles[activeWifi].password;}
  printerIP=prefs.getString("ip","192.168.1.9");printerPort=prefs.getUShort("port",7125);
  printerType=prefs.getString("ptype","moonraker");printerEnabled=prefs.getBool("penabled",true);
  if(printerType!="bambu"&&printerType!="octoprint"&&printerType!="prusalink")printerType="moonraker";
  moonIP=prefs.getString("moonip",printerType=="moonraker"?printerIP:"192.168.1.9");
  bambuIP=prefs.getString("bambuip",printerType=="bambu"?printerIP:"");
  bambuSerial=prefs.getString("bserial","");
  bambuAccessCode=prefs.getString("bcode","");
  octoIP=prefs.getString("octoip",printerType=="octoprint"?printerIP:"");prusaIP=prefs.getString("prusaip",printerType=="prusalink"?printerIP:"");
  octoKey=prefs.getString("octokey","");prusaKey=prefs.getString("prusakey","");
  moonPort=prefs.getUShort("moonport",printerPort);octoPort=prefs.getUShort("octoport",80);prusaPort=prefs.getUShort("prusaport",80);
  printerIP=printerType=="bambu"?bambuIP:printerType=="octoprint"?octoIP:printerType=="prusalink"?prusaIP:moonIP;
  printerPort=printerType=="octoprint"?octoPort:printerType=="prusalink"?prusaPort:moonPort;
  intervalMs=constrain(prefs.getInt("intvl",500),100,10000);
  activeLedCount=constrain(prefs.getInt("ledcnt",24),5,MAX_LED_COUNT);
  int oldBright=constrain(prefs.getInt("bright",65),0,100);
  int oldPeriod=constrain(prefs.getInt("pulsems",1400),200,10000);
  uint8_t oldMask=prefs.isKey("pmask")?prefs.getUChar("pmask",0):0;
  bool oldLast=prefs.getBool("plast",false);
  if(!prefs.isKey("pmask")){String mode=prefs.getString("pulsemode","off");oldMask=mode=="all"?0x1f:(mode=="last"?1:0);oldLast=mode=="last";}
  for(int i=0;i<6;i++){
    String key=String(i);
    String oldColor=i==0?prefs.getString("color",DEFAULT_ROW_COLORS[0]):prefs.getString((String("c")+i).c_str(),DEFAULT_ROW_COLORS[i]);
    rowColors[i]=normalizeColor(prefs.getString((String("rc")+key).c_str(),oldColor),DEFAULT_ROW_COLORS[i]);loadPalette(prefs,i);
    rowBrightness[i]=constrain(prefs.getInt((String("rb")+key).c_str(),oldBright),0,100);
    rowPulse[i]=prefs.getBool((String("re")+key).c_str(),(oldMask&(1<<i))!=0);
    rowEnabled[i]=prefs.getBool((String("ri")+key).c_str(),true);
    rowStyle[i]=constrain((int)prefs.getUChar((String("rs")+key).c_str(),(i==0&&oldLast)?1:0),0,13);if(rowStyle[i]==4)rowStyle[i]=3;
    rowPeriod[i]=constrain(prefs.getInt((String("rp")+key).c_str(),oldPeriod),0,5000);
  }
  prefs.end();
}
// Read back writes so a failed/full NVS is never reported as a successful save.
bool storeString(Preferences& storage,const String& key,const String& value){
  if(storage.isKey(key.c_str())&&storage.getString(key.c_str(),"")==value)return true;
  storage.putString(key.c_str(),value);
  return storage.isKey(key.c_str())&&storage.getString(key.c_str(),"")==value;
}
bool storeInt(Preferences& storage,const String& key,int value){
  if(storage.isKey(key.c_str())&&storage.getInt(key.c_str())==value)return true;
  return storage.putInt(key.c_str(),value)==sizeof(int32_t);
}
bool storeByte(Preferences& storage,const String& key,uint8_t value){
  if(storage.isKey(key.c_str())&&storage.getUChar(key.c_str())==value)return true;
  return storage.putUChar(key.c_str(),value)==sizeof(uint8_t);
}
bool storePort(Preferences& storage,const String& key,uint16_t value){
  if(storage.isKey(key.c_str())&&storage.getUShort(key.c_str())==value)return true;
  return storage.putUShort(key.c_str(),value)==sizeof(uint16_t);
}
bool storeBool(Preferences& storage,const String& key,bool value){
  if(storage.isKey(key.c_str())&&storage.getBool(key.c_str())==value)return true;
  return storage.putBool(key.c_str(),value)==sizeof(uint8_t);
}
bool removeStored(Preferences& storage,const String& key){return !storage.isKey(key.c_str())||storage.remove(key.c_str());}
bool saveWifiSettings(){
  if(!prefs.begin("k2led",false))return false;
  bool ok=storeString(prefs,"host",webHost);
  for(int i=0;i<MAX_WIFI_PROFILES;i++){
    String nameKey=String("ws")+i,passwordKey=String("wp")+i,modeKey=String("wm")+i,ipKey=String("wi")+i,gatewayKey=String("wg")+i,subnetKey=String("wn")+i,dnsKey=String("wd")+i;
    if(i<wifiProfileCount){
      ok=storeString(prefs,nameKey,wifiProfiles[i].name)&&ok;ok=storeString(prefs,passwordKey,wifiProfiles[i].password)&&ok;
      ok=storeBool(prefs,modeKey,wifiProfiles[i].useStatic)&&ok;
      ok=storeString(prefs,ipKey,wifiProfiles[i].staticIP)&&ok;ok=storeString(prefs,gatewayKey,wifiProfiles[i].gateway)&&ok;
      ok=storeString(prefs,subnetKey,wifiProfiles[i].subnet)&&ok;ok=storeString(prefs,dnsKey,wifiProfiles[i].dns)&&ok;
    }else{
      ok=removeStored(prefs,nameKey)&&ok;ok=removeStored(prefs,passwordKey)&&ok;ok=removeStored(prefs,modeKey)&&ok;
      ok=removeStored(prefs,ipKey)&&ok;ok=removeStored(prefs,gatewayKey)&&ok;ok=removeStored(prefs,subnetKey)&&ok;ok=removeStored(prefs,dnsKey)&&ok;
    }
  }
  ok=storeByte(prefs,"wactive",activeWifi)&&ok;
  // Commit the profile count last; keep legacy credentials if any write failed.
  if(ok)ok=storeByte(prefs,"wcount",wifiProfileCount);
  if(ok){ok=removeStored(prefs,"ssid")&&ok;ok=removeStored(prefs,"pass")&&ok;}
  prefs.end();return ok;
}
bool savePrinterSettings(){
  if(!prefs.begin("k2led",false))return false;
  bool ok=true;
  const String names[]={"ip","ptype","moonip","bambuip","octoip","prusaip","bserial","bcode","octokey","prusakey"};
  const String values[]={printerIP,printerType,moonIP,bambuIP,octoIP,prusaIP,bambuSerial,bambuAccessCode,octoKey,prusaKey};
  for(int i=0;i<10;i++)ok=storeString(prefs,names[i],values[i])&&ok;
  ok=storePort(prefs,"port",printerPort)&&ok;ok=storePort(prefs,"moonport",moonPort)&&ok;
  ok=storePort(prefs,"octoport",octoPort)&&ok;ok=storePort(prefs,"prusaport",prusaPort)&&ok;
  ok=storeBool(prefs,"penabled",printerEnabled)&&ok;
  prefs.end();return ok;
}

void updateLEDs();
bool validPrinterProfile(const PrinterProfile& p){
  IPAddress ip;
  if(p.id.length()!=8||p.name.isEmpty()||p.name.length()>48||!ip.fromString(p.ip)||p.port==0||p.network.length()>32)return false;
  for(char c:p.id)if(!isxdigit((unsigned char)c))return false;
  for(char c:p.name)if((unsigned char)c<32)return false;
  if(!p.mac.isEmpty()){
    if(p.mac.length()!=17)return false;
    for(int i=0;i<17;i++)if(i%3==2?p.mac[i]!=':':!isxdigit((unsigned char)p.mac[i]))return false;
  }
  if(p.type=="bambu"){
    if(p.port!=8883||p.serial.length()<6||p.serial.length()>40||p.secret.isEmpty()||p.secret.length()>64)return false;
    for(char c:p.serial)if(!isalnum((unsigned char)c))return false;
  }else if(p.type=="octoprint"||p.type=="prusalink"){
    if(p.secret.isEmpty()||p.secret.length()>128)return false;
  }else if(p.type!="moonraker")return false;
  return true;
}
int savedPrinterIndex(const String& id){
  for(int i=0;i<printerProfileCount;i++)if(printerProfiles[i].id==id)return i;
  return -1;
}
bool readPrinterProfileBlob(Preferences& storage,String& data){
  if(!storage.isKey("profiles")){data="";return true;}
  size_t length=storage.getBytesLength("profiles");if(!length||length>8192)return false;
  char* buffer=(char*)malloc(length+1);if(!buffer)return false;
  bool ok=storage.getBytes("profiles",buffer,length)==length;
  buffer[length]=0;if(ok){data=String(buffer);ok=data.length()==length;}free(buffer);return ok;
}
bool writePrinterProfileBlob(Preferences& storage,const String& data){
  String previous;if(!readPrinterProfileBlob(storage,previous))return false;
  if(previous==data)return true;
  if(storage.putBytes("profiles",data.c_str(),data.length())!=data.length())return false;
  String verified;return readPrinterProfileBlob(storage,verified)&&verified==data;
}
bool commitPrinterProfiles(const PrinterProfile* list,int count,int active,bool enabled){
  if(!printerProfileStorageValid||count<0||count>MAX_PRINTER_PROFILES||active < -1||active>=count||(enabled&&active<0))return false;
  JsonDocument d;d["schema"]=1;d["active"]=active;d["enabled"]=enabled;
  JsonArray rows=d["profiles"].to<JsonArray>();
  for(int i=0;i<count;i++){
    if(!validPrinterProfile(list[i]))return false;
    for(int j=0;j<i;j++)if(list[j].id==list[i].id)return false;
    JsonObject r=rows.add<JsonObject>();const PrinterProfile& p=list[i];
    r["id"]=p.id;r["name"]=p.name;r["type"]=p.type;r["ip"]=p.ip;r["port"]=p.port;
    r["mac"]=p.mac;r["serial"]=p.serial;r["secret"]=p.secret;r["network"]=p.network;
  }
  String data;serializeJson(d,data);if(data.length()>8192)return false;
  Preferences storage;if(!storage.begin("lgprinters",false))return false;
  // The profile set and selected printer are committed as one NVS value.
  bool ok=writePrinterProfileBlob(storage,data);storage.end();if(!ok)return false;
  for(int i=0;i<count;i++)printerProfiles[i]=list[i];
  for(int i=count;i<MAX_PRINTER_PROFILES;i++)printerProfiles[i]=PrinterProfile();
  printerProfileCount=count;activePrinter=active;printerEnabled=enabled;return true;
}
void applySavedPrinter(){
  ++printerConfigRevision;bambuMqtt.disconnect();
  printerConnected=false;printerState="standby";printerProgress=0;printerFile="";
  lastPrinterReadAt=0;lastBambuAttempt=0;lastPoll=0;lastBambuReport=0;
  if(activePrinter>=0&&activePrinter<printerProfileCount){
    const PrinterProfile& p=printerProfiles[activePrinter];
    printerType=p.type;printerIP=p.ip;printerPort=p.port;
    if(p.type=="bambu"){bambuIP=p.ip;bambuSerial=p.serial;bambuAccessCode=p.secret;}
    else if(p.type=="octoprint"){octoIP=p.ip;octoPort=p.port;octoKey=p.secret;}
    else if(p.type=="prusalink"){prusaIP=p.ip;prusaPort=p.port;prusaKey=p.secret;}
    else{moonIP=p.ip;moonPort=p.port;}
  }else{printerEnabled=false;printerIP="";}
  lastError=printerEnabled?"Waiting for first poll":"Printer disconnected";updateLEDs();
}
void loadPrinterProfiles(){
  Preferences storage;String data;
  if(!storage.begin("lgprinters",false)){printerEnabled=false;lastError="Could not load saved printers";return;}
  bool readable=readPrinterProfileBlob(storage,data);storage.end();
  if(!readable){printerProfileStorageValid=false;printerEnabled=false;lastError="Could not load saved printers";return;}
  PrinterProfile next[MAX_PRINTER_PROFILES];int count=0,active=-1;bool enabled=printerEnabled;
  if(!data.isEmpty()){
    JsonDocument d;
    if(data.length()>8192||deserializeJson(d,data.c_str(),data.length())||d["schema"].as<int>()!=1||!d["profiles"].is<JsonArray>()||!d["active"].is<int>()||!d["enabled"].is<bool>()){
      printerProfileStorageValid=false;printerEnabled=false;lastError="Could not load saved printers";return;
    }
    JsonArray rows=d["profiles"].as<JsonArray>();active=d["active"].as<int>();enabled=d["enabled"].as<bool>();
    if(rows.size()>MAX_PRINTER_PROFILES||active < -1||active>=(int)rows.size()||(enabled&&active<0)){printerProfileStorageValid=false;printerEnabled=false;return;}
    for(JsonObject r:rows){
      PrinterProfile& p=next[count++];p.id=r["id"].as<String>();p.name=r["name"].as<String>();p.type=r["type"].as<String>();p.ip=r["ip"].as<String>();p.port=r["port"]|0;
      p.mac=r["mac"].as<String>();p.serial=r["serial"].as<String>();p.secret=r["secret"].as<String>();p.network=r["network"].as<String>();
      bool valid=validPrinterProfile(p);for(int j=0;j<count-1;j++)if(next[j].id==p.id)valid=false;
      if(!valid){printerProfileStorageValid=false;printerEnabled=false;lastError="Could not load saved printers";return;}
    }
    for(int i=0;i<count;i++)printerProfiles[i]=next[i];printerProfileCount=count;activePrinter=active;printerEnabled=enabled;
  }else{
    // Import the selected legacy printer once; default addresses for other adapters are not saved printers.
    Preferences old;
    if(old.begin("k2led",true)){
      const char* types[]={"moonraker","bambu","octoprint","prusalink"};const char* keys[]={"moonip","bambuip","octoip","prusaip"};
      const String ips[]={moonIP,bambuIP,octoIP,prusaIP};
      const String secrets[]={"",bambuAccessCode,octoKey,prusaKey};const uint16_t ports[]={moonPort,8883,octoPort,prusaPort};
      for(int i=0;i<4;i++){
        if(printerType!=types[i])continue;
        if(!old.isKey(keys[i])&&!(printerType==types[i]&&old.isKey("ip")))continue;
        PrinterProfile p;p.id=String(i+1,HEX);while(p.id.length()<8)p.id="0"+p.id;
        p.name=String(types[i])+" · "+ips[i];p.type=types[i];p.ip=ips[i];p.port=ports[i];p.secret=secrets[i];p.network=ssid;
        if(i==1)p.serial=bambuSerial;
        if(validPrinterProfile(p)){next[count]=p;if(p.type==printerType&&p.ip==printerIP)active=count;count++;}
      }old.end();
    }
    enabled=enabled&&active>=0;
    if(!commitPrinterProfiles(next,count,active,enabled)){
      // Retain the migrated settings in RAM and retry persistence on the next explicit save.
      for(int i=0;i<count;i++)printerProfiles[i]=next[i];printerProfileCount=count;activePrinter=active;printerEnabled=enabled;
    }
  }
  applySavedPrinter();
}
void addSavedPrinters(JsonDocument& d){
  JsonArray rows=d["savedPrinters"].to<JsonArray>();
  for(int i=0;i<printerProfileCount;i++){
    const PrinterProfile& p=printerProfiles[i];JsonObject r=rows.add<JsonObject>();
    r["id"]=p.id;r["name"]=p.name;r["type"]=p.type;r["ip"]=p.ip;r["port"]=p.port;r["mac"]=p.mac;
    r["serial"]=p.serial;r["network"]=p.network;r["hasSecret"]=!p.secret.isEmpty();
    r["active"]=i==activePrinter;r["enabled"]=i==activePrinter&&printerEnabled;
    r["connected"]=i==activePrinter&&printerEnabled&&printerConnected;
  }
  d["printerProfileStorageOK"]=printerProfileStorageValid;d["maxSavedPrinters"]=MAX_PRINTER_PROFILES;
}

bool saveLighting(){
  if(!prefs.begin("k2led",false))return false;
  bool ok=storeInt(prefs,"intvl",intervalMs);
  ok=storeInt(prefs,"ledcnt",activeLedCount)&&ok;
  for(int i=0;i<6;i++){
    String key=String(i);
    ok=storeString(prefs,"rc"+key,rowColors[i])&&ok;
    ok=storeByte(prefs,"pn"+key,rowColorCount[i])&&ok;
    for(int j=0;j<11;j++)ok=storeString(prefs,"pc"+key+j,rowExtraColors[i][j])&&ok;
    ok=storeInt(prefs,"rb"+key,rowBrightness[i])&&ok;
    ok=storeBool(prefs,"re"+key,rowPulse[i])&&ok;
    ok=storeBool(prefs,"ri"+key,rowEnabled[i])&&ok;
    ok=storeByte(prefs,"rs"+key,rowStyle[i])&&ok;
    ok=storeInt(prefs,"rp"+key,rowPeriod[i])&&ok;
  }
  prefs.end();return ok;
}
void updateLEDs();
bool saveLightingBackup(){
  Preferences backup;
  if(!backup.begin("k2ledbak",false))return false;
  backup.putBool("valid",false);
  bool ok=backup.putInt("intvl",intervalMs)>0 && backup.putInt("ledcnt",activeLedCount)>0;
  for(int i=0;i<6;i++){
    String key=String(i);
    ok=(backup.putString((String("rc")+key).c_str(),rowColors[i])>0)&&ok;ok=savePalette(backup,i)&&ok;
    ok=(backup.putInt((String("rb")+key).c_str(),rowBrightness[i])>0)&&ok;
    ok=(backup.putBool((String("re")+key).c_str(),rowPulse[i])>0)&&ok;
    ok=(backup.putBool((String("ri")+key).c_str(),rowEnabled[i])>0)&&ok;
    ok=(backup.putUChar((String("rs")+key).c_str(),rowStyle[i])>0)&&ok;
    ok=(backup.putInt((String("rp")+key).c_str(),rowPeriod[i])>0)&&ok;
  }
  lightingBackupAvailable=ok && backup.putBool("valid",true)>0;
  backup.end();
  return lightingBackupAvailable;
}
bool restoreLightingBackup(){
  Preferences backup;
  if(!backup.begin("k2ledbak",true))return false;
  if(!backup.getBool("valid",false)){backup.end();lightingBackupAvailable=false;return false;}
  String colors[6];uint8_t brightness[6],styles[6];bool pulse[6],enabled[6];uint16_t periods[6];
  int count=constrain(backup.getInt("ledcnt",24),5,MAX_LED_COUNT);
  int interval=constrain(backup.getInt("intvl",500),100,10000);
  for(int i=0;i<6;i++){
    String key=String(i);
    colors[i]=normalizeColor(backup.getString((String("rc")+key).c_str(),DEFAULT_ROW_COLORS[i]),DEFAULT_ROW_COLORS[i]);loadPalette(backup,i);
    brightness[i]=constrain(backup.getInt((String("rb")+key).c_str(),65),0,100);
    pulse[i]=backup.getBool((String("re")+key).c_str(),false);
    enabled[i]=backup.getBool((String("ri")+key).c_str(),true);
    styles[i]=constrain((int)backup.getUChar((String("rs")+key).c_str(),0),0,13);
    if(styles[i]==4)styles[i]=3;
    periods[i]=constrain(backup.getInt((String("rp")+key).c_str(),1400),0,5000);
  }
  backup.end();
  activeLedCount=count;intervalMs=interval;
  for(int i=0;i<6;i++){rowColors[i]=colors[i];rowBrightness[i]=brightness[i];rowPulse[i]=pulse[i];rowEnabled[i]=enabled[i];rowStyle[i]=styles[i];rowPeriod[i]=periods[i];}
  bool saved=saveLighting();updateLEDs();
  return saved;
}
void addRows(JsonDocument& d){for(int i=0;i<6;i++){const char* name=i==0?"progress":stateNames[i];d["rows"][name]["color"]=rowColors[i];d["rows"][name]["colourCount"]=rowColorCount[i];JsonArray palette=d["rows"][name]["colours"].to<JsonArray>();palette.add(rowColors[i]);for(int j=0;j<11;j++)palette.add(rowExtraColors[i][j]);d["rows"][name]["brightness"]=rowBrightness[i];d["rows"][name]["pulse"]=rowPulse[i];d["rows"][name]["enabled"]=rowEnabled[i];d["rows"][name]["period"]=rowPeriod[i];d["rows"][name]["style"]=rowStyle[i];}}

int statusIndex(String st){if(st=="printing")return 0;if(st=="heating")return 1;if(st=="paused")return 2;if(st=="complete")return 3;if(st=="error")return 4;return 5;}
String normalState(String s){s.toLowerCase();if(s=="complete"||s=="completed")return "complete";if(s=="printing")return "printing";if(s=="paused")return "paused";if(s=="error"||s=="cancelled")return "error";if(s=="heating")return "heating";return "standby";}
void updateLEDs(){
  if(!LED_ENABLED)return;
  if(strip.numPixels()!=activeLedCount){strip.clear();strip.show();strip.updateLength(activeLedCount);}
  String st=simEnabled?String(stateNames[simStatus]):normalState(printerState);
  if(!simEnabled&&!printerConnected)st="standby";
  int idx=statusIndex(st),pairs=(activeLedCount+1)/2;
  int count=st=="printing"?(int)ceilf((simEnabled?simProgress:printerProgress*100.0f)*pairs/100.0f):(st=="standby"&&!rowPulse[idx]&&rowStyle[idx]<6?0:pairs);
  count=constrain(count,0,pairs);if(!rowEnabled[idx])count=0;
  uint32_t effectivePeriod=rowPeriod[idx]?rowPeriod[idx]:1400;
  uint32_t cycleMs=millis()%effectivePeriod,colourTick=cycleMs*65536UL/effectivePeriod;
  float cycle=cycleMs/(float)effectivePeriod;
  float scale=0.625f-0.375f*cosf(6.2831853f*cycle);
  float travel=cycle*2.0f*fmaxf(0,count-1);float head=travel<=count-1?travel:2.0f*(count-1)-travel;float centre=(count-1)/2.0f;float radius=fmaxf(0.5f,centre);float outward=1.0f-fabsf(2.0f*cycle-1.0f);float front=outward*radius;
  static bool randomPairOn[MAX_LED_PAIRS]={};
  static uint32_t randomPairNext[MAX_LED_PAIRS]={};
  static int randomState=-1,randomLength=0;
  uint32_t randomNow=millis();
  bool randomEffect=rowStyle[idx]==6;
  if(!randomEffect)randomState=-1;
  else if(randomState!=idx||randomLength!=activeLedCount){
    randomState=idx;randomLength=activeLedCount;
    for(int pair=0;pair<MAX_LED_PAIRS;pair++){
      randomPairOn[pair]=false;
      randomPairNext[pair]=randomNow+random(20,180);
    }
  }
  if(randomEffect){
    uint32_t base=constrain((int)rowPeriod[idx]/12,55,220);
    for(int pair=0;pair<pairs;pair++){
      if((int32_t)(randomNow-randomPairNext[pair])>=0){
        randomPairOn[pair]=!randomPairOn[pair];
        randomPairNext[pair]=randomNow+random(base/2,base*2+1);
      }
    }
  }
  strip.setBrightness(map(rowBrightness[idx],0,100,0,255));
  for(int i=0;i<activeLedCount;i++){
    uint32_t rgb=effectColour(idx,i,rowStyle[idx],colourTick);uint8_t r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    int pair=i/2;bool lit=i<activeLedCount&&pair<count&&(!randomEffect||randomPairOn[pair]);float level=1.0f;
    if(lit&&rowPulse[idx]&&rowPeriod[idx]>0){
      if(rowStyle[idx]==2)level=fmaxf(0.08f,1.0f-fabsf(pair-head)/2.2f);
      else if(rowStyle[idx]==3)level=fmaxf(0.08f,1.0f-fabsf(fabsf(pair-centre)-front)/1.3f);
      else if(rowStyle[idx]==5)level=0.16f+0.84f*(0.5f+0.5f*cosf(6.2831853f*(pair/fmaxf(1,count-1)-cycle)));
      else if(rowStyle[idx]==0||rowStyle[idx]>=10||(rowStyle[idx]==1&&pair==count-1))level=scale;
    }
    if(lit&&rowStyle[idx]==7){
      float trail=fmodf(cycle*count-pair+count,count);
      level=fmaxf(0.0f,1.0f-trail/3.2f);
      lit=level>0.02f;
    }else if(lit&&rowStyle[idx]==8){
      int step=(int)(cycle*2*count);
      lit=step<count?pair<=step:pair>step-count;
    }else if(lit&&rowStyle[idx]==9){
      lit=(pair%2)==(int)(cycle*2);
    }
    strip.setPixelColor(i,lit?strip.Color((uint8_t)(r*level),(uint8_t)(g*level),(uint8_t)(b*level),0):strip.Color(0,0,0,0));
  }
  strip.show();
}

bool startRecoveryAP(){
  if(apMode)return true;
  if(apPassword.isEmpty()&&!wifiProfileCount){Serial.println("Setup AP unavailable: no unique password");return false;}
  lastApAttempt=millis();
  WiFi.mode(WIFI_AP_STA);
  if(!WiFi.softAP(AP_NAME,apPassword.isEmpty()?INITIAL_AP_PASSWORD:apPassword.c_str())){
    apMode=false;
    Serial.println("Setup AP failed; retrying");
    return false;
  }
  apMode=true;
  Serial.println("Setup AP: LayerGlow-Setup · http://"+WiFi.softAPIP().toString());
  return true;
}
bool validWebHost(const String& name){
  if(name.length()<1||name.length()>32||name[0]=='-'||name[name.length()-1]=='-')return false;
  for(char c:name)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
  return true;
}
void maintainMdns(uint32_t now){
  extern volatile bool discoveryRunning;
  if(WiFi.status()!=WL_CONNECTED||discoveryRunning||otaUploadInProgress)return;
  if(mdnsStarted&&!mdnsNeedsRestart&&mdnsHostActive==webHost)return;
  if(mdnsChangeAt&&(int32_t)(now-mdnsChangeAt)<0)return;
  if(lastMdnsAttempt&&now-lastMdnsAttempt<15000)return;
  lastMdnsAttempt=now;
  if(mdnsStarted){MDNS.end();mdnsStarted=false;}
  mdnsStarted=MDNS.begin(webHost);
  if(mdnsStarted){mdnsHostActive=webHost;mdnsNeedsRestart=false;mdnsChangeAt=0;MDNS.addService("http","tcp",80);Serial.println("Panel: http://"+webHost+".local/");}
  else Serial.println("mDNS unavailable; local IP and setup AP remain accessible");
}
void connectWifiProfile(uint8_t index,uint32_t now){
  if(index>=wifiProfileCount)return;
  activeWifi=index;ssid=wifiProfiles[index].name;wifiPassword=wifiProfiles[index].password;
  WiFi.disconnect(false);
  const WifiProfile& profile=wifiProfiles[index];
  IPAddress local(0,0,0,0),gateway(0,0,0,0),mask(0,0,0,0),dns(0,0,0,0);
  if(profile.useStatic){
    if(!local.fromString(profile.staticIP)||!gateway.fromString(profile.gateway)||!mask.fromString(profile.subnet)||!dns.fromString(profile.dns)||
       !WiFi.config(local,gateway,mask,dns)){
      Serial.println("Static IP failed; trying DHCP");
      WiFi.config(IPAddress(0,0,0,0),IPAddress(0,0,0,0),IPAddress(0,0,0,0));
    }
  }else if(!WiFi.config(IPAddress(0,0,0,0),IPAddress(0,0,0,0),IPAddress(0,0,0,0)))
    Serial.println("Could not enable DHCP");
  WiFi.begin(ssid.c_str(),wifiPassword.c_str());
  wifiAttemptStarted=now;lastWifiAttempt=now;wifiConnectedSince=0;
  Serial.println("Trying saved Wi-Fi: "+ssid);
}
void startNetwork(){
  // DHCP hostname must be set before startRecoveryAP() enables Wi-Fi.
  // The router name follows the configurable .local host on each boot.
  if(!WiFi.setHostname(webHost.equalsIgnoreCase("layerglow") ? "LayerGlow" : webHost.c_str()))
    Serial.println("Could not set Wi-Fi hostname");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  // Bring up the rescue AP immediately; do not block the web server for 10 seconds.
  startRecoveryAP();
  if(wifiProfileCount)connectWifiProfile(activeWifi,millis());
}
void finishWifiScan(uint32_t now){
  WiFi.scanDelete();wifiScanActive=false;wifiScanLaunched=false;
  // Resume saved-network attempts after the HTTP response has reached an AP client.
  wifiScanResumeAt=now+700;
}
void startWifiScan(uint32_t now){
  wifiScanActive=true;wifiScanLaunched=false;wifiScanStarted=now;wifiScanNextTry=now+350;
  // Connecting to an unavailable saved network makes esp_wifi_scan_start fail.
  WiFi.setAutoReconnect(false);
  if(WiFi.status()!=WL_CONNECTED)WiFi.disconnect(false);
}
int pollWifiScan(uint32_t now){
  if(now-wifiScanStarted>=12000){finishWifiScan(now);return -2;}
  if(!wifiScanLaunched){
    if((int32_t)(now-wifiScanNextTry)<0)return -1;
    WiFi.scanDelete();
    int result=WiFi.scanNetworks(true,true);
    if(result==-2){wifiScanNextTry=now+750;return -1;}
    wifiScanLaunched=true;
    return -1;
  }
  int count=WiFi.scanComplete();
  if(count==-1)return -1;
  if(count<0){WiFi.scanDelete();wifiScanLaunched=false;wifiScanNextTry=now+750;return -1;}
  return count;
}
void maintainNetwork(uint32_t now){
  if(otaUploadInProgress)return;
  if(wifiScanActive){
    if(now-wifiScanStarted<16000)return;
    finishWifiScan(now);
  }
  if(wifiScanResumeAt){
    if((int32_t)(now-wifiScanResumeAt)<0)return;
    wifiScanResumeAt=0;WiFi.setAutoReconnect(true);
    if(!wifiChangePending&&WiFi.status()!=WL_CONNECTED&&wifiProfileCount)connectWifiProfile(activeWifi,now);
  }
  if(wifiChangePending&&(int32_t)(now-wifiChangeAt)>=0){
    wifiChangePending=false;
    ++printerConfigRevision;
    startRecoveryAP();
    if(wifiProfileCount)connectWifiProfile(activeWifi,now);
    else{WiFi.disconnect(false);ssid="";wifiPassword="";wifiAttemptStarted=0;wifiConnectedSince=0;}
    wifiLostSince=now;
    return;
  }
  if(wifiChangePending)return;
  if(WiFi.status()==WL_CONNECTED){
    if(!wifiConnectedSince){
      wifiConnectedSince=now;
      ++printerConfigRevision;lastBambuAttempt=0;
      configTime(0,0,"pool.ntp.org","time.cloudflare.com");
      mdnsNeedsRestart=true;
      if(wifiProfileCount){
        if(prefs.begin("k2led",false)){if(prefs.getUChar("wactive",255)!=activeWifi)prefs.putUChar("wactive",activeWifi);prefs.end();}
      }
      Serial.println("Wi-Fi connected: http://"+WiFi.localIP().toString());
    }
    wifiLostSince=0;
    maintainMdns(now);
    // Leave a minute for setup to finish, and do not disconnect an active AP client.
    if(apMode&&!adminHash.isEmpty()&&now-wifiConnectedSince>=60000&&WiFi.softAPgetStationNum()==0){
      if(WiFi.softAPdisconnect(true)){
        apMode=false;
        Serial.println("Setup AP stopped after home Wi-Fi connected");
      }
    }
    return;
  }
  wifiConnectedSince=0;
  mdnsNeedsRestart=true;
  if(!wifiLostSince){wifiLostSince=now;wifiAttemptStarted=now;}
  if(now-wifiLostSince>=8000&&!apMode&&now-lastApAttempt>=8000)startRecoveryAP();
  if(!wifiProfileCount||wifiScanActive)return;
  // A connected recovery phone must not prevent saved networks being retried.
  // Slow down retries while it is present and keep the AP running.
  uint32_t retryAfter=apMode&&WiFi.softAPgetStationNum()>0?30000UL:(wifiProfileCount==1?20000UL:12000UL);
  if(now-wifiAttemptStarted<retryAfter)return;
  if(wifiProfileCount==1){
    if(now-wifiAttemptStarted>=retryAfter){wifiAttemptStarted=now;WiFi.reconnect();}
    return;
  }
  // Try each remembered network without shutting down the recovery AP.
  connectWifiProfile((activeWifi+1)%wifiProfileCount,now);
}
// Only POD snapshots cross the task boundary. UI/LED state stays on the main task.
struct HttpPrinterRequest {uint32_t revision;uint8_t type;uint16_t port;char ip[16],key[129];};
struct HttpPrinterResult {uint32_t revision,readAt;bool connected;float progress;char state[32],file[256],error[96];};
QueueHandle_t httpPrinterRequests=nullptr,httpPrinterResults=nullptr;
bool httpPrinterPending=false;
void markPrinterOffline(const String& reason);
// Explicit declaration also keeps Arduino sketch prototype generation type-safe.
void readHttpPrinter(const HttpPrinterRequest& config,HttpPrinterResult& result);
void readHttpPrinter(const HttpPrinterRequest& config,HttpPrinterResult& result){
  result={};result.revision=config.revision;
  WiFiClient client;HTTPClient http;
  bool moon=config.type==0,octo=config.type==1;
  String path=moon?"/printer/objects/query?print_stats&virtual_sdcard":octo?"/api/job":"/api/v1/status";
  String url="http://"+String(config.ip)+":"+String(config.port)+path;
  http.setConnectTimeout(700);http.setTimeout(1000);http.useHTTP10(true);
  if(!http.begin(client,url)){strlcpy(result.error,"Invalid printer URL",sizeof(result.error));return;}
  if(!moon)http.addHeader("X-Api-Key",config.key);
  int response=http.GET();
  if(response!=200){snprintf(result.error,sizeof(result.error),"HTTP %d",response);http.end();return;}
  JsonDocument doc;DeserializationError err=deserializeJson(doc,http.getStream());http.end();
  if(err){snprintf(result.error,sizeof(result.error),"JSON: %s",err.c_str());return;}
  String state,file;float progress=0;
  if(moon){
    JsonVariant status=doc["result"]["status"];
    if(status["print_stats"].isNull()||status["virtual_sdcard"].isNull()){strlcpy(result.error,"Missing printer fields",sizeof(result.error));return;}
    state=status["print_stats"]["state"].as<String>();file=status["print_stats"]["filename"].as<String>();
    progress=status["virtual_sdcard"]["progress"].as<float>();
  }else{
    state=octo?doc["state"].as<String>():doc["printer"]["state"].as<String>();state.toLowerCase();
    if(state.isEmpty()){strlcpy(result.error,"Missing printer fields",sizeof(result.error));return;}
    if(state=="printing"||state=="pausing"||state=="resuming")state="printing";
    else if(state=="paused")state="paused";
    else if(state=="finished"||state=="complete")state="complete";
    else if(state=="error"||state=="attention"||state=="cancelling"||state=="offline")state="error";
    else state="standby";
    progress=(octo?doc["progress"]["completion"].as<float>():doc["job"]["progress"].as<float>())/100.f;
    file=octo?doc["job"]["file"]["name"].as<String>():String("");
  }
  if(state.isEmpty()||!isfinite(progress)){strlcpy(result.error,"Missing printer fields",sizeof(result.error));return;}
  result.connected=true;result.progress=constrain(progress,0.f,1.f);result.readAt=millis();
  strlcpy(result.state,state.c_str(),sizeof(result.state));strlcpy(result.file,file.c_str(),sizeof(result.file));
}
void httpPrinterTask(void*){
  HttpPrinterRequest request;HttpPrinterResult result;
  for(;;){
    if(xQueueReceive(httpPrinterRequests,&request,portMAX_DELAY)==pdTRUE){
      readHttpPrinter(request,result);
      xQueueOverwrite(httpPrinterResults,&result);
    }
  }
}
bool startHttpPrinterTask(){
  httpPrinterRequests=xQueueCreate(1,sizeof(HttpPrinterRequest));
  httpPrinterResults=xQueueCreate(1,sizeof(HttpPrinterResult));
  if(httpPrinterRequests&&httpPrinterResults&&xTaskCreate(httpPrinterTask,"printer-http",6144,nullptr,1,nullptr)==pdPASS)return true;
  if(httpPrinterRequests)vQueueDelete(httpPrinterRequests);
  if(httpPrinterResults)vQueueDelete(httpPrinterResults);
  httpPrinterRequests=nullptr;httpPrinterResults=nullptr;return false;
}
void maintainHttpPrinter(uint32_t now){
  HttpPrinterResult result;
  if(httpPrinterResults&&xQueueReceive(httpPrinterResults,&result,0)==pdTRUE){
    httpPrinterPending=false;
    // An old request must not resurrect a disconnected or newly selected printer.
    if(result.revision==printerConfigRevision&&printerEnabled&&printerType!="bambu"&&WiFi.status()==WL_CONNECTED){
      if(result.connected){printerConnected=true;printerState=result.state;printerProgress=result.progress;printerFile=result.file;lastError="";lastPrinterReadAt=result.readAt;updateLEDs();}
      else markPrinterOffline(result.error);
    }
  }
  if(!printerEnabled||printerType=="bambu"||otaUploadInProgress)return;
  if(WiFi.status()!=WL_CONNECTED){markPrinterOffline("No printer connection (setup AP)");return;}
  if(!httpPrinterRequests){markPrinterOffline("Printer worker unavailable");return;}
  if(httpPrinterPending||now-lastPoll<(printerConnected?2500UL:6000UL))return;
  HttpPrinterRequest request={};request.revision=printerConfigRevision;
  request.type=printerType=="moonraker"?0:printerType=="octoprint"?1:2;request.port=printerPort;
  strlcpy(request.ip,printerIP.c_str(),sizeof(request.ip));
  strlcpy(request.key,(request.type==1?octoKey:prusaKey).c_str(),sizeof(request.key));
  if(xQueueSend(httpPrinterRequests,&request,0)==pdTRUE){httpPrinterPending=true;lastPoll=now;}
}
void markPrinterOffline(const String& reason){
  bool repaint=printerConnected||printerState!="standby"||printerProgress!=0||!printerFile.isEmpty();
  printerConnected=false;printerState="standby";printerProgress=0;printerFile="";lastError=reason;
  if(repaint&&!simEnabled)updateLEDs();
}
void onBambuReport(char* topic,byte* payload,unsigned int length){
  if(!printerEnabled)return;
  JsonDocument doc;
  if(deserializeJson(doc,payload,length))return;
  JsonVariant p=doc["print"];
  if(p.isNull())return;
  bool gotStatus=false;
  if(!p["gcode_state"].isNull()){
    String state=p["gcode_state"].as<String>();state.toUpperCase();
    if(state=="RUNNING")printerState="printing";
    else if(state=="PREPARE"||state=="SLICING")printerState="heating";
    else if(state=="PAUSE")printerState="paused";
    else if(state=="FINISH")printerState="complete";
    else if(state=="FAILED")printerState="error";
    else if(state=="IDLE")printerState="standby";
    gotStatus=true;
  }
  if(!p["mc_percent"].isNull()){printerProgress=constrain(p["mc_percent"].as<float>()/100.f,0.f,1.f);gotStatus=true;}
  if(!p["subtask_name"].isNull()){printerFile=p["subtask_name"].as<String>();gotStatus=true;}
  if(!gotStatus)return;
  lastBambuReport=millis();printerConnected=true;lastError="";lastPrinterReadAt=lastBambuReport;
  if(!simEnabled)updateLEDs();
}
void maintainBambu(uint32_t now){
  if(!printerEnabled||printerType!="bambu"){
    if(bambuMqtt.connected())bambuMqtt.disconnect();
    if(bambuMqtt.getBufferSize()>256)bambuMqtt.setBufferSize(256);
    return;
  }
  // Do not do printer I/O during an upload. The upload handler owns the socket time.
  if(otaUploadInProgress)return;
  if(WiFi.status()!=WL_CONNECTED){
    if(bambuMqtt.connected())bambuMqtt.disconnect();
    markPrinterOffline("No printer connection (setup AP)");
    return;
  }
  if(!bambuMqtt.connected()){
    markPrinterOffline(lastError=="Bambu access denied"?lastError:String("Bambu MQTT offline"));
    if(lastBambuAttempt&&now-lastBambuAttempt<30000)return;
    lastBambuAttempt=now;
    if(bambuIP.isEmpty()||bambuSerial.isEmpty()||bambuAccessCode.isEmpty()){lastError="Bambu settings missing";return;}
    if(bambuMqtt.getBufferSize()<32768&&!bambuMqtt.setBufferSize(32768)){lastError="Not enough memory for Bambu MQTT";return;}
    bambuMqtt.setServer(bambuIP.c_str(),8883);
    String clientID="layerglow-"+WiFi.macAddress();clientID.replace(":","");
    if(!bambuMqtt.connect(clientID.c_str(),"bblp",bambuAccessCode.c_str())){
      int result=bambuMqtt.state();
      lastError=(result==4||result==5)?"Bambu access denied":"Bambu MQTT offline";
      return;
    }
    String report="device/"+bambuSerial+"/report";
    if(!bambuMqtt.subscribe(report.c_str())){bambuMqtt.disconnect();markPrinterOffline("Bambu MQTT offline");return;}
    lastBambuReport=millis();
    lastBambuRequest=0;
  }
  if(!bambuMqtt.loop()){markPrinterOffline("Bambu MQTT offline");return;}
  // connect()/loop() can take time: do not subtract a newly received timestamp from stale now.
  now=millis();
  if(now-lastBambuReport>35000){bambuMqtt.disconnect();markPrinterOffline("Bambu status timeout");return;}
  if(!lastBambuRequest||now-lastBambuRequest>=30000){
    String request="device/"+bambuSerial+"/request";
    if(!bambuMqtt.publish(request.c_str(),"{\"pushing\":{\"command\":\"pushall\",\"push_target\":1,\"sequence_id\":\"1\",\"version\":1}}")){
      bambuMqtt.disconnect();markPrinterOffline("Bambu MQTT offline");return;
    }
    lastBambuRequest=now;
  }
}
struct PrinterCandidate {String type, name, ip, serial; uint16_t port;};
PrinterCandidate discoveryResults[12];
int discoveryCount=0;
volatile bool discoveryRunning=false;
volatile uint16_t discoveryChecked=0, discoveryTotal=0;
volatile uint8_t discoveryPhase=0; // 0 advertising, 1 local subnet, 2 finished
void addDiscovered(const String& type,const String& name,const String& ip,uint16_t port,const String& serial=""){
  if(ip.isEmpty()||ip==WiFi.localIP().toString()||port==0)return;
  for(int i=0;i<discoveryCount;i++)if(discoveryResults[i].type==type&&discoveryResults[i].ip==ip)return;
  if(discoveryCount>=12)return;
  PrinterCandidate& p=discoveryResults[discoveryCount++];p.type=type;p.name=name.length()?name:type;p.ip=ip;p.port=port;p.serial=serial;
}
String discoveryHeader(const String& message,const char* header){
  String lower=message;lower.toLowerCase();String key="\n"+String(header);int pos=lower.indexOf(key);
  if(pos<0)return "";
  pos+=key.length();
  int end=message.indexOf('\n',pos);
  String value=message.substring(pos,end<0?message.length():end);
  value.trim();return value;
}
bool isMoonraker(IPAddress ip,uint16_t port,String* discoveredName=nullptr){
  if(port==0||port==443)return false;
  WiFiClient client;HTTPClient request;request.setConnectTimeout(400);request.setTimeout(600);
  if(!request.begin(client,"http://"+ip.toString()+":"+String(port)+"/printer/info"))return false;
  int status=request.GET();bool yes=false;
  if(status==200&&request.getSize()<4096){JsonDocument doc;yes=!deserializeJson(doc,request.getString())&&!doc["result"]["state"].isNull();if(yes&&discoveredName)*discoveredName=doc["result"]["hostname"].as<String>();}
  request.end();return yes;
}
void printerDiscoveryTask(void*){
  // Runs only on demand. The HTTP server remains responsive while network discovery waits.
  if(WiFi.status()==WL_CONNECTED){
    WiFiUDP udp;
    if(udp.begin(0)){
      const char* search="M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:2021\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: urn:bambulab-com:device:3dprinter:1\r\n\r\n";
      for(int n=0;n<2;n++){udp.beginPacket(IPAddress(239,255,255,250),2021);udp.write((const uint8_t*)search,strlen(search));udp.endPacket();vTaskDelay(pdMS_TO_TICKS(200));}
      uint32_t until=millis()+1800;
      while((int32_t)(until-millis())>0){
        int bytes=udp.parsePacket();if(bytes>0){char packet[768];int len=udp.read(packet,sizeof(packet)-1);if(len>0){packet[len]=0;String msg(packet);String lower=msg;lower.toLowerCase();if(lower.indexOf("urn:bambulab-com:device:3dprinter:1")>=0){String serial=discoveryHeader(msg,"usn:"),name=discoveryHeader(msg,"devname.bambu.com:");serial.toUpperCase();if(serial.length()>=6&&serial.length()<=40)addDiscovered("bambu",name,udp.remoteIP().toString(),8883,serial);}}}
        vTaskDelay(pdMS_TO_TICKS(35));
      }
      udp.stop();
    }
    int n=MDNS.queryService("octoprint","tcp");
    for(int i=0;i<n&&i<12;i++){IPAddress ip=MDNS.address(i);uint16_t port=MDNS.port(i);if(ip!=IPAddress()&&port!=443)addDiscovered("octoprint",MDNS.instanceName(i),ip.toString(),port);}
    n=MDNS.queryService("http","tcp");
    for(int i=0;i<n&&i<10&&discoveryCount<12;i++){
      IPAddress ip=MDNS.address(i);uint16_t port=MDNS.port(i);String name=MDNS.instanceName(i);
      if(ip==IPAddress()||ip==WiFi.localIP())continue;
      if(isMoonraker(ip,port))addDiscovered("moonraker",name,ip.toString(),port);
      else if(port!=7125&&isMoonraker(ip,7125))addDiscovered("moonraker",name,ip.toString(),7125);
    }
    // Creality K2 often does not advertise Moonraker via mDNS. Probe only
    // the ESP32's own /24, constrained further by the actual subnet mask.
    IPAddress own=WiFi.localIP(),mask=WiFi.subnetMask(),gateway=WiFi.gatewayIP();
    discoveryPhase=1;discoveryTotal=508;
    for(int pass=0;pass<2&&discoveryCount<12&&WiFi.status()==WL_CONNECTED;pass++){
      uint16_t port=pass==0?7125:80;
      
      for(int host=1;host<255&&discoveryCount<12&&WiFi.status()==WL_CONNECTED;host++){
        discoveryChecked++;
        IPAddress target(own[0],own[1],own[2],host);
        bool sameSubnet=true;for(int octet=0;octet<4;octet++)if((target[octet]&mask[octet])!=(own[octet]&mask[octet]))sameSubnet=false;
        if(!sameSubnet||target==own||target==gateway){vTaskDelay(1);continue;}
        WiFiClient probe;
        if(probe.connect(target,port,180)){
          probe.stop();String name;
          if(isMoonraker(target,port,&name))addDiscovered("moonraker",name.length()?name:"Creality / Moonraker",target.toString(),port);
        }
        probe.stop();vTaskDelay(1);
      }
    }
  }
  discoveryPhase=2;discoveryRunning=false;vTaskDelete(nullptr);
}

void jsonReply(int code, JsonDocument& d){String s;serializeJson(d,s);server.send(code,"application/json",s);}
String passwordDigest(const String& salt,const String& password){
  String material=salt+":"+password;
  unsigned char digest[32];
  mbedtls_sha256((const unsigned char*)material.c_str(),material.length(),digest,0);
  char hex[65];for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
  return String(hex);
}
bool constantTimeEqual(const String& a,const String& b){
  if(a.length()!=b.length())return false;
  uint8_t difference=0;
  for(size_t i=0;i<a.length();i++)difference|=(uint8_t)(a[i]^b[i]);
  return difference==0;
}
void loadAdminSettings(){
  Preferences storage;
  if(!storage.begin("layerglow-sec",true))return;
  adminSalt=storage.getString("salt","");adminHash=storage.getString("hash","");
  apPassword=storage.getString("apPass","");storage.end();
  if(adminSalt.length()!=32||adminHash.length()!=64)adminHash="";
  if(apPassword.length()<12||apPassword.length()>63)apPassword="";
}
bool saveAdminSettings(const String& password,const String& newAP){
  uint8_t saltBytes[16];esp_fill_random(saltBytes,sizeof(saltBytes));
  char saltHex[33];for(int i=0;i<16;i++)snprintf(saltHex+i*2,3,"%02x",saltBytes[i]);
  String nextSalt(saltHex),nextHash=passwordDigest(nextSalt,password);
  Preferences storage;if(!storage.begin("layerglow-sec",false))return false;
  bool ok=storage.putString("salt",nextSalt)==nextSalt.length();
  ok=storage.putString("apPass",newAP)==newAP.length()&&ok;
  // Write the pairing marker last. A partial write must remain unpaired.
  if(ok)ok=storage.putString("hash",nextHash)==nextHash.length();
  storage.end();if(ok){adminSalt=nextSalt;adminHash=nextHash;apPassword=newAP;}return ok;
}
bool rotateSetupPassword(bool clearAdmin){
  const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  uint8_t random[20];esp_fill_random(random,sizeof(random));
  String generated;for(uint8_t value:random)generated+=alphabet[value%(sizeof(alphabet)-1)];
  Preferences storage;
  if(!storage.begin("layerglow-sec",false))return false;
  bool ok=storage.putString("apPass",generated)==generated.length();
  if(ok&&clearAdmin&&!adminHash.isEmpty())ok=storage.remove("hash");
  storage.end();
  if(!ok)return false;
  apPassword=generated;
  if(clearAdmin)adminHash="";
  Serial.println("LayerGlow-Setup password: "+apPassword);
  return true;
}
void provisionFreshAP(){
  if(wifiProfileCount||!apPassword.isEmpty())return;
  if(!rotateSetupPassword(false))Serial.println("Setup AP disabled until secure password storage works");
}
bool checkAdminCredentials(){
  String header=server.header("Authorization");
  if(!header.startsWith("Basic ")||header.length()>180)return false;
  const String encoded=header.substring(6);
  unsigned char decoded[128]={};size_t length=0;
  if(mbedtls_base64_decode(decoded,sizeof(decoded)-1,&length,(const unsigned char*)encoded.c_str(),encoded.length())!=0)return false;
  decoded[length]=0;
  String credentials((const char*)decoded);
  if(!credentials.startsWith("admin:"))return false;
  return constantTimeEqual(passwordDigest(adminSalt,credentials.substring(6)),adminHash);
}
bool requireAdmin(bool respond=true){
  if(adminHash.isEmpty()){
    if(respond)server.send(423,"text/plain","Pair this device using its physical BOOT button");
    return false;
  }
  if(authRetryAt&&(int32_t)(millis()-authRetryAt)<0){if(respond)server.send(429,"text/plain","Try again shortly");return false;}
  authRetryAt=0;
  if(checkAdminCredentials()){authFailures=0;return true;}
  if(++authFailures>=8){authRetryAt=millis()+30000;authFailures=0;}
  if(respond){server.sendHeader("WWW-Authenticate","Basic realm=\"LayerGlow\", charset=\"UTF-8\"");server.send(401,"text/plain","Administrator password required");}
  return false;
}
void physicalAdminRecovery(uint32_t now){
  if(digitalRead(PAIR_BUTTON_PIN)!=LOW){physicalResetAt=0;return;}
  if(!physicalResetAt){physicalResetAt=now;return;}
  if(now-physicalResetAt<10000||restartAfterPairing)return;
  if(rotateSetupPassword(true)){
    Serial.println("Setup Wi-Fi and admin access reset. Release BOOT to restart, then pair again.");
    restartAfterPairing=true;pairButtonReleasedAt=0;
  }else Serial.println("Recovery failed: could not save a new setup Wi-Fi password.");
  physicalResetAt=now;
}
bool verifyOtaSignature(const uint8_t digest[32]){
  if(!otaSignatureValid||otaSignatureLength!=256)return false;
  mbedtls_pk_context key;mbedtls_pk_init(&key);
  int result=mbedtls_pk_parse_public_key(&key,(const unsigned char*)OTA_PUBLIC_KEY,sizeof(OTA_PUBLIC_KEY));
  if(result==0)result=mbedtls_pk_verify(&key,MBEDTLS_MD_SHA256,digest,32,otaSignature,otaSignatureLength);
  mbedtls_pk_free(&key);return result==0;
}
bool allowMutation(bool respond=true){
  if(!requireAdmin(respond))return false;
  String host=server.hostHeader();host.toLowerCase();
  if(host.endsWith(":80"))host.remove(host.length()-3);
  bool local=host==WiFi.localIP().toString()||host==WiFi.softAPIP().toString()||host==webHost||host==webHost+".local";
  String origin=server.header("Origin");origin.toLowerCase();
  if(origin.endsWith(":80"))origin.remove(origin.length()-3);
  if(!local||(!origin.isEmpty()&&origin!="http://"+host)){
    if(respond)server.send(403,"text/plain","Request origin rejected");
    return false;
  }
  return true;
}
void routes(){
  server.on("/",HTTP_GET,[](){
    server.sendHeader("Cache-Control","no-store");
    if(adminHash.isEmpty()){
      server.send(200,"text/html; charset=utf-8",R"PAIR(<!doctype html><html><meta name="viewport" content="width=device-width,initial-scale=1"><title>LayerGlow pairing</title><style>body{font:18px system-ui;max-width:32rem;margin:3rem auto;padding:1rem}input,button{font:inherit;display:block;width:100%;box-sizing:border-box;margin:1rem 0;padding:.7rem}</style><h1>LayerGlow · pierwsze zabezpieczenie</h1><p>Przytrzymaj fizyczny przycisk BOOT na urządzeniu podczas zapisywania. / Hold the device's BOOT button while saving.</p><form method="post" action="/api/security/enroll"><label>Hasło administratora / Admin password<input name="adminPassword" type="password" minlength="12" maxlength="64" required></label><label>Nowe hasło sieci LayerGlow-Setup / New setup Wi-Fi password<input name="apPassword" type="password" minlength="12" maxlength="63" required></label><button type="submit">Zapisz i uruchom ponownie / Save and restart</button></form><p>Zapisz oba hasła. Po restarcie przeglądarka poprosi o użytkownika <b>admin</b> i nowe hasło. / Keep both passwords; after restart log in as <b>admin</b>.</p></html>)PAIR");
      return;
    }
    if(!requireAdmin())return;
    server.send_P(200,"text/html; charset=utf-8",PAGE);
  });
  server.on("/api/security/enroll",HTTP_POST,[](){
    if(!adminHash.isEmpty()){server.send(409,"text/plain","Already paired");return;}
    String host=server.hostHeader(),origin=server.header("Origin");host.toLowerCase();origin.toLowerCase();
    if(host.endsWith(":80"))host.remove(host.length()-3);
    bool local=host==WiFi.localIP().toString()||host==WiFi.softAPIP().toString()||host==webHost||host==webHost+".local";
    if(!local||(!origin.isEmpty()&&origin!="http://"+host)){server.send(403,"text/plain","Invalid pairing origin");return;}
    if(digitalRead(PAIR_BUTTON_PIN)!=LOW){server.send(403,"text/plain","Hold BOOT while submitting");return;}
    String password=server.arg("adminPassword"),newAP=server.arg("apPassword");
    if(password.length()<12||password.length()>64||newAP.length()<12||newAP.length()>63){server.send(400,"text/plain","Both passwords must have at least 12 characters");return;}
    if(!saveAdminSettings(password,newAP)){server.send(500,"text/plain","Could not save credentials");return;}
    server.send(200,"text/html; charset=utf-8","<h1>LayerGlow paired</h1><p>Release BOOT to restart the device. / Puść BOOT, aby uruchomić urządzenie ponownie.</p><p>Reconnect to LayerGlow-Setup with your new Wi-Fi password if needed; log in as admin.</p>");
    restartAfterPairing=true;pairButtonReleasedAt=0;
  });
  server.on("/api/ota/capacity",HTTP_GET,[](){if(!requireAdmin())return;JsonDocument d;const esp_partition_t* slot=esp_ota_get_next_update_partition(nullptr);d["appSlotBytes"]=slot?slot->size:0;jsonReply(200,d);});
  server.on("/api/network/info",HTTP_GET,[](){if(!requireAdmin())return;
    JsonDocument d;bool online=WiFi.status()==WL_CONNECTED;
    JsonObject w=d["wifi"].to<JsonObject>();w["connected"]=online;w["ssid"]=online?WiFi.SSID():ssid;w["ip"]=online?WiFi.localIP().toString():"";w["gateway"]=online?WiFi.gatewayIP().toString():"";w["subnet"]=online?WiFi.subnetMask().toString():"";w["dns"]=online?WiFi.dnsIP().toString():"";w["rssi"]=online?WiFi.RSSI():0;w["channel"]=online?WiFi.channel():0;w["mac"]=WiFi.macAddress();
    JsonObject a=d["ap"].to<JsonObject>();a["active"]=apMode;a["ssid"]=apMode?AP_NAME:"";a["ip"]=apMode?WiFi.softAPIP().toString():"";a["clients"]=apMode?WiFi.softAPgetStationNum():0;
    JsonObject p=d["printer"].to<JsonObject>();p["connected"]=printerConnected;p["test"]=simEnabled;p["type"]=printerType;p["ip"]=printerIP;p["port"]=printerType=="bambu"?8883:printerPort;p["error"]=lastError;p["enabled"]=printerEnabled;p["state"]=printerState;p["progress"]=printerProgress;p["filename"]=printerFile;p["lastReadAgeSeconds"]=lastPrinterReadAt?((millis()-lastPrinterReadAt)/1000):-1;
    d["webHost"]=webHost;d["mdnsReady"]=online&&mdnsStarted&&mdnsHostActive==webHost&&!mdnsNeedsRestart;
    JsonArray saved=d["savedNetworks"].to<JsonArray>();
    for(int i=0;i<wifiProfileCount;i++){
      JsonObject item=saved.add<JsonObject>();item["index"]=i;item["ssid"]=wifiProfiles[i].name;
      item["active"]=i==activeWifi;item["connected"]=online&&WiFi.SSID()==wifiProfiles[i].name;
      item["ipMode"]=wifiProfiles[i].useStatic?"static":"dhcp";
      if(wifiProfiles[i].useStatic){item["staticIP"]=wifiProfiles[i].staticIP;item["gateway"]=wifiProfiles[i].gateway;item["subnet"]=wifiProfiles[i].subnet;item["dns"]=wifiProfiles[i].dns;}
    }
    d["maxSavedNetworks"]=MAX_WIFI_PROFILES;addSavedPrinters(d);
    jsonReply(200,d);
  });
  // Public, minimal health response keeps the v2.51 updater able to confirm the first secured boot.
  server.on("/api/health",HTTP_GET,[](){JsonDocument d;d["version"]="2.53";d["bootId"]=String(bootIdentifier,HEX);d["uptimeSeconds"]=millis()/1000;jsonReply(200,d);});
  server.on("/api/status",HTTP_GET,[](){if(!requireAdmin())return;JsonDocument d;d["version"]="2.53";d["bootId"]=String(bootIdentifier,HEX);d["updateAvailable"]=updateAvailable;d["lightingBackup"]=lightingBackupAvailable;d["printerEnabled"]=printerEnabled;d["connected"]=printerConnected;d["ap"]=apMode;d["state"]=normalState(printerState);d["progress"]=printerProgress;d["filename"]=printerFile;d["error"]=lastError;d["ssid"]=ssid;d["webHost"]=webHost;d["ip"]=printerIP;d["port"]=printerPort;d["printerType"]=printerType;d["moonIP"]=moonIP;d["bambuIP"]=bambuIP;d["octoIP"]=octoIP;d["prusaIP"]=prusaIP;d["moonPort"]=moonPort;d["octoPort"]=octoPort;d["prusaPort"]=prusaPort;d["hasOctoKey"]=!octoKey.isEmpty();d["hasPrusaKey"]=!prusaKey.isEmpty();d["bambuSerial"]=bambuSerial;d["hasBambuCode"]=!bambuAccessCode.isEmpty();d["activePrinterId"]=activePrinter>=0?printerProfiles[activePrinter].id:"";d["interval"]=intervalMs;d["ledCount"]=activeLedCount;d["sim"]=simEnabled;d["simState"]=stateNames[simStatus];d["simProgress"]=simProgress;d["simCycleElapsed"]=simEnabled&&simStatus==0?(millis()-simCycleStartedAt)%SIM_CYCLE_MS:0;addRememberedColours(d);addRows(d);jsonReply(200,d);});
  server.on("/api/printers/discovery",HTTP_POST,[](){if(!allowMutation())return;
    if(WiFi.status()!=WL_CONNECTED){server.send(503,"text/plain","Wi-Fi not connected");return;}
    if(otaUploadInProgress||wifiScanActive){server.send(409,"text/plain","Network is busy");return;}
    if(!discoveryRunning){discoveryCount=0;discoveryChecked=0;discoveryTotal=0;discoveryPhase=0;discoveryRunning=true;if(xTaskCreate(printerDiscoveryTask,"printer-discovery",8192,nullptr,1,nullptr)!=pdPASS){discoveryRunning=false;server.send(503,"text/plain","Discovery unavailable");return;}}
    JsonDocument d;d["searching"]=true;jsonReply(200,d);
  });
  server.on("/api/printers/discovery",HTTP_GET,[](){if(!requireAdmin())return;JsonDocument d;d["searching"]=discoveryRunning;d["phase"]=discoveryPhase;d["checked"]=discoveryChecked;d["total"]=discoveryTotal;if(!discoveryRunning){JsonArray a=d["printers"].to<JsonArray>();for(int i=0;i<discoveryCount;i++){JsonObject p=a.add<JsonObject>();p["type"]=discoveryResults[i].type;p["name"]=discoveryResults[i].name;p["ip"]=discoveryResults[i].ip;p["port"]=discoveryResults[i].port;p["serial"]=discoveryResults[i].serial;}}jsonReply(200,d);});
  server.on("/api/wifi/scan",HTTP_GET,[](){if(!requireAdmin())return;
    JsonDocument d;
    if(!wifiScanActive){
      if(discoveryRunning){server.send(409,"text/plain","Printer discovery in progress");return;}
      startWifiScan(millis());
    }
    int n=pollWifiScan(millis());
    if(n==-1){d["scanning"]=true;jsonReply(200,d);return;}
    if(n<0){d["error"]="scan_failed";jsonReply(503,d);return;}
    JsonArray a=d["networks"].to<JsonArray>();
    for(int i=0;i<n && i<35;i++){
      String name=WiFi.SSID(i);if(name.isEmpty())continue;
      JsonObject net=a.add<JsonObject>();net["ssid"]=name;net["rssi"]=WiFi.RSSI(i);net["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    finishWifiScan(millis());d["scanning"]=false;jsonReply(200,d);
  });
  server.on("/api/wifi",HTTP_POST,[](){if(!allowMutation())return;
    String newSSID=server.arg("ssid");newSSID.trim();
    String newHost=server.hasArg("host")?server.arg("host"):webHost;newHost.trim();newHost.toLowerCase();if(newHost.endsWith(".local"))newHost.remove(newHost.length()-6);
    if(newSSID.length()>32){server.send(400,"text/plain","Invalid SSID");return;}
    if(!validWebHost(newHost)){server.send(400,"text/plain","Invalid device name");return;}
    String newPassword=server.arg("pass");
    if(newPassword.length()>64){server.send(400,"text/plain","Wi-Fi password too long");return;}
    String ipMode=server.arg("ipMode");if(ipMode.isEmpty())ipMode="dhcp";
    if(newSSID.length()&&ipMode!="dhcp"&&ipMode!="static"){server.send(400,"text/plain","Choose DHCP or static IP");return;}
    String staticIP=server.arg("staticIP"),staticGateway=server.arg("staticGateway"),staticSubnet=server.arg("staticSubnet"),staticDns=server.arg("staticDns");
    staticIP.trim();staticGateway.trim();staticSubnet.trim();staticDns.trim();
    if(newSSID.length()&&ipMode=="static"){
      IPAddress address,gateway,subnet,dns;
      if(!address.fromString(staticIP)||!gateway.fromString(staticGateway)||!subnet.fromString(staticSubnet)||!dns.fromString(staticDns)||
         address==IPAddress(0,0,0,0)||gateway==IPAddress(0,0,0,0)||subnet==IPAddress(0,0,0,0)||dns==IPAddress(0,0,0,0)||
         ((uint32_t)address&(uint32_t)subnet)!=((uint32_t)gateway&(uint32_t)subnet)||address==gateway){
        server.send(400,"text/plain","Invalid static IP, router, subnet or DNS");return;
      }
    }
    int index=-1;for(int i=0;i<wifiProfileCount;i++)if(wifiProfiles[i].name==newSSID){index=i;break;}
    if(newSSID.length()&&index<0&&wifiProfileCount>=MAX_WIFI_PROFILES){server.send(409,"text/plain","Saved Wi-Fi list is full");return;}
    bool hostChanged=webHost!=newHost,changed=false;
    if(newSSID.length()){
      if(index<0)index=wifiProfileCount++;
      String password=newPassword.length()?newPassword:(wifiProfiles[index].name==newSSID?wifiProfiles[index].password:"");
      changed=index!=activeWifi||wifiProfiles[index].name!=newSSID||wifiProfiles[index].password!=password||
        wifiProfiles[index].useStatic!=(ipMode=="static")||(ipMode=="static"&&
        (wifiProfiles[index].staticIP!=staticIP||wifiProfiles[index].gateway!=staticGateway||wifiProfiles[index].subnet!=staticSubnet||wifiProfiles[index].dns!=staticDns))||
        WiFi.status()!=WL_CONNECTED||WiFi.SSID()!=newSSID;
      wifiProfiles[index].name=newSSID;wifiProfiles[index].password=password;wifiProfiles[index].useStatic=ipMode=="static";
      wifiProfiles[index].staticIP=wifiProfiles[index].useStatic?staticIP:"";wifiProfiles[index].gateway=wifiProfiles[index].useStatic?staticGateway:"";
      wifiProfiles[index].subnet=wifiProfiles[index].useStatic?staticSubnet:"";wifiProfiles[index].dns=wifiProfiles[index].useStatic?staticDns:"";
      activeWifi=index;ssid=newSSID;wifiPassword=password;
    }
    webHost=newHost;
    if(!saveWifiSettings()){server.send(500,"text/plain","Could not save settings");return;}
    if(hostChanged){mdnsNeedsRestart=true;mdnsChangeAt=millis()+2500;lastMdnsAttempt=0;}
    if(changed){wifiChangePending=true;wifiChangeAt=millis()+1500;}
    server.send(200,"text/plain; charset=utf-8",changed?"Switching Wi-Fi":"Saved");
  });
  server.on("/api/wifi/select",HTTP_POST,[](){if(!allowMutation())return;
    String value=server.arg("index");
    if(value.length()!=1||value[0]<'0'||value[0]>'7'||value.toInt()>=wifiProfileCount){server.send(400,"text/plain","Invalid saved network");return;}
    activeWifi=value.toInt();ssid=wifiProfiles[activeWifi].name;wifiPassword=wifiProfiles[activeWifi].password;
    if(!saveWifiSettings()){server.send(500,"text/plain","Could not save settings");return;}wifiChangePending=true;wifiChangeAt=millis()+1500;
    server.send(200,"text/plain; charset=utf-8","Switching Wi-Fi");
  });
  server.on("/api/wifi/delete",HTTP_POST,[](){if(!allowMutation())return;
    String value=server.arg("index");
    if(value.length()!=1||value[0]<'0'||value[0]>'7'||value.toInt()>=wifiProfileCount){server.send(400,"text/plain","Invalid saved network");return;}
    int index=value.toInt();bool removedActive=index==activeWifi;
    for(int i=index;i+1<wifiProfileCount;i++)wifiProfiles[i]=wifiProfiles[i+1];
    wifiProfiles[--wifiProfileCount]=WifiProfile();
    if(wifiProfileCount){if(index<activeWifi)activeWifi--;else if(removedActive)activeWifi=min(index,(int)wifiProfileCount-1);ssid=wifiProfiles[activeWifi].name;wifiPassword=wifiProfiles[activeWifi].password;}
    else{activeWifi=0;ssid="";wifiPassword="";}
    if(!saveWifiSettings()){server.send(500,"text/plain","Could not save settings");return;}
    if(removedActive){wifiChangePending=true;wifiChangeAt=millis()+1500;}
    server.send(200,"text/plain; charset=utf-8","Removed");
  });
  server.on("/api/printers/saved",HTTP_GET,[](){if(!requireAdmin())return;JsonDocument d;addSavedPrinters(d);jsonReply(200,d);});
  server.on("/api/printer",HTTP_POST,[](){if(!allowMutation())return;
    String id=server.arg("profileId");int index=id.isEmpty()?-1:savedPrinterIndex(id);
    if(!id.isEmpty()&&index<0){server.send(404,"text/plain","Saved printer not found");return;}
    if(index<0&&printerProfileCount>=MAX_PRINTER_PROFILES){server.send(400,"text/plain","Saved printer limit reached");return;}
    PrinterProfile p=index>=0?printerProfiles[index]:PrinterProfile();
    String oldType=p.type;p.name=server.arg("printerName");p.name.trim();p.type=server.arg("printerType");
    p.ip=server.arg("ip");p.ip.trim();p.mac=server.arg("printerMAC");p.mac.trim();p.mac.toUpperCase();
    p.serial=server.arg("bambuSerial");p.serial.trim();p.serial.toUpperCase();
    p.port=p.type=="bambu"?8883:strictCount(server.arg("port"),1,65535);
    if(p.type!=oldType)p.secret="";
    String secret=server.arg(p.type=="bambu"?"bambuCode":"apiKey");secret.trim();
    if(!secret.isEmpty())p.secret=secret;
    if(p.type=="moonraker"){p.secret="";p.serial="";}else if(p.type!="bambu")p.serial="";
    if(p.name.isEmpty())p.name=p.type+" · "+p.ip;
    p.network=WiFi.status()==WL_CONNECTED?WiFi.SSID():ssid;
    if(index<0){do{p.id=String(esp_random(),HEX);while(p.id.length()<8)p.id="0"+p.id;}while(savedPrinterIndex(p.id)>=0);}
    if(!validPrinterProfile(p)|| (p.type!="bambu"&&strictCount(server.arg("port"),1,65535)<1)){
      server.send(400,"text/plain","Invalid saved printer settings");return;
    }
    PrinterProfile next[MAX_PRINTER_PROFILES];for(int i=0;i<printerProfileCount;i++)next[i]=printerProfiles[i];
    int count=printerProfileCount;if(index<0)index=count++;next[index]=p;
    if(!commitPrinterProfiles(next,count,index,true)){server.send(500,"text/plain","Could not save settings");return;}
    applySavedPrinter();server.send(200,"text/plain; charset=utf-8","Saved");
  });
  server.on("/api/printers/select",HTTP_POST,[](){if(!allowMutation())return;
    int index=savedPrinterIndex(server.arg("id"));if(index<0){server.send(404,"text/plain","Saved printer not found");return;}
    if(!commitPrinterProfiles(printerProfiles,printerProfileCount,index,true)){server.send(500,"text/plain","Could not save settings");return;}
    applySavedPrinter();server.send(200,"text/plain","Saved");
  });
  server.on("/api/printers/delete",HTTP_POST,[](){if(!allowMutation())return;
    int index=savedPrinterIndex(server.arg("id"));if(index<0){server.send(404,"text/plain","Saved printer not found");return;}
    PrinterProfile next[MAX_PRINTER_PROFILES];int count=0;
    for(int i=0;i<printerProfileCount;i++)if(i!=index)next[count++]=printerProfiles[i];
    int active=activePrinter==index?-1:activePrinter>index?activePrinter-1:activePrinter;
    bool removedActive=index==activePrinter;
    if(!commitPrinterProfiles(next,count,active,printerEnabled&&!removedActive)){server.send(500,"text/plain","Could not save settings");return;}
    if(removedActive)applySavedPrinter();server.send(200,"text/plain","Removed");
  });
  server.on("/api/printer/disconnect",HTTP_POST,[](){if(!allowMutation())return;
    if(!commitPrinterProfiles(printerProfiles,printerProfileCount,activePrinter,false)){server.send(500,"text/plain","Could not save settings");return;}
    applySavedPrinter();server.send(200,"text/plain; charset=utf-8","Disconnected");
  });
  server.on("/api/lighting",HTTP_POST,[](){if(!allowMutation())return;
    int leds=strictCount(server.arg("ledCount"),5,MAX_LED_COUNT);
    if(leds<0){server.send(400,"text/plain","Invalid LED count");return;}
    String colors[6],extras[6][11];uint8_t colourCounts[6],br[6],styles[6];bool enabled[6],lightsOn[6];uint16_t periods[6];
    for(int i=0;i<6;i++){
      String id=i==0?"progress":String(stateNames[i]);
      colors[i]=normalizeColor(server.arg("c_"+id),"");int b=server.arg("b_"+id).toInt(),p=server.arg("pp_"+id).toInt(),style=server.arg("ps_"+id).toInt();
      if(colors[i].isEmpty()||b<0||b>100||p<0||p>5000||style<0||style>13||style==4){server.send(400,"text/plain","Invalid LED row: "+id);return;}
      int n=server.hasArg("n_"+id)?strictCount(server.arg("n_"+id),1,12):1;
      if(n<0){server.send(400,"text/plain","Invalid LED row: "+id);return;}
      colourCounts[i]=n;
      for(int j=0;j<11;j++){
        String field=String("c")+(j+1)+"_"+id;
        extras[i][j]=server.hasArg(field)?normalizeColor(server.arg(field),""):rowExtraColors[i][j];
        if(extras[i][j].isEmpty()){server.send(400,"text/plain","Invalid LED row: "+id);return;}
      }
      br[i]=b;periods[i]=p;styles[i]=style;enabled[i]=server.hasArg("pe_"+id);lightsOn[i]=server.hasArg("on_"+id);
    }
    activeLedCount=leds;
    for(int i=0;i<6;i++){rowColors[i]=colors[i];rowColorCount[i]=colourCounts[i];for(int j=0;j<11;j++)rowExtraColors[i][j]=extras[i][j];rowBrightness[i]=br[i];rowPeriod[i]=periods[i];rowStyle[i]=styles[i];rowPulse[i]=enabled[i];rowEnabled[i]=lightsOn[i];}
    bool saved=saveLighting();updateLEDs();if(!saved){server.send(500,"text/plain","Could not save settings");return;}server.send(200,"text/plain","Saved");
  });
  server.on("/api/lighting/defaults",HTTP_POST,[](){if(!allowMutation())return;
    activeLedCount=24;intervalMs=500;
    for(int i=0;i<6;i++){rowColors[i]=DEFAULT_ROW_COLORS[i];rowColorCount[i]=1;for(int j=0;j<11;j++)rowExtraColors[i][j]=DEFAULT_EXTRA_COLORS[j];rowBrightness[i]=65;rowPulse[i]=false;rowEnabled[i]=true;rowStyle[i]=0;rowPeriod[i]=1400;}
    bool saved=saveLighting();updateLEDs();if(!saved){server.send(500,"text/plain","Could not save settings");return;}JsonDocument d;d["ok"]=true;d["ledCount"]=activeLedCount;d["interval"]=intervalMs;addRows(d);jsonReply(200,d);
  });
  server.on("/api/lighting/backup",HTTP_POST,[](){if(!allowMutation())return;if(!saveLightingBackup()){server.send(500,"text/plain","LED backup failed");return;}JsonDocument d;d["ok"]=true;d["lightingBackup"]=true;jsonReply(200,d);});
  server.on("/api/lighting/restore",HTTP_POST,[](){if(!allowMutation())return;if(!restoreLightingBackup()){server.send(500,"text/plain","Could not restore LED settings");return;}JsonDocument d;d["ok"]=true;d["lightingBackup"]=true;d["ledCount"]=activeLedCount;d["interval"]=intervalMs;addRows(d);jsonReply(200,d);});
  server.on("/api/colours/recent",HTTP_POST,[](){if(!allowMutation())return;String colour=normalizeColor(server.arg("colour"),"");if(colour.isEmpty()){server.send(400,"text/plain","Invalid colour");return;}if(!rememberColour(colour)){server.send(500,"text/plain","Could not save colour");return;}JsonDocument d;d["ok"]=true;addRememberedColours(d);jsonReply(200,d);});
  server.on("/api/simulation",HTTP_POST,[](){if(!allowMutation())return;bool enabled=server.arg("enabled")=="1";int status=statusIndex(server.arg("status"));if(enabled&&(!simEnabled||simStatus!=status)){simProgress=0;simCycleStartedAt=millis();}if(!enabled)simProgress=0;simEnabled=enabled;simStatus=status;updateLEDs();JsonDocument d;d["ok"]=true;jsonReply(200,d);});
  server.on("/api/update/available",HTTP_POST,[](){if(!allowMutation())return;updateAvailable=server.arg("available")=="1";server.send(200,"text/plain","OK");});
  server.on("/api/update",HTTP_POST,[](){
    if(!allowMutation())return;
    bool ok=otaImageReady&&otaBytesWritten>0&&!Update.hasError()&&updateError.isEmpty();
    if(!ok&&updateError.isEmpty())updateError="No completed firmware upload";
    otaImageReady=false;otaUploadInProgress=false;
    server.send(ok?200:500,"text/plain",ok?"OK":("Update failed: "+updateError));
    if(ok){delay(350);ESP.restart();}
  },[](){
    HTTPUpload& upload=server.upload();
    if(upload.status==UPLOAD_FILE_START){
      otaImageReady=false;otaBytesWritten=0;updateError="";
      if(otaHashStarted){mbedtls_sha256_free(&otaHash);otaHashStarted=false;}
      otaSignatureValid=false;otaSignatureLength=0;
      if(!allowMutation(false)){updateError="Request origin rejected";return;}
      otaUploadInProgress=true;
      String filename=upload.filename;filename.toLowerCase();
      if(!filename.endsWith(".bin")){updateError="Expected .bin";return;}
      String signature=server.header("X-LayerGlow-Signature");
      if(signature.length()<340||signature.length()>348||mbedtls_base64_decode(otaSignature,sizeof(otaSignature),&otaSignatureLength,(const unsigned char*)signature.c_str(),signature.length())!=0||otaSignatureLength!=sizeof(otaSignature)){
        updateError="Signed firmware and matching .sig file required";return;
      }
      otaSignatureValid=true;
      if(!Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)){updateError=Update.errorString();return;}
      mbedtls_sha256_init(&otaHash);mbedtls_sha256_starts(&otaHash,0);otaHashStarted=true;
    }else if(upload.status==UPLOAD_FILE_WRITE){
      if(otaUploadInProgress&&updateError.isEmpty()){
        size_t written=Update.write(upload.buf,upload.currentSize);otaBytesWritten+=written;
        if(written!=upload.currentSize)updateError=Update.errorString();
        else if(otaHashStarted)mbedtls_sha256_update(&otaHash,upload.buf,written);
      }
    }else if(upload.status==UPLOAD_FILE_END){
      uint8_t digest[32]={};
      if(otaHashStarted){mbedtls_sha256_finish(&otaHash,digest);mbedtls_sha256_free(&otaHash);otaHashStarted=false;}
      if(otaUploadInProgress&&updateError.isEmpty()&&otaBytesWritten>0&&verifyOtaSignature(digest)){
        if(Update.end(true))otaImageReady=true;else updateError=Update.errorString();
      }else{if(updateError.isEmpty())updateError="Firmware signature mismatch";Update.abort();}
      otaUploadInProgress=false;
    }else if(upload.status==UPLOAD_FILE_ABORTED){
      if(otaHashStarted){mbedtls_sha256_free(&otaHash);otaHashStarted=false;}
      Update.abort();updateError="Aborted";otaImageReady=false;otaUploadInProgress=false;
    }
  });
}
void setup(){bootIdentifier=esp_random();pinMode(STATUS_LED_PIN,OUTPUT);digitalWrite(STATUS_LED_PIN,HIGH);Serial.begin(115200);pinMode(PAIR_BUTTON_PIN,INPUT_PULLUP);delay(450);loadAdminSettings();loadSettings();provisionFreshAP();loadRememberedColours();seedRememberedColours();bool migrateName=webHost=="k2-led";if(migrateName)webHost="layerglow";if(migrateLegacyWifi||migrateName)saveWifiSettings();webHost.toLowerCase();if(!validWebHost(webHost))webHost="layerglow";{Preferences backup;if(backup.begin("k2ledbak",true)){lightingBackupAvailable=backup.getBool("valid",false);backup.end();}}if(LED_ENABLED){strip.updateLength(activeLedCount);strip.begin();strip.clear();strip.show();}loadPrinterProfiles();bambuTLS.setInsecure();bambuTLS.setHandshakeTimeout(2);bambuMqtt.setCallback(onBambuReport);bambuMqtt.setKeepAlive(15);bambuMqtt.setSocketTimeout(2);startNetwork();if(!startHttpPrinterTask())Serial.println("Printer worker unavailable");const char* requestHeaders[]={"Origin","Authorization","X-LayerGlow-Signature"};server.collectHeaders(requestHeaders,3);routes();server.begin();Serial.println("LayerGlow v2.53 web server ready");}
void loop(){
  server.handleClient();
  uint32_t now=millis();
  // GPIO9 is a boot strap pin: restarting while BOOT is held enters the ROM downloader.
  if(restartAfterPairing){
    if(digitalRead(PAIR_BUTTON_PIN)==LOW)pairButtonReleasedAt=0;
    else if(!pairButtonReleasedAt)pairButtonReleasedAt=now;
    else if(now-pairButtonReleasedAt>=500)ESP.restart();
  }
  physicalAdminRecovery(now);
  maintainNetwork(now);
  maintainBambu(now);
  now=millis();
  // Active LOW: update = two long flashes, pause, one short flash, pause.
  bool ledLit;
  if(WiFi.status()!=WL_CONNECTED){
    ledLit=(now%1200)<90;
  }else if(updateAvailable){
    uint32_t beat=now%2200;
    ledLit=(beat<350)||(beat>=530&&beat<880)||(beat>=1400&&beat<1490);
  }else{
    uint32_t beat=now%1200;
    ledLit=(beat<90)||(beat>=200&&beat<290);
  }
  digitalWrite(STATUS_LED_PIN,ledLit?LOW:HIGH);
  // GitHub TLS could block this single-core web server for seconds.
  // The browser checks GitHub and reports the update flag via /api/update/available.
  maintainHttpPrinter(now);
  static uint32_t lastRandomRender=0;
  if(now-lastRandomRender>=40){
    String activeState=simEnabled?String(stateNames[simStatus]):(!printerConnected?"standby":normalState(printerState));
    int idx=statusIndex(activeState);
    if(rowEnabled[idx]&&(rowStyle[idx]>=6||(rowPulse[idx]&&rowStyle[idx]>=2))){lastRandomRender=now;updateLEDs();}
  }
  if(simEnabled&&simStatus==0){
    uint32_t phase=(now-simCycleStartedAt)%SIM_CYCLE_MS;
    int nextProgress=min(100,(int)(phase/SIM_STEP_MS)*5);
    if(nextProgress!=simProgress){simProgress=nextProgress;updateLEDs();}
  }
  if(rowPulse[statusIndex(simEnabled?String(stateNames[simStatus]):(!printerConnected?String("standby"):normalState(printerState)))]&&now-lastPulseFrame>=40){lastPulseFrame=now;updateLEDs();}
  delay(2);
}

