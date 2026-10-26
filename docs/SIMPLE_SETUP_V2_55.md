# LayerGlow v2.55 — proste ustawienie hasła

**Ta instrukcja dotyczy starszej wersji.** Na czas testów v2.56 otwiera panel bez hasła; zobacz [instrukcję v2.56](OPEN_SETUP_TEST_V2_56.md).

Ta wersja usuwa wymóg przytrzymania BOOT i wysyłania `PAIR` przy ustawianiu hasła. Zapisane sieci domowe, drukarki i ustawienia LED pozostają w pamięci.

1. Rozpakuj `LayerGlow_v2_55_Arduino_IDE.zip` i otwórz `LayerGlow_v2_55/LayerGlow_v2_55.ino` w Arduino IDE. Folder `src` musi pozostać obok szkicu.
2. Użyj **esp32 by Espressif Systems 3.3.12**, bibliotek **ArduinoJson 7.4.3** i **Adafruit NeoPixel 1.15.5**. Dla Tenstar Robot ESP32-C3 SuperMini wybierz **ESP32C3 Dev Module**, właściwy port, **USB CDC On Boot: Enabled**, **4 MB Minimal SPIFFS (1.9 MB APP with OTA)** oraz **Erase All Flash Before Sketch Upload: Disabled**.
3. Wgraj szkic. Połącz się z `LayerGlow-Setup`, używając obecnego hasła. Jeżeli nie udało Ci się ukończyć parowania w v2.54, będzie to `layerglowsetup`. Otwórz **http://192.168.4.1/**.
4. Wpisz obecne hasło sieci i jedno nowe hasło (12–63 znaki), po czym kliknij **Zapisz hasło**. Nie trzeba naciskać żadnego przycisku. Po restarcie połącz się z `LayerGlow-Setup` nowym hasłem. Do panelu zaloguj się jako **admin**, używając tego samego nowego hasła.

Jeżeli później zapomnisz hasła, przy uruchomionym programie przytrzymaj BOOT przez 10 sekund albo wyślij `RESET_AP` w Monitorze portu szeregowego Arduino IDE (115200). Po restarcie tymczasowe hasło `layerglowsetup` zostanie przywrócone i formularz ponownie poprosi o stare oraz nowe hasło. RESET/RST/EN tylko restartuje płytkę.

Przed zapisaniem nowego hasła działa tylko sieć konfiguracyjna. Po zapisaniu urządzenie może połączyć się z zapisaną siecią domową. Domyślnie używa DHCP; opcjonalny stały IP można ustawić w panelu. Adres `layerglow.local` korzysta z mDNS i zależy od obsługi mDNS w sieci i urządzeniu klienta.

Kompilacja i testy automatyczne nie zastępują sprawdzenia połączenia na fizycznej płytce.
