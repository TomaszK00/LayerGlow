"""Compile actual profile storage/load code against the pinned ArduinoJson library."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'PlatformIO/src/main.cpp').read_text()
structs=s[s.index('constexpr uint8_t MAX_PRINTER_PROFILES'):s.index('WiFiClientSecure bambuTLS;')]
functions=s[s.index('void updateLEDs();\nbool validPrinterProfile'):s.index('bool saveLighting(){')]
harness=(root/'tests/printer_profiles_json.cpp').read_text().replace('// PROFILE_FUNCTIONS',structs+'\n'+functions)
headers=root/'PlatformIO/.pio/libdeps/esp32c3/ArduinoJson/src'
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'profiles.cpp';binary=Path(tmp)/'profiles';cpp.write_text(harness)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(headers),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
