# Historia zmian — LayerGlow

## v2.43 — wersja testowa (24 października 2026)

- Wybór stanu testowego od razu uruchamia test na podglądzie i podłączonym pasku LED. Odczyt z drukarki wyłącza symulację.
- Drukowanie zwiększa postęp o 5% co 500 ms, zatrzymuje się przy 100% na sekundę i powtarza. Pozostałe stany pozostają aktywne do zmiany wyboru.
- Usunięto checkboxy automatycznego postępu i powtarzania oraz ręczny suwak postępu. Podgląd animuje się między zwykłymi odczytami statusu; zaktualizowano FAQ w czterech językach.
- Kompilacja GitHub Actions przeszła; test sprzętowy pozostaje do wykonania.

## v2.42 — wersja testowa (24 października 2026)

- Stan testowy, postęp, liczba świecących LED i aktualny stan są w jednym, spokojniejszym wizualnie wierszu podglądu, również w zwiniętym podglądzie mobilnym.
- Odczyt z drukarki pokazuje rzeczywisty stan i procent; po utracie połączenia nie wyświetla starej wartości jako bieżącego odczytu.
- Kompilacja GitHub Actions przeszła; test sprzętowy pozostaje do wykonania.

## v2.41 — wersja testowa (24 października 2026)

- Dodano Odczyt z drukarki do listy Stan testowy. Ten wybór wyłącza symulację; każdy inny stan włącza test podglądu i podłączonego paska.
- Usunięto checkbox Tryb testowy, a Automatyczne odtwarzanie postępu przeniesiono na jego miejsce. Powtarzanie po 100% pozostało obok; etykieta postępu na telefonie rozróżnia odczyt i test.
- Poprawiono FAQ w czterech językach i sprawdzono przełączanie między odczytem, stanami testowymi oraz automatycznym postępem. Kompilacja GitHub Actions przeszła; test sprzętowy pozostaje do wykonania.

## v2.40 — wersja testowa (24 października 2026)

- Podgląd LED pozostaje jednym rzędem o stałej szerokości niezależnie od długości fizycznego paska.
- Do 48 LED pokazuje wszystkie pola; przy 49–300 LED 48 pól reprezentuje cały pasek. Kolory, postęp i efekty są próbkowane wzdłuż rzeczywistej długości. Przy skalowaniu pojawia się krótkie objaśnienie.
- Liczba fizycznych LED i sterowanie podłączonym paskiem pozostają bez zmian. Sprawdzono logikę podglądu dla 5, 24, 25, 48, 49 i 300 LED oraz 14 efektów. Kompilacja GitHub Actions przeszła; test sprzętowy pozostaje do wykonania.

## v2.39 — wersja testowa (24 października 2026)

- Zamiast trzech kropek motywu przy stopce są trzy pola z haczykiem w jednym wierszu: Auto, Jasny i Ciemny.
- Aktywny jest zawsze dokładnie jeden motyw. Zachowano zapis wyboru w przeglądarce oraz reakcję trybu Auto na motyw telefonu.
- Sprawdzono logikę interfejsu i kompilację GitHub Actions; test na urządzeniu pozostaje do wykonania.

## v2.38 — wersja testowa (24 października 2026)

- Na telefonie górne menu chowa się, gdy tytuł „Kolory i ustawienia LED” wchodzi pod przyklejony podgląd LED. Menu wraca, gdy tytuł jest znów widoczny. Układ komputerowy pozostaje bez zmian.
- Przy stopce zamiast listy wyboru motywu są trzy kropki w jednym wierszu: automatyczny, jasny i ciemny. Wybrany tryb ma podpis, tłumaczenie i zapisuje się w przeglądarce.
- Sprawdzono logikę interfejsu i kompilację GitHub Actions; test na urządzeniu pozostaje do wykonania.

## v2.37 — wersja testowa (24 października 2026)

- Na telefonie przyciski dodawania i usuwania kolorów są w pierwszym rzędzie przy 1–4 kolorach, w drugim przy 5–10 i w trzecim przy 11–12. Układ mieści się także na wąskich ekranach.
- Nazwa wybranego efektu jest obok etykiety „Efekt”, nad listą wyboru.
- Sprawdzono logikę interfejsu i kompilację GitHub Actions; test na urządzeniu pozostaje do wykonania.

## v2.36 — wersja testowa (24 października 2026)

- Przeniesiono sygnały niebieskiej diody do trzech rozwijanych pytań FAQ z krótkimi wzorami błysków. Usunięto osobny blok z opisem diody.
- Przy stopce dodano motyw automatyczny zgodny z systemem, jasny i ciemny dla całej strony. Wybór jest zapamiętywany w przeglądarce, a tryb automatyczny reaguje na zmianę motywu urządzenia.
- Zaktualizowano cztery języki i sprawdzono wyszukiwanie FAQ oraz przełączanie motywu. Kompilacja GitHub Actions przeszła; test sprzętowy pozostaje do wykonania.

## v2.35 — wersja testowa (24 października 2026)

- Rozbudowano FAQ do 29 pytań z wyszukiwarką: pierwsze uruchomienie, Wi-Fi, drukarki, LED i kolory, tryb testowy, kopia ustawień oraz aktualizacje OTA.
- Cała nowa pomoc jest dostępna po polsku, angielsku, hiszpańsku i niemiecku. Każdą odpowiedź można rozwinąć osobno w pełnoekranowym FAQ na telefonie.
- Kontrola logiki strony i kompilacja GitHub Actions przeszły; test na urządzeniu pozostaje do wykonania.

## v2.34 — wersja testowa (23 października 2026)

- W panelu sieci, instrukcji OTA, FAQ i komunikatach restartu nazwa urządzenia to LayerGlow. Poprawiono wszystkie cztery wersje językowe.
- W technicznych krokach wyboru płytki i wgrywania pozostawiono oznaczenie ESP32-C3, bo określa typ układu.
- Kompilacja GitHub Actions przeszła; sprawdzenie na urządzeniu pozostaje do wykonania.

## v2.33 — wersja testowa (23 października 2026)

- Ustawiono nazwę klienta DHCP przed uruchomieniem Wi-Fi. Router może teraz pokazywać `LayerGlow` zamiast automatycznej nazwy ESP32.
- Adres `layerglow.local` i sieć ratunkowa `LayerGlow-Setup` pozostają takie same. Własna nazwa urządzenia pojawi się w routerze po ponownym uruchomieniu.
- Kompilacja GitHub Actions przeszła; potwierdzenie na fizycznym routerze pozostaje do wykonania.

## v2.32 — wersja testowa (23 października 2026)

- Poszerzono kolumnę kolorów na komputerze; przyciski + i − nie nachodzą już na Jasność. Nazwy etapów są większe o 1 px, a z etykiet Jasności usunięto „(%)”.
- We wszystkich językach nazwa aplikacji brzmi LayerGlow. W tytule, nagłówku i stopce dodano „by FatGiantCat”.
- Dodano własną ikonę strony z symbolem L i trzema punktami LED.
- Nowy plik OTA ma tylko nazwę `LayerGlow_v2_32_OTA.bin`. Usunięto zdublowane pliki K2 z wersji v2.27–v2.31; starsze oryginalne binarki zachowano.
- Test interfejsu i kompilacja przeszły; test na urządzeniu pozostaje do wykonania.

## v2.31 — wersja testowa (23 października 2026)

- Zwinięte pola Jasność i Puls pozostają obok siebie także na węższym telefonie.
- Po ustawieniu wartości mobilny suwak chowa się po około 800 ms; nazwa i wybrana wartość pozostają widoczne.
- Test interfejsu przeszedł; działanie na urządzeniu nadal wymaga sprawdzenia.

## v2.30 — wersja testowa (23 października 2026)

- Na telefonie górne menu to jeden wiersz etykiet i flaga języka.
- W podglądzie LED postęp testu, liczba świecących LED i stan drukarki są w jednym krótkim wierszu. Ukryto mobilny suwak postępu i duży procent.
- Jasność i Puls pozostają obok siebie także po rozwinięciu. Wybrany efekt jest pokazany przy etykiecie Efekt.
- Sprawdzono działanie interfejsu; test na urządzeniu nadal pozostaje do wykonania.

## v2.29 — wersja testowa (23 października 2026)

- Ujednolicono mobilne menu, odstępy, tekst i pola formularzy; główne przyciski mają wygodny rozmiar do dotknięcia.
- Podgląd LED zajmuje mniej miejsca podczas przewijania, a ustawienia etapów są podzielone na wyraźne karty.
- Po rozwinięciu suwaki Jasność i Puls korzystają z całej szerokości karty; paleta układa się czytelnie na wąskich telefonach.
- Sprawdzono reguły CSS i działanie interfejsu; test na urządzeniu nadal pozostaje do wykonania.

## v2.28 — wersja testowa (23 października 2026)

- Na telefonie panele Sieć, Drukarka, Aktualizacja i FAQ otwierają się na całym ekranie i zamykają przyciskiem; wybór języka pozostaje małym oknem.
- Jasność w każdym etapie ustawia rozwijany suwak 0–100%, a Puls ma wspólny panel z przełącznikiem i suwakiem 0–5000 ms.
- Wybrany efekt jest widoczny także na telefonie. Skrócono etykietę trybu testowego, aby pole powtarzania mieściło się obok.
- Test interfejsu przeszedł; binarkę OTA skompilował GitHub Actions. Test na urządzeniu i drukarce jest nadal potrzebny.

## v2.27 — wersja testowa (23 października 2026)

- Rozdzielono okna Sieć i Drukarka. Jeden przycisk zapisuje ustawienia Wi-Fi i rozpoczyna połączenie.
- Dodano bieżący odczyt stanu drukarki, postępu i nazwy pliku, rozłączanie oraz wybór jednej z wielu wykrytych drukarek.
- Wyszukiwanie sprawdza także porty Moonraker 7125 i 80 w lokalnej podsieci oraz wyświetla procent od 0 do 100.
- W mobilnym wyborze języka przywrócono flagi; panel nosi nazwę LayerGlow.
- Kompilacja GitHub Actions zakończona pomyślnie; OTA i wykrywanie wymagają testu na urządzeniu.

Poniższe opisy pochodzą z instrukcji README dołączonych do oryginalnych paczek oraz z historii projektu. Wyjaśniają, **po co przygotowano kolejną wersję**. Nie oznacza to, że każda została ponownie przetestowana na fizycznej drukarce i module ESP32.

**Wersja ogłaszana przez GitHub OTA: 2.32 (testowa).** Wersja **2.16** jest zachowana w archiwum, ale nie została ustawiona jako aktualna w `Software/update.json`.

| Wersja | Dlaczego powstała |
| --- | --- |
| **2.16** | Poprawa skanowania Wi-Fi w trybie AP `K2-LED-Setup`, blokada prób ponownego łączenia na czas skanowania, rozróżnianie błędu od pustej listy sieci. Dodana **wewnętrzna kopia ustawień LED** i przyciski zapisu/przywracania. |
| **2.15** | Rozwiązanie problemów z odzyskiwaniem połączenia Wi-Fi: automatyczne uruchamianie AP po ok. 8 sekundach, ponowne próby łączenia, reset modułu radiowego bez utraty konfiguracji, automatyczne zamykanie AP po powrocie domowej sieci. |
| **2.14** | Czerwone tło przycisku **Aktualizacja** i biały tekst po wykryciu nowej wersji. Nowe efekty LED: kometa, wędrujące wypełnienie i naprzemienne pary. |
| **2.13** | Nowy efekt **Losowe pary** — niezależne włączanie i wyłączanie par LED w losowych, szybkich odstępach, również w podglądzie i teście. |
| **2.12** | Zmieniony sygnał niebieskiej diody informujący o aktualizacji: **dwa długie błyski, a następnie jeden krótki**. Uzupełniono FAQ. |
| **2.11** | Możliwość wyłączenia świecenia osobno dla każdego etapu; automatyczny zapis ustawień, usunięcie zbędnego efektu „boki–środek–boki” i poprawa układu na telefonach. |
| **2.10** | Naprawa wykrywania aktualizacji, gdy na GitHubie brakowało pliku `update.json`. Dodano alternatywne sprawdzanie plików BIN oraz sygnały stanu Wi-Fi/aktualizacji niebieską diodą. |
| **2.9** | Uporządkowanie panelu aktualizacji GitHub i osobne okno **Aktualizacja**; poprawki wyglądu nagłówka, stopki i podglądu LED. |
| **2.8** | Rozwinięcie systemu aktualizacji GitHub przez manifest z numerem wersji i SHA-256, rozwój efektów LED oraz tłumaczeń interfejsu. |
| **2.7** | Automatyczne odświeżenie panelu po OTA, trwałe komunikaty o braku połączenia oraz nowe efekty od środka, od boków i fali. |
| **2.6** | Przebudowa wyglądu strony i ustawień dla poszczególnych stanów drukarki; efekt **Knight Rider** i powiadomienia w prawym dolnym rogu. |
| **2.5** | Oddzielne pulsowanie dla stanów drukarki, automatyczne odtwarzanie postępu w teście, pełne resetowanie konfiguracji LED oraz czytelniejsze komunikaty podczas OTA. |
| **2.4** | Automatyczne zapisywanie kolorów i ustawień LED, przywracanie domyślnych kolorów oraz instrukcja zmiany domowej sieci Wi-Fi. |
| **2.3** | Wybór 12/24 diod, obsługa pulsowania i zapętlonych testów, status niebieskiej diody, cztery języki oraz osobny obraz USB do pierwszej instalacji. |
| **2.2** | Stara paczka ZIP istnieje w spisie plików, ale nie udało się uzyskać jej oryginalnej zawartości. Nie publikuję zmyślonej listy zmian. |
| **2.1** | Skanowanie sieci Wi-Fi w panelu, podstawowe EN/PL, aktualizacja .bin przez WWW oraz podgląd i test diod. |
| **2.0** | Pierwszy prototyp ESP32-C3 z panelem WWW i odczytem postępu z Moonraker, jeszcze **bez fizycznego paska LED**. |

## Gdzie są pliki?

- [Kod źródłowy Arduino](../Arduino/README.md) — `.ino` dla wersji 2.0, 2.1, 2.3–2.16.
- [Oprogramowanie archiwalne](../Software/archive/README.md) — pozostałe pliki `.bin`, w tym 2.3 USB.
- [Aktualny manifest OTA](../Software/update.json) — nadal wskazuje 2.15.
- [Główna strona projektu](../README.md).

## Ważne

Archiwum nie jest automatyczną listą aktualizacji. Stare pliki `_OTA.bin` wymagają zgodnego układu partycji i zainstalowanego programu z funkcją OTA. Pierwsze wgrywanie przez USB opisano w [instrukcji instalacji](INSTALLATION.md).

Nie każdy kod historyczny ma takie same opcje i zabezpieczenia. Nigdy nie udostępniaj panelu ESP32 bezpośrednio w Internecie. Archiwalny firmware może mieć domyślne hasło AP i panel bez uwierzytelniania.
