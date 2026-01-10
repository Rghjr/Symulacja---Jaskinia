#include "common.h"
#include "common_helpers.h"

volatile sig_atomic_t kontynuuj = 1;
void obsluga_sigterm(int sig) { kontynuuj = 0; }

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
    snprintf(buf, sizeof(buf), "[%s] [PID:%d] [GENERATOR] %s\n", ts, getpid(), wiadomosc);

    int fd = open("jaskinia_common.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, strlen(buf));
        close(fd);
    }

    fd = open("jaskinia_generator.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
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

void zarejestruj_zwiedzajacego(ShmZwiedzajacy* shm_zwiedzajacy, pid_t pid) {
    int idx = __sync_fetch_and_add(&shm_zwiedzajacy->licznik, 1);
    if (idx < MAX_ZWIEDZAJACYCH) {
        shm_zwiedzajacy->pidy[idx] = pid;
    }
    else {
        loguj_wiadomoscf("WARN: MAX_ZWIEDZAJACYCH=%d przekroczony, nie rejestruje PID=%d",
            MAX_ZWIEDZAJACYCH, pid);
        __sync_fetch_and_sub(&shm_zwiedzajacy->licznik, 1);
    }
}

int czekaj_na_opiekuna_gotowy(int msgid_ack, pid_t pid_opiekuna) {
    WiadomoscOpiekunAck ack;
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 2;

    for (int i = 0; i < 20; i++) {
        if (msgrcv(msgid_ack, &ack, sizeof(WiadomoscOpiekunAck) - sizeof(long),
            pid_opiekuna, IPC_NOWAIT) != -1) {
            return 1;
        }
        usleep(OPOZNIENIE_SPAWN_OPIEKUNA * 1000);
    }

    loguj_wiadomoscf("WARN: Opiekun PID=%d nie wyslal ACK w czasie", pid_opiekuna);
    return 0;
}

int main() {
    signal(SIGTERM, obsluga_sigterm);

    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    srand(time(NULL) ^ getpid());

    INIT_SEMAFOR_LOG();
    loguj_wiadomosc("START");

    ShmJaskinia* shm_j = NULL;
    ShmZwiedzajacy* shm_zwiedzajacy = NULL;

    if (podlacz_shm_helper(KLUCZ_SHM_JASKINIA, (void**)&shm_j) == -1 ||
        podlacz_shm_helper(KLUCZ_SHM_ZWIEDZAJACY, (void**)&shm_zwiedzajacy) == -1) {
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc pamieci wspoldzielonej");
        BEZPIECZNY_SHMDT(shm_j);
        BEZPIECZNY_SHMDT(shm_zwiedzajacy);
        return 1;
    }

    int msgid_ack = podlacz_msg_helper(KLUCZ_MSG_OPIEKUN_ACK);
    if (msgid_ack == -1) {
        loguj_wiadomosc("WARN: Nie mozna podlaczyc MSG_OPIEKUN_ACK, sync moze zawiec");
    }

    loguj_wiadomoscf("Generator wystartowany PID=%d", getpid());

    loguj_wiadomosc("Czekam na otwarcie jaskini (Tp)");

    pthread_mutex_lock(&shm_j->mutex);
    while (!shm_j->otwarta && kontynuuj) {
        pthread_cond_wait(&shm_j->cond_otwarta, &shm_j->mutex);
    }
    pthread_mutex_unlock(&shm_j->mutex);

    if (!kontynuuj) {
        loguj_wiadomosc("SHUTDOWN przed otwarciem jaskini");
        BEZPIECZNY_SHMDT(shm_j);
        BEZPIECZNY_SHMDT(shm_zwiedzajacy);
        return 0;
    }

    loguj_wiadomosc("Jaskinia otwarta, generuje zwiedzajacych");
    loguj_wiadomoscf("Konfiguracja: opoznienie=%d-%ds, limit=%d",
        OPOZNIENIE_GENERATORA_MIN, OPOZNIENIE_GENERATORA_MAX, MAX_ZWIEDZAJACYCH);

    int licznik = 0;
    int licznik_retry_fork = 0;
    const int MAX_RETRY_FORK = 5;

    while (kontynuuj) {
        pthread_mutex_lock(&shm_j->mutex);
        int otwarta = shm_j->otwarta;
        pthread_mutex_unlock(&shm_j->mutex);

        if (!otwarta) {
            loguj_wiadomosc("Jaskinia zamknieta, zatrzymuje generowanie");
            break;
        }

        if (shm_zwiedzajacy->licznik >= MAX_ZWIEDZAJACYCH) {
            loguj_wiadomoscf("Limit zwiedzajacych osiagniety (%d/%d), czekam",
                shm_zwiedzajacy->licznik, MAX_ZWIEDZAJACYCH);
            sleep(5);
            continue;
        }

        int wiek = MIN_WIEK + (rand() % (MAX_WIEK - MIN_WIEK + 1));
        int powtorna = (rand() % 100) < SZANSA_POWTORNA ? 1 : 0;
        int poprz_trasa = (rand() % 2) + 1;
        pid_t pid_opiekuna = 0;

        if (wiek < 8) {
            if (rand() % 100 < SZANSA_DZIECKO_OPIEKUN) {
                if (shm_zwiedzajacy->licznik >= MAX_ZWIEDZAJACYCH - 1) {
                    loguj_wiadomosc("Brak miejsca na pare opiekun-dziecko, czekam");
                    sleep(2);
                    continue;
                }

                pid_t opiekun = fork();
                if (opiekun == -1) {
                    perror("fork opiekun");
                    loguj_wiadomosc("ERROR: Nie mozna fork procesu opiekuna");
                    sleep(1);
                    continue;
                }

                if (opiekun == 0) {
                    int wiek_opiekuna = MIN_WIEK_OPIEKUNA +
                        (rand() % (MAX_WIEK_OPIEKUNA - MIN_WIEK_OPIEKUNA + 1));
                    char w[16], p[16], t[16], o[16], g[16];
                    snprintf(w, sizeof(w), "%d", wiek_opiekuna);
                    snprintf(p, sizeof(p), "0");
                    snprintf(t, sizeof(t), "1");
                    snprintf(o, sizeof(o), "0");
                    snprintf(g, sizeof(g), "1");

                    execl("./zwiedzajacy", "zwiedzajacy", w, p, t, o, g, NULL);
                    perror("execl opiekun");
                    exit(1);
                }

                pid_opiekuna = opiekun;
                zarejestruj_zwiedzajacego(shm_zwiedzajacy, pid_opiekuna);

                if (msgid_ack != -1) {
                    czekaj_na_opiekuna_gotowy(msgid_ack, pid_opiekuna);
                }
                else {
                    usleep(OPOZNIENIE_SPAWN_OPIEKUNA * 1000);
                }

                loguj_wiadomoscf("Wygenerowano opiekuna PID=%d wiek=%d-%d",
                    pid_opiekuna, MIN_WIEK_OPIEKUNA, MAX_WIEK_OPIEKUNA);
            }
        }

        loguj_wiadomoscf("Generuje zwiedzajacego #%d: wiek=%d powtorna=%d poprz=%d opiekun=%d",
            licznik + 1, wiek, powtorna, poprz_trasa, pid_opiekuna);

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork zwiedzajacy");
            loguj_wiadomoscf("ERROR: Nie mozna fork zwiedzajacego (retry %d/%d)",
                licznik_retry_fork + 1, MAX_RETRY_FORK);

            licznik_retry_fork++;
            if (licznik_retry_fork >= MAX_RETRY_FORK) {
                loguj_wiadomosc("CRITICAL: Zbyt wiele nieudanych fork, przerywam");
                break;
            }
            sleep(1);
            continue;
        }

        licznik_retry_fork = 0;

        if (pid == 0) {
            char w[16], p[16], t[16], o[16], g[16];
            snprintf(w, sizeof(w), "%d", wiek);
            snprintf(p, sizeof(p), "%d", powtorna);
            snprintf(t, sizeof(t), "%d", poprz_trasa);
            snprintf(o, sizeof(o), "%d", pid_opiekuna);
            snprintf(g, sizeof(g), "0");

            execl("./zwiedzajacy", "zwiedzajacy", w, p, t, o, g, NULL);
            perror("execl zwiedzajacy");
            exit(1);
        }

        zarejestruj_zwiedzajacego(shm_zwiedzajacy, pid);
        licznik++;

        int opoznienie = OPOZNIENIE_GENERATORA_MIN +
            (rand() % (OPOZNIENIE_GENERATORA_MAX - OPOZNIENIE_GENERATORA_MIN + 1));
        sleep(opoznienie);
    }

    loguj_wiadomoscf("SHUTDOWN: wygenerowano=%d zarejestrowano=%d",
        licznik, shm_zwiedzajacy->licznik);

    BEZPIECZNY_SHMDT(shm_j);
    BEZPIECZNY_SHMDT(shm_zwiedzajacy);
    return 0;
}