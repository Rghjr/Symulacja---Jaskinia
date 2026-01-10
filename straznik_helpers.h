#ifndef STRAZNIK_HELPERS_H
#define STRAZNIK_HELPERS_H

#include "common.h"

void wyczysc_ipc(void);

void loguj_wiadomosc(const char* wiadomosc);
void loguj_wiadomoscf(const char* format, ...);

static inline int utworz_shm(key_t klucz, size_t rozmiar) {
    int shmid = shmget(klucz, rozmiar, IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1) {
        if (errno == EEXIST) {
            loguj_wiadomoscf("BLAD KRYTYCZNY: SHM klucz=%#x juz istnieje!", klucz);
            return -2;
        }
        loguj_wiadomoscf("BLAD: shmget(%#x, %zu): %s (errno=%d)", klucz, rozmiar, strerror(errno), errno);
        return -1;
    }

    loguj_wiadomoscf("Utworzono SHM: klucz=%#x id=%d rozmiar=%zu", klucz, shmid, rozmiar);
    return shmid;
}

static inline int utworz_sem(key_t klucz, int liczba, int wartosc_init) {
    int semid = semget(klucz, liczba, IPC_CREAT | IPC_EXCL | 0600);
    if (semid == -1) {
        if (errno == EEXIST) {
            loguj_wiadomoscf("BLAD KRYTYCZNY: SEM klucz=%#x juz istnieje!", klucz);
            return -2;
        }
        loguj_wiadomoscf("BLAD: semget(%#x, %d): %s (errno=%d)", klucz, liczba, strerror(errno), errno);
        return -1;
    }

    union semun arg;
    arg.val = wartosc_init;
    for (int i = 0; i < liczba; i++) {
        if (semctl(semid, i, SETVAL, arg) == -1) {
            loguj_wiadomoscf("BLAD: semctl(%#x, %d, SETVAL, %d): %s", klucz, i, wartosc_init, strerror(errno));
        }
    }

    loguj_wiadomoscf("Utworzono SEM: klucz=%#x id=%d liczba=%d val=%d", klucz, semid, liczba, wartosc_init);
    return semid;
}

static inline int utworz_msg(key_t klucz) {
    int msgid = msgget(klucz, IPC_CREAT | IPC_EXCL | 0600);
    if (msgid == -1) {
        if (errno == EEXIST) {
            loguj_wiadomoscf("BLAD KRYTYCZNY: MSG klucz=%#x juz istnieje!", klucz);
            return -2;
        }
        loguj_wiadomoscf("BLAD: msgget(%#x): %s (errno=%d)", klucz, strerror(errno), errno);
        return -1;
    }

    loguj_wiadomoscf("Utworzono MSG: klucz=%#x id=%d", klucz, msgid);
    return msgid;
}

#define SPRAWDZ_EEXIST_I_ZAKONCZ(warunek, typ_zasobu) \
    do { \
        if (warunek) { \
            loguj_wiadomosc("=== BLAD KRYTYCZNY: KONFLIKT ZASOBOW " typ_zasobu " ==="); \
            loguj_wiadomosc("Poprzednie uruchomienie nie zostalo poprawnie zakonczone."); \
            loguj_wiadomosc("ROZWIAZANIE: Uruchom 'make clean' aby wyczyscic zasoby IPC."); \
            wyczysc_ipc(); \
            return 1; \
        } \
    } while(0)

static inline void zakoncz_proces(pid_t pid, const char* nazwa, int timeout) {
    if (pid <= 0) return;

    if (kill(pid, 0) != 0) {
        loguj_wiadomoscf("%s (PID=%d) juz nie istnieje", nazwa, pid);
        return;
    }

    loguj_wiadomoscf("Wysylam SIGTERM -> %s (PID=%d)", nazwa, pid);
    kill(pid, SIGTERM);

    for (int i = 0; i < timeout; i++) {
        int status;
        pid_t wynik = waitpid(pid, &status, WNOHANG);
        if (wynik == pid) {
            loguj_wiadomoscf("%s zakonczyl sie (czekano %ds)", nazwa, i);
            return;
        }
        if (wynik == -1) {
            loguj_wiadomoscf("%s: waitpid blad: %s", nazwa, strerror(errno));
            return;
        }
        sleep(1);
    }

    loguj_wiadomoscf("TIMEOUT dla %s - wysylam SIGKILL", nazwa);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    loguj_wiadomoscf("%s zakonczony (SIGKILL)", nazwa);
}

#endif