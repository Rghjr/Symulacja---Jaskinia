#include "common.h"
#include "common_helpers.h"

volatile sig_atomic_t kontynuuj = 1;
void obsluga_sigterm(int sig) { (void)sig; kontynuuj = 0; }

int globalny_semid_log = -1;

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
    snprintf(buf, sizeof(buf), "[%s] [PID:%d] [KASJER] %s\n", ts, getpid(), wiadomosc);

    int fd = open("jaskinia_common.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, strlen(buf));
        close(fd);
    }

    fd = open("jaskinia_kasjer.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
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

#define MAX_OPIEKUNOWIE 500

typedef struct {
    pid_t pid;
    int trasa;
} OpiekunTrasa;

typedef struct {
    OpiekunTrasa lista[MAX_OPIEKUNOWIE];
    int licznik;
} BazaOpiekunow;

BazaOpiekunow baza_opiekunow = { .licznik = 0 };

void zapisz_trase_opiekuna(pid_t pid_opiekuna, int trasa) {
    if (baza_opiekunow.licznik >= MAX_OPIEKUNOWIE) {
        loguj_wiadomoscf("WARN: Baza opiekunow pelna (%d), nie zapisuje PID=%d",
            MAX_OPIEKUNOWIE, pid_opiekuna);
        return;
    }

    baza_opiekunow.lista[baza_opiekunow.licznik].pid = pid_opiekuna;
    baza_opiekunow.lista[baza_opiekunow.licznik].trasa = trasa;
    baza_opiekunow.licznik++;
}

int sprawdz_trase_opiekuna(pid_t pid_opiekuna) {
    for (int i = 0; i < baza_opiekunow.licznik; i++) {
        if (baza_opiekunow.lista[i].pid == pid_opiekuna) {
            return baza_opiekunow.lista[i].trasa;
        }
    }
    return 0;
}

typedef struct {
    int trasa1;
    int trasa2;
    int odrzuconych;
    int dzieci_z_opiekunami;
    int dzieci_bez_opiekunow;
    int seniorow;
    int powtornych;
    int dzieci_darmo;
} Statystyki;

Statystyki statystyki = { 0 };

void wyswietl_raport() {
    loguj_wiadomosc("================================================================");
    loguj_wiadomosc("                    RAPORT KONCOWY - KASJER                     ");
    loguj_wiadomosc("================================================================");
    loguj_wiadomoscf("Trasa 1 zaakceptowano:          %4d zwiedzajacych", statystyki.trasa1);
    loguj_wiadomoscf("Trasa 2 zaakceptowano:          %4d zwiedzajacych", statystyki.trasa2);
    loguj_wiadomoscf("Odrzucono:                       %4d zwiedzajacych", statystyki.odrzuconych);
    loguj_wiadomosc("----------------------------------------------------------------");
    loguj_wiadomoscf("Dzieci <8 z opiekunami:          %4d par", statystyki.dzieci_z_opiekunami);
    loguj_wiadomoscf("Dzieci <3 (darmowy wstep):       %4d zwiedzajacych", statystyki.dzieci_darmo);
    loguj_wiadomoscf("Dzieci odrzucone:                %4d zwiedzajacych", statystyki.dzieci_bez_opiekunow);
    loguj_wiadomoscf("Seniorzy >75:                    %4d zwiedzajacych", statystyki.seniorow);
    loguj_wiadomoscf("Powtorne wizyty (50%% znizka):    %4d zwiedzajacych", statystyki.powtornych);
    loguj_wiadomosc("----------------------------------------------------------------");
    int suma_zaakceptowanych = statystyki.trasa1 + statystyki.trasa2;
    int suma_przetworzonych = suma_zaakceptowanych + statystyki.odrzuconych;
    loguj_wiadomoscf("SUMA przetworzonych:             %4d zwiedzajacych", suma_przetworzonych);
    loguj_wiadomoscf("SUMA zaakceptowanych:            %4d zwiedzajacych", suma_zaakceptowanych);
    loguj_wiadomoscf("Wspolczynnik akceptacji:         %3d%%",
        suma_przetworzonych > 0 ? (suma_zaakceptowanych * 100 / suma_przetworzonych) : 0);
    loguj_wiadomosc("================================================================");
}

int sprawdz_opiekuna(pid_t pid_opiekuna) {
    for (int retry = 0; retry < PROBY_SPRAWDZ_OPIEKUNA; retry++) {
        if (czy_proces_zyje(pid_opiekuna)) {
            return 1;
        }
        if (retry < PROBY_SPRAWDZ_OPIEKUNA - 1) {
            usleep(100000);
        }
    }
    return 0;
}

int main() {
    signal(SIGTERM, obsluga_sigterm);
    srand(time(NULL) ^ getpid());

    INIT_SEMAFOR_LOG();
    loguj_wiadomosc("START");

    ShmJaskinia* shm_j = NULL;

    if (podlacz_shm_helper(KLUCZ_SHM_JASKINIA, (void**)&shm_j) == -1) {
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc KLUCZ_SHM_JASKINIA");
        return 1;
    }

    loguj_wiadomoscf("Kasjer wystartowany PID=%d", getpid());

    int msgid = podlacz_msg_helper(KLUCZ_MSG_KASJER);
    if (msgid == -1) {
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc KLUCZ_MSG_KASJER");
        BEZPIECZNY_SHMDT(shm_j);
        return 1;
    }

    loguj_wiadomosc("Gotowy: kolejka priorytetowa (powtorne > zwykle)");
    loguj_wiadomosc("Sledzenie tras opiekunow wlaczone");
    loguj_wiadomosc("REGULAMIN: Dzieci <8 i ich opiekunowie TYLKO trasa 2");

    // CZEKAJ NA OTWARCIE JASKINI!
    loguj_wiadomosc("Czekam na otwarcie jaskini (Tp)");

    pthread_mutex_lock(&shm_j->mutex);
    while (!shm_j->otwarta && kontynuuj) {
        pthread_cond_wait(&shm_j->cond_otwarta, &shm_j->mutex);
    }
    pthread_mutex_unlock(&shm_j->mutex);

    if (!kontynuuj) {
        loguj_wiadomosc("SHUTDOWN przed otwarciem jaskini");
        BEZPIECZNY_SHMDT(shm_j);
        return 0;
    }

    loguj_wiadomosc("Jaskinia otwarta - rozpoczynam prace");

    WiadomoscKasjer zadanie;
    WiadomoscOdpowiedz odpowiedz;

    while (kontynuuj) {
        pthread_mutex_lock(&shm_j->mutex);
        int otwarta = shm_j->otwarta;
        pthread_mutex_unlock(&shm_j->mutex);

        if (!otwarta) {
            loguj_wiadomosc("Jaskinia zamknieta, czekam na SIGTERM");
            while (kontynuuj) sleep(1);
            break;
        }

        int otrzymano = 0;

        if (msgrcv(msgid, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long),
            TYP_MSG_POWTORNA, IPC_NOWAIT) != -1) {
            otrzymano = 1;
            statystyki.powtornych++;
        }
        else if (msgrcv(msgid, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long),
            TYP_MSG_ZADANIE, IPC_NOWAIT) != -1) {
            otrzymano = 1;
        }
        else if (errno == ENOMSG) {
            ssize_t wynik = msgrcv(msgid, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long),
                0, 0);

            if (wynik != -1) {
                otrzymano = 1;
                if (zadanie.mtype == TYP_MSG_POWTORNA) {
                    statystyki.powtornych++;
                }
            }
            else if (errno == EINTR) {
                continue;
            }
            else if (errno == EIDRM) {
                loguj_wiadomosc("Kolejka usunieta, zamykam");
                break;
            }
        }

        if (!otrzymano) {
            if (errno != ENOMSG && errno != EINTR) {
                loguj_wiadomoscf("ERROR: msgrcv: %s", strerror(errno));
                usleep(INTERWAL_POLLING * 1000);
            }
            continue;
        }

        int decyzja = DECYZJA_ODRZUCONY;
        int trasa = 0;

        // DZIECI <8
        if (zadanie.wiek < 8) {
            if (zadanie.pid_opiekuna > 0 && sprawdz_opiekuna(zadanie.pid_opiekuna)) {
                int trasa_opiekuna = sprawdz_trase_opiekuna(zadanie.pid_opiekuna);

                if (trasa_opiekuna > 0) {
                    if (trasa_opiekuna == 2) {
                        trasa = 2;
                        decyzja = DECYZJA_TRASA2;

                        if (zadanie.wiek < 3) {
                            statystyki.dzieci_darmo++;
                        }
                        statystyki.dzieci_z_opiekunami++;
                    }
                    else {
                        loguj_wiadomoscf("REJECT: PID=%d dziecko<%d opiekun=%d ma trasê %d (wymagana 2)",
                            zadanie.pid_zwiedzajacego, zadanie.wiek, zadanie.pid_opiekuna, trasa_opiekuna);
                        statystyki.dzieci_bez_opiekunow++;
                    }
                }
                else {
                    loguj_wiadomoscf("REJECT: PID=%d dziecko<%d opiekun=%d nie przetworzony jeszcze",
                        zadanie.pid_zwiedzajacego, zadanie.wiek, zadanie.pid_opiekuna);
                    statystyki.dzieci_bez_opiekunow++;
                }
            }
            else {
                loguj_wiadomoscf("REJECT: PID=%d dziecko<%d %s",
                    zadanie.pid_zwiedzajacego, zadanie.wiek,
                    zadanie.pid_opiekuna > 0 ? "opiekun nie istnieje" : "bez opiekuna");
                statystyki.dzieci_bez_opiekunow++;
            }
        }
        // OPIEKUN DZIECKA <8 - TYLKO TRASA 2
        else if (zadanie.czy_opiekun) {
            decyzja = DECYZJA_TRASA2;
            trasa = 2;
            zapisz_trase_opiekuna(zadanie.pid_zwiedzajacego, trasa);
            loguj_wiadomoscf("ACCEPT: PID=%d opiekun (dziecko <8) -> trasa 2",
                zadanie.pid_zwiedzajacego);
        }
        // SENIORZY >75 - TYLKO TRASA 2
        else if (zadanie.wiek > 75) {
            decyzja = DECYZJA_TRASA2;
            trasa = 2;
            statystyki.seniorow++;
        }
        // POWTÓRNE WIZYTY - INNA TRASA
        else if (zadanie.powtorna_wizyta) {
            if (zadanie.poprzednia_trasa >= 1 && zadanie.poprzednia_trasa <= 2) {
                trasa = (zadanie.poprzednia_trasa == 1) ? 2 : 1;
                decyzja = (trasa == 1) ? DECYZJA_TRASA1 : DECYZJA_TRASA2;
            }
            else {
                loguj_wiadomoscf("REJECT: Nieprawidlowa poprzednia trasa=%d", zadanie.poprzednia_trasa);
            }
        }
        // ZWYKLI - LOSOWA TRASA
        else {
            trasa = (rand() % 2) + 1;
            decyzja = (trasa == 1) ? DECYZJA_TRASA1 : DECYZJA_TRASA2;

            if (zadanie.wiek >= MIN_WIEK_OPIEKUNA && zadanie.wiek <= MAX_WIEK_OPIEKUNA) {
                zapisz_trase_opiekuna(zadanie.pid_zwiedzajacego, trasa);
            }
        }

        odpowiedz.mtype = zadanie.pid_zwiedzajacego;
        odpowiedz.decyzja = decyzja;
        odpowiedz.przydzielona_trasa = trasa;

        if (msgsnd(msgid, &odpowiedz, sizeof(WiadomoscOdpowiedz) - sizeof(long), 0) != -1) {
            if (decyzja != DECYZJA_ODRZUCONY) {
                loguj_wiadomoscf("ACCEPT: PID=%d trasa=%d", zadanie.pid_zwiedzajacego, trasa);

                if (trasa == 1) {
                    statystyki.trasa1++;
                }
                else if (trasa == 2) {
                    statystyki.trasa2++;
                }
            }
            else {
                statystyki.odrzuconych++;
            }
        }
        else {
            loguj_wiadomoscf("ERROR: msgsnd odpowiedz: %s", strerror(errno));
        }
    }

    loguj_wiadomosc("================================================================");
    loguj_wiadomosc("Jaskinia zamknieta, generuje raport koncowy");
    wyswietl_raport();

    loguj_wiadomosc("SHUTDOWN");
    BEZPIECZNY_SHMDT(shm_j);
    return 0;
}