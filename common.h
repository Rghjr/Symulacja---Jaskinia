#ifndef COMMON_H
#define COMMON_H

/// ================= PODSTAWOWE INCLUDE'Y =================
/// Wszystkie potrzebne do IPC, procesów i logowania
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>


/// ================= KONFIGURACJA SYMULACJI =================
/// Limity osób na trasach
#define N1 15
#define N2 20

/// Pojemność kładki
#define K 5

/// Czasy zwiedzania tras (sekundy)
#define T1 10
#define T2 15

/// Czas otwarcia i zamknięcia jaskini
#define Tp 0
#define Tk 1800

/// Generator zwiedzających – losowe opóźnienia
#define GENERATOR_MIN_DELAY 1
#define GENERATOR_MAX_DELAY 3

/// Szansa na powtórną wizytę (%)
#define REPEAT_CHANCE 10


/// ================= KLUCZE IPC =================
/// Klucz pamięci dzielonej
#define SHM_KEY 0x1234

/// Klucz zestawu semaforów
#define SEM_KEY 0x5678

/// Klucze kolejek komunikatów
#define MSG_KEY_KASJER       0x9ABC
#define MSG_KEY_PRZEWODNIK1  0xDEF0
#define MSG_KEY_PRZEWODNIK2  0x1234


/// ================= KIERUNKI KŁADKI =================
/// Brak ruchu
#define KIERUNEK_PUSTY 0

/// Ruch do środka jaskini
#define KIERUNEK_WEJSCIE 1

/// Ruch na zewnątrz
#define KIERUNEK_WYJSCIE 2


/// ================= DECYZJE KASJERA =================
/// Zwiedzający odrzucony
#define DECYZJA_ODRZUCONY 0

/// Zwiedzający idzie na trasę 1
#define DECYZJA_TRASA1 1

/// Zwiedzający idzie na trasę 2
#define DECYZJA_TRASA2 2


/// ================= TYPY KOMUNIKATÓW =================
/// Żądanie do kasjera
#define MSG_TYPE_REQUEST 1

/// Odpowiedź kasjera
#define MSG_TYPE_RESPONSE 2

/// Komunikat do przewodnika
#define MSG_TYPE_ZWIEDZAJACY 3


/// ================= SEMAFORY =================
/// Mutex do synchronizacji dostępu do kładki
#define SEM_MUTEX_KLADKA 0

/// Licznik miejsc na kładce
#define SEM_MIEJSCA_KLADKA 1

/// Mutex trasy 1
#define SEM_MUTEX_TRASA1 2

/// Mutex trasy 2
#define SEM_MUTEX_TRASA2 3

/// Mutex do logów (jeśli kilka procesów loguje naraz)
#define SEM_MUTEX_LOG 4

/// Łączna liczba semaforów
#define SEM_COUNT 5


/// ================= STRUKTURA semun =================
/// Wymagana przez semctl (SysV jest stare i upierdliwe)
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};


/// ================= PAMIĘĆ DZIELONA =================
/// Wspólny stan całej symulacji
typedef struct {
    int jaskinia_otwarta;
    int osoby_na_trasie1;
    int osoby_na_trasie2;
    int kierunek_kladki;
    int osoby_na_kladce;
    time_t timestamp_start;

    /// PID-y głównych procesów
    pid_t pid_przewodnik1;
    pid_t pid_przewodnik2;
    pid_t pid_straznik;
    pid_t pid_kasjer;
    pid_t pid_generator;
} SharedMemory;


/// ================= KOMUNIKATY =================
/// Komunikat wysyłany do kasjera
typedef struct {
    long mtype;
    pid_t pid_zwiedzajacego;
    int wiek;
    int powtorna_wizyta;
    int poprzednia_trasa;
    int ma_opiekuna;
    pid_t pid_opiekuna;
} MessageKasjer;

/// Odpowiedź kasjera
typedef struct {
    long mtype;
    int decyzja;
    int przydzielona_trasa;
} MessageOdpowiedz;

/// Komunikat do przewodnika
typedef struct {
    long mtype;
    pid_t pid_zwiedzajacego;
    int wiek;
} MessagePrzewodnik;


/// ================= OPERACJE NA SEMAFORACH =================
/// Blokujące opuszczenie semafora (P)
void sem_wait(int semid, int sem_num);

/// Podniesienie semafora (V)
void sem_signal(int semid, int sem_num);

/// Próba opuszczenia semafora bez blokowania
int sem_trywait(int semid, int sem_num);

/// Ustawienie początkowej wartości semafora
void sem_init_value(int semid, int sem_num, int value);


/// ================= LOGOWANIE =================
/// Proste logowanie do pliku – pomocnicze dla debugowania

/// Zapis prostego komunikatu
void log_message(const char* process, const char* message);

/// Logowanie z formatowaniem printf
void log_formatted(const char* process, const char* format, ...);

#endif
