# LayerGlow v2.54: odzyskanie `LayerGlow-Setup` przez Arduino IDE

**Ta instrukcja dotyczy starszej wersji.** W v2.55 ustawienie hasła nie wymaga przycisku ani Monitora portu. Zobacz [prostą instrukcję v2.55](SIMPLE_SETUP_V2_55.md).

Ta wersja zastępuje v2.53, gdy nie możesz połączyć się z `LayerGlow-Setup`. Przy pierwszym uruchomieniu v2.54 po aktualizacji stare hasło sieci konfiguracyjnej i hasło administratora są resetowane **jednorazowo**. Zapisane sieci domowe, drukarki i ustawienia LED pozostają w pamięci.

1. Rozpakuj `LayerGlow_v2_54_Arduino_IDE.zip`. Otwórz `LayerGlow_v2_54/LayerGlow_v2_54.ino` w Arduino IDE; folder `src` musi pozostać obok szkicu.
2. Zainstaluj **esp32 by Espressif Systems** (kompilacja sprawdzona z wersją **3.3.12**) oraz biblioteki **ArduinoJson 7.4.3** i **Adafruit NeoPixel 1.15.5**. Wybierz płytkę zgodną z ESP32-C3 (dla SuperMini: **ESP32C3 Dev Module**), właściwy port, **USB CDC On Boot: Enabled** i układ partycji **4 MB Minimal SPIFFS, 1.9 MB APP with OTA**. Ustaw **Erase All Flash Before Sketch Upload: Disabled**.
3. Wgraj szkic przez USB. Po restarcie połącz telefon lub komputer z `LayerGlow-Setup`, używając **tymczasowego hasła `layerglowsetup`**. Jeśli urządzenie pamięta wcześniejszą próbę, wybierz „Zapomnij sieć” i połącz się ponownie. Otwórz **http://192.168.4.1/**.
4. Strona parowania wymaga nowych haseł administratora i sieci konfiguracyjnej, każde co najmniej 12 znaków. Przy zapisywaniu przytrzymaj działający przycisk **BOOT**. Jeśli przycisk nie jest wykrywany, otwórz Monitor portu szeregowego w Arduino IDE (**115200**), wyślij `PAIR` i zapisz formularz w ciągu 2 minut. Po zapisie puść BOOT, jeśli był trzymany. Po restarcie hasło `layerglowsetup` przestaje działać; użyj hasła wybranego w formularzu.

Jeśli później zapomnisz hasła, przy uruchomionym programie przytrzymaj działający **BOOT przez 10 sekund** albo wyślij `RESET_AP` w Monitorze portu USB. Po restarcie `LayerGlow-Setup` znów używa `layerglowsetup`, ale wymaga ponownego ustawienia nowych haseł. Przycisk **RST/EN** jedynie restartuje płytkę; jego przytrzymania program nie wykryje. Nie trzymaj BOOT podczas włączania ani resetowania urządzenia.

Przed ukończeniem parowania urządzenie pracuje tylko jako punkt dostępowy, co stabilizuje połączenie konfiguracyjne. Potem domowe Wi-Fi domyślnie używa DHCP. `http://layerglow.local/` korzysta z mDNS i może działać przy DHCP lub stałym IP, jeśli telefon/komputer i sieć obsługują mDNS. Jeśli AP nadal odrzuca tymczasowe hasło po wgraniu v2.54, zapisz komunikaty z Monitora portu od uruchomienia urządzenia i dokładny model płytki. Działanie na fizycznym urządzeniu wymaga potwierdzenia.
