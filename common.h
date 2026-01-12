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

#define N1 15
#define N2 20
#define K 5

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

#define T1 10
#define T2 15
#define Tp 0
#define Tk 20

#define OPOZNIENIE_GENERATORA_MIN 0
#define OPOZNIENIE_GENERATORA_MAX 5
#define SZANSA_POWTORNA 5
#define SZANSA_DZIECKO_OPIEKUN 70
#define MIN_WIEK 1
#define MAX_WIEK 80
#define MIN_WIEK_OPIEKUNA 25
#define MAX_WIEK_OPIEKUNA 60
#define MAX_ZWIEDZAJACYCH 1000

#define CZAS_ZBIERANIA_GRUPY 5
#define CZAS_PRZECHODZENIA_KLADKA 200

#define TIMEOUT_ODPOWIEDZ_BILET 30
#define INTERWAL_POLLING 100
#define TIMEOUT_PUSTA_JASKINIA 300
#define WYPRZEDZENIE_SYGNAL_ZAMKNIECIA 10
#define TIMEOUT_CZEKAJ_CLEANUP 10
#define MAX_PROB_RETRY 10
#define INTERWAL_LOG 10

#define KLUCZ_SHM_JASKINIA 0x1001
#define KLUCZ_SHM_KLADKA1 0x1002
#define KLUCZ_SHM_KLADKA2 0x1003
#define KLUCZ_SHM_TRASA1 0x1004
#define KLUCZ_SHM_TRASA2 0x1005
#define KLUCZ_SHM_ZWIEDZAJACY 0x1009

#define KLUCZ_SEM_KLADKA1_MIEJSCA 0x2002
#define KLUCZ_SEM_KLADKA2_MIEJSCA 0x2004
#define KLUCZ_SEM_LOG 0x2005
#define KLUCZ_SEM_TRASA1_MUTEX 0x2006
#define KLUCZ_SEM_TRASA2_MUTEX 0x2007

#define KLUCZ_MSG_KASJER 0x3001
#define KLUCZ_MSG_PRZEWODNIK1 0x3002
#define KLUCZ_MSG_PRZEWODNIK2 0x3003

#define TYP_MSG_ZADANIE 1
#define TYP_MSG_ODPOWIEDZ 2
#define TYP_MSG_ZWIEDZAJACY 3
#define TYP_MSG_POWTORNA 4

#define DECYZJA_ODRZUCONY 0
#define DECYZJA_TRASA1 1
#define DECYZJA_TRASA2 2

#define KIERUNEK_PUSTY 0
#define KIERUNEK_WEJSCIE 1
#define KIERUNEK_WYJSCIE 2

typedef struct {
    volatile int otwarta;
    pthread_mutex_t mutex;
    pthread_cond_t cond_otwarta;
} ShmJaskinia;

typedef struct {
    volatile int kierunek;
    volatile int osoby;
    volatile pid_t przewodnik_pid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ShmKladka;

typedef struct {
    volatile int osoby;
} ShmTrasa;

typedef struct {
    volatile int licznik;
    pid_t pidy[MAX_ZWIEDZAJACYCH];
} ShmZwiedzajacy;

typedef struct {
    long mtype;
    pid_t pid_zwiedzajacego;
    int wiek;
    int powtorna_wizyta;
    int poprzednia_trasa;
    pid_t pid_opiekuna;
} WiadomoscKasjer;

typedef struct {
    long mtype;
    int decyzja;
    int przydzielona_trasa;
} WiadomoscOdpowiedz;

typedef struct {
    long mtype;
    pid_t pid_zwiedzajacego;
    int wiek;
} WiadomoscPrzewodnik;

union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};

static inline int bezpieczny_strtol(const char* str, int* wynik, int min, int max) {
    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    if (val < min || val > max) return -1;
    *wynik = (int)val;
    return 0;
}

extern int globalny_semid_log;

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

static inline ssize_t bezpieczny_zapis_wszystko(int fd, const void* buf, size_t ilosc) {
    size_t zapisane = 0;
    const char* ptr = (const char*)buf;

    while (zapisane < ilosc) {
        ssize_t n = write(fd, ptr + zapisane, ilosc - zapisane);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        zapisane += n;
    }
    return zapisane;
}

static inline int bezpieczny_semop(int semid, struct sembuf* ops, size_t liczba) {
    while (1) {
        if (semop(semid, ops, liczba) == 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
    }
}

static inline void bezpieczny_zapis_logu(const char* buf, size_t dlugosc, const char* nazwa_pliku) {
    int sem_zdobyty = 0;

    if (globalny_semid_log != -1) {
        struct sembuf op = { 0, -1, 0 };
        if (bezpieczny_semop(globalny_semid_log, &op, 1) == 0) {
            sem_zdobyty = 1;
        }
    }

    int fd = open(nazwa_pliku, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, dlugosc);
        close(fd);
    }

    if (sem_zdobyty) {
        struct sembuf op = { 0, 1, 0 };
        bezpieczny_semop(globalny_semid_log, &op, 1);
    }
}

static inline void bezpieczny_sem_wait(int semid, int numer) {
    struct sembuf op = { numer, -1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

static inline void bezpieczny_sem_signal(int semid, int numer) {
    struct sembuf op = { numer, 1, 0 };
    bezpieczny_semop(semid, &op, 1);
}

#endif