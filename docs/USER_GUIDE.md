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

Górna część panelu zawiera duży odczyt częstotliwości rysowany pełnymi blokami `█`, boczne pionowe wskaźniki głośności i sygnału oraz ikonki stanu. Dolna część pokazuje listę konfiguracji. Terminal aktualizuje kursorem VT100 wyłącznie zmienione wiersze, dlatego normalna praca nie czyści ekranu i nie powoduje migania.

Na ekranie głównym `↑`/`↓` zmienia głośność, `←`/`→` uruchamia seek w dół/w górę, a `S` przełącza mono i auto stereo. Klawisz `P` włącza edycję częstotliwości, a ponowne `P` ją wyłącza. Klawisz `O` analogicznie otwiera i zamyka dolną listę. W menu `↑`/`↓` wybiera pole, a `Enter` rozpoczyna edycję. W edycji większości pól góra/prawo zwiększa, a dół/lewo zmniejsza wartość. `Esc` wraca o jeden poziom.

Dla częstotliwości:

- `←`/`→` wybiera jedną z pięciu cyfr formatu `XXX.XX`;
- `↑`/`↓` dodaje lub odejmuje wartość pozycji z normalnym przeniesieniem między cyframi;
- klawisz `0…9` zastępuje wybraną cyfrę i przesuwa kursor dalej;
- wartość jest natychmiast ograniczana do aktywnego zakresu (`50–115 MHz` albo eksperymentalnie `50–291,60 MHz`) i osiągalnej siatki PLL.

Odczyt ma zawsze dwie cyfry po przecinku i znaczące zera z przodu, na przykład `050.00 MHz`, `087.50 MHz` albo `108.00 MHz`. Zaznaczenie pozycji `0,1 MHz` i naciśnięcie `↓` przy `106.00 MHz` daje `105.90 MHz`. Najmłodsza pozycja jest kwantowana do 50 kHz, dlatego ostatnia wyświetlana cyfra jest równa `0` albo `5`. Po 5 sekundach bez wejścia tryb edycji wyłącza się automatycznie.

Klawisz `M` działa jako globalne mute, a `R` wymusza pełne odświeżenie. Działają też zamienniki strzałek `HJKL`.

W terminalu nie wybiera się pasma ani odstępu kanałów. Dla każdej zadanej częstotliwości firmware automatycznie dobiera rejestrowe pasmo, bazę 50/76/87 MHz i `SPACE`. Domyślny zakres syntezera kończy się na `50 MHz` i `115 MHz`.

Pozycja `Zakres rozszerzony` w dolnej liście odblokowuje limit wynikający wyłącznie z szerokości pól rejestru: `87,0 MHz + 1023 × 200 kHz = 291,60 MHz`. Powyżej 115 MHz firmware dobiera 50, 100 albo 200 kHz tak, aby `CHAN` zmieścił się w 10 bitach. Ten tryb jest oznaczony jako `EKSP.` i nie jest gwarantowany przez producenta — PLL lub tor RF mogą przestać działać poprawnie. Wyłączenie opcji przy częstotliwości ponad 115 MHz natychmiast ogranicza ustawienie do `115,00 MHz`.

RDA5807M zwraca 7-bitowy `RSSI` w zakresie `0…127`; terminal pokazuje kod i procent pełnej skali. Jest to skala logarytmiczna bez opublikowanego przez producenta przelicznika na dBm lub dBµV. W szczególności `127` nie oznacza `0 dBi` — dBi opisuje zysk anteny.

## RDS

Dekoder obsługuje:

- PI i PTY;
- PS (8-znakowa nazwa stacji);
- RadioText 2A i 2B, maksymalnie 64 znaki;
- flagi TP/TA oraz muzyka/mowa;
- czas CT z grupy 4A wraz z lokalnym przesunięciem strefy.

Po zmianie stacji bufor RDS jest zerowany. Grupy z niekorygowalnym błędem bloku A lub B są pomijane. Każdy znak PS i RadioText zostaje opublikowany dopiero po trzech identycznych odbiorach na tej samej pozycji; pojedyncze zakłócenie nie nadpisuje już stabilnego tekstu. PCD8544 ma font ASCII, dlatego nierozpoznane znaki rozszerzone są zastępowane spacją.

## Zapis ustawień

Każda zmiana ustawia licznik opóźnienia. Dopiero 3 sekundy po ostatnim ruchu program zapisuje cały rekord do Flash. Wskaźnik terminala pokazuje `OCZEKUJE`, a po zapisie `OK`. Nie odłączaj zasilania w chwili samego zapisu, chociaż poprzednia poprawna kopia pozostaje bezpieczna na drugiej stronie Flash.
