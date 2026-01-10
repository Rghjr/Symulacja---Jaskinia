#ifndef PRZEWODNIK_HELPERS_H
#define PRZEWODNIK_HELPERS_H

#include "common.h"

void loguj_wiadomosc(const char* wiadomosc);
void loguj_wiadomoscf(const char* format, ...);

static inline void wyslij_sygnal_do_grupy(pid_t* grupa, int liczba, int sygnal, const char* opis) {
    int wyslano = 0;
    for (int i = 0; i < liczba; i++) {
        if (grupa[i] > 0 && kill(grupa[i], 0) == 0) {
            kill(grupa[i], sygnal);
            wyslano++;
        }
    }
    loguj_wiadomoscf("Wyslano sygnal (%s) do %d/%d zwiedzajacych", opis, wyslano, liczba);
}

static inline void zablokuj_obie_kladki(ShmKladka* k1, ShmKladka* k2, int kierunek) {
    pid_t moj_pid = getpid();
    const char* nazwa_kierunku = (kierunek == KIERUNEK_WEJSCIE) ? "WEJSCIE" : "WYJSCIE";

    loguj_wiadomoscf("Blokuje obie kladki (kierunek: %s)", nazwa_kierunku);

    pthread_mutex_lock(&k1->mutex);

    while (k1->osoby > 0 || k1->przewodnik_pid != 0) {
        pthread_cond_wait(&k1->cond, &k1->mutex);
    }

    pthread_mutex_lock(&k2->mutex);

    while (k2->osoby > 0 || k2->przewodnik_pid != 0) {
        pthread_mutex_unlock(&k1->mutex);
        pthread_cond_wait(&k2->cond, &k2->mutex);
        pthread_mutex_unlock(&k2->mutex);

        pthread_mutex_lock(&k1->mutex);
        while (k1->osoby > 0 || k1->przewodnik_pid != 0) {
            pthread_cond_wait(&k1->cond, &k1->mutex);
        }
        pthread_mutex_lock(&k2->mutex);
    }

    k1->przewodnik_pid = moj_pid;
    k1->kierunek = kierunek;

    k2->przewodnik_pid = moj_pid;
    k2->kierunek = kierunek;

    loguj_wiadomoscf("Obie kladki zablokowane (PID=%d, kierunek=%s)", moj_pid, nazwa_kierunku);

    pthread_mutex_unlock(&k2->mutex);
    pthread_mutex_unlock(&k1->mutex);
}

static inline void przeprowadz_przez_kladke(
    int liczba_osob,
    ShmKladka* kladka,
    int sem_miejsca,
    int numer_kladki,
    int offset,
    pid_t* grupa,
    const char* nazwa_kierunku
) {
    if (liczba_osob == 0) return;

    for (int i = 0; i < liczba_osob; i++) {
        bezpieczny_sem_wait(sem_miejsca, 0);

        pthread_mutex_lock(&kladka->mutex);
        kladka->osoby++;
        int aktualne = kladka->osoby;

        if (aktualne > K) {
            loguj_wiadomoscf("CRITICAL: Kladka %d przekroczona! %d > %d", numer_kladki, aktualne, K);
        }
        pthread_mutex_unlock(&kladka->mutex);

        usleep(CZAS_PRZECHODZENIA_KLADKA * 1000);

        pthread_mutex_lock(&kladka->mutex);
        kladka->osoby--;
        pthread_mutex_unlock(&kladka->mutex);

        bezpieczny_sem_signal(sem_miejsca, 0);
    }

    if (strcmp(nazwa_kierunku, "WYJSCIE") == 0 && grupa != NULL) {
        for (int i = 0; i < liczba_osob; i++) {
            pid_t pid = grupa[offset + i];
            if (pid > 0 && kill(pid, 0) == 0) {
                kill(pid, SIGUSR2);
            }
        }
    }
}

static inline void zwolnij_obie_kladki(ShmKladka* k1, ShmKladka* k2) {
    loguj_wiadomosc("Zwalniam obie kladki");

    pthread_mutex_lock(&k1->mutex);
    pthread_mutex_lock(&k2->mutex);

    if (k1->osoby == 0 && k2->osoby == 0) {
        k1->kierunek = KIERUNEK_PUSTY;
        k1->przewodnik_pid = 0;

        k2->kierunek = KIERUNEK_PUSTY;
        k2->przewodnik_pid = 0;

        loguj_wiadomosc("Kladki zwolnione - dostepne dla innych");

        pthread_cond_broadcast(&k1->cond);
        pthread_cond_broadcast(&k2->cond);
    }
    else {
        loguj_wiadomoscf("WARN: Po zakonczeniu k1=%d k2=%d zwiedzajacych!", k1->osoby, k2->osoby);
    }

    pthread_mutex_unlock(&k2->mutex);
    pthread_mutex_unlock(&k1->mutex);
}

#endif