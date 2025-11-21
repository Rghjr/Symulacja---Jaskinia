Opis Projektu – Temat 13: Jaskinia
1. Wprowadzenie

Celem projektu jest stworzenie symulacji systemu organizacji zwiedzania jaskini, w której funkcjonują dwie niezależne trasy turystyczne. System powinien odwzorowywać zasady regulaminu, ograniczenia fizyczne infrastruktury (kładki wejściowe i wyjściowe), sposób działania przewodników, strażnika oraz kasjera, a także zachowania zwiedzających.
Wyniki działania symulacji muszą zostać zapisane w jednym lub kilku plikach tekstowych, stanowiących raport z przebiegu całego procesu.

2. Opis środowiska i założeń

Zwiedzanie jaskini odbywa się w przedziale czasowym od Tp do Tk. W tym czasie turyści mogą wejść do jaskini na jedną z dwóch tras:

Trasa 1 – maksymalna liczba zwiedzających: N1

Trasa 2 – maksymalna liczba zwiedzających: N2

Wejście i wyjście z jaskini odbywa się poprzez dwie wąskie kładki, z których każda ma pojemność K (przy czym K < Ni). Kładka umożliwia w danym momencie ruch wyłącznie w jednym kierunku – nie jest możliwe jednoczesne wchodzenie i wychodzenie.

Zwiedzanie trwa:

na trasie 1 – T1 jednostek czasu,

na trasie 2 – T2 jednostek czasu.

Zwiedzający pojawiają się losowo, każdy w wieku od 1 do 80 lat.

3. Zasady wejścia i regulamin

Zwiedzający mogą wchodzić na trasy zgodnie z następującymi zasadami:

Dzieci poniżej 3 lat – nie płacą za bilet.

Dzieci poniżej 8 lat:

mogą zwiedzać wyłącznie trasę 2,

muszą pozostawać pod opieką osoby dorosłej.

Osoby powyżej 75 roku życia – mogą zwiedzać wyłącznie trasę 2.

Zwiedzający powracający (ok. 10%) – mogą zakupić bilet z 50% zniżką i wejść na inną trasę niż poprzednio, z pominięciem kolejki, pod warunkiem że regulamin na to pozwala.

4. Kładka wejściowa i wyjściowa

Kładka ma pojemność K, co oznacza, że jednocześnie może znajdować się na niej najwyżej K osób, przy czym ruch może odbywać się tylko w jednym kierunku:

wchodzenie → wyłącznie gdy nikt nie wychodzi,

wychodzenie → wyłącznie gdy nikt nie wchodzi.

Przewodnik może rozpocząć wycieczkę dopiero wtedy, gdy kładka jest całkowicie pusta.

5. Praca przewodników

Dla każdej trasy działa niezależny przewodnik. Jego zadania:

kontrola liczby turystów wchodzących na trasę (nie więcej niż N1/N2),

kontrola ruchu na kładce (wejście tylko przy pustej kładce),

informowanie systemu o rozpoczęciu i zakończeniu wycieczki,

reakcja na sygnały strażnika:

Sygnał przerwania (strażnik):

jeśli sygnał dotrze przed wyruszeniem grupy, wycieczka nie jest prowadzona, a zwiedzający opuszczają jaskinię,

jeśli sygnał dotrze w trakcie wycieczki, grupa kończy zwiedzanie normalnie.

6. Rola kasjera

Kasjer odpowiada za:

sprzedaż biletów (z uwzględnieniem zasad regulaminu i konieczności obecności opiekuna),

kwalifikowanie turystów na właściwą trasę,

obsługę zwiedzających powracających i pomijających kolejkę,

kierowanie zwiedzających do kolejki właściwej dla każdej trasy.

7. Rola zwiedzających

Każdy zwiedzający:

pojawia się w losowym momencie,

kupuje bilet i trafia do odpowiedniej kolejki,

czeka na wejście na trasę zgodnie z zasadami,

po zakończeniu wycieczki opuszcza jaskinię poprzez kładkę,

może w szczególnych przypadkach powtórzyć zwiedzanie innej trasy (z pominięciem kolejki).

8. Rola strażnika

Strażnik:

nadzoruje godziny pracy jaskini,

wysyła sygnały sygnał1 (dla trasy 1) i sygnał2 (dla trasy 2) informujące o konieczności zakończenia przyjmowania nowych grup przed czasem Tk,

zapewnia, że po wysłaniu sygnału żadna nowa wycieczka nie rozpocznie się.

9. Synchronizacja i wymogi systemowe

W projekcie należy zaimplementować mechanizmy synchronizacji, które umożliwią:

kontrolę ilości zwiedzających w kolejkach,

kontrolę ruchu jednokierunkowego po kładce,

zapobieganie wejściu grupy, gdy kładka jest zajęta,

równoległą obsługę dwóch tras,

właściwą komunikację między przewodnikiem, kasjerem, zwiedzającymi i strażnikiem.

10. Raport z działania symulacji

Symulacja powinna generować raport w postaci jednego lub kilku plików tekstowych, w których znajdą się:

czas pojawienia się zwiedzających,

zakup biletów wraz z typem trasy,

wejścia i wyjścia z jaskini,

rozpoczęcia i zakończenia wycieczek,

momenty zmiany stanu kładki,

informacje o wysłanych sygnałach strażnika.

11. Testy, które należy przewidzieć

Projekt powinien umożliwić przetestowanie następujących scenariuszy:

Normalna praca jaskini bez sygnałów strażnika.

Otrzymanie sygnału przerwania przed wyjściem grupy.

Otrzymanie sygnału przerwania w trakcie zwiedzania.

Obsługa opiekuna z dzieckiem <8 lat.

Obsługa osoby starszej (>75 lat).

Odmowa wejścia na niewłaściwą trasę.

Poprawna obsługa zwiedzających powracających i pomijających kolejkę.

Kontrola pojemności kładki i tras.

12. Zawartość projektu

W ramach projektu należy przygotować:

program przewodnika (dla obu tras),

program kasjera,

program zwiedzającego,

program strażnika,

raport wynikowy,

plik opisowy .md zgodnie z formatem:
