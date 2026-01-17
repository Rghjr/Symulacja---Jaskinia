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

/// Główne limity systemu - wymagane w zadaniu
#define N1 15  /// Max osób jednocześnie na trasie 1
#define N2 20  /// Max osób jednocześnie na trasie 2
#define K 5    /// Max osób na kładce (K < Ni)

/// Sprawdzamy czy parametry mają sens - kompilator wyrzuci błąd jak nie
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

/// Czasy w sekundach - jak długo co trwa
#define T1 10  /// Zwiedzanie trasy 1
#define T2 15  /// Zwiedzanie trasy 2
#define Tp 0   /// Czas planowania (nieużywany bo 0)
#define Tk 20  /// Jak długo kasa działa

/// Ustawienia generatora zwiedzających
#define OPOZNIENIE_GENERATORA_MIN 0  /// Min przerwa między ludźmi
#define OPOZNIENIE_GENERATORA_MAX 5  /// Max przerwa między ludźmi
#define SZANSA_POWTORNA 10           /// 10% ludzi wraca drugi raz
#define SZANSA_DZIECKO_OPIEKUN 70    /// 70% dzieci przychodzi z dorosłym
#define MIN_WIEK 1                   /// Najmłodsze dziecko
#define MAX_WIEK 80                  /// Najstarszy senior
#define MIN_WIEK_OPIEKUNA 25         /// Opiekun musi mieć min 25 lat
#define MAX_WIEK_OPIEKUNA 60         /// Opiekun musi mieć max 60 lat
#define MAX_ZWIEDZAJACYCH 1000       ///Limit procesów w systemie

/// Parametry techniczne
#define CZAS_ZBIERANIA_GRUPY 5         /// Przewodnik czeka max 5s na pełną grupę
#define CZAS_PRZECHODZENIA_KLADKA 200  /// Każdy idzie 200ms przez kładkę

/// Timeouty żeby nie czekać w nieskończoność
#define TIMEOUT_ODPOWIEDZ_BILET 30        /// Max czekanie na kasjer
#define INTERWAL_POLLING 100              /// Co ile ms sprawdzać (polling)
#define TIMEOUT_PUSTA_JASKINIA 300        /// Max czekanie aż wszyscy wyjdą
#define WYPRZEDZENIE_SYGNAL_ZAMKNIECIA 10 /// Sygnał zamknięcia 10s przed końcem
#define TIMEOUT_CZEKAJ_CLEANUP 10         /// Ile czekać przy sprzątaniu
#define MAX_PROB_RETRY 10                 /// Ile razy próbować podłączyć IPC
#define INTERWAL_LOG 10                   /// Co ile sekund logować stan
#define PROBY_SPRAWDZ_OPIEKUNA 3          /// Ile razy sprawdzić czy opiekun żyje

/// Klucze IPC - losowe żeby nie kolidowały z innymi programami
#define KLUCZ_SHM_JASKINIA 0x7A2F      /// Czy jaskinia otwarta/zamknięta
#define KLUCZ_SHM_KLADKA1 0x4B91       /// Stan kładki 1
#define KLUCZ_SHM_KLADKA2 0x8E3C       /// Stan kładki 2
#define KLUCZ_SHM_TRASA1 0x2D74        /// Ile osób na trasie 1
#define KLUCZ_SHM_TRASA2 0x6A1E        /// Ile osób na trasie 2
#define KLUCZ_SHM_ZWIEDZAJACY 0x9F42   /// Lista PIDów zwiedzających

/// Klucze dla semaforów
#define KLUCZ_SEM_KLADKA1_MIEJSCA 0x3C8B  /// Semafor limitujący kładkę 1 (max K)
#define KLUCZ_SEM_KLADKA2_MIEJSCA 0x5D29  /// Semafor limitujący kładkę 2 (max K)
#define KLUCZ_SEM_LOG 0x1A7E              /// Mutex do zapisu logów
#define KLUCZ_SEM_TRASA1_MUTEX 0x4F63     /// Mutex do licznika trasy 1
#define KLUCZ_SEM_TRASA2_MUTEX 0x8B94     /// Mutex do licznika trasy 2

/// Klucze dla kolejek komunikatów
#define KLUCZ_MSG_KASJER 0x2E5A        /// Kolejka do kasjera (prośby o bilety)
#define KLUCZ_MSG_PRZEWODNIK1 0x7C1F   /// Kolejka do przewodnika 1
#define KLUCZ_MSG_PRZEWODNIK2 0x9A48   /// Kolejka do przewodnika 2

/// Typy wiadomości w kolejkach - żeby kasjer wiedział co to za request
#define TYP_MSG_ZADANIE 1      /// Zwykłe zadanie (pierwsza wizyta)
#define TYP_MSG_ODPOWIEDZ 2    /// Odpowiedź od kasjera
#define TYP_MSG_ZWIEDZAJACY 3  /// Info o zwiedzającym do przewodnika
#define TYP_MSG_POWTORNA 4     /// Powtórna wizyta (wyższy priorytet!)

/// Decyzje kasjera
#define DECYZJA_ODRZUCONY 0  /// Nie wpuszczamy (np. dziecko bez opiekuna)
#define DECYZJA_TRASA1 1     /// Idziesz na trasę 1
#define DECYZJA_TRASA2 2     /// Idziesz na trasę 2

/// Kierunki na kładce - ważne bo kładka wąska (tylko jeden kierunek!)
#define KIERUNEK_PUSTY 0    /// Nikt nie idzie, można zablokować
#define KIERUNEK_WEJSCIE 1  /// Ludzie wchodzą do jaskini
#define KIERUNEK_WYJSCIE 2  /// Ludzie wychodzą z jaskini

/// Stan jaskini - czy otwarta czy już zamknięta
typedef struct {
    volatile int otwarta;           /// 1 = otwarta, 0 = zamknięta
    pthread_mutex_t mutex;          /// Mutex do bezpiecznej zmiany stanu
    pthread_cond_t cond_otwarta;    /// Condition variable - budzimy procesy gdy otwieramy
} ShmJaskinia;

/// Stan kładki - kto i w którą stronę idzie
typedef struct {
    volatile int kierunek;          /// WEJSCIE/WYJSCIE/PUSTY
    volatile int osoby;             /// Ile osób aktualnie na kładce
    volatile pid_t przewodnik_pid;  /// PID przewodnika który zablokował kładkę
    pthread_mutex_t mutex;          /// Mutex do zmian
    pthread_cond_t cond;            /// Condition variable do oczekiwania
} ShmKladka;

/// Licznik osób na trasie - prosta struktura
typedef struct {
    volatile int osoby;  /// Ile osób aktualnie zwieda
} ShmTrasa;

/// Lista wszystkich aktywnych zwiedzających - do cleanup
typedef struct {
    volatile int licznik;                  /// Ile jest zwiedzających
    pid_t pidy[MAX_ZWIEDZAJACYCH];        /// Tablica ich PIDów
} ShmZwiedzajacy;

/// Wiadomość do kasjera - prośba o bilet
typedef struct {
    long mtype;                /// Typ: TYP_MSG_ZADANIE lub TYP_MSG_POWTORNA
    pid_t pid_zwiedzajacego;   /// Mój PID (żeby kasjer wiedział komu odpowiedzieć)
    int wiek;                  /// Mój wiek (dla regulaminu)
    int powtorna_wizyta;       /// Czy to mój drugi raz (50% zniżka!)
    int poprzednia_trasa;      /// Jeśli powtórna - na której byłem
    pid_t pid_opiekuna;        /// PID opiekuna jeśli jestem dzieckiem
    int czy_opiekun;           /// Czy sam jestem opiekunem dziecka
} WiadomoscKasjer;

/// Odpowiedź od kasjera - czy dostałem bilet
typedef struct {
    long mtype;              /// PID zwiedzającego (żeby każdy dostał swoją odpowiedź)
    int decyzja;             /// ODRZUCONY/TRASA1/TRASA2
    int przydzielona_trasa;  /// Konkretny numer trasy
} WiadomoscOdpowiedz;

/// Wiadomość do przewodnika - dołączam do grupy
typedef struct {
    long mtype;              /// TYP_MSG_ZWIEDZAJACY
    pid_t pid_zwiedzajacego; /// Mój PID
    int wiek;                /// Mój wiek (dla statystyk)
} WiadomoscPrzewodnik;

/// Unia pomocnicza dla starszych wersji POSIX (semctl wymaga)
union semun {
    int val;                    /// Wartość dla SETVAL
    struct semid_ds* buf;       /// Bufor dla IPC_STAT
    unsigned short* array;      /// Tablica dla GETALL
};

/// Pomocnicza funkcja - bezpieczna konwersja string→int z walidacją
static inline int bezpieczny_strtol(const char* str, int* wynik, int min, int max) {
    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    if (val < min || val > max) return -1;
    *wynik = (int)val;
    return 0;
}

/// Globalny semafor do logowania - żeby logi się nie mieszały
extern int globalny_semid_log;

/// Podłącz się do semafora logów z retry
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

/// Zapisz cały bufor do pliku - retry przy EINTR
static inline ssize_t bezpieczny_zapis_wszystko(int fd, const void* buf, size_t ilosc) {
    size_t zapisane = 0;
    const char* ptr = (const char*)buf;

    while (zapisane < ilosc) {
        ssize_t n = write(fd, ptr + zapisane, ilosc - zapisane);
        if (n == -1) {
            if (errno == EINTR) continue;  /// Przerwał sygnał - próbuj dalej
            return -1;
        }
        zapisane += n;
    }
    return zapisane;
}

/// Bezpieczny semop - retry przy przerwaniu sygnałem
static inline int bezpieczny_semop(int semid, struct sembuf* ops, size_t liczba) {
    while (1) {
        if (semop(semid, ops, liczba) == 0) {
            return 0;
        }
        if (errno != EINTR) {  /// Jeśli nie EINTR to prawdziwy błąd
            return -1;
        }
        /// Przy EINTR próbuj ponownie
    }
}

/// Zapisz do logu z mutexem - żeby wiele procesów mogło logować równocześnie
static inline void bezpieczny_zapis_logu(const char* buf, size_t dlugosc, const char* nazwa_pliku) {
    int sem_zdobyty = 0;

    /// Próba zdobycia mutexu
    if (globalny_semid_log != -1) {
        struct sembuf op = { 0, -1, 0 };
        if (bezpieczny_semop(globalny_semid_log, &op, 1) == 0) {
            sem_zdobyty = 1;
        }
    }

    /// Zapis do pliku
    int fd = open(nazwa_pliku, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, dlugosc);
        close(fd);
    }

    /// Zwolnienie mutexu
    if (sem_zdobyty) {
        struct sembuf op = { 0, 1, 0 };
        bezpieczny_semop(globalny_semid_log, &op, 1);
    }
}

/// P operation (wait) na semaforze
static inline void bezpieczny_sem_wait(int semid, int numer) {
    struct sembuf op = { numer, -1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

/// V operation (signal) na semaforze
static inline void bezpieczny_sem_signal(int semid, int numer) {
    struct sembuf op = { numer, 1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

#endif