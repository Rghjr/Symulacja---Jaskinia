#include "common.h"
#include "common_helpers.h"
#include "straznik_helpers.h"

int globalny_semid_log = -1;

volatile sig_atomic_t zakonczenie_zadane = 0;
volatile sig_atomic_t sigchld_otrzymany = 0;

void obsluga_sygnalu(int sig) {
    (void)sig;
    zakonczenie_zadane = 1;
}

void obsluga_sigchld(int sig) {
    (void)sig;
    sigchld_otrzymany = 1;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void loguj_wiadomosc(const char* wiadomosc) {
    int sem_zdobyty = 0;
    char ts[64], buf[512];

    if (globalny_semid_log != -1) {
        struct sembuf op = { 0, -1, 0 };
        if (bezpieczny_semop(globalny_semid_log, &op, 1) == 0) {
            sem_zdobyty = 1;
        }
    }

    time_t teraz = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&teraz));
    snprintf(buf, sizeof(buf), "[%s] [PID:%d] [STRAZNIK] %s\n", ts, getpid(), wiadomosc);

    int fd = open("jaskinia_common.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, strlen(buf));
        close(fd);
    }

    if (sem_zdobyty) {
        struct sembuf op = { 0, 1, 0 };
        bezpieczny_semop(globalny_semid_log, &op, 1);
    }
}

void loguj_wiadomoscf(const char* format, ...) {
    char wiadomosc[512];
    va_list args;
    va_start(args, format);
    vsnprintf(wiadomosc, sizeof(wiadomosc), format, args);
    va_end(args);
    loguj_wiadomosc(wiadomosc);
}

void wyczysc_ipc() {
    loguj_wiadomosc("Rozpoczynam czyszczenie IPC");
    int shmid, semid, msgid;

    if ((shmid = shmget(KLUCZ_SHM_JASKINIA, 0, 0)) != -1) {
        ShmJaskinia* shm = (ShmJaskinia*)shmat(shmid, NULL, 0);
        if (shm != (void*)-1) {
            pthread_mutex_destroy(&shm->mutex);
            pthread_cond_destroy(&shm->cond_otwarta);
            shmdt(shm);
        }
        shmctl(shmid, IPC_RMID, NULL);
    }

    if ((shmid = shmget(KLUCZ_SHM_KLADKA1, 0, 0)) != -1) {
        ShmKladka* k1 = (ShmKladka*)shmat(shmid, NULL, 0);
        if (k1 != (void*)-1) {
            pthread_mutex_destroy(&k1->mutex);
            pthread_cond_destroy(&k1->cond);
            shmdt(k1);
        }
        shmctl(shmid, IPC_RMID, NULL);
    }

    if ((shmid = shmget(KLUCZ_SHM_KLADKA2, 0, 0)) != -1) {
        ShmKladka* k2 = (ShmKladka*)shmat(shmid, NULL, 0);
        if (k2 != (void*)-1) {
            pthread_mutex_destroy(&k2->mutex);
            pthread_cond_destroy(&k2->cond);
            shmdt(k2);
        }
        shmctl(shmid, IPC_RMID, NULL);
    }

    if ((shmid = shmget(KLUCZ_SHM_TRASA1, 0, 0)) != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    if ((shmid = shmget(KLUCZ_SHM_TRASA2, 0, 0)) != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    if ((shmid = shmget(KLUCZ_SHM_ZWIEDZAJACY, 0, 0)) != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }

    if ((semid = semget(KLUCZ_SEM_KLADKA1_MIEJSCA, 0, 0)) != -1) {
        semctl(semid, 0, IPC_RMID);
    }
    if ((semid = semget(KLUCZ_SEM_KLADKA2_MIEJSCA, 0, 0)) != -1) {
        semctl(semid, 0, IPC_RMID);
    }
    if ((semid = semget(KLUCZ_SEM_LOG, 0, 0)) != -1) {
        semctl(semid, 0, IPC_RMID);
    }
    if ((semid = semget(KLUCZ_SEM_TRASA1_MUTEX, 0, 0)) != -1) {
        semctl(semid, 0, IPC_RMID);
    }
    if ((semid = semget(KLUCZ_SEM_TRASA2_MUTEX, 0, 0)) != -1) {
        semctl(semid, 0, IPC_RMID);
    }

    if ((msgid = msgget(KLUCZ_MSG_KASJER, 0)) != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
    if ((msgid = msgget(KLUCZ_MSG_PRZEWODNIK1, 0)) != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
    if ((msgid = msgget(KLUCZ_MSG_PRZEWODNIK2, 0)) != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }

    loguj_wiadomosc("Czyszczenie IPC zakonczone");
}

int main() {
    signal(SIGINT, obsluga_sygnalu);
    signal(SIGTERM, obsluga_sygnalu);
    signal(SIGCHLD, obsluga_sigchld);

    loguj_wiadomosc("=== START STRAZNIKA ===");
    loguj_wiadomosc("Strategia kladek: Lock->Cross->Unlock (maksymalna przepustowosc)");

    loguj_wiadomosc("Czyszczenie starych zasobow IPC");
    wyczysc_ipc();

    loguj_wiadomosc("Tworzenie nowych zasobow IPC");

    int sem_log = utworz_sem(KLUCZ_SEM_LOG, 1, 1);
    if (sem_log == -2) {
        loguj_wiadomosc("=== BLAD KRYTYCZNY: KONFLIKT ZASOBOW ===");
        loguj_wiadomosc("ROZWIAZANIE: Uruchom 'make clean'");
        return 1;
    }
    if (sem_log == -1) {
        perror("semget KLUCZ_SEM_LOG");
        loguj_wiadomosc("BLAD: Nie udalo sie utworzyc semafora logow");
        return 1;
    }
    globalny_semid_log = sem_log;

    int shmid_jaskinia = utworz_shm(KLUCZ_SHM_JASKINIA, sizeof(ShmJaskinia));
    int shmid_kladka1 = utworz_shm(KLUCZ_SHM_KLADKA1, sizeof(ShmKladka));
    int shmid_kladka2 = utworz_shm(KLUCZ_SHM_KLADKA2, sizeof(ShmKladka));
    int shmid_trasa1 = utworz_shm(KLUCZ_SHM_TRASA1, sizeof(ShmTrasa));
    int shmid_trasa2 = utworz_shm(KLUCZ_SHM_TRASA2, sizeof(ShmTrasa));
    int shmid_zwiedzajacy = utworz_shm(KLUCZ_SHM_ZWIEDZAJACY, sizeof(ShmZwiedzajacy));

    SPRAWDZ_EEXIST_I_ZAKONCZ(shmid_jaskinia == -2 || shmid_kladka1 == -2 ||
        shmid_kladka2 == -2 || shmid_trasa1 == -2 ||
        shmid_trasa2 == -2 || shmid_zwiedzajacy == -2, "SHM");

    if (shmid_jaskinia == -1 || shmid_kladka1 == -1 || shmid_kladka2 == -1 ||
        shmid_trasa1 == -1 || shmid_trasa2 == -1 || shmid_zwiedzajacy == -1) {
        perror("shmget SHM");
        loguj_wiadomosc("BLAD: Nie udalo sie utworzyc SHM");
        wyczysc_ipc();
        return 1;
    }

    int sem1_miejsca = utworz_sem(KLUCZ_SEM_KLADKA1_MIEJSCA, 1, K);
    int sem2_miejsca = utworz_sem(KLUCZ_SEM_KLADKA2_MIEJSCA, 1, K);
    int sem_trasa1_mutex = utworz_sem(KLUCZ_SEM_TRASA1_MUTEX, 1, 1);
    int sem_trasa2_mutex = utworz_sem(KLUCZ_SEM_TRASA2_MUTEX, 1, 1);

    SPRAWDZ_EEXIST_I_ZAKONCZ(sem1_miejsca == -2 || sem2_miejsca == -2 ||
        sem_trasa1_mutex == -2 || sem_trasa2_mutex == -2, "SEM");

    if (sem1_miejsca == -1 || sem2_miejsca == -1 ||
        sem_trasa1_mutex == -1 || sem_trasa2_mutex == -1) {
        perror("semget SEM");
        loguj_wiadomosc("BLAD: Nie udalo sie utworzyc semaforow");
        wyczysc_ipc();
        return 1;
    }

    int msg_kasjer = utworz_msg(KLUCZ_MSG_KASJER);
    int msg_przewodnik1 = utworz_msg(KLUCZ_MSG_PRZEWODNIK1);
    int msg_przewodnik2 = utworz_msg(KLUCZ_MSG_PRZEWODNIK2);

    SPRAWDZ_EEXIST_I_ZAKONCZ(msg_kasjer == -2 || msg_przewodnik1 == -2 ||
        msg_przewodnik2 == -2, "MSG");

    if (msg_kasjer == -1 || msg_przewodnik1 == -1 || msg_przewodnik2 == -1) {
        perror("msgget MSG");
        loguj_wiadomosc("BLAD: Nie udalo sie utworzyc kolejek komunikatow");
        wyczysc_ipc();
        return 1;
    }

    loguj_wiadomosc("Inicjalizuje struktury globalne");

    ShmJaskinia* shm_j = (ShmJaskinia*)shmat(shmid_jaskinia, NULL, 0);
    ShmKladka* shm_k1 = (ShmKladka*)shmat(shmid_kladka1, NULL, 0);
    ShmKladka* shm_k2 = (ShmKladka*)shmat(shmid_kladka2, NULL, 0);
    ShmTrasa* shm_t1 = (ShmTrasa*)shmat(shmid_trasa1, NULL, 0);
    ShmTrasa* shm_t2 = (ShmTrasa*)shmat(shmid_trasa2, NULL, 0);
    ShmZwiedzajacy* shm_zwiedzajacy = (ShmZwiedzajacy*)shmat(shmid_zwiedzajacy, NULL, 0);

    if (shm_j == (void*)-1 || shm_k1 == (void*)-1 || shm_k2 == (void*)-1 ||
        shm_t1 == (void*)-1 || shm_t2 == (void*)-1 || shm_zwiedzajacy == (void*)-1) {
        perror("shmat SHM");
        loguj_wiadomosc("BLAD: shmat failed");
        wyczysc_ipc();
        return 1;
    }

    shm_j->otwarta = 0;
    shm_k1->kierunek = KIERUNEK_PUSTY;
    shm_k1->osoby = 0;
    shm_k1->przewodnik_pid = 0;
    shm_k2->kierunek = KIERUNEK_PUSTY;
    shm_k2->osoby = 0;
    shm_k2->przewodnik_pid = 0;
    shm_t1->osoby = 0;
    shm_t2->osoby = 0;
    memset(shm_zwiedzajacy, 0, sizeof(ShmZwiedzajacy));

    time_t czas_startu;

    loguj_wiadomosc("Inicjalizuje pthread mutex i condition variables");

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    int ret;
    if ((ret = pthread_mutex_init(&shm_j->mutex, &mutex_attr)) != 0) {
        fprintf(stderr, "pthread_mutex_init shm_j: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_mutex_init shm_j failed");
        wyczysc_ipc();
        return 1;
    }
    if ((ret = pthread_cond_init(&shm_j->cond_otwarta, &cond_attr)) != 0) {
        fprintf(stderr, "pthread_cond_init shm_j: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_cond_init shm_j failed");
        wyczysc_ipc();
        return 1;
    }
    if ((ret = pthread_mutex_init(&shm_k1->mutex, &mutex_attr)) != 0) {
        fprintf(stderr, "pthread_mutex_init shm_k1: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_mutex_init shm_k1 failed");
        wyczysc_ipc();
        return 1;
    }
    if ((ret = pthread_cond_init(&shm_k1->cond, &cond_attr)) != 0) {
        fprintf(stderr, "pthread_cond_init shm_k1: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_cond_init shm_k1 failed");
        wyczysc_ipc();
        return 1;
    }
    if ((ret = pthread_mutex_init(&shm_k2->mutex, &mutex_attr)) != 0) {
        fprintf(stderr, "pthread_mutex_init shm_k2: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_mutex_init shm_k2 failed");
        wyczysc_ipc();
        return 1;
    }
    if ((ret = pthread_cond_init(&shm_k2->cond, &cond_attr)) != 0) {
        fprintf(stderr, "pthread_cond_init shm_k2: %s\n", strerror(ret));
        loguj_wiadomosc("BLAD: pthread_cond_init shm_k2 failed");
        wyczysc_ipc();
        return 1;
    }

    pthread_mutexattr_destroy(&mutex_attr);
    pthread_condattr_destroy(&cond_attr);

    loguj_wiadomosc("Obiekty pthread zainicjalizowane (PROCESS_SHARED)");
    loguj_wiadomosc("Uruchamiam workery");

    pid_t pid_kasjer = fork();
    if (pid_kasjer == -1) {
        perror("fork kasjer");
        loguj_wiadomosc("BLAD: fork kasjer failed");
        wyczysc_ipc();
        return 1;
    }
    if (pid_kasjer == 0) {
        execl("./kasjer", "kasjer", NULL);
        perror("execl kasjer");
        exit(1);
    }
    loguj_wiadomoscf("Uruchomiono kasjer: PID=%d", pid_kasjer);

    pid_t pid_przewodnik1 = fork();
    if (pid_przewodnik1 == -1) {
        perror("fork przewodnik1");
        loguj_wiadomosc("BLAD: fork przewodnik1 failed");
        wyczysc_ipc();
        return 1;
    }
    if (pid_przewodnik1 == 0) {
        execl("./przewodnik", "przewodnik", "1", NULL);
        perror("execl przewodnik1");
        exit(1);
    }
    loguj_wiadomoscf("Uruchomiono przewodnik1: PID=%d", pid_przewodnik1);

    pid_t pid_przewodnik2 = fork();
    if (pid_przewodnik2 == -1) {
        perror("fork przewodnik2");
        loguj_wiadomosc("BLAD: fork przewodnik2 failed");
        wyczysc_ipc();
        return 1;
    }
    if (pid_przewodnik2 == 0) {
        execl("./przewodnik", "przewodnik", "2", NULL);
        perror("execl przewodnik2");
        exit(1);
    }
    loguj_wiadomoscf("Uruchomiono przewodnik2: PID=%d", pid_przewodnik2);

    loguj_wiadomoscf("Workery uruchomione: kasjer=%d p1=%d p2=%d", pid_kasjer, pid_przewodnik1, pid_przewodnik2);

    sleep(1);

    pid_t pid_generator = fork();
    if (pid_generator == -1) {
        perror("fork generator");
        loguj_wiadomosc("BLAD: fork generator failed");
        wyczysc_ipc();
        return 1;
    }
    if (pid_generator == 0) {
        execl("./generator", "generator", NULL);
        perror("execl generator");
        exit(1);
    }
    loguj_wiadomoscf("Uruchomiono generator: PID=%d", pid_generator);

    if (Tp > 0) {
        time_t teraz = time(NULL);
        struct tm* tm_info = localtime(&teraz);
        int aktualne_sekundy = tm_info->tm_hour * 3600 + tm_info->tm_min * 60 + tm_info->tm_sec;

        if (aktualne_sekundy < Tp) {
            int czas_czekania = Tp - aktualne_sekundy;
            loguj_wiadomoscf("Czekam do godziny otwarcia Tp (%d sekund)", czas_czekania);
            sleep(czas_czekania);
        }
    }

    loguj_wiadomosc("OTWIERAM JASKINIE (Tp osiagniete)");

    pthread_mutex_lock(&shm_j->mutex);
    shm_j->otwarta = 1;
    pthread_cond_broadcast(&shm_j->cond_otwarta);
    pthread_mutex_unlock(&shm_j->mutex);

    czas_startu = time(NULL);
    loguj_wiadomoscf("Jaskinia otwarta na %d sekund (lub Ctrl+C)", Tk);

    int sygnaly_wyslane = 0;

    while (!zakonczenie_zadane) {
        int uplynelo = difftime(time(NULL), czas_startu);

        if (!sygnaly_wyslane && uplynelo >= (Tk - WYPRZEDZENIE_SYGNAL_ZAMKNIECIA)) {
            loguj_wiadomosc("Wysylam sygnaly zamkniecia do przewodnikow (przed Tk)");

            loguj_wiadomoscf("SIGUSR1 -> przewodnik1 (PID=%d)", pid_przewodnik1);
            kill(pid_przewodnik1, SIGUSR1);

            loguj_wiadomoscf("SIGUSR2 -> przewodnik2 (PID=%d)", pid_przewodnik2);
            kill(pid_przewodnik2, SIGUSR2);

            sygnaly_wyslane = 1;
        }

        if (uplynelo >= Tk) {
            loguj_wiadomosc("Uplynal czas Tk, rozpoczynam zamykanie");
            break;
        }

        sleep(1);
    }

    if (zakonczenie_zadane) {
        loguj_wiadomosc("=== OTRZYMANO CTRL+C - SYSTEMATYCZNE ZAMYKANIE ===");
    }

    loguj_wiadomosc("ZAMYKAM JASKINIE (brak nowych zwiedzajacych)");

    pthread_mutex_lock(&shm_j->mutex);
    shm_j->otwarta = 0;
    pthread_cond_broadcast(&shm_j->cond_otwarta);
    pthread_mutex_unlock(&shm_j->mutex);

    loguj_wiadomosc("Czekam az wszyscy zwiedzajacy opuszcza jaskinie");
    int licznik_czekania = 0;
    while (licznik_czekania < TIMEOUT_PUSTA_JASKINIA) {
        int t1 = shm_t1->osoby;
        int t2 = shm_t2->osoby;

        if (t1 == 0 && t2 == 0) {
            loguj_wiadomosc("Jaskinia pusta - wszyscy wyszli");
            break;
        }

        if (licznik_czekania % INTERWAL_LOG == 0) {
            loguj_wiadomoscf("Oczekiwanie: trasa1=%d trasa2=%d (czas=%ds)", t1, t2, licznik_czekania);
        }

        sleep(1);
        licznik_czekania++;
    }

    loguj_wiadomosc("=== ROZPOCZYNAM SYSTEMATYCZNY CLEANUP ===");

    loguj_wiadomoscf("PID-y workerow: generator=%d kasjer=%d p1=%d p2=%d",
        pid_generator, pid_kasjer, pid_przewodnik1, pid_przewodnik2);

    int liczba_zwiedzajacych = shm_zwiedzajacy->licznik;
    loguj_wiadomoscf("Lista zwiedzajacych: %d procesow", liczba_zwiedzajacych);

    pid_t zwiedzajacy[MAX_ZWIEDZAJACYCH];
    int prawidlowi_zwiedzajacy = 0;
    for (int i = 0; i < liczba_zwiedzajacych && i < MAX_ZWIEDZAJACYCH; i++) {
        zwiedzajacy[prawidlowi_zwiedzajacy++] = shm_zwiedzajacy->pidy[i];
    }

    BEZPIECZNY_SHMDT(shm_j);
    BEZPIECZNY_SHMDT(shm_k1);
    BEZPIECZNY_SHMDT(shm_k2);
    BEZPIECZNY_SHMDT(shm_t1);
    BEZPIECZNY_SHMDT(shm_t2);
    BEZPIECZNY_SHMDT(shm_zwiedzajacy);

    loguj_wiadomosc("KROK 1/4: Zamykanie generatora");
    zakoncz_proces(pid_generator, "generator", TIMEOUT_CZEKAJ_CLEANUP);

    if (prawidlowi_zwiedzajacy > 0) {
        loguj_wiadomoscf("KROK 2/4: Zamykanie zwiedzajacych (%d procesow)", prawidlowi_zwiedzajacy);
        for (int i = 0; i < prawidlowi_zwiedzajacy; i++) {
            pid_t pid = zwiedzajacy[i];
            if (czy_proces_zyje(pid)) {
                kill(pid, SIGTERM);
            }
        }

        loguj_wiadomosc("Czekam na zakonczenie zwiedzajacych");
        for (int i = 0; i < prawidlowi_zwiedzajacy; i++) {
            pid_t pid = zwiedzajacy[i];
            if (pid > 0) {
                int status;
                for (int j = 0; j < 3; j++) {
                    if (waitpid(pid, &status, WNOHANG) == pid) break;
                    if (!czy_proces_zyje(pid)) break;
                    sleep(1);
                }
                if (czy_proces_zyje(pid)) {
                    kill(pid, SIGKILL);
                    waitpid(pid, NULL, 0);
                }
            }
        }
        loguj_wiadomosc("Wszyscy zwiedzajacy zakonczeni");
    }
    else {
        loguj_wiadomosc("KROK 2/4: Brak zwiedzajacych do zamkniecia");
    }

    loguj_wiadomosc("KROK 3/4: Zamykanie kasjera");
    zakoncz_proces(pid_kasjer, "kasjer", TIMEOUT_CZEKAJ_CLEANUP);

    loguj_wiadomosc("KROK 4/4: Zamykanie przewodnikow");
    zakoncz_proces(pid_przewodnik1, "przewodnik1", TIMEOUT_CZEKAJ_CLEANUP);
    zakoncz_proces(pid_przewodnik2, "przewodnik2", TIMEOUT_CZEKAJ_CLEANUP);

    loguj_wiadomosc("Wszystkie procesy robocze zakonczone");

    wyczysc_ipc();

    loguj_wiadomosc("=== STRAZNIK ZAKONCZYL PRACE (ostatni proces) ===");
    return 0;
}