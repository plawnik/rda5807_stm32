# Architektura firmware

## Pętla główna

Projekt nie wymaga RTOS. Pętla działa kooperacyjnie i wykonuje krótkie zadania:

1. odczyt enkodera i odbijanie czterech przyciskow;
2. parsowanie znaków odebranych przez USART1 lub USB CDC w przerwaniu;
3. odczyt statusu RDA5807M co 50 ms i dekodowanie nowych grup RDS;
4. zlozenie obrazu PCD8544 w RAM i uruchomienie transferu SPI DMA;
5. okresowe odświeżenie panelu VT100;
6. odroczony zapis zmienionych ustawień.

Strojenie i wyszukiwanie są uruchamiane zapisem rejestrów, a ich zakończenie jest sprawdzane w kolejnych przebiegach pętli. Interfejs nie zamraża się na czas operacji tunera.

## Moduły

| Moduł | Odpowiedzialność |
|---|---|
| `app_config` | domyślne wartości, zakresy pasm, siatka i sanityzacja |
| `rda5807` | rejestry 0x02–0x07, tuning, seek, status 0x0A–0x0F |
| `rds_decoder` | grupy 0A/0B, 2A/2B i 4A bez zależności od HAL |
| `pcd8544` | framebuffer 504 B, prymitywy graficzne i pełna ramka przez SPI1 TX DMA |
| `spi` / `dma` | oficjalny STM32 HAL, SPI1 master TX i DMA1 Channel 3 |
| `input` | TIM2 jako enkoder oraz debouncing przycisku enkodera i PA0/PA2/PA8 |
| `radio_app` | stan aplikacji, menu i synchronizacja sprzętu |
| `lcd_ui` | ekran główny, hierarchiczne menu, stacje i przewijanie tekstu |
| `splash_animation` | losowany ekran startu/zamykania i dekoder ramek RLE |
| `terminal_ui` | wspólny parser VT100 dla UART/USB, duże cyfry i różnicowe odświeżanie wierszy |
| `settings_store` | dwie strony Flash, sekwencja rekordu i CRC32 |
| `uart_debug` | bezpieczne formatowanie i równoległe wysyłanie przez USART1 oraz USB CDC |
| `USB_DEVICE` | oficjalny stos ST USB Device/CDC, deskryptory i most do terminala |

## Przepływ stanu

`radio_app` jest jedynym właścicielem konfiguracji użytkownika. LCD i terminal wywołują te same funkcje zmiany ustawień, więc nie mogą się rozjechać. Po zmianie:

- wartość jest sanityzowana;
- odpowiednie rejestry RDA są aktualizowane;
- widoki dostają nowy numer rewizji;
- konfiguracja zostaje oznaczona jako oczekująca na zapis.

LCD i terminal przekazuja zadana czestotliwosc do tego samego planera w
`app_config`. Planer domyslnie ogranicza ja do `50-115 MHz`, wybiera baze
50/76/87 MHz, `BAND` i `SPACE`, a nastepnie sprawdza 10-bitowe pole `CHAN`.
Po wlaczeniu trybu eksperymentalnego poszerza `SPACE`, az wartosc zmiesci sie
w polu, maksymalnie do kodowego limitu `291,6 MHz`. Uzytkownik nie wybiera
recznego pasma ani dolnej bazy.

Panel terminala buduje kazdy z 35 wierszy niezaleznie i przechowuje 32-bitowy
skrot ostatnio wyslanej wersji. W zwyklej pracy wysylane sa wylacznie zmienione
wiersze wraz z sekwencja pozycjonowania kursora. Co 5 sekund firmware ponawia
inicjalizacje/rozmiar terminala i przepisuje wszystkie wiersze w miejscu, bez
sekwencji czyszczenia ekranu. Dzieki temu terminal uruchomiony po radiu odzyska
pelny panel, ale nie miga.

Kazda ramka LCD powstaje w `pcd8544_t.buffer`. `pcd8544_update()` wysyla dwa
polecenia ustawienia adresu, ustawia D/C i CE, po czym zleca HAL jeden transfer
DMA 504 bajtow. Przed modyfikacja bufora kolejny render czeka na zakonczenie
poprzedniego DMA, wiec kontroler nigdy nie dostaje ramki zmienianej w locie.

Po wlaczeniu zasilania radio startuje automatycznie i odtwarza jedna z
wlaczonych animacji. Uzytkownik wybiera kompletny interfejs lokalny: enkoder
lub trzy przyciski. Dlugie przytrzymanie aktywnego OK zapisuje ustawienia,
wylacza tuner, USB i LCD oraz wprowadza STM32 w STOP. PB4 i PA8 pozostaja
zrodlami wybudzenia; po wybudzeniu wracaja zegar 72/48 MHz, USB, animacja,
radio i oba interfejsy terminala.

## Układ pamięci Flash

| Zakres | Rozmiar | Przeznaczenie |
|---|---:|---|
| `0x08000000–0x0800F7FF` | 62 KiB | kod i stałe firmware |
| `0x0800F800–0x0800FBFF` | 1 KiB | rekord ustawień A |
| `0x0800FC00–0x0800FFFF` | 1 KiB | rekord ustawień B |

Skrypt linkera ogranicza region `FLASH` do 62 KiB. Rekord zawiera magic, wersję formatu, rozmiar danych, rosnącą sekwencję, ustawienia i CRC32. Podczas zapisu kasowana jest strona nieaktywna; staje się aktywna dopiero po zapisaniu i weryfikacji.

## Testowalność

`app_config` i `rds_decoder` nie zaleza od STM32 HAL. `make test` kompiluje je
natywnym GCC i sprawdza automatyczny dobor pasma, zakres podstawowy
`50-115 MHz`, limit eksperymentalny `291,6 MHz`, siatke PLL, konsensus
segmentow PS/RadioText, filtrowanie A/B i PI oraz czas RDS. Osobny test panelu
terminalowego korzysta z atrap UART i potwierdza brak czyszczenia ekranu,
skroty, przejscie `106.00 -> 105.90` i uzycie pelnych blokow `█` zamiast `#`.
`make firmware` osobno buduje caly obraz dla Cortex-M3.
