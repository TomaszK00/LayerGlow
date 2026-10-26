# LayerGlow v2.56 — otwarty dostęp na czas testów

W tej wersji sieć `LayerGlow-Setup` jest otwarta. Panel nie wymaga logowania ani ustawiania hasła przy pierwszym uruchomieniu. Hasła zapisanych domowych sieci Wi-Fi pozostają potrzebne do połączenia z routerem. Zapisane sieci, drukarki i ustawienia LED pozostają w pamięci.

1. Rozpakuj `LayerGlow_v2_56_Arduino_IDE.zip`. W Arduino IDE otwórz `LayerGlow_v2_56/LayerGlow_v2_56.ino`; folder `src` ma pozostać obok szkicu.
2. Użyj **esp32 by Espressif Systems 3.3.12**, bibliotek **ArduinoJson 7.4.3** i **Adafruit NeoPixel 1.15.5**. Dla Tenstar Robot ESP32-C3 SuperMini wybierz **ESP32C3 Dev Module**, odpowiedni port, **USB CDC On Boot: Enabled**, partycje **4 MB Minimal SPIFFS (1.9 MB APP with OTA)** i **Erase All Flash Before Sketch Upload: Disabled**.
3. Wgraj szkic. Na telefonie lub komputerze wybierz otwartą sieć **LayerGlow-Setup**. Jeśli urządzenie pamięta jej stare hasło, wybierz „Zapomnij sieć” i połącz się ponownie. Otwórz **http://192.168.4.1/**. Główny panel powinien pojawić się od razu.

Po połączeniu z domowym Wi-Fi użyj `http://layerglow.local/` albo adresu IP LayerGlow z routera. Sieć konfiguracyjna zamyka się po minucie od połączenia z domowym Wi-Fi, jeśli nikt z niej nie korzysta, i wraca po utracie tego połączenia.

To jest tymczasowa wersja do testów: każda osoba w zasięgu otwartej sieci lub mająca dostęp do tej samej sieci domowej może otworzyć panel i zmieniać ustawienia. W tej wersji nie wpisuj w panelu danych, których nie chcesz udostępniać innym użytkownikom tych sieci.

Kompilacja i testy automatyczne nie potwierdzają działania radia na fizycznej płytce.
