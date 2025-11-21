# Opis projektu — Temat 13: Jaskinia

## Dane identyfikacyjne pliku

**Nazwa pliku:** `NAZWISKO_IMIĘ_NR_ALBUMU_opis_temat13.md`

> Uwaga: przed oddaniem zamień `NAZWISKO`, `IMIĘ` i `NR_ALBUMU` na swoje dane.

---

## 1. Streszczenie projektu

Celem projektu jest opracowanie symulacji organizacji zwiedzania jaskini z dwiema równoległymi trasami turystycznymi. Symulacja ma odwzorować zachowanie zwiedzających, obsługę kasową, pracę przewodników oraz reakcję strażnika na polecenia przerwania przyjmowania nowych grup. Wyniki symulacji mają być zapisywane w formie czytelnego raportu tekstowego.

## 2. Założenia i parametry wejściowe

* Okres działania jaskini: od (T_p) do (T_k).
* Trasa 1 — maks. liczba zwiedzających: **N1**.
* Trasa 2 — maks. liczba zwiedzających: **N2**.
* Pojemność kładki (wejście/wyjście): **K** (przy założeniu **K < Ni** dla każdej trasy).
* Czas zwiedzania trasy 1: **T1** (jednostki czasu).
* Czas zwiedzania trasy 2: **T2** (jednostki czasu).
* Generacja zwiedzających: losowa, wiek z rozkładu dyskretnego 1–80 lat.
* Udział powracających zwiedzających: około 10% — mają zniżkę 50% i (jeżeli regulamin pozwala) mogą wejść na drugą trasę z pierwszeństwem (omijać kolejkę).

## 3. Regulamin i ograniczenia wiekowe

1. Dzieci poniżej 3 lat: bilet bezpłatny.
2. Dzieci poniżej 8 lat: mogą zwiedzać tylko trasę 2 i muszą przebywać pod opieką osoby dorosłej.
3. Osoby powyżej 75 roku życia: mogą zwiedzać tylko trasę 2.
4. Powtarzający zwiedzający (ok. 10%): mają prawo do 50% zniżki, mogą wejść na *drugą* trasę omijając kolejkę (tylko gdy regulamin to umożliwia).

## 4. Opis elementów systemu (moduły)

### 4.1 Kasjer

* Obsługa sprzedaży biletów i kontroli uprawnień do wyboru trasy (wiek, opiekun dla dziecka <8 lat, zasady dla osób >75 lat).
* Oznaczanie zwiedzających powracających i przyznawanie zniżek oraz prawa do wejścia bez kolejki.
* Kierowanie osoby/rodziny do właściwej kolejki.

### 4.2 Zwiedzający

* Model zachowania: przyjście (losowe), zakup biletu, oczekiwanie w kolejce, wejście na trasę, zwiedzanie, wyjście.
* Dla dzieci <8 lat — konieczność obecności opiekuna.

### 4.3 Przewodnik (po jednej instancji na trasę)

* Zbieranie grupy do maks. pojemności trasy (Ni).
* Dopilnowanie, aby przed wyruszeniem na trasę kładka była pusta.
* Zezwalanie na wejście na kładkę maksymalnie po **K** osób jednocześnie (kładka jednokierunkowa).
* Monitorowanie czasu zwiedzania (T1/T2) i ogłaszanie zakończenia.
* Reakcja na sygnał strażnika:

  * jeśli sygnał otrzymany przed wyjściem grupy — nie prowadzi wycieczki, grupa opuszcza jaskinię;
  * jeśli sygnał otrzymany w trakcie zwiedzania — grupa kończy zwiedzanie normalnie.

### 4.4 Strażnik

* Nadawanie sygnałów `sygnał1` i `sygnał2` informujących przewodników o konieczności zakończenia przyjmowania nowych grup (np. zbliżony czas Tk).
* Zapewnienie, że po wysłaniu sygnału nie rozpoczynają się żadne nowe wycieczki na danej trasie.

## 5. Wymagania synchronizacyjne

* Kładka umożliwia ruch w danym momencie tylko w jednym kierunku. Należy zastosować mechanizm zapewniający wykluczenie wzajemne ruchu wejścia i wyjścia na kładce.
* Przed rozpoczęciem wejścia przewodnik musi upewnić się, że na kładce nie ma osób (stan pusty).
* Na trasie nie może przebywać więcej osób niż Ni.
* Powracający zwiedzający (z prawem pominięcia kolejki) muszą być obsłużeni tak, aby nie naruszać ograniczeń Ni i K.

### Proponowane mechanizmy synchronizacji

* Semafory/liczniki dostępów (`semaphore` / `counting semaphore`) do ograniczania wejść na kładkę.
* Mutex/monitor do ochrony stanu kładki (kierunek ruchu i liczba osób)
* Zdarzenia/warunki (`condition variable`) do komunikacji przewodnika z kasjerem/zwiedzającymi o możliwości rozpoczęcia wejścia.

## 6. Struktura projektu i pliki wynikowe

* `kasjer.*` — moduł symulujący sprzedaż biletów.
* `zwiedzajacy.*` — moduł reprezentujący zachowanie zwiedzających.
* `przewodnik1.*`, `przewodnik2.*` — moduły przewodników dla trasy 1 i 2.
* `straznik.*` — moduł sterujący sygnałami.
* `main.*` — moduł uruchamiający symulację (konfiguracja parametrów).
* `logs/` — katalog z plikami logów/raportami (np. `raport_YYYYMMDD_HHMMSS.txt`).

Pliki logów powinny zawierać:

* znacznik czasu zdarzenia,
* zdarzenie (przyjście, zakup biletu, wejście na kładkę, rozpoczęcie zwiedzania, zakończenie, wyjście, sygnał strażnika),
* identyfikator zwiedzającego (np. ID + wiek),
* dodatkowe informacje (trasa, czy uprawnia do zniżki itp.).

## 7. Interfejs konfiguracji

Plik konfiguracyjny (`config.json` lub podobny) powinien zawierać parametry:

* `Tp`, `Tk`
* `N1`, `N2`
* `K`
* `T1`, `T2`
* tempo generacji zwiedzających (np. lambda dla procesu Poissona lub średni interwał)
* udział powracających (np. 0.1)

## 8. Scenariusze testowe

Opis i oczekiwane rezultaty dla kluczowych scenariuszy:

### Test 1 — Praca nominalna

* Brak sygnałów strażnika przed zakończeniem pracy.
* Oczekiwane: system przyjmuje i obsługuje zwiedzających zgodnie z ograniczeniami K, N1, N2; logi odzwierciedlają przebieg.

### Test 2 — Sygnał przed wyruszeniem grupy

* Strażnik wysyła sygnał dla trasy przed startem zaplanowanej grupy.
* Oczekiwane: przewodnik nie prowadzi wycieczki, grupa opuszcza jaskinię; brak rozpoczęcia wycieczki w logach.

### Test 3 — Sygnał w trakcie zwiedzania

* Strażnik wysyła sygnał dla trasy, gdy grupa jest w trakcie zwiedzania.
* Oczekiwane: grupa kończy zwiedzanie normalnie; po sygnale nie rozpoczynają się nowe wycieczki.

### Test 4 — Dziecko <8 z opiekunem

* Para dorosły + dziecko kupuje bilety; dziecko może wejść tylko na trasę 2.
* Oczekiwane: para przydzielona do trasy 2, nie dopuszcza się wejścia dziecka bez opiekuna.

### Test 5 — Osoba >75

* Osoba starsza zostaje skierowana tylko na trasę 2.

### Test 6 — Powtarzający zwiedzający

* Ok. 10% osób ponownie odwiedza jaskinię tego samego dnia z 50% zniżką i pominięciem kolejki.
* Oczekiwane: system obsługuje takie osoby z pierwszeństwem, przy zachowaniu limitów N i K.

## 9. Proponowana implementacja — wskazówki techniczne

* Język: dowolny język wspierający wielowątkowość (np. **Python** z `threading`/`multiprocessing`, **Java** z `java.util.concurrent`, **C/C++** z pthreads).
* Do logów użyć struktury tekstowej z czytelnymi wpisami; każdemu wpisowi przypisać timestamp.
* Użyć deterministycznych generatorów pseudolosowych (opcjonalnie przydatne do testów reproducibility) — np. z seedem.
* W implementacji testów uwzględnić tryb „fast-forward” umożliwiający szybsze wykonanie scenariuszy (skalowanie czasu).

## 10. Instrukcja uruchomienia (przykładowa)

1. Przygotuj `config.json` z parametrami symulacji.
2. Uruchom program główny, np.:

```bash
python main.py --config config.json --seed 42
```

3. Po zakończeniu symulacji sprawdź katalog `logs/` i odczytaj wygenerowany raport.

## 11. Kryteria oceny (sugerowane)

* Zgodność z regulaminem (zasady wiekowe, ograniczenia N i K).
* Poprawność synchronizacji (brak kolizji ruchu na kładce, brak przekroczeń Ni).
* Kompletny i czytelny raport zdarzeń.
* Pokrycie testów scenariuszy.
* Czytelność i modularność kodu.

## 12. Dodatki (opcjonalne)

* Interfejs graficzny lub prosty dashboard wyświetlający aktualny stan kolejek, kładek i trasy.
* Eksport logów do formatu CSV dla dalszej analizy.
* Wersja deterministyczna do automatycznych testów jednostkowych.

---

*Plik gotowy do wklejenia do dokumentu `.md`. Jeśli chcesz, mogę od razu dołączyć przykładową implementację (wielowątkową) w Pythonie z wygenerowanymi testami — wtedy dołączę także plik konfiguracyjny i przykładowy log.*
