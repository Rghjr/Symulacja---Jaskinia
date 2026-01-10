#include "common.h"
#include "common_helpers.h"
#include "przewodnik_helpers.h"

volatile sig_atomic_t kontynuuj = 1;
volatile sig_atomic_t zamkniecie_otrzymane = 0;
volatile sig_atomic_t na_trasie = 0;
volatile sig_atomic_t alarm_otrzymany = 0;

void obsluga_sigterm(int sig) { kontynuuj = 0; }
void obsluga_zamkniecie(int sig) { zamkniecie_otrzymane = 1; }
void obsluga_alarm(int sig) { alarm_otrzymany = 1; }

int NUMER;

void loguj_wiadomosc(const char* wiadomosc) {
    char nazwa_pliku[64];
    snprintf(nazwa_pliku, sizeof(nazwa_pliku), "jaskinia_przewodnik%d.log", NUMER);

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
    snprintf(buf, sizeof(buf), "[%s] [PID:%d] [PRZEWODNIK%d] %s\n", ts, getpid(), NUMER, wiadomosc);

    int fd = open("jaskinia_common.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, strlen(buf));
        close(fd);
    }

    fd = open(nazwa_pliku, O_WRONLY | O_CREAT | O_APPEND, 0644);
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uzycie: %s <1|2>\n", argv[0]);
        return 1;
    }

    int tmp;
    if (bezpieczny_strtol(argv[1], &tmp, 1, 2) != 0) {
        fprintf(stderr, "ERROR: Numer musi byc 1 lub 2\n");
        return 1;
    }
    NUMER = tmp;

    signal(SIGTERM, obsluga_sigterm);
    signal(NUMER == 1 ? SIGUSR1 : SIGUSR2, obsluga_zamkniecie);
    signal(SIGALRM, obsluga_alarm);

    INIT_SEMAFOR_LOG();

    loguj_wiadomosc("START");
    loguj_wiadomoscf("Obsluguje %s", NUMER == 1 ? "SIGUSR1" : "SIGUSR2");

    srand(time(NULL) ^ getpid());

    ShmJaskinia* shm_j = NULL;
    ShmKladka* shm_k1 = NULL;
    ShmKladka* shm_k2 = NULL;
    ShmTrasa* shm_t = NULL;

    if (podlacz_shm_helper(KLUCZ_SHM_JASKINIA, (void**)&shm_j) == -1 ||
        podlacz_shm_helper(KLUCZ_SHM_KLADKA1, (void**)&shm_k1) == -1 ||
        podlacz_shm_helper(KLUCZ_SHM_KLADKA2, (void**)&shm_k2) == -1 ||
        podlacz_shm_helper(NUMER == 1 ? KLUCZ_SHM_TRASA1 : KLUCZ_SHM_TRASA2, (void**)&shm_t) == -1) {
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc pamieci wspoldzielonej");
        BEZPIECZNY_SHMDT(shm_j);
        BEZPIECZNY_SHMDT(shm_k1);
        BEZPIECZNY_SHMDT(shm_k2);
        BEZPIECZNY_SHMDT(shm_t);
        return 1;
    }

    loguj_wiadomoscf("Przewodnik %d wystartowany PID=%d", NUMER, getpid());

    int sem1_miejsca = podlacz_sem_helper(KLUCZ_SEM_KLADKA1_MIEJSCA);
    int sem2_miejsca = podlacz_sem_helper(KLUCZ_SEM_KLADKA2_MIEJSCA);
    int sem_trasa_mutex = podlacz_sem_helper(NUMER == 1 ? KLUCZ_SEM_TRASA1_MUTEX : KLUCZ_SEM_TRASA2_MUTEX);
    int msgid = podlacz_msg_helper(NUMER == 1 ? KLUCZ_MSG_PRZEWODNIK1 : KLUCZ_MSG_PRZEWODNIK2);

    if (sem1_miejsca == -1 || sem2_miejsca == -1 || sem_trasa_mutex == -1 || msgid == -1) {
        loguj_wiadomosc("ERROR: Nie mozna podlaczyc semaforow lub kolejki");
        BEZPIECZNY_SHMDT(shm_j);
        BEZPIECZNY_SHMDT(shm_k1);
        BEZPIECZNY_SHMDT(shm_k2);
        BEZPIECZNY_SHMDT(shm_t);
        return 1;
    }

    int max_osoby = (NUMER == 1) ? N1 : N2;
    int czas = (NUMER == 1) ? T1 : T2;

    loguj_wiadomoscf("Gotowy: max=%d czas=%ds K=%d", max_osoby, czas, K);

    WiadomoscPrzewodnik wiadomosc;

    while (kontynuuj) {
        pthread_mutex_lock(&shm_j->mutex);
        int otwarta = shm_j->otwarta;
        pthread_mutex_unlock(&shm_j->mutex);

        if (!otwarta) {
            loguj_wiadomosc("Jaskinia zamknieta, czekam na SIGTERM");
            while (kontynuuj) sleep(1);
            break;
        }

        pid_t grupa[max_osoby];
        int liczba = 0;

        loguj_wiadomosc("Zbieram grupe");

        alarm_otrzymany = 0;
        alarm(CZAS_ZBIERANIA_GRUPY);

        while (liczba < max_osoby && !alarm_otrzymany) {
            ssize_t wynik = msgrcv(msgid, &wiadomosc, sizeof(WiadomoscPrzewodnik) - sizeof(long),
                TYP_MSG_ZWIEDZAJACY, 0);

            if (wynik != -1) {
                grupa[liczba++] = wiadomosc.pid_zwiedzajacego;
            }
            else if (errno == EINTR) {
                if (alarm_otrzymany) {
                    break;
                }
                continue;
            }
            else if (errno == EIDRM) {
                loguj_wiadomosc("Kolejka usunieta, zamykam");
                alarm(0);
                goto cleanup;
            }
        }

        alarm(0);

        if (liczba == 0) {
            usleep(500000);
            continue;
        }

        loguj_wiadomoscf("Grupa zebrana: %d zwiedzajacych", liczba);

        sigset_t maska, stara_maska;
        sigemptyset(&maska);
        sigaddset(&maska, NUMER == 1 ? SIGUSR1 : SIGUSR2);
        sigprocmask(SIG_BLOCK, &maska, &stara_maska);

        int czy_odwolac = (zamkniecie_otrzymane && !na_trasie);
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);

        if (czy_odwolac) {
            loguj_wiadomoscf("Sygnal zamkniecia przed trasa - odwoluje grupe %d osob", liczba);
            for (int i = 0; i < liczba; i++) {
                if (czy_proces_zyje(grupa[i])) kill(grupa[i], SIGUSR1);
            }
            continue;
        }

        wyslij_sygnal_do_grupy(grupa, liczba, SIGRTMIN + 0, "grupa zebrana");

        loguj_wiadomosc("Rezerwuje miejsca na trasie");

        bezpieczny_sem_wait(sem_trasa_mutex, 0);
        int poprzednia_wartosc = shm_t->osoby;
        int nowa_wartosc = poprzednia_wartosc + liczba;

        if (nowa_wartosc > max_osoby) {
            int dozwolone = max_osoby - poprzednia_wartosc;
            if (dozwolone < 0) dozwolone = 0;

            loguj_wiadomoscf("WARN: Limit trasy Ni=%d! bylo=%d dozwolone=%d", max_osoby, poprzednia_wartosc, dozwolone);

            shm_t->osoby = max_osoby;
            bezpieczny_sem_signal(sem_trasa_mutex, 0);

            for (int i = dozwolone; i < liczba; i++) {
                if (czy_proces_zyje(grupa[i])) {
                    kill(grupa[i], SIGUSR1);
                    loguj_wiadomoscf("Odrzucono PID=%d (przekroczenie limitu)", grupa[i]);
                }
            }

            liczba = dozwolone;
            if (liczba == 0) {
                loguj_wiadomosc("Cala grupa odrzucona - limit trasy osiagniety");
                continue;
            }
        }
        else {
            shm_t->osoby = nowa_wartosc;
            bezpieczny_sem_signal(sem_trasa_mutex, 0);
        }

        loguj_wiadomoscf("Trasa zarezerwowana: bylo=%d teraz=%d/%d", poprzednia_wartosc, nowa_wartosc, max_osoby);

        loguj_wiadomosc("Blokuje kladki (WEJSCIE)");
        zablokuj_obie_kladki(shm_k1, shm_k2, KIERUNEK_WEJSCIE);

        int na_k1 = liczba / 2;
        int na_k2 = liczba - na_k1;

        loguj_wiadomosc("Przeprowadzam grupe (WEJSCIE)");
        wyslij_sygnal_do_grupy(grupa, liczba, SIGRTMIN + 1, "przechodzenie");

        if (na_k1 > 0) przeprowadz_przez_kladke(na_k1, shm_k1, sem1_miejsca, 1, 0, NULL, "WEJSCIE");
        if (na_k2 > 0) przeprowadz_przez_kladke(na_k2, shm_k2, sem2_miejsca, 2, 0, NULL, "WEJSCIE");

        loguj_wiadomosc("Zwalniam kladki (inne grupy moga przechodzic)");
        zwolnij_obie_kladki(shm_k1, shm_k2);

        loguj_wiadomoscf("Zwiedzanie rozpoczete: trasa=%d czas=%ds", NUMER, czas);

        sigprocmask(SIG_BLOCK, &maska, &stara_maska);
        na_trasie = 1;
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);

        wyslij_sygnal_do_grupy(grupa, liczba, SIGRTMIN + 2, "zwiedzanie");
        sleep(czas);

        sigprocmask(SIG_BLOCK, &maska, &stara_maska);
        na_trasie = 0;
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);

        loguj_wiadomosc("Zwiedzanie zakonczone - wracamy");

        loguj_wiadomosc("Blokuje kladki (WYJSCIE)");
        zablokuj_obie_kladki(shm_k1, shm_k2, KIERUNEK_WYJSCIE);

        loguj_wiadomosc("Przeprowadzam grupe (WYJSCIE)");
        if (na_k1 > 0) przeprowadz_przez_kladke(na_k1, shm_k1, sem1_miejsca, 1, 0, grupa, "WYJSCIE");
        if (na_k2 > 0) przeprowadz_przez_kladke(na_k2, shm_k2, sem2_miejsca, 2, na_k1, grupa, "WYJSCIE");

        loguj_wiadomosc("Zwalniam kladki i grupe");
        zwolnij_obie_kladki(shm_k1, shm_k2);

        bezpieczny_sem_wait(sem_trasa_mutex, 0);
        shm_t->osoby -= liczba;
        bezpieczny_sem_signal(sem_trasa_mutex, 0);

        loguj_wiadomoscf("Wycieczka zakonczona: trasa=%d zwiedzajacych=%d", NUMER, liczba);
    }

cleanup:
    loguj_wiadomosc("SHUTDOWN");
    BEZPIECZNY_SHMDT(shm_j);
    BEZPIECZNY_SHMDT(shm_k1);
    BEZPIECZNY_SHMDT(shm_k2);
    BEZPIECZNY_SHMDT(shm_t);
    return 0;
}