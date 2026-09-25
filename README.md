# RDA5807M + STM32F103 radio

[![Firmware](https://github.com/plawnik/rda5807_stm32/actions/workflows/firmware.yml/badge.svg)](https://github.com/plawnik/rda5807_stm32/actions/workflows/firmware.yml)
[![Latest release](https://img.shields.io/github/v/release/plawnik/rda5807_stm32?display_name=release&label=firmware)](https://github.com/plawnik/rda5807_stm32/releases/latest)

Odbiornik FM zbudowany na `STM32F103C8T6` i `RDA5807M`. Obsługuje enkoder z przyciskiem, wyświetlacz Nokia `PCD8544` 84×48, RDS, zapis ustawień w pamięci Flash oraz kolorowy panel sterowania w terminalu ANSI/VT100.

PCD8544 jest podłączony **bezpośrednio do STM32**. Projekt nie używa
ekspandera: grafika powstaje w 504-bajtowym buforze RAM, a cały ekran jest
wysyłany jednym transferem SPI1 TX DMA.

> Firmware kompiluje się i ma testy automatyczne, ale przed podłączeniem końcówki audio należy jeszcze zweryfikować na fizycznym egzemplarzu płytki mapę pinów oraz polaryzację podświetlenia.

## Najszybszy start

1. Pobierz ZIP z gotowym firmware z sekcji [Releases](https://github.com/plawnik/rda5807_stm32/releases/latest).
2. Wgraj `rda5807_stm32.hex` albo `rda5807_stm32.bin` przez ST-Link.
3. Podłącz moduł zgodnie z tabelą poniżej.
4. Ustaw terminal na `115200 8N1`, emulację ANSI/VT100 i kodowanie UTF-8.
5. Radio uruchomi się automatycznie. Krótki klik enkodera otwiera menu, a
   długie przytrzymanie pokazuje animację zamykania i przechodzi do standby.

Instrukcje dla STM32CubeProgrammer, `st-flash` i OpenOCD znajdują się w [docs/FLASHING.md](docs/FLASHING.md).

## Połączenia

| Funkcja | Pin STM32F103 | Kierunek | Uwagi |
|---|---:|---|---|
| RDA5807 SCL | PB10 | wyjście OD | I²C2, wymagany rezystor podciągający |
| RDA5807 SDA | PB11 | I/O OD | I²C2, wymagany rezystor podciągający |
| Enkoder A | PA15 | wejście | TIM2 CH1, wewnętrzne podciąganie |
| Enkoder B | PB3 | wejście | TIM2 CH2, wewnętrzne podciąganie |
| Przycisk enkodera | PB4 | wejście | aktywny stan niski, zwierany do GND |
| Przycisk lewo | PA0 | wejście | pull-up, zwierany do GND |
| Przycisk prawo | PA2 | wejście | pull-up, zwierany do GND |
| Przycisk OK | PA8 | wejście | pull-up, zwierany do GND |
| PCD8544 CLK | PA5 | wyjście | SPI1 SCK |
| PCD8544 DIN | PA7 | wyjście | SPI1 MOSI, TX only |
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

## Sterowanie lokalne

| Widok | Obrót / PA0-PA2 | Klik enkodera / PA8 | Długie przytrzymanie enkodera |
|---|---|---|---|
| Ekran główny | konfigurowalnie: krok 50/100 kHz, seek albo lista stacji | otwarcie menu | animacja i standby |
| Menu | wybór pozycji | wejście, edycja albo wykonanie akcji | animacja i standby |
| Edycja | zmiana wartości | zatwierdzenie | animacja i standby |

Menu LCD ma poziom kategorii i poziom opcji. Kategoria `Stacje` udostępnia do
12 wpisów: kreator dodania (częstotliwość i nazwa), strojenie, edycję oraz
usuwanie. `BAND`, baza 50/65 MHz i `SPACE` nie są pokazywane użytkownikowi —
program wylicza je dla każdej częstotliwości.

## Terminal ANSI/VT100

Panel terminalowy ma układ 112×35 znaków. W górnej części pokazuje zbudowaną z pełnych bloków `█` częstotliwość `MHz`, duży odczyt głośności, pionowe wskaźniki głośności i RSSI oraz kolorowe ikonki statusu stereo/mono, RDS, stacji, mute i seek. W dolnej części pozostaje przewijana lista ustawień.

Ekran nie jest okresowo czyszczony. Firmware wylicza skrót każdego wiersza i
za pomocą pozycjonowania kursora VT100 wysyła tylko wiersze, których treść się
zmieniła. Co 5 sekund ponawia inicjalizację terminala i przepisuje panel w
miejscu, dzięki czemu Putty uruchomiony po radiu odzyska widok bez migania.
`R` wymusza takie samo bezpieczne przerysowanie.

| Klawisz | Działanie |
|---|---|
| `P` | włączenie lub wyłączenie edycji częstotliwości; tryb wyłącza się też po 5 s bezczynności |
| `O` | włączenie lub wyłączenie dolnej listy opcji |
| `↑` / `↓` na ekranie głównym | głośniej / ciszej |
| `←` / `→` na ekranie głównym | seek do poprzedniej / następnej stacji |
| `S` na ekranie głównym | przełączenie mono / auto stereo |
| `↑` / `↓` w menu | wybór pola; podczas edycji zmiana wartości |
| `←` / `→` w edycji częstotliwości | wybór cyfry |
| `Enter` | wejście do edycji, zatwierdzenie lub uruchomienie akcji |
| `0…9` | zastąpienie zaznaczonej cyfry częstotliwości i przejście dalej |
| `Esc` | wyjście z edycji lub powrót z menu do podglądu |
| `M` | natychmiastowe mute/unmute |
| `R` | wymuszenie pełnego odświeżenia |
| `HJKL` | zamienniki strzałek |

Częstotliwość terminala jest zawsze pokazywana jako `XXX.XX MHz`, na przykład `050.00`, `087.50` albo `108.00`. Domyślnie terminal udostępnia udokumentowany zakres syntezera `50–115 MHz`; użytkownik nie wybiera pasma ani odstępu kanałów. Firmware sam dobiera `BAND`, bazę 50/76/87 MHz i `SPACE` 50 lub 100 kHz. Wpis spoza zakresu jest ograniczany do najbliższej granicy, a najmłodsza pozycja odczytu jest wyrównywana do osiągalnej siatki 50 kHz.

Opcja `Zakres rozszerzony` w terminalu i LCD odblokowuje czysto rejestrowy
zakres do `291,60 MHz`, wyliczony jako `87,0 MHz + 1023 × 200 kHz`. Firmware
automatycznie przechodzi wtedy na `SPACE` 100 lub 200 kHz, gdy 10-bitowy
`CHAN` nie mieści żądanej częstotliwości. Jest to świadomy tryb eksperymentalny
poza zakresem gwarantowanym przez producenta: PLL może się nie zablokować,
odczyt częstotliwości nie potwierdza rzeczywistego odbioru, a analogowy tor RF
nie jest przeznaczony dla całego zakresu `115–291,60 MHz`.

`RSSI[6:0]` jest pokazywany jako kod `0…127` oraz procent tej skali. Według dokumentacji RDA5807M skala jest logarytmiczna, ale producent nie podaje przelicznika na dBm ani dBµV. `127` oznacza maksimum wewnętrznej skali, a nie `0 dBi`; dBi jest jednostką zysku anteny, nie mocy odebranego sygnału.

PS i RadioText nie są przepisywane na ekran po pojedynczej grupie. Dekoder
potwierdza całe segmenty i stosuje histerezę przed zastąpieniem stabilnej
treści. Pojedyncza podejrzana zmiana PI albo bitu A/B jest odrzucana, więc nie
zeruje poprawnego tekstu.

## Dostępne ustawienia RDA5807M

`Terminal + LCD` oznacza opcję dostępną w obu interfejsach. Ręczny wybór
pasma, dolnej bazy i `SPACE` jest ukryty wszędzie; oba interfejsy korzystają z
jednego automatycznego planera częstotliwości.

### Strojenie i wyszukiwanie

| Opcja | Gdzie | Wartości / domyślna | Co zmienia i jaki ma wpływ |
|---|---|---|---|
| `Czestotliwosc` | Terminal + LCD | `50–115 MHz`, domyślnie `106,10 MHz` | Uruchamia strojenie PLL. Program sam wybiera bazę, `BAND`, `SPACE` i `CHAN`. Zmiana częstotliwości zeruje dane poprzedniej stacji RDS. |
| `Zakres rozszerzony` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WYL.` | Podnosi limit z `115,00` do rejestrowych `291,60 MHz`. Jest to tryb eksperymentalny bez gwarancji blokady PLL i odbioru przez analogowy tor RF. |
| `Szukaj w gore` | Terminal + LCD | akcja | Ustawia bit `SEEKUP` i uruchamia sprzętowy seek w kierunku rosnącej częstotliwości. Operacja kończy się po znalezieniu stacji spełniającej aktywne progi albo na granicy pasma. Na ekranie głównym terminala odpowiada jej `→`. |
| `Szukaj w dol` | Terminal + LCD | akcja | Jak wyżej, ale szuka w kierunku malejącej częstotliwości. Na ekranie głównym terminala odpowiada jej `←`. |
| `Koniec szukania` | Terminal + LCD | `ZAPETL` / `STOP`; domyślnie `ZAPETL` | Steruje `SKMODE`. `ZAPETL` przechodzi z końca pasma na jego początek i szuka dalej; `STOP` kończy operację na granicy. |
| `Alg. szukania` | Terminal + LCD | `SNR` / `RSSI`; domyślnie `SNR` | Wybiera sposób kwalifikowania znalezionej stacji. SNR opiera decyzję na jakości sygnału, a tryb RSSI dodaje ocenę jego poziomu; wynik zależy także od odpowiedniego progu poniżej. |
| `Prog szukania` | Terminal + LCD | `0…15`, domyślnie `8` | Ustawia `SEEKTH`, czyli próg SNR. Wyższa wartość zwykle odrzuca więcej słabych lub zaszumionych częstotliwości, ale może pominąć użyteczną stację. |
| `Stary prog` | Terminal + LCD | `0…63`, domyślnie `16` | Ustawia `SEEK_TH_OLD`, używany przez udokumentowany tryb seek RSSI. Wyższy próg zawęża wyniki do silniejszych sygnałów; dokumentacja nie podaje jego bezwzględnej jednostki, więc najlepiej dobrać go doświadczalnie. |
| `Usrednianie RSSI` | Terminal + LCD | `0,2…5,0 s`, krok `0,1 s`, domyślnie `1,0 s` | Ustawia czas okna średniej pokazywanej na LCD i w terminalu. Krótsze okno reaguje szybciej, dłuższe ogranicza skakanie wskaźnika. Nie zmienia pracy toru RF. |

### Dźwięk i tor odbiorczy

| Opcja | Gdzie | Wartości / domyślna | Co zmienia i jaki ma wpływ |
|---|---|---|---|
| `Glosnosc` | Terminal + LCD | `0…15`, domyślnie `8` | Ustawia 4-bitową, logarytmiczną regulację wyjścia audio. `0` wycisza wyjście i ustawia wysoką impedancję; wyższe liczby nie oznaczają liniowego przyrostu głośności. Na ekranie głównym terminala sterują nią `↑` i `↓`. |
| `Wyciszenie` | Terminal + LCD | `NIE` / `TAK`, domyślnie `NIE` | Steruje `DMUTE`. Wycisza tor audio bez zmiany częstotliwości i głośności, więc po wyłączeniu mute wracają poprzednie ustawienia. Skrót globalny: `M`. |
| `Tryb audio` | Terminal + LCD | `AUTO STEREO` / `MONO`, domyślnie `AUTO STEREO` | `AUTO STEREO` pozwala układowi przełączyć się na stereo po wykryciu pilota. Wymuszone mono usuwa separację kanałów, ale przy słabym sygnale zwykle zmniejsza słyszalny szum. Skrót terminala: `S`. |
| `Podbicie basu` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WYL.` | Włącza wewnętrzny `BASS`. Zwiększa udział niskich częstotliwości; może poprawić odsłuch na małym głośniku, ale także szybciej przesterować dalszy tor audio. |
| `Deemfaza` | Terminal + LCD | `50 µs` / `75 µs`, domyślnie `50 µs` | Dopasowuje filtr deemfazy do standardu nadawania. W Europie używa się zwykle 50 µs, a w Ameryce Północnej 75 µs. Zła wartość zmienia ilość wysokich tonów i słyszalnego szumu. |
| `Soft mute` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Automatycznie ścisza dźwięk, gdy odbiór staje się słaby lub zaszumiony. Ogranicza gwałtowny szum między stacjami kosztem chwilowego spadku głośności. |
| `Soft blend` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Przy pogorszeniu odbioru płynnie zmniejsza separację stereo w stronę mono. Zwykle daje przyjemniejszy odsłuch słabej stacji bez nagłego przełączenia. |
| `Prog soft blend` | Terminal + LCD | `0…31`, krok opisany jako `2 dB`, domyślnie `16` | Ustawia punkt, przy którym działa soft blend. Większa wartość przesuwa próg według wewnętrznej skali układu; ponieważ datasheet nie opisuje całej charakterystyki, należy dobrać ją odsłuchem. |
| `AFC` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Automatyczna kontrola częstotliwości koryguje niewielkie odstrojenie odbiornika. Wyłączenie ma sens głównie podczas pomiarów lub eksperymentów; w normalnym odbiorze może pogorszyć stabilność dostrojenia. |
| `Nowy demodulator` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Ustawia `NEW_METHOD`. Producent deklaruje poprawę czułości odbioru o około `1 dB`; wyłączenie pozwala porównać zachowanie starszej metody. |
| `Wejscie LNA` | Terminal + LCD | `wylaczone`, `LNAN`, `LNAP`, `dual`; domyślnie `LNAP` | Wybiera wejście wzmacniacza niskoszumowego. Musi odpowiadać fizycznemu podłączeniu modułu; zła wartość może bardzo osłabić albo całkowicie usunąć odbiór. |
| `Prad LNA` | Terminal + LCD | `1,8`, `2,1`, `2,5`, `3,0 mA`; domyślnie `1,8 mA` | Zmienia prąd pracy LNA. Większy prąd może wpłynąć na szumy i liniowość przy silnych sygnałach, ale zwiększa pobór energii; nie jest to regulacja głośności ani bezpośredni „gain”. |

### RDS/RBDS

| Opcja | Gdzie | Wartości / domyślna | Co zmienia i jaki ma wpływ |
|---|---|---|---|
| `Dekoder RDS` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Włącza sprzętowy dekoder RDS. Po wyłączeniu nie są aktualizowane PS, RadioText, PI, PTY ani czas CT; odbiór audio FM działa nadal. |
| `Standard RDS` | Terminal + LCD | `RDS` / `RBDS`, domyślnie `RDS` | Wybiera regionalną interpretację danych przez układ: RDS jest właściwy m.in. dla Europy, RBDS dla Ameryki Północnej. Nie zmienia częstotliwości ani toru audio. |

PS i RadioText są zatwierdzane segmentami po zgodnych powtórzeniach. Stabilna
treść ma dodatkową histerezę, a zmiany PI i A/B są filtrowane osobno. Chroni to
interfejs przed miganiem po pojedynczej błędnej grupie, ale pozwala szybciej
zbudować tekst niż potwierdzanie każdego znaku w izolacji.

### Sterowanie, wyświetlacz i system

| Opcja | Gdzie | Wartości / domyślna | Co zmienia i jaki ma wpływ |
|---|---|---|---|
| `Kontrast LCD` | Terminal + LCD | `20…127`, domyślnie `56` | Zmienia napięcie sterujące matrycą PCD8544. Za mała wartość daje blady obraz, za duża ciemne tło i zlewanie pikseli; optymalna zależy od egzemplarza oraz temperatury. |
| `Negatyw LCD` | Terminal + LCD | `NIE` / `TAK`, domyślnie `NIE` | Zamienia jasne i ciemne piksele przez tryb kontrolera LCD. Nie modyfikuje zawartości ekranu ani interfejsu terminalowego. |
| `Podswietlenie` | Terminal + LCD | `WYL.` / `WL.`, domyślnie `WL.` | Steruje wyjściem podświetlenia PCD8544. Wpływa na pobór prądu podświetlenia, ale nie na kontrast matrycy. |
| `Ruch enkodera` | Terminal + LCD | krok 50/100 kHz, seek, lista stacji | Określa działanie obrotu enkodera na ekranie głównym. |
| `Przyciski L/P` | Terminal + LCD | krok 50/100 kHz, seek, lista stacji | Określa niezależne działanie PA0 i PA2 na ekranie głównym. |
| `Stacje` | LCD | maks. 12 nazw i częstotliwości | Pozwala dodać, dostroić, edytować i usunąć wpis; lista jest zapisywana razem z konfiguracją. |
| `Ustawienia domyslne` | Terminal + LCD | akcja | Przywraca wszystkie wartości domyślne, stroi `106,10 MHz`, czyści listę stacji i bieżące dane RDS, po czym oznacza konfigurację do zapisu w Flash. |

I²S, tryb testowy `DIRECT_MODE`, zapisy do rejestrów zastrzeżonych i ryzykowny tryb `NON_CALIBRATE` nie są udostępnione. Wyświetlacz oraz płytka nie wykorzystują I²S, a wpisywanie nieudokumentowanych wartości nie daje tu funkcjonalnej korzyści. Pełne uzasadnienie i mapowanie ustawień na rejestry: [docs/REGISTER_OPTIONS.md](docs/REGISTER_OPTIONS.md).

## Animacje startu i zamykania

Firmware zawiera cztery oryginalne, monochromatyczne animacje pixel-art:
skalę radia, equalizer, antenę i zasilanie. Maski w
[`splash_config.h`](f103radio/Core/Inc/splash_config.h) określają, z których
animacji losowany jest start i shutdown. Narzędzie
[`tools/convert_splash.py`](tools/convert_splash.py) zamienia własny GIF/PNG na
ramki RLE. Obrazy z podanego projektu ArtStation nie są redystrybuowane bez
jednoznacznej licencji; procedura dodania legalnych plików jest opisana w
[`assets/splash/README.md`](assets/splash/README.md).

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

STM32CubeIDE nadal może otworzyć plik [`f103radio.ioc`](f103radio/f103radio.ioc), ale głównym, powtarzalnym systemem budowania jest repozytoryjny `Makefile`. Repo zawiera oficjalny STM32CubeF1 `v1.8.7`; dokładne piny commitów HAL/CMSIS są w [`f103radio/Drivers/VERSIONS.md`](f103radio/Drivers/VERSIONS.md).

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
- [Wersje oficjalnego HAL i CMSIS](f103radio/Drivers/VERSIONS.md)
- [Źródłowe datasheety](docs/datasheets/README.md)

## Ważne przed pierwszym uruchomieniem

- Wszystkie sygnały logiczne pracują z poziomem `3,3 V`.
- I²C wymaga zewnętrznych rezystorów podciągających, typowo `4,7 kΩ` do 3,3 V.
- Podświetlenia nie wolno zasilać z GPIO, jeśli jego prąd przekracza bezpieczny prąd pinu — użyj tranzystora i rezystora.
- Po wgraniu sprawdź najpierw komunikację I²C i obraz LCD, a dopiero później tor audio.
