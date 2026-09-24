# Sprzęt i połączenia

## Założenia

Projekt jest przygotowany dla STM32F103C8T6 z zewnętrznym kwarcem 8 MHz, modułu RDA5807M z własnym zegarem 32,768 kHz oraz wyświetlacza PCD8544 z Nokii 5110/3310. Mikrokontroler działa z częstotliwością 72 MHz.

PCD8544 jest sterowany programowo bezpośrednio z GPIO. Linie PB6 i PB7, które w starym kodzie tworzyły I²C1 dla testowego ekspandera LCD, są teraz zwykłymi wyjściami `CLK` i `DIN`.

## Mapa pinów

| Urządzenie | Sygnał | STM32 | Konfiguracja |
|---|---|---:|---|
| RDA5807M | SCLK | PB10 | I²C2 SCL, open drain, 400 kHz |
| RDA5807M | SDIO | PB11 | I²C2 SDA, open drain, 400 kHz |
| Enkoder | A | PA15 | TIM2 CH1, pull-up |
| Enkoder | B | PB3 | TIM2 CH2, pull-up |
| Enkoder | SW | PB4 | GPIO input, pull-up, aktywny niski |
| PCD8544 | CLK | PB6 | GPIO push-pull |
| PCD8544 | DIN | PB7 | GPIO push-pull |
| PCD8544 | D/C | PB8 | GPIO push-pull |
| PCD8544 | CE | PB9 | GPIO push-pull, aktywny niski |
| PCD8544 | RST | PB12 | GPIO push-pull, aktywny niski |
| PCD8544 | LIGHT | PB13 | GPIO push-pull / sterowanie tranzystorem |
| Adapter USB-UART | RX | PA9 | USART1 TX |
| Adapter USB-UART | TX | PA10 | USART1 RX |
| ST-Link | SWDIO | PA13 | debug/programowanie |
| ST-Link | SWCLK | PA14 | debug/programowanie |

JTAG jest wyłączony, ale SWD pozostaje aktywny. Dzięki temu PA15, PB3 i PB4 są dostępne dla enkodera.

## Zasilanie i poziomy

- STM32, RDA5807M, PCD8544 i UART muszą używać logiki 3,3 V.
- Połącz masy wszystkich modułów.
- Przy każdym module umieść kondensator 100 nF możliwie blisko zasilania; zastosuj również kondensator zbiorczy odpowiedni dla płytki.
- Dodaj rezystory podciągające SCL i SDA do 3,3 V. Wartość 4,7 kΩ jest dobrym punktem startowym; uwzględnij rezystory już obecne na module.
- Nie podłączaj bezpośrednio 5-woltowego TX adaptera UART do PA10.

## PCD8544

Sterownik korzysta z programowego interfejsu szeregowego. Nie wymaga sprzętowego SPI, dzięki czemu przypisania można zmienić wyłącznie w `Core/Inc/board_config.h` oraz w konfiguracji GPIO.

Niektóre moduły Nokia mają rezystor podświetlenia, inne nie. PB13 powinien sterować bramką/bazą tranzystora, jeżeli prąd podświetlenia jest większy niż bezpieczne obciążenie pinu. Polaryzację można zmienić makrem `LCD_BACKLIGHT_ACTIVE_STATE`.

## Enkoder

Typowe połączenie:

- wspólny styk enkodera do GND;
- A do PA15;
- B do PB3;
- styk przycisku do PB4, drugi styk do GND.

Wejścia mają wewnętrzne podciąganie. Filtr cyfrowy TIM2 jest ustawiony na wartość 15, a przycisk ma programowe odbijanie styków 25 ms. Jeśli kierunek jest odwrotny, zmień `ENCODER_DIRECTION` z `1` na `-1`.

## RDA5807M i audio

Firmware zakłada pojedyncze wejście antenowe `LNAP`, zgodne z domyślną konfiguracją rejestru 0x05. Rodzaj wejścia i prąd LNA można zmienić w menu.

Tor RF i audio wykonaj zgodnie ze schematem referencyjnym właściwego modułu RDA5807M. Nie prowadź zegarów LCD i UART równolegle do wejścia antenowego ani analogowych wyjść audio na długim odcinku. Projekt firmware nie uruchamia I²S — dźwięk wychodzi wyłącznie analogowo z RDA5807M.

## Zmiana pinów

1. Zmień makra w `f103radio/Core/Inc/board_config.h`.
2. Zaktualizuj inicjalizację w `gpio.c`, jeżeli nowe piny leżą na innym porcie.
3. Zaktualizuj `f103radio.ioc`, aby późniejsza regeneracja CubeMX nie przywróciła starego mapowania.
4. Uruchom `make ci`.
