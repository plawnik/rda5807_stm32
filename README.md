# RDA5807M + STM32F103 radio

[![Firmware](https://github.com/plawnik/rda5807_stm32/actions/workflows/firmware.yml/badge.svg)](https://github.com/plawnik/rda5807_stm32/actions/workflows/firmware.yml)
[![Latest release](https://img.shields.io/github/v/release/plawnik/rda5807_stm32?display_name=release&label=firmware)](https://github.com/plawnik/rda5807_stm32/releases/latest)

Odbiornik FM zbudowany na `STM32F103C8T6` i `RDA5807M`. Obsługuje enkoder z przyciskiem, wyświetlacz Nokia `PCD8544` 84×48, RDS, zapis ustawień w pamięci Flash oraz kolorowy panel sterowania w terminalu ANSI/VT100.

PCD8544 jest podłączony **bezpośrednio do GPIO mikrokontrolera**. Projekt nie używa ekspandera ani magistrali I²C do obsługi wyświetlacza.

> Firmware kompiluje się i ma testy automatyczne, ale przed podłączeniem końcówki audio należy jeszcze zweryfikować na fizycznym egzemplarzu płytki mapę pinów oraz polaryzację podświetlenia.

## Najszybszy start

1. Pobierz ZIP z gotowym firmware z sekcji [Releases](https://github.com/plawnik/rda5807_stm32/releases/latest).
2. Wgraj `rda5807_stm32.hex` albo `rda5807_stm32.bin` przez ST-Link.
3. Podłącz moduł zgodnie z tabelą poniżej.
4. Ustaw terminal na `115200 8N1`, emulację ANSI/VT100 i kodowanie UTF-8.
5. Obrót enkodera na ekranie głównym zmienia częstotliwość zgodnie z wybranym krokiem kanału (`25/50/100/200 kHz`); krótki klik otwiera menu, a długie przytrzymanie wycisza radio.

Instrukcje dla STM32CubeProgrammer, `st-flash` i OpenOCD znajdują się w [docs/FLASHING.md](docs/FLASHING.md).

## Połączenia

| Funkcja | Pin STM32F103 | Kierunek | Uwagi |
|---|---:|---|---|
| RDA5807 SCL | PB10 | wyjście OD | I²C2, wymagany rezystor podciągający |
| RDA5807 SDA | PB11 | I/O OD | I²C2, wymagany rezystor podciągający |
| Enkoder A | PA15 | wejście | TIM2 CH1, wewnętrzne podciąganie |
| Enkoder B | PB3 | wejście | TIM2 CH2, wewnętrzne podciąganie |
| Przycisk enkodera | PB4 | wejście | aktywny stan niski, zwierany do GND |
| PCD8544 CLK | PB6 | wyjście | programowy interfejs szeregowy |
| PCD8544 DIN | PB7 | wyjście | bezpośrednio z GPIO |
| PCD8544 D/C | PB8 | wyjście | dane/polecenie |
| PCD8544 CE | PB9 | wyjście | chip enable |
| PCD8544 RST | PB12 | wyjście | reset wyświetlacza |
| PCD8544 LIGHT | PB13 | wyjście | w razie większego prądu użyj tranzystora |
| UART TX | PA9 | wyjście | USART1, 115200 8N1 |
| UART RX | PA10 | wejście | USART1, poziomy 3,3 V |
| SWDIO / SWCLK | PA13 / PA14 | I/O | programowanie i debugowanie |

Domyślne piny są zebrane w jednym miejscu: [`board_config.h`](f103radio/Core/Inc/board_config.h). Szczegóły elektryczne i uwagi o zasilaniu są w [docs/HARDWARE.md](docs/HARDWARE.md).

## Co pokazuje ekran

Ekran główny zawiera:

- nazwę stacji z RDS PS;
- częstotliwość w MHz, zawsze z trzema miejscami po przecinku;
- poziom głośności i stan wyciszenia;
- surowy poziom RSSI `0…127` oraz informację, czy układ rozpoznał stację FM;
- stereo/mono, synchronizację RDS i stan strojenia;
- przewijany RadioText albo typ programu i kod PI.

Wybrana, zbyt długa pozycja menu przewija się automatycznie. Napisy na PCD8544 są zapisane po polsku bez znaków diakrytycznych, ponieważ w pamięci mikrokontrolera znajduje się mała czcionka ASCII. Terminal używa pełnego UTF-8.

## Sterowanie enkoderem

| Widok | Obrót | Krótki klik | Długie przytrzymanie |
|---|---|---|---|
| Ekran główny | strojenie o wybrany krok kanału | otwarcie menu | mute/unmute |
| Menu | wybór pozycji | wejście w edycję lub wykonanie akcji | powrót na ekran główny |
| Edycja | zmiana wartości | zatwierdzenie | wyjście z edycji |

## Terminal ANSI/VT100

Panel terminalowy ma układ 112×35 znaków. W górnej części pokazuje duże cyfry częstotliwości `MHz`, duży odczyt głośności, pionowe wskaźniki głośności i RSSI oraz kolorowe ikonki statusu stereo/mono, RDS, stacji, mute i seek. W dolnej części pozostaje przewijana lista wszystkich ustawień.

Ekran nie jest okresowo czyszczony ani rysowany od początku. Firmware wylicza skrót każdego wiersza i za pomocą pozycjonowania kursora VT100 wysyła tylko te wiersze, których treść naprawdę się zmieniła. Eliminuje to miganie terminala; pełne przerysowanie następuje tylko po uruchomieniu oraz po naciśnięciu `R`.

| Klawisz | Działanie |
|---|---|
| `E` | edycja dużego odczytu częstotliwości; tryb wyłącza się po 5 s bezczynności |
| `O` | aktywacja dolnej listy opcji |
| `P` lub `[` | seek do poprzedniej stacji |
| `N` lub `]` | seek do następnej stacji |
| `↑` / `↓` | wybór pola menu; w edycji zmiana wartości lub zaznaczonej cyfry |
| `←` / `→` | zmiana opcji; w edycji częstotliwości wybór cyfry |
| `Enter` | wejście do edycji, zatwierdzenie lub uruchomienie akcji |
| `0…9` | zastąpienie zaznaczonej cyfry częstotliwości i przejście dalej |
| `Esc` | wyjście z edycji lub powrót z menu do podglądu |
| `M` | natychmiastowe mute/unmute |
| `R` | wymuszenie pełnego odświeżenia |
| `WASD` albo `HJKL` | zamienniki strzałek |

Częstotliwość jest zawsze pokazywana jako `108.000 MHz` (trzy miejsca po przecinku). Wartość wpisana poza zakresem bieżącego pasma jest automatycznie ograniczana do minimum lub maksimum. Zmiana pasma, dolnej granicy pasma wschodniego albo kroku kanału natychmiast ogranicza częstotliwość i wyrównuje ją do nowej siatki.

## Dostępne ustawienia RDA5807M

- częstotliwość, pasmo oraz odstęp kanałów 25/50/100/200 kHz;
- głośność 0…15, mute, mono/auto stereo i bass boost;
- RDS/RBDS oraz deemfaza 50/75 µs;
- wyszukiwanie góra/dół, stop albo zawijanie na granicy pasma;
- algorytm SNR/RSSI i oba udokumentowane progi wyszukiwania;
- soft mute, soft blend i próg soft blend;
- AFC i nowsza metoda demodulacji;
- wejście LNA i prąd LNA;
- kontrast, negatyw i podświetlenie LCD;
- przywrócenie ustawień domyślnych.

I²S, tryb testowy `DIRECT_MODE`, zapisy do rejestrów zastrzeżonych i ryzykowny tryb `NON_CALIBRATE` nie są udostępnione. Wyświetlacz oraz płytka nie wykorzystują I²S, a wpisywanie nieudokumentowanych wartości nie daje tu funkcjonalnej korzyści. Pełne uzasadnienie i mapowanie ustawień na rejestry: [docs/REGISTER_OPTIONS.md](docs/REGISTER_OPTIONS.md).

## Zapamiętywanie ustawień

Ostatnie dwie strony pamięci Flash (`0x0800F800` i `0x0800FC00`) tworzą prostą emulację EEPROM. Każdy rekord ma wersję, licznik sekwencji i CRC32. Zapis następuje dopiero po 3 sekundach bez zmian i jest wykonywany naprzemiennie na obu stronach, dzięki czemu:

- częstotliwość i pozostałe opcje wracają po restarcie;
- liczba kasowań Flash jest ograniczona;
- przerwanie zasilania podczas zapisu pozostawia poprzedni rekord;
- linker nie może umieścić kodu w obszarze konfiguracji.

## Budowanie lokalnie

Wymagane są `make`, `gcc` oraz GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`).

```bash
make test       # testy logiki radia i różnicowego panelu VT100
make firmware   # ELF, HEX, BIN i MAP w build/firmware
make ci         # testy i firmware, tak samo jak GitHub Actions
```

STM32CubeIDE nadal może otworzyć plik [`f103radio.ioc`](f103radio/f103radio.ioc), ale głównym, powtarzalnym systemem budowania jest repozytoryjny `Makefile`.

## CI/CD i wydania

Każdy push i pull request uruchamia testy oraz kompilację dla Cortex-M3. Push do `main` dodatkowo:

1. tworzy ZIP z plikami `.bin`, `.hex`, `.elf`, `.map`, instrukcją programowania i sumami SHA-256;
2. usuwa poprzednie automatyczne wydanie `firmware-latest`;
3. publikuje nowe wydanie jako [Latest Release](https://github.com/plawnik/rda5807_stm32/releases/latest).

W repozytorium pozostaje więc jedno aktualne automatyczne wydanie, a numer buildu i SHA commita znajdują się w opisie oraz w paczce.

## Dokumentacja

- [Sprzęt i połączenia](docs/HARDWARE.md)
- [Obsługa radia i terminala](docs/USER_GUIDE.md)
- [Architektura firmware](docs/ARCHITECTURE.md)
- [Opcje i rejestry RDA5807M](docs/REGISTER_OPTIONS.md)
- [Wgrywanie firmware](docs/FLASHING.md)

## Ważne przed pierwszym uruchomieniem

- Wszystkie sygnały logiczne pracują z poziomem `3,3 V`.
- I²C wymaga zewnętrznych rezystorów podciągających, typowo `4,7 kΩ` do 3,3 V.
- Podświetlenia nie wolno zasilać z GPIO, jeśli jego prąd przekracza bezpieczny prąd pinu — użyj tranzystora i rezystora.
- Po wgraniu sprawdź najpierw komunikację I²C i obraz LCD, a dopiero później tor audio.
