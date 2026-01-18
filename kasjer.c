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

/// Struktura do zbierania statystyk - raport na końcu
typedef struct {
    int trasa1;
    int trasa2;
    int odrzuconych;
    int dzieci_z_opiekunem;
    int dzieci_bez_opiekunow;
    int seniorow;
    int powtornych;
    int dzieci_darmo;
    int opiekunow;
} Statystyki;

Statystyki statystyki = { 0 };

/// Wyświetl raport końcowy - ładnie sformatowany
void wyswietl_raport() {
    loguj_wiadomosc("================================================================");
    loguj_wiadomosc("                    RAPORT KONCOWY - KASJER                     ");
    loguj_wiadomosc("================================================================");
    loguj_wiadomoscf("Trasa 1 zaakceptowano:          %4d zwiedzajacych", statystyki.trasa1);
    loguj_wiadomoscf("Trasa 2 zaakceptowano:          %4d zwiedzajacych", statystyki.trasa2);
    loguj_wiadomoscf("Odrzucono:                       %4d zwiedzajacych", statystyki.odrzuconych);
    loguj_wiadomosc("----------------------------------------------------------------");
    loguj_wiadomoscf("Opiekunow (TRASA 2):             %4d zwiedzajacych", statystyki.opiekunow);
    loguj_wiadomoscf("Dzieci <8 z opiekunem:           %4d zwiedzajacych", statystyki.dzieci_z_opiekunem);
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

int main() {
    signal(SIGTERM, obsluga_sigterm);
    signal(SIGINT, SIG_IGN);
    srand(time(NULL) ^ getpid());

    INIT_SEMAFOR_LOG();
    loguj_wiadomosc("START");

    ShmJaskinia* shm_j = NULL;

    if (podlacz_shm_helper(KLUCZ_SHM_JASKINIA, (void**)&shm_j) == -1) {
        perror("shmget KLUCZ_SHM_JASKINIA");
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc KLUCZ_SHM_JASKINIA");
        return 1;
    }

    loguj_wiadomoscf("Kasjer wystartowany PID=%d", getpid());

    int msgid = podlacz_msg_helper(KLUCZ_MSG_KASJER);
    if (msgid == -1) {
        perror("msgget KLUCZ_MSG_KASJER");
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc KLUCZ_MSG_KASJER");
        BEZPIECZNY_SHMDT(shm_j);
        return 1;
    }

    loguj_wiadomosc("Gotowy: kolejka priorytetowa (powtorne > zwykle)");
    loguj_wiadomosc("REGULAMIN: Dzieci <8 z opiekunem TYLKO trasa 2");
    loguj_wiadomosc("REGULAMIN: Opiekunowie dzieci <8 TYLKO trasa 2");

    loguj_wiadomosc("Czekam na otwarcie jaskini (Tp)");

    /// Czekaj aż jaskinia się otworzy
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

    /// Główna pętla - obsługa próśb o bilety
    while (kontynuuj) {
        pthread_mutex_lock(&shm_j->mutex);
        int otwarta = shm_j->otwarta;
        pthread_mutex_unlock(&shm_j->mutex);

        if (!otwarta) {
            loguj_wiadomosc("Jaskinia zamknieta, przetwarzam pozostale zadania");
            /// Przetwarzaj pozostałe zadania z kolejki zamiast od razu kończyć
        }

        int otrzymano = 0;

        /// PRIORYTET 1: Powtórne wizyty (50% zniżka)
        if (msgrcv(msgid, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long),
            TYP_MSG_POWTORNA, IPC_NOWAIT) != -1) {
            otrzymano = 1;
            statystyki.powtornych++;
        }
        /// PRIORYTET 2: Zwykłe wizyty
        else if (msgrcv(msgid, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long),
            TYP_MSG_ZADANIE, IPC_NOWAIT) != -1) {
            otrzymano = 1;
        }
        /// Jeśli obie puste - czekaj na cokolwiek (blocking)
        else if (errno == ENOMSG) {
            /// Jeśli jaskinia zamknięta i kolejka pusta - koniec
            if (!otwarta) {
                loguj_wiadomosc("Jaskinia zamknieta i kolejka pusta - zakoncz");
                break;
            }

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
            else {
                perror("msgrcv KLUCZ_MSG_KASJER");
                loguj_wiadomoscf("ERROR: msgrcv: %s", strerror(errno));
            }
        }

        if (!otrzymano) {
            if (errno != ENOMSG && errno != EINTR) {
                loguj_wiadomoscf("ERROR: msgrcv: %s", strerror(errno));
                usleep(INTERWAL_POLLING * 1000);
            }
            continue;
        }

        /// LOGIKA PRZYDZIELANIA TRASY - implementacja regulaminu
        int decyzja = DECYZJA_ODRZUCONY;
        int trasa = 0;

        /// REGUŁA 1: Opiekunowie dzieci <8 → TYLKO TRASA 2
        if (zadanie.czy_opiekun) {
            decyzja = DECYZJA_TRASA2;
            trasa = 2;
            statystyki.opiekunow++;
            loguj_wiadomoscf("ACCEPT: PID=%d opiekun (dziecko <8) -> trasa 2",
                zadanie.pid_zwiedzajacego);
        }
        /// REGUŁA 2: Dzieci <8 lat - MUSZĄ mieć opiekuna, TYLKO TRASA 2
        else if (zadanie.wiek < 8) {
            if (zadanie.pid_opiekuna > 0 && czy_proces_zyje(zadanie.pid_opiekuna)) {
                trasa = 2;
                decyzja = DECYZJA_TRASA2;

                if (zadanie.wiek < 3) {  /// Darmowy wstęp dla <3 lat
                    statystyki.dzieci_darmo++;
                }
                statystyki.dzieci_z_opiekunem++;
            }
            else {  /// Dziecko bez opiekuna - ODRZUCONE
                loguj_wiadomoscf("REJECT: PID=%d dziecko<%d %s",
                    zadanie.pid_zwiedzajacego, zadanie.wiek,
                    zadanie.pid_opiekuna > 0 ? "opiekun nie istnieje" : "bez opiekuna");
                statystyki.dzieci_bez_opiekunow++;
            }
        }
        /// REGUŁA 3: Seniorzy >75 lat → TYLKO TRASA 2
        else if (zadanie.wiek > 75) {
            decyzja = DECYZJA_TRASA2;
            trasa = 2;
            statystyki.seniorow++;
        }
        /// REGUŁA 4: Powtórna wizyta - druga trasa (odwrotna niż poprzednia)
        else if (zadanie.powtorna_wizyta) {
            if (zadanie.poprzednia_trasa >= 1 && zadanie.poprzednia_trasa <= 2) {
                trasa = (zadanie.poprzednia_trasa == 1) ? 2 : 1;
                decyzja = (trasa == 1) ? DECYZJA_TRASA1 : DECYZJA_TRASA2;
            }
            else {
                loguj_wiadomoscf("REJECT: Nieprawidlowa poprzednia trasa=%d", zadanie.poprzednia_trasa);
            }
        }
        /// REGUŁA 5: Normalni dorośli - losowa trasa
        else {
            trasa = (rand() % 2) + 1;  /// 1 lub 2
            decyzja = (trasa == 1) ? DECYZJA_TRASA1 : DECYZJA_TRASA2;
        }

        /// Wyślij odpowiedź do zwiedzającego
        odpowiedz.mtype = zadanie.pid_zwiedzajacego;  /// Jego PID jako typ wiadomości
        odpowiedz.decyzja = decyzja;
        odpowiedz.przydzielona_trasa = trasa;

        /// Sprawdź czy proces nadal żyje przed wysłaniem
        if (!czy_proces_zyje(zadanie.pid_zwiedzajacego)) {
            loguj_wiadomoscf("WARN: Zwiedzajacy PID=%d juz nie istnieje, pomijam odpowiedz",
                zadanie.pid_zwiedzajacego);
            continue;
        }

        /// Obsługa błędów przy msgsnd
        if (msgsnd(msgid, &odpowiedz, sizeof(WiadomoscOdpowiedz) - sizeof(long), IPC_NOWAIT) != -1) {
            if (decyzja != DECYZJA_ODRZUCONY) {
                loguj_wiadomoscf("ACCEPT: PID=%d trasa=%d", zadanie.pid_zwiedzajacego, trasa);

                /// Aktualizuj statystyki
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
            /// Szczegółowa obsługa błędów
            if (errno == EIDRM) {
                loguj_wiadomosc("ERROR: Kolejka usunieta podczas wysylania odpowiedzi");
                break;  /// Kolejka usunięta - kończymy pracę
            }
            else if (errno == EAGAIN) {
                loguj_wiadomoscf("WARN: Kolejka pelna, odpowiedz do PID=%d pominięta",
                    zadanie.pid_zwiedzajacego);
            }
            else if (errno == EINVAL) {
                loguj_wiadomoscf("ERROR: msgsnd EINVAL - nieprawidlowy rozmiar lub mtype=%ld",
                    odpowiedz.mtype);
            }
            else {
                perror("msgsnd odpowiedz");
                loguj_wiadomoscf("ERROR: msgsnd odpowiedz: %s (errno=%d)", strerror(errno), errno);
            }
        }
    }

    /// Koniec pracy - pokaż raport
    loguj_wiadomosc("================================================================");
    loguj_wiadomosc("Jaskinia zamknieta, generuje raport koncowy");
    wyswietl_raport();

    loguj_wiadomosc("SHUTDOWN");
    BEZPIECZNY_SHMDT(shm_j);
    return 0;
}