#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <limits.h>
#include <pthread.h>

// Parametry g³ówne systemu
#define N1 15  // Maksymalna liczba osób jednoczeœnie na trasie 1
#define N2 20  // Maksymalna liczba osób jednoczeœnie na trasie 2
#define K 5    // Maksymalna wielkoœæ grupy prowadzonej przez przewodnika

// Walidacja parametrów N1, N2, K
#if N1 <= 0
#error "N1 musi byc > 0"
#endif

#if N2 <= 0
#error "N2 musi byc > 0"
#endif

#if K <= 0
#error "K musi byc > 0"
#endif

#if K >= N1
#error "K musi byc < N1"
#endif

#if K >= N2
#error "K musi byc < N2"
#endif

// Parametry czasowe (w sekundach)
#define T1 10  // Czas zwiedzania trasy 1
#define T2 15  // Czas zwiedzania trasy 2
#define Tp 0   // Czas planowania (nieu¿ywany)
#define Tk 20  // Czas pracy kasy biletowej

// Parametry generatora zwiedzaj¹cych
#define OPOZNIENIE_GENERATORA_MIN 0  // Minimalne opóŸnienie miêdzy generowaniem zwiedzaj¹cych
#define OPOZNIENIE_GENERATORA_MAX 5  // Maksymalne opóŸnienie miêdzy generowaniem zwiedzaj¹cych
#define SZANSA_POWTORNA 10           // Szansa (w %) na powrotn¹ wizytê
#define SZANSA_DZIECKO_OPIEKUN 70    // Szansa (w %) ¿e dziecko przyjdzie z opiekunem
#define MIN_WIEK 1                   // Minimalny wiek zwiedzaj¹cego
#define MAX_WIEK 80                  // Maksymalny wiek zwiedzaj¹cego
#define MIN_WIEK_OPIEKUNA 25         // Minimalny wiek opiekuna
#define MAX_WIEK_OPIEKUNA 60         // Maksymalny wiek opiekuna
#define MAX_ZWIEDZAJACYCH 1000       // Maksymalna liczba zwiedzaj¹cych w systemie

// Parametry czasowe dotycz¹ce zwiedzania
#define CZAS_ZBIERANIA_GRUPY 5         // Czas oczekiwania na zebranie grupy (sekundy)
#define CZAS_PRZECHODZENIA_KLADKA 200  // Czas przechodzenia przez k³adkê (milisekundy)

// Timeouty i interwa³y
#define TIMEOUT_ODPOWIEDZ_BILET 30        // Timeout oczekiwania na odpowiedŸ od kasjera
#define INTERWAL_POLLING 100              // Interwa³ pollingu (milisekundy)
#define TIMEOUT_PUSTA_JASKINIA 300        // Timeout oczekiwania na pust¹ jaskiniê przy zamykaniu
#define WYPRZEDZENIE_SYGNAL_ZAMKNIECIA 10 // Wyprzedzenie sygna³u zamkniêcia jaskini
#define TIMEOUT_CZEKAJ_CLEANUP 10         // Timeout oczekiwania podczas czyszczenia zasobów
#define MAX_PROB_RETRY 10                 // Maksymalna liczba prób ponowienia operacji
#define INTERWAL_LOG 10                   // Interwa³ logowania (sekundy)
#define PROBY_SPRAWDZ_OPIEKUNA 3          // Liczba prób sprawdzenia opiekuna

// Klucze IPC dla pamiêci wspó³dzielonej (zmienione na losowe)
#define KLUCZ_SHM_JASKINIA 0x7A2F      // Klucz dla stanu jaskini (otwarta/zamkniêta)
#define KLUCZ_SHM_KLADKA1 0x4B91       // Klucz dla k³adki na trasie 1
#define KLUCZ_SHM_KLADKA2 0x8E3C       // Klucz dla k³adki na trasie 2
#define KLUCZ_SHM_TRASA1 0x2D74        // Klucz dla licznika osób na trasie 1
#define KLUCZ_SHM_TRASA2 0x6A1E        // Klucz dla licznika osób na trasie 2
#define KLUCZ_SHM_ZWIEDZAJACY 0x9F42   // Klucz dla listy aktywnych zwiedzaj¹cych

// Klucze IPC dla semaforów (zmienione na losowe)
#define KLUCZ_SEM_KLADKA1_MIEJSCA 0x3C8B  // Semafor limituj¹cy miejsca na k³adce 1
#define KLUCZ_SEM_KLADKA2_MIEJSCA 0x5D29  // Semafor limituj¹cy miejsca na k³adce 2
#define KLUCZ_SEM_LOG 0x1A7E              // Semafor synchronizuj¹cy zapis do logów
#define KLUCZ_SEM_TRASA1_MUTEX 0x4F63     // Mutex dla operacji na trasie 1
#define KLUCZ_SEM_TRASA2_MUTEX 0x8B94     // Mutex dla operacji na trasie 2

// Klucze IPC dla kolejek komunikatów (zmienione na losowe)
#define KLUCZ_MSG_KASJER 0x2E5A        // Kolejka komunikatów kasjera
#define KLUCZ_MSG_PRZEWODNIK1 0x7C1F   // Kolejka komunikatów przewodnika trasy 1
#define KLUCZ_MSG_PRZEWODNIK2 0x9A48   // Kolejka komunikatów przewodnika trasy 2

// Typy komunikatów w kolejkach
#define TYP_MSG_ZADANIE 1      // Typ: zadanie/¿¹danie
#define TYP_MSG_ODPOWIEDZ 2    // Typ: odpowiedŸ na ¿¹danie
#define TYP_MSG_ZWIEDZAJACY 3  // Typ: informacja o zwiedzaj¹cym
#define TYP_MSG_POWTORNA 4     // Typ: powrotna wizyta

// Decyzje kasjera
#define DECYZJA_ODRZUCONY 0  // Zwiedzaj¹cy odrzucony
#define DECYZJA_TRASA1 1     // Przydzielono trasê 1
#define DECYZJA_TRASA2 2     // Przydzielono trasê 2

// Kierunki ruchu na k³adce
#define KIERUNEK_PUSTY 0    // K³adka pusta
#define KIERUNEK_WEJSCIE 1  // Ruch w kierunku wejœcia do jaskini
#define KIERUNEK_WYJSCIE 2  // Ruch w kierunku wyjœcia z jaskini

// Struktura pamiêci wspó³dzielonej - stan jaskini
typedef struct {
    volatile int otwarta;           // Czy jaskinia jest otwarta (1) czy zamkniêta (0)
    pthread_mutex_t mutex;          // Mutex zabezpieczaj¹cy dostêp
    pthread_cond_t cond_otwarta;    // Zmienna warunkowa dla stanu otwarcia
} ShmJaskinia;

// Struktura pamiêci wspó³dzielonej - stan k³adki
typedef struct {
    volatile int kierunek;          // Aktualny kierunek ruchu na k³adce
    volatile int osoby;             // Liczba osób aktualnie na k³adce
    volatile pid_t przewodnik_pid;  // PID przewodnika prowadz¹cego grupê
    pthread_mutex_t mutex;          // Mutex zabezpieczaj¹cy dostêp
    pthread_cond_t cond;            // Zmienna warunkowa dla synchronizacji
} ShmKladka;

// Struktura pamiêci wspó³dzielonej - stan trasy
typedef struct {
    volatile int osoby;  // Liczba osób aktualnie na trasie
} ShmTrasa;

// Struktura pamiêci wspó³dzielonej - lista zwiedzaj¹cych
typedef struct {
    volatile int licznik;                  // Liczba aktywnych zwiedzaj¹cych
    pid_t pidy[MAX_ZWIEDZAJACYCH];        // Tablica PID-ów zwiedzaj¹cych
} ShmZwiedzajacy;

// Struktura komunikatu do kasjera
typedef struct {
    long mtype;                // Typ wiadomoœci
    pid_t pid_zwiedzajacego;   // PID zwiedzaj¹cego
    int wiek;                  // Wiek zwiedzaj¹cego
    int powtorna_wizyta;       // Czy to powrotna wizyta (1/0)
    int poprzednia_trasa;      // Numer poprzedniej trasy (dla powrotnych wizyt)
    pid_t pid_opiekuna;        // PID opiekuna (jeœli jest)
    int czy_opiekun;           // Czy osoba jest opiekunem (1/0)
} WiadomoscKasjer;

// Struktura komunikatu odpowiedzi od kasjera
typedef struct {
    long mtype;              // Typ wiadomoœci (PID zwiedzaj¹cego)
    int decyzja;             // Decyzja: odrzucony/trasa1/trasa2
    int przydzielona_trasa;  // Numer przydzielonej trasy
} WiadomoscOdpowiedz;

// Struktura komunikatu do przewodnika
typedef struct {
    long mtype;              // Typ wiadomoœci
    pid_t pid_zwiedzajacego; // PID zwiedzaj¹cego
    int wiek;                // Wiek zwiedzaj¹cego
} WiadomoscPrzewodnik;

// Unia pomocnicza dla operacji na semaforach
union semun {
    int val;                    // Wartoœæ dla SETVAL
    struct semid_ds* buf;       // Bufor dla IPC_STAT, IPC_SET
    unsigned short* array;      // Tablica dla GETALL, SETALL
};

// Funkcja pomocnicza - bezpieczna konwersja stringa na int z walidacj¹
static inline int bezpieczny_strtol(const char* str, int* wynik, int min, int max) {
    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    if (val < min || val > max) return -1;
    *wynik = (int)val;
    return 0;
}

// Globalny identyfikator semafora dla logowania
extern int globalny_semid_log;

// Funkcja pomocnicza - pod³¹czenie do semafora logowania z retry
static inline int podlacz_sem_log() {
    int retry = 0;
    while (retry < MAX_PROB_RETRY) {
        int semid = semget(KLUCZ_SEM_LOG, 0, 0);
        if (semid != -1) return semid;
        usleep(INTERWAL_POLLING * 1000);
        retry++;
    }
    return -1;
}

// Funkcja pomocnicza - bezpieczny zapis ca³ego bufora do pliku
static inline ssize_t bezpieczny_zapis_wszystko(int fd, const void* buf, size_t ilosc) {
    size_t zapisane = 0;
    const char* ptr = (const char*)buf;

    while (zapisane < ilosc) {
        ssize_t n = write(fd, ptr + zapisane, ilosc - zapisane);
        if (n == -1) {
            if (errno == EINTR) continue;  // Retry przy przerwaniu sygna³em
            return -1;
        }
        zapisane += n;
    }
    return zapisane;
}

// Funkcja pomocnicza - bezpieczna operacja na semaforze z obs³ug¹ EINTR
static inline int bezpieczny_semop(int semid, struct sembuf* ops, size_t liczba) {
    while (1) {
        if (semop(semid, ops, liczba) == 0) {
            return 0;
        }
        if (errno != EINTR) {  // Jeœli b³¹d inny ni¿ przerwanie sygna³em
            return -1;
        }
        // Przy EINTR - próbuj ponownie
    }
}

// Funkcja pomocnicza - bezpieczny zapis do logu z synchronizacj¹
static inline void bezpieczny_zapis_logu(const char* buf, size_t dlugosc, const char* nazwa_pliku) {
    int sem_zdobyty = 0;

    // Próba zdobycia semafora logowania
    if (globalny_semid_log != -1) {
        struct sembuf op = { 0, -1, 0 };
        if (bezpieczny_semop(globalny_semid_log, &op, 1) == 0) {
            sem_zdobyty = 1;
        }
    }

    // Otwarcie i zapis do pliku
    int fd = open(nazwa_pliku, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, dlugosc);
        close(fd);
    }

    // Zwolnienie semafora logowania
    if (sem_zdobyty) {
        struct sembuf op = { 0, 1, 0 };
        bezpieczny_semop(globalny_semid_log, &op, 1);
    }
}

// Funkcja pomocnicza - wait na semaforze (P operation)
static inline void bezpieczny_sem_wait(int semid, int numer) {
    struct sembuf op = { numer, -1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

// Funkcja pomocnicza - signal na semaforze (V operation)
static inline void bezpieczny_sem_signal(int semid, int numer) {
    struct sembuf op = { numer, 1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

#endif