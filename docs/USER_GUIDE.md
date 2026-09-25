# Instrukcja użytkownika

## Uruchomienie

Po starcie program:

1. odczytuje i sprawdza CRC zapisanej konfiguracji;
2. inicjalizuje RDA5807M i stroi ostatnią częstotliwość;
3. uruchamia PCD8544, enkoder i terminal;
4. ponawia próbę wykrycia radia co 2 sekundy, jeżeli moduł nie odpowiada.

Na ekranie `BRAK RADIA` oznacza problem z komunikacją I²C. Najpierw sprawdź zasilanie, wspólną masę, PB10/PB11 i rezystory podciągające.

## Ekran główny

- Obrót w lewo/prawo przestraja radio o aktualnie wybrany krok kanału: 25, 50, 100 albo 200 kHz.
- Krótki klik otwiera menu.
- Przytrzymanie przez około 0,75 s przełącza wyciszenie.
- Symbol `>` w prawym górnym rogu oznacza aktywne strojenie lub wyszukiwanie.
- `S` oznacza stereo, `M` mono.
- `Q` pokazuje surowy RSSI w zakresie 0…127; nie jest to wartość w dBm.
- Dolny wiersz przewija RadioText. Gdy tekstu jeszcze nie ma, pokazuje PTY i PI.

## Menu wyświetlacza

Obrót wybiera pozycję. Krótki klik wchodzi w edycję; wtedy obrót zmienia wartość, a kolejny klik kończy edycję. Długie przytrzymanie cofa do ekranu głównego. Akcje `Szukaj w gore`, `Szukaj w dol` i `Ustawienia domyslne` wykonują się po pojedynczym kliknięciu.

Wybrana pozycja jest odwrócona. Jeżeli jej nazwa i wartość nie mieszczą się w 12 znakach, wiersz automatycznie się przewija.

## Terminal

Parametry portu: `115200 bit/s`, 8 bitów danych, brak parzystości, 1 bit stopu, brak kontroli przepływu. Terminal powinien obsługiwać ANSI/VT100 oraz UTF-8. Zalecany rozmiar okna to co najmniej 112 kolumn i 35 wierszy.

Przykładowe uruchomienie w Linuksie:

```bash
picocom -b 115200 /dev/ttyUSB0
# albo
minicom -D /dev/ttyUSB0 -b 115200
```

Górna część panelu zawiera duży odczyt częstotliwości, boczne pionowe wskaźniki głośności i sygnału oraz ikonki stanu. Dolna część pokazuje listę konfiguracji. Terminal aktualizuje kursorem VT100 wyłącznie zmienione wiersze, dlatego normalna praca nie czyści ekranu i nie powoduje migania.

Klawisz `E` uruchamia edycję częstotliwości. Klawisz `O` aktywuje dolną listę; wtedy `↑`/`↓` wybiera pole, a `Enter` rozpoczyna edycję. W edycji większości pól góra/prawo zwiększa, a dół/lewo zmniejsza wartość. `Esc` wraca o jeden poziom.

Dla częstotliwości:

- `←`/`→` wybiera jedną z sześciu cyfr wartości w kHz;
- `↑`/`↓` zwiększa lub zmniejsza zaznaczoną cyfrę;
- klawisz `0…9` zastępuje wybraną cyfrę i przesuwa kursor dalej;
- wartość jest natychmiast ograniczana do aktualnego pasma i siatki kanałowej.

Odczyt ma zawsze trzy miejsca po przecinku, na przykład `87.500 MHz` albo `108.000 MHz`. Po 5 sekundach bez wejścia tryb edycji wyłącza się automatycznie.

`P` lub `[` uruchamia seek w dół, a `N` lub `]` seek w górę. Klawisz `M` działa jako globalne mute, a `R` wymusza pełne odświeżenie. Działają też zamienniki `WASD` i `HJKL`.

Po zmianie pasma częstotliwość jest natychmiast ograniczana do nowego zakresu. Przykładowo `108.000 MHz` zmieni się na `91.000 MHz` po wybraniu pasma `76–91 MHz`. Zmiana dolnej granicy pasma wschodniego z 50 na 65 MHz działa w ten sam sposób. Zmiana kroku kanału wyrównuje bieżącą częstotliwość do nowej siatki, a enkoder od tej chwili używa właśnie tego kroku.

## RDS

Dekoder obsługuje:

- PI i PTY;
- PS (8-znakowa nazwa stacji);
- RadioText 2A i 2B, maksymalnie 64 znaki;
- flagi TP/TA oraz muzyka/mowa;
- czas CT z grupy 4A wraz z lokalnym przesunięciem strefy.

Po zmianie stacji bufor RDS jest zerowany. Grupy z niekorygowalnym błędem bloku A lub B są pomijane. PCD8544 ma font ASCII, dlatego nierozpoznane znaki rozszerzone są zastępowane spacją.

## Zapis ustawień

Każda zmiana ustawia licznik opóźnienia. Dopiero 3 sekundy po ostatnim ruchu program zapisuje cały rekord do Flash. Wskaźnik terminala pokazuje `OCZEKUJE`, a po zapisie `OK`. Nie odłączaj zasilania w chwili samego zapisu, chociaż poprzednia poprawna kopia pozostaje bezpieczna na drugiej stronie Flash.
