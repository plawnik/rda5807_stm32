# Opcje RDA5807M

Implementacja opiera sie na
[`datasheets/RDA5807M-extended.pdf`](datasheets/RDA5807M-extended.pdf).
Rejestry sa skladane przez maski bitowe; kod nie uzywa pol bitowych C, ktorych
kolejnosc zalezy od kompilatora.

## Pola obslugiwane przez firmware

| Rejestr | Bity | Ustawienie | Zakres w interfejsie |
|---:|---:|---|---|
| 0x02 | 14 | mute | wyciszone / normalna praca |
| 0x02 | 13 | mono | mono / auto stereo |
| 0x02 | 12 | bass | wył. / wł. |
| 0x02 | 9:7 | seek | kierunek, uruchomienie, stop/zawijanie |
| 0x02 | 3 | RDS | wył. / wł. |
| 0x02 | 2 | new method | wył. / wł. |
| 0x03 | 15:6 | kanał | wyliczony z częstotliwości |
| 0x03 | 3:2 | pasmo | dobierane automatycznie z baz 50/76/87 MHz |
| 0x03 | 1:0 | odstep | dobierany automatycznie: 25/50/100/200 kHz |
| 0x04 | 13 | RBDS | RDS / RBDS |
| 0x04 | 11 | deemfaza | 50 / 75 µs |
| 0x04 | 9 | soft mute | wył. / wł. |
| 0x04 | 8 | AFC disable | w interfejsie pokazane jako AFC wł./wył. |
| 0x05 | 14:13 | algorytm seek | SNR / RSSI (`10`) |
| 0x05 | 11:8 | SEEKTH | 0…15 |
| 0x05 | 7:6 | wejście LNA | brak, LNAN, LNAP, dual |
| 0x05 | 5:4 | prąd LNA | 1,8 / 2,1 / 2,5 / 3,0 mA |
| 0x05 | 3:0 | głośność | 0…15 |
| 0x07 | 14:10 | próg soft blend | 0…31, jednostka 2 dB wg dokumentacji |
| 0x07 | 9 | dolna granica pasma 3 | automatycznie 50 MHz dla dolnego zakresu |
| 0x07 | 7:2 | stary próg seek | 0…63 |
| 0x07 | 1 | soft blend | wył. / wł. |
| 0x0B | 15:9 | RSSI | surowy kod logarytmiczny 0…127 |

Oba interfejsy przyjmuja czestotliwosc z zakresu `50-115 MHz` i same dobieraja
`BAND`, bit bazy 50/65 MHz oraz `SPACE`. Pole `CHAN[9:0]` jest sprawdzane przed
zapisem. Opcjonalny tryb eksperymentalny pozwala wykorzystac pelne rownanie
kodowe BAND0: `87 MHz + CHAN * SPACE`, czyli maksymalnie
`87 MHz + 1023 * 200 kHz = 291,6 MHz`. Ta wartosc nie jest deklarowanym przez
producenta zakresem RF.

Dokumentacja nie definiuje przeliczenia `RSSI[6:0]` na dBm ani dBµV, dlatego interfejs nie przypisuje kodowi sztucznej jednostki fizycznej.

## Opcje aplikacji bez osobnego pola RDA

| Opcja | Zakres | Dzialanie |
|---|---|---|
| `Zakres rozszerzony` | wyl./wl. | zmienia limit planera z 115 MHz na kodowe 291,6 MHz |
| `Usrednianie RSSI` | 0,2-5,0 s, krok 0,1 s | usrednia probki statusu bez zmiany rejestrow tunera |
| `Ruch enkodera` | 50 kHz / 100 kHz / seek / stacje | wybiera akcje obrotu na ekranie glownym |
| `Przyciski L/P` | 50 kHz / 100 kHz / seek / stacje | wybiera niezalezna akcje PA0/PA2 |
| `Lista stacji` | 12 wpisow | zapisuje nazwe i czestotliwosc w emulowanym EEPROM |

## Pola celowo zablokowane

| Pole | Powód |
|---|---|
| `NON_CALIBRATE` / sterowanie RCLK | dokumentacja ostrzega o wymaganiach termicznych i zegarowych; moduł tego nie potrzebuje |
| `DIRECT_MODE` | bit przeznaczony wyłącznie do testów |
| GPIO1/2/3 RDA | nie są podłączone w przyjętym sprzęcie; status jest czytany po I²C |
| `I2S_ENABLE` i cały rejestr 0x06 | sprzęt nie wykorzystuje I²S |
| `OPEN_MODE` | zapis do zastrzeżonych rejestrów pozostaje zamknięty |
| rejestry za 0x08 | warianty FP/HS opisują dodatkowe tryby, których zgodność z RDA5807M nie jest wystarczająco pewna |

Rejestr 0x06 jest zawsze zerowy. Sterownik zapisuje sekwencyjnie wyłącznie rejestry 0x02–0x07 i czyta status/RDS 0x0A–0x0F. Dzięki temu opcje dotyczące nieobecnego I²S ani tryby fabryczne nie mogą zostać przypadkowo aktywowane.
