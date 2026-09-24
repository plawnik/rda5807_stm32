# Wgrywanie firmware

## Zawartość paczki Release

| Plik | Zastosowanie |
|---|---|
| `rda5807_stm32.bin` | surowy obraz, adres początkowy `0x08000000` |
| `rda5807_stm32.hex` | obraz Intel HEX z adresami |
| `rda5807_stm32.elf` | debugowanie i symbole |
| `rda5807_stm32.map` | mapa linkera |
| `SHA256SUMS.txt` | kontrola integralności plików |
| `BUILD_INFO.txt` | commit i numer buildu |

## STM32CubeProgrammer

1. Podłącz ST-Link: SWDIO, SWCLK, GND i odpowiednie zasilanie odniesienia.
2. Wybierz interfejs `ST-LINK` i tryb `SWD`.
3. Otwórz plik `.hex` i wybierz `Download`.
4. Wykonaj reset mikrokontrolera.

Dla pliku `.bin` ustaw adres startowy `0x08000000`.

## st-flash

```bash
st-flash --reset write rda5807_stm32.bin 0x08000000
```

## OpenOCD

Przykład dla ST-Link i STM32F1:

```bash
openocd \
  -f interface/stlink.cfg \
  -f target/stm32f1x.cfg \
  -c "program rda5807_stm32.elf verify reset exit"
```

## Weryfikacja sumy

W katalogu z rozpakowaną paczką:

```bash
sha256sum -c SHA256SUMS.txt
```

## Ustawienia zapisane w Flash

Pełne kasowanie układu usuwa także konfigurację z dwóch ostatnich stron Flash. Zwykłe wgranie obrazu, który zajmuje tylko pierwsze 62 KiB, nie powinno ich nadpisywać. Jeżeli chcesz świadomie przywrócić ustawienia domyślne, użyj pozycji menu albo wykonaj pełne kasowanie układu.
