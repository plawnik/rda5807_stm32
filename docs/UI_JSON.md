# Menu z projektu PCD8544

Źródłem jest `assets/menu/radio-menu.json`, przekazany eksport v2 z edytora
`tools/lcd-layout-editor/index.html`. HTML jest identyczny z załącznikiem.
Grafiki, fonty, współrzędne i pozycje menu są kompilowane do Flash przez
`tools/generate_lcd_layout.py`.

## Ekran główny

Współrzędne odnoszą się do lewego górnego piksela. Bitmapy są kopiowane
z JSON, z zachowaniem ich pionowego pakowania i niepełnych stron 8 pikseli.

| Pole | x, y | Rozmiar | Zachowanie |
|---|---|---|---|
| Siła sygnału FM | 0, 0 | 4×36 | Warianty `lvl0…lvl8` według średniego RSSI |
| RDS | 5, 0 | 20×4 | Bitmapa tylko przy synchronizacji RDS i włączonym dekoderze |
| Głośność | 80, 0 | 4×36 | Poziomy 0…15 zaokrąglone do wariantów 0…8 |
| Bass boost | 28, 0 | 15×4 | Wariant on/off |
| Mono/stereo | 46, 0 | 13×4 | Stan odbiornika; wymuszone mono pokazuje mono |
| RTC | 62, 0 | 18×5 | `HH:MM`, 3×4, zwężony separator; nieustawiony `--:--` |
| Nazwa zapisanego wpisu | 6, 31 | 72×5 | Nazwa do 32 znaków; `FM`, gdy brak wpisu; środek lub przewijanie |
| Częstotliwość | 6, 36 | 72×8 | Font 5×7, środek, np. `100MHz`, `106.1MHz`, `97.55MHz` |
| Ikona stacji | 0, 37 | 4×4 | Wariant station/noise z bitu FM_TRUE |
| Ikona głośnika | 80, 37 | 4×4 | Mute przy głośności 0 lub włączonym wyciszeniu |
| Logo | 6, 5 | 72×26 | Dwie wbudowane grafiki zmieniane co 30 s przy braku RDS |
| RSSI liczbowo | 0, 44 | 8×4 | `round(RSSI × 99 / 127)` |
| Głośność liczbowo | 76, 44 | 8×4 | 0…15; przy mute nastawa pozostaje widoczna |
| Tryb sterowania | 27, 44 | 31×4 | Bitmapa seek / frequency / station ostatnio użytego sterowania |
| PS | 6, 5 | 32×5 | 3×4, do lewej |
| PTY | 46, 5 | 32×5 | Skróty z opisu, do prawej; uwzględniony wybór RDS/RBDS |
| RadioText | 6, 13 | 72×5 | Początek tekstu, nieruchomy, przycięty do pola |
| Radio tekst plus | 6, 21 | 72×5 | Ten sam RadioText, przewijany co 300 ms, pięć spacji odstępu |

„Radio tekst plus” oznacza tutaj drugi wiersz według opisu w projekcie,
czyli przewijany RadioText. Nie jest to dekoder tagów ODA RadioText+.
Nieznaczące spacje na końcu nazwy są usuwane przy zapisie. Ostatni wiersz
pikseli pod nazwą, PS/PTY i częstotliwością pozostaje wolny.

### Progi wskaźnika sygnału

RDA5807 zwraca logarytmiczny kod 0…127. Dokumentacja nie definiuje
przelicznika na dBm, więc nie przypisujemy mu fikcyjnej jednostki mocy.
Początkowe, empiryczne progi to:

| Poziom | Minimalny surowy RSSI |
|---|---:|
| 0 | 0 |
| 1 | 8 |
| 2 | 16 |
| 3 | 24 |
| 4 | 32 |
| 5 | 40 |
| 6 | 50 |
| 7 | 60 |
| 8 | 70 + stereo + synchronizacja RDS |

Bez stereo lub RDS najwyższy poziom jest ograniczony do 7. Progi można
skalibrować po testach odbiornika i anteny w `lcd_home_signal_level()`.

## Menu i obsługa

Pierwszy poziom to lista ośmiu kategorii z JSON i `Wyjscie`, font 5×7,
pięć wierszy. Każde podmenu ma `Wstecz`. Dłuższa zaznaczona etykieta się
przewija. Enkoder i lewo/prawo działają równolegle; klik lub OK zatwierdza.

Pola typu `bar` mają segmentowany pasek, `toggle` i `choice` używają etykiet
z JSON, a informacja o wersji jest tylko do odczytu. Częstotliwość w menu
zmienia się co 50 kHz. Uśrednianie RSSI ma wybory 0,2 / 0,5 / 1 / 2 / 5 s;
kontrast zakres 20…127. Opcje eksperckie pominięte w projekcie nadal są
dostępne w terminalu UART/USB.

W `Stacje` są obie pozycje z projektu: `Dodaj stacje` i `Zarzadzaj lista`.
Kreator najpierw ustawia częstotliwość, następnie nazwę do 32 znaków.
W edytorze nazwy lewo/prawo lub obrót wybiera pozycję; OK przełącza między
wyborem pozycji a edycją znaku. Za tekstem są `ZAPISZ` i `ANULUJ`.
Zapisane stacje można stroić, zmieniać i usuwać; usunięcie wymaga `TAK`.

`System > Ustawienia domyslne` pyta o potwierdzenie i zaczyna od `NIE`.
Przywraca nastawy z JSON, w tym AFC wyłączone, i usuwa listę stacji.
Istniejące ustawienia Flash są zachowywane przy aktualizacji; wartości
fabryczne z projektu nie nadpisują ich samoczynnie. Format 5 przenosi
ustawienia i krótkie nazwy stacji z formatów 2/3/4.

Z opisu pola zegara wynikają dodatkowe pozycje w `System`:

- `Ustaw zegar`: godzina → OK → minuta → OK zapisuje do RTC;
- `Zegar z RDS`: włącza/wyłącza korekcję czasu CT.

Ręczne ustawienie wyłącza CT, aby odebrana grupa nie nadpisała od razu
wybranej godziny. Wybór jest zapisywany w domenie backup RTC. RTC pracuje
z LSI i utrzymuje czas w STOP; podtrzymanie po całkowitym odcięciu zasilania
zależy od sprzętu, a LSI nie daje dokładności kwarcu zegarkowego.

## Zmiana projektu i kompilacja

```sh
# Zapisz eksport edytora jako assets/menu/radio-menu.json, następnie:
make layout
make ci
```

Do commita dodaj JSON oraz wygenerowane `lcd_layout.c` i `lcd_layout.h`.
`make test` i `make firmware` sprawdzają, czy tablice odpowiadają źródłu.
Nowy, nieznany rodzaj funkcji wymaga jawnego powiązania w generatorze i
obsługi w firmware; opis tekstowy nie jest wykonywalnym programem.

Testy uruchamiają produkcyjny kod menu, model radia, rysowanie i zapis Flash
z atrapami peryferiów. Porównują bufor pikseli z oryginalnym JSON, weryfikują
przewijanie z odstępem, limity nastaw, kreator stacji, potwierdzenia,
ustawianie zegara i migrację Flash. Odbiór RF i zachowanie na rzeczywistym
wyświetlaczu wymagają wgrania firmware na płytkę.
