#ifndef COMMON_HELPERS_H
#define COMMON_HELPERS_H

#include "common.h"

static inline int podlacz_shm_helper(key_t klucz, void** ptr) {
    int retry = 0;
    while (retry < MAX_PROB_RETRY) {
        int shmid = shmget(klucz, 0, 0);
        if (shmid != -1) {
            *ptr = shmat(shmid, NULL, 0);
            if (*ptr != (void*)-1) return 0;
            sleep(1);
            retry++;
            continue;
        }
        sleep(1);
        retry++;
    }
    return -1;
}

static inline int podlacz_sem_helper(key_t klucz) {
    int retry = 0;
    while (retry < MAX_PROB_RETRY) {
        int semid = semget(klucz, 0, 0);
        if (semid != -1) return semid;
        sleep(1);
        retry++;
    }
    return -1;
}

static inline int podlacz_msg_helper(key_t klucz) {
    int retry = 0;
    while (retry < MAX_PROB_RETRY) {
        int msgid = msgget(klucz, 0);
        if (msgid != -1) return msgid;
        sleep(1);
        retry++;
    }
    return -1;
}

#define BEZPIECZNY_SHMDT(ptr) \
    do { \
        if (ptr) { \
            shmdt(ptr); \
            ptr = NULL; \
        } \
    } while(0)

#define INIT_SEMAFOR_LOG() \
    do { \
        globalny_semid_log = podlacz_sem_log(); \
        if (globalny_semid_log == -1) { \
            fprintf(stderr, "OSTRZEZENIE: Nie mozna podlaczyc semafora logow\n"); \
        } \
    } while(0)

static inline int czy_proces_zyje(pid_t pid) {
    return (pid > 0 && kill(pid, 0) == 0);
}

#define CZEKAJ_NA_ZAMKNIECIE(shm_jaskinia, flaga_kontynuuj) \
    do { \
        if (!(shm_jaskinia)->otwarta) { \
            loguj_wiadomosc("Jaskinia zamknieta, czekam na SIGTERM"); \
            while (flaga_kontynuuj) sleep(1); \
            break; \
        } \
    } while(0)

#endif