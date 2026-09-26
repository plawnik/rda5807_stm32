# Sprzet i polaczenia

## Zalozenia

Projekt jest przygotowany dla `STM32F103C8T6` z zewnetrznym kwarcem 8 MHz,
modulu `RDA5807M` z zegarem 32,768 kHz oraz wyswietlacza `PCD8544` 84x48.
Mikrokontroler pracuje z czestotliwoscia 72 MHz, a cala logika ma poziom
`3,3 V`.

PCD8544 jest podlaczony bezposrednio do STM32. Nie ma ekspandera GPIO ani
posredniego polaczenia I2C. Obraz jest skladany w 504-bajtowym framebufferze w
RAM, a kompletne 6 bankow po 84 bajty jest wysylane jednym transferem
`SPI1 TX DMA` w poziomym trybie adresowania kontrolera.

## Mapa pinow

| Urzadzenie | Sygnal | STM32 | Konfiguracja |
|---|---|---:|---|
| RDA5807M | SCLK | PB10 | I2C2 SCL, open drain, 400 kHz |
| RDA5807M | SDIO | PB11 | I2C2 SDA, open drain, 400 kHz |
| Enkoder | A | PA15 | TIM2 CH1, pull-up |
| Enkoder | B | PB3 | TIM2 CH2, pull-up |
| Enkoder | SW | PB4 | EXTI4, pull-up, aktywny stan niski |
| Przycisk lewo | SW | PA0 | GPIO input, pull-up, zwierany do GND |
| Przycisk prawo | SW | PA2 | GPIO input, pull-up, zwierany do GND |
| Przycisk OK | SW | PA8 | GPIO input, pull-up, zwierany do GND |
| PCD8544 | CLK | PA5 | SPI1 SCK, push-pull |
| PCD8544 | DIN | PA7 | SPI1 MOSI, push-pull |
| PCD8544 | D/C | PB8 | GPIO output |
| PCD8544 | CE | PB9 | GPIO output, aktywny stan niski |
| PCD8544 | RST | PB12 | GPIO output, aktywny stan niski |
| PCD8544 | LIGHT | PB13 | GPIO output / sterowanie tranzystorem |
| Adapter USB-UART | RX | PA9 | USART1 TX, 115200 8N1 |
| Adapter USB-UART | TX | PA10 | USART1 RX, 115200 8N1 |
| USB FS | D- | PA11 | natywny USB device / CDC ACM |
| USB FS | D+ | PA12 | natywny USB device / CDC ACM, pull-up 1,5 kOhm |
| ST-Link | SWDIO | PA13 | debug/programowanie |
| ST-Link | SWCLK | PA14 | debug/programowanie |

JTAG jest wylaczony, lecz SWD pozostaje aktywny. Dzieki temu PA15, PB3 i PB4
sa dostepne dla enkodera. Przycisk enkodera na PB4 i osobny OK na PA8 sa
zrodlami wybudzenia ze stanu STOP. Firmware reaguje na ten, ktory odpowiada
wybranemu trybowi sterowania.

## Zasilanie i poziomy

- Polacz masy wszystkich modulow.
- Przy kazdym module umiesc 100 nF mozliwie blisko zasilania oraz kondensator
  zbiorczy odpowiedni dla plytki.
- SCL i SDA wymagaja podciagania do 3,3 V. `4,7 kOhm` jest dobrym punktem
  startowym; uwzglednij rezystory juz obecne na module.
- Nie podlaczaj 5-woltowego TX adaptera UART bezposrednio do PA10.
- Jezeli podswietlenie pobiera wiecej niz bezpieczny prad GPIO, PB13 ma
  sterowac tranzystorem, a nie diodami bezposrednio.

## PCD8544 i DMA

SPI1 pracuje tylko jako nadajnik, w trybie 0, MSB first, z konserwatywnym
zegarem 1,125 MHz.
DMA1 Channel 3 czyta bezposrednio bufor `pcd8544_t.buffer[504]`. Funkcje
rysujace nigdy nie wysylaja pojedynczych pikseli do wyswietlacza. Modyfikuja
wylacznie framebuffer, a `pcd8544_update()` ustawia adres X/Y na poczatek i
uruchamia jeden asynchroniczny transfer calej ramki.

Zmiana PA5/PA7 na inne piny SPI wymaga zmiany konfiguracji SPI/MSP, nie tylko
makr GPIO. Polaryzacje podswietlenia mozna zmienic przez
`LCD_BACKLIGHT_ACTIVE_STATE` w `board_config.h`.

## Enkoder i przyciski

Wspolny styk enkodera oraz jeden styk kazdego przycisku polacz z GND. Wejscia
maja wewnetrzne podciaganie. TIM2 stosuje filtr cyfrowy 15, a wszystkie
przyciski maja programowy debounce 25 ms. Jezeli kierunek enkodera jest
odwrotny, zmien `ENCODER_DIRECTION` z `1` na `-1`.

Trzy osobne przyciski sa pelna alternatywa dla obrotu i klikniecia enkodera.
Opcja `Sterowanie` wybiera jeden interfejs, aby przypadkowe drgania drugiego
nie wywolywaly akcji. PA8 dziala jako krotkie OK, dlugie wylaczenie i
wybudzenie. Akcje lewo/prawo na ekranie glownym sa konfigurowalne niezaleznie
od akcji obrotu enkodera.

## USB Virtual COM Port

Natywny kontroler USB STM32 pracuje jako CDC ACM. Windows 10/11 oraz typowe
dystrybucje Linux rozpoznaja go jako port szeregowy bez dodatkowej aplikacji.
Ten sam panel VT100 oraz te same klawisze dzialaja jednoczesnie przez USB i
USART1.

PA11 i PA12 podlacz bezposrednio do D- i D+. USB wymaga zegara 48 MHz, ktory
firmware uzyskuje z PLL 72 MHz przez dzielnik 1,5. Potrzebny jest rezystor
pull-up 1,5 kOhm z D+ do 3,3 V; zwykle znajduje sie na Blue Pill. Dla wlasnej
plytki produkcyjnej nalezy uzyc wlasnego legalnego VID/PID zamiast
demonstracyjnej pary ST `0483:5740`.

## RDA5807M i audio

Firmware domyslnie wybiera pojedyncze wejscie `LNAP`. Rodzaj wejscia i prad
LNA mozna zmienic w menu. Tor RF i analogowe wyjscia audio wykonaj zgodnie ze
schematem referencyjnym konkretnego modulu. Projekt nie wlacza I2S.

Nie prowadz sygnalow CLK LCD i UART rownolegle do wejscia antenowego ani
analogowych wyjsc audio na dlugim odcinku.

## Zmiana pinow

1. Zmien mape w `f103radio/Core/Inc/board_config.h`.
2. Zaktualizuj `gpio.c` oraz, dla SPI/DMA, `stm32f1xx_hal_msp.c` i `dma.c`.
3. Zaktualizuj `f103radio/f103radio.ioc`, aby CubeMX nie przywrocil starego
   mapowania.
4. Uruchom `make ci` i sprawdz sprzet na stole.
