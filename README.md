# Opis projektu — Temat 13: Jaskinia

## 1. Streszczenie projektu

Projekt przedstawia symulację organizacji zwiedzania jaskini posiadającej dwie niezależne trasy turystyczne. System odwzorowuje zachowania zwiedzających, obsługę kasjera, działanie przewodników oraz reakcje strażnika. Implementacja musi respektować regulamin oraz ograniczenia infrastrukturalne (m.in. pojemność kładki, limity tras). Wszystkie zdarzenia są rejestrowane w logach.

## 2. Założenia działania systemu

* Okres zwiedzania: od (T_p) do (T_k).
* Maksymalna liczba zwiedzających:

  * Trasa 1: **N1**
  * Trasa 2: **N2**
* Pojemność kładki: **K** (z założeniem **K < Ni**).
* Czas zwiedzania: **T1** (trasa 1), **T2** (trasa 2).
* Zwiedzający generowani losowo, wiek 1–80.
* ~10% to zwiedzający powracający — 50% zniżki + pominięcie kolejki.

## 3. Zasady regulaminowe

1. Dzieci <3 lat — bilet bezpłatny.
2. Dzieci <8 lat:

   * tylko trasa 2,
   * wymagany opiekun.
3. Osoby >75 lat — tylko trasa 2.
4. Zwiedzający powracający — zniżka 50% + pominięcie kolejki.

## 4. Moduły systemu

### 4.1 Kasjer

* Sprawdza wiek i zgodność wyboru trasy z regulaminem.
* Sprzedaje bilety, nalicza zniżki.
* Kieruje do odpowiedniej kolejki.
* Generuje logi kasjera w `logs/kasjer.log`.

### 4.2 Zwiedzający

* Kupuje bilet, oczekuje w kolejce, wchodzi na trasę, zwiedza i opuszcza jaskinię.
* Dziecko <8 lat wymaga opiekuna.
* Każdy zwiedzający generuje logi w `logs/zwiedzajacy.log`.

### 4.3 Przewodnik (oddzielny dla każdej trasy)

* Zbiera grupę (limit N1/N2).
* Czeka na pustą kładkę.
* Wpuszcza jednocześnie max **K** osób.
* Odpowiada za rozpoczęcie i zakończenie zwiedzania.
* Reaguje na sygnały strażnika.
* Generuje logi przewodnika w `logs/przewodnik_<trasa>.log`.

### 4.4 Strażnik

* Wysyła sygnały blokujące przyjmowanie nowych grup.
* Po sygnale nie może rozpocząć się nowa wycieczka.
* Generuje logi strażnika w `logs/straznik.log`.

## 5. Synchronizacja systemu

* Kładka jednokierunkowa — kontrolowana przez semafor lub zmienną w pamięci współdzielonej.
* Limit osób na trasie — licznik w pamięci współdzielonej.
* Komunikacja między procesami: kolejki do przewodnika, kasjera i strażnika.
* Procesy: zwiedzający, przewodnicy, kasjer, strażnik, każdy działa niezależnie.

## 6. Struktura projektu

* `kasjer.cpp / .h` — logika sprzedaży biletów + logi kasjera.
* `zwiedzajacy.cpp / .h` — obsługa zwiedzających + logi wszystkich zwiedzających.
* `przewodnik1.cpp`, `przewodnik2.cpp` — zarządzanie trasami + logi przewodników.
* `straznik.cpp` — generowanie sygnałów + logi strażnika.
* `main.cpp` — uruchamia symulację.
* `logs/` — katalog wszystkich plików logów.

## 7. Interfejs konfiguracji

Plik `config.json` definiuje parametry symulacji:

* **Tp**, **Tk** – czas startu i końca pracy,
* **N1**, **N2** – limity grup,
* **K** – pojemność kładki,
* **T1**, **T2** – czas zwiedzania tras,
* **tempo generacji zwiedzających** – np. λ procesu Poissona,
* **udział powracających** – np. 0.1,
* opcjonalnie: seed RNG, tryb debug, rozszerzone logi.

## 8. Scenariusze testowe

### Test 1 — Praca nominalna

**Oczekiwane:** prawidłowa obsługa kolejek; brak przekroczeń K, N1, N2; pełny cykl w logach.

### Test 2 — Sygnał przed rozpoczęciem zwiedzania

**Oczekiwane:** grupa anulowana; brak wpisu „rozpoczęcie zwiedzania”.

### Test 3 — Sygnał w trakcie zwiedzania

**Oczekiwane:** grupa kończy normalnie; brak nowych wycieczek po sygnale.

### Test 4 — Dziecko <8 + opiekun

**Oczekiwane:** automatyczne skierowanie na trasę 2; dziecko nie przejdzie bez opiekuna.

### Test 5 — Osoba >75

**Oczekiwane:** przydział wyłącznie na trasę 2.

### Test 6 — Zwiedzający powracający

**Oczekiwane:** zniżka 50%; pominięcie kolejki; oznaczenie w logach.

---

Dokument opisuje implementację projektu w języku C++.
