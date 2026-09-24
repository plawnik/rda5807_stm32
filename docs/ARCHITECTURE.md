# Architektura firmware

## Pętla główna

Projekt nie wymaga RTOS. Pętla działa kooperacyjnie i wykonuje krótkie zadania:

1. odczyt enkodera i odbijanie przycisku;
2. parsowanie znaków odebranych przez UART w przerwaniu;
3. odczyt statusu RDA5807M co 50 ms i dekodowanie nowych grup RDS;
4. aktualizacja PCD8544;
5. okresowe odświeżenie panelu VT100;
6. odroczony zapis zmienionych ustawień.

Strojenie i wyszukiwanie są uruchamiane zapisem rejestrów, a ich zakończenie jest sprawdzane w kolejnych przebiegach pętli. Interfejs nie zamraża się na czas operacji tunera.

## Moduły

| Moduł | Odpowiedzialność |
|---|---|
| `app_config` | domyślne wartości, zakresy pasm, siatka i sanityzacja |
| `rda5807` | rejestry 0x02–0x07, tuning, seek, status 0x0A–0x0F |
| `rds_decoder` | grupy 0A/0B, 2A/2B i 4A bez zależności od HAL |
| `pcd8544` | bezpośredni bit-bang GPIO, framebuffer 504 B, font i grafika |
| `input` | TIM2 jako enkoder i debouncing przycisku |
| `radio_app` | stan aplikacji, menu i synchronizacja sprzętu |
| `lcd_ui` | ekran główny, ikony, menu i przewijanie tekstu |
| `terminal_ui` | parser VT100, bufor UART i kolorowy panel |
| `settings_store` | dwie strony Flash, sekwencja rekordu i CRC32 |
| `uart_debug` | ograniczone długością, bezpieczne formatowanie wyjścia |

## Przepływ stanu

`radio_app` jest jedynym właścicielem konfiguracji użytkownika. LCD i terminal wywołują te same funkcje zmiany ustawień, więc nie mogą się rozjechać. Po zmianie:

- wartość jest sanityzowana;
- odpowiednie rejestry RDA są aktualizowane;
- widoki dostają nowy numer rewizji;
- konfiguracja zostaje oznaczona jako oczekująca na zapis.

## Układ pamięci Flash

| Zakres | Rozmiar | Przeznaczenie |
|---|---:|---|
| `0x08000000–0x0800F7FF` | 62 KiB | kod i stałe firmware |
| `0x0800F800–0x0800FBFF` | 1 KiB | rekord ustawień A |
| `0x0800FC00–0x0800FFFF` | 1 KiB | rekord ustawień B |

Skrypt linkera ogranicza region `FLASH` do 62 KiB. Rekord zawiera magic, wersję formatu, rozmiar danych, rosnącą sekwencję, ustawienia i CRC32. Podczas zapisu kasowana jest strona nieaktywna; staje się aktywna dopiero po zapisaniu i weryfikacji.

## Testowalność

`app_config` i `rds_decoder` nie zależą od STM32 HAL. `make test` kompiluje je natywnym GCC i sprawdza zakresy częstotliwości, zawijanie pasma, PS, RadioText, flagę A/B i czas RDS. `make firmware` osobno buduje cały obraz dla Cortex-M3.
