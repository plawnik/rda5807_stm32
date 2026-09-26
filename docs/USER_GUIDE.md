# Instrukcja uzytkownika

## Uruchomienie i standby

Po podaniu zasilania radio automatycznie:

1. odczytuje najnowsza poprawna konfiguracje z Flash;
2. inicjalizuje LCD i pokazuje losowa animacje startowa;
3. uruchamia RDA5807M na ostatniej czestotliwosci;
4. wlacza enkoder, przyciski i panel terminalowy.

Napis `BRAK RADIA` oznacza brak odpowiedzi RDA5807M. Sprawdz zasilanie,
wspolna mase, PB10/PB11 i rezystory podciagajace. Firmware ponawia wykrywanie
modulu co 2 sekundy.

Dlugie przytrzymanie aktywnego przycisku OK (PB4 dla enkodera albo PA8 dla
trybu trzech przyciskow, okolo 0,75 s) pokazuje animacje
zamykania, zapisuje oczekujace ustawienia, wylacza tuner i LCD, po czym usypia
STM32 w trybie STOP. Kolejne nacisniecie PB4 albo PA8 - krotkie lub dlugie -
budzi radio i ponownie odtwarza animacje startowa.

## Ekran glowny LCD

Ekran pokazuje czestotliwosc, RDS PS/RadioText, stereo/mono, stan strojenia,
glosnosc i usredniony kod RSSI. Po wlaczeniu `Podbicie basu` w prawym gornym
polu pojawia sie maly piktogram glosnika z fala basowa. Kod RSSI ma zakres
`0...127`; nie jest wartoscia
w dBm ani dBi.

Na ekranie glownym:

- obrot enkodera wykonuje akcje wybrana w `Sterowanie > Ruch enkodera`;
- PA0/PA2 wykonuja akcje wybrana w `Sterowanie > Przyciski L/P`;
- wybrany w `Sterowanie > Sterowanie` interfejs obsluguje calosc UI;
- krotki klik enkodera albo PA8 otwiera menu;
- dlugie przytrzymanie aktywnego OK przechodzi do standby.

Dostepne akcje lewo/prawo to krok 50 kHz, krok 100 kHz, seek albo przejscie
po zapisanej liscie stacji.

## Menu LCD

Menu ma dwa poziomy jak w starszych telefonach Nokia. Najpierw wybiera sie
kategorie, a potem opcje. Obrot enkodera lub PA0/PA2 przesuwa zaznaczenie;
krotki klik enkodera albo PA8 zatwierdza.

| Kategoria | Zawartosc |
|---|---|
| `Radio` | czestotliwosc, zakres rozszerzony, seek w gore/dol |
| `Dzwiek` | glosnosc, mute, mono/stereo, bass, deemfaza |
| `Odbior` | algorytm/progi seek, usrednianie RSSI, soft mute/blend, AFC, LNA |
| `RDS` | dekoder RDS i wybor RDS/RBDS |
| `Sterowanie` | wybor enkoder / trzy przyciski oraz osobna akcja lewo/prawo |
| `Ekran` | kontrast, negatyw i podswietlenie |
| `Stacje` | dodawanie, strojenie, edycja i usuwanie stacji |
| `System` | ustawienia domyslne |
| `Wyjscie` | powrot na ekran glowny |

Reczne `BAND`, dolna baza 50/65 MHz i `SPACE` nie sa pokazywane. Firmware
dobiera je automatycznie do zadanej czestotliwosci.

### Lista stacji

Mozna zapisac do 12 stacji. `Dodaj stacje` uruchamia prosty kreator:

1. ustaw czestotliwosc z krokiem 50 kHz;
2. wybierz osiem znakow nazwy;
3. zatwierdz ostatni znak.

Po wybraniu zapisanej stacji mozna ja odtworzyc, zmienic czestotliwosc, zmienic
nazwe albo usunac. Przy akcji `Lista stacji` ruch lewo/prawo przechodzi po
pozycjach cyklicznie.

## Terminal ANSI/VT100 przez UART i USB

Panel jest nadawany jednoczesnie przez USART1 (PA9/PA10) i natywny USB CDC
(PA11/PA12). Ustaw port na `115200 8N1`, bez kontroli przeplywu, ANSI/VT100 i
UTF-8. Parametry ustawione dla USB CDC sa informacyjne, ale `115200 8N1`
ulatwia zachowanie identycznej konfiguracji obu polaczen.
Zalecany rozmiar to 112 kolumn i 35 wierszy. Firmware wysyla polecenie zmiany
rozmiaru, ale nie kazdy emulator terminala je honoruje; w takim programie
rozszerz okno recznie.

Panel aktualizuje kursorem tylko zmienione wiersze. Co 5 sekund ponawia
inicjalizacje i przepisuje caly panel w miejscu, bez czyszczenia ekranu. Dzieki
temu Putty uruchomiony juz po radiu odzyska kompletny widok, a normalne
odswiezanie nie miga.

| Klawisz | Ekran glowny / dzialanie globalne |
|---|---|
| `Up` / `Down` | glosniej / ciszej |
| `Left` / `Right` | seek w dol / w gore |
| `P` | wlacz/wylacz edycje czestotliwosci |
| `O` | wlacz/wylacz dolne menu opcji |
| `S` | mono / auto stereo |
| `M` | mute/unmute |
| `R` | wymus przerysowanie wszystkich wierszy bez czyszczenia |
| `H J K L` | odpowiedniki strzalek |

W edycji czestotliwosci:

- `Left`/`Right` wybiera jedna cyfre formatu `XXX.XX`;
- `Up`/`Down` dodaje lub odejmuje wartosc wybranej pozycji z normalnym
  przeniesieniem, np. `106.00 -> 105.90`;
- `0...9` wpisuje cyfre i przechodzi do nastepnej;
- `P`, `Enter` lub `Esc` konczy edycje;
- po 5 sekundach bezczynnosci edycja konczy sie automatycznie.

Czestotliwosc jest wyswietlana ze znaczacymi zerami, np. `050.00 MHz`. W
zwyklym trybie zostaje ograniczona do `50-115 MHz`. `Zakres rozszerzony`
odblokowuje eksperymentalny limit kodowy `291,60 MHz`, wynikajacy z
`87 MHz + 1023 * 200 kHz`. Powyzej 115 MHz producent nie gwarantuje blokady PLL
ani poprawnej pracy toru RF.

## RDS

Dekoder obsluguje PI, PTY, PS, RadioText 2A/2B, TP/TA, muzyka/mowa i czas CT.
Segment nie trafia od razu na ekran: musi zostac potwierdzony, a stabilny
segment ma histereze przed zastapieniem. Pojedyncza podejrzana zmiana PI albo
bitu A/B jest ignorowana, dlatego bledna grupa nie zeruje calego tekstu.

Po rzeczywistej zmianie stacji stan RDS jest resetowany. Rozszerzone znaki,
ktorych nie ma w malej czcionce LCD, sa pokazywane jako spacja.

## Usrednianie RSSI

`Usrednianie RSSI` ustawia okno od 0,2 do 5,0 s w krokach 0,1 s; domyslnie
1,0 s. Surowe probki sa zbierane co 50 ms, a interfejs pokazuje srednia z
biezacego zakonczonego okna. Mniejsza wartosc reaguje szybciej, wieksza mniej
skacze.

## Zapis ustawien

Zmiana konfiguracji uruchamia 3-sekundowe opoznienie. Po okresie bez kolejnej
zmiany caly rekord, lacznie z lista stacji, trafia do jednej z dwoch stron
Flash. Rekord ma numer sekwencji i CRC32. Przy zaniku zasilania podczas zapisu
poprzednia poprawna kopia pozostaje na drugiej stronie.
