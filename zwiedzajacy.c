#include "common.h"
#include "common_helpers.h"

int globalny_semid_log = -1;

/// Maszyna stanów zwiedzającego - kontrolowana sygnałami
volatile sig_atomic_t odwolano = 0;       /// SIGUSR1 - odwołano (odrzucony przez kasjera/przewodnika)
volatile sig_atomic_t w_grupie = 0;       /// SIGRTMIN+0 - zebrali mnie do grupy
volatile sig_atomic_t na_kladce = 0;      /// SIGRTMIN+1 - idę przez kładkę
volatile sig_atomic_t zwiedzam = 0;       /// SIGRTMIN+2 - zwiedzam trasę
volatile sig_atomic_t moze_wyjsc = 0;     /// SIGUSR2 - przeszedłem kładkę przy wyjściu
volatile sig_atomic_t alarm_otrzymany = 0;  /// SIGALRM - timeout
volatile sig_atomic_t sigterm_otrzymany = 0; /// SIGTERM - shutdown

void obsluga_sigusr1(int sig) { (void)sig; odwolano = 1; }
void obsluga_sigrtmin0(int sig) { (void)sig; w_grupie = 1; }
void obsluga_sigrtmin1(int sig) { (void)sig; na_kladce = 1; }
void obsluga_sigrtmin2(int sig) { (void)sig; zwiedzam = 1; }
void obsluga_sigusr2(int sig) { (void)sig; moze_wyjsc = 1; }
void obsluga_alarm(int sig) { (void)sig; alarm_otrzymany = 1; }
void obsluga_sigterm(int sig) { (void)sig; sigterm_otrzymany = 1; }

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
    snprintf(buf, sizeof(buf), "[%s] [PID:%d] [ZWIEDZAJACY] %s\n", ts, getpid(), wiadomosc);

    int fd = open("jaskinia_common.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        bezpieczny_zapis_wszystko(fd, buf, strlen(buf));
        close(fd);
    }

    fd = open("jaskinia_zwiedzajacy.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
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
    /// Argumenty: wiek powtorna poprz_trasa pid_opiekuna czy_opiekun
    if (argc != 6) {
        fprintf(stderr, "Uzycie: %s <wiek> <powtorna> <poprz_trasa> <pid_opiekuna> <czy_opiekun>\n", argv[0]);
        return 1;
    }

    int wiek, powtorna, poprz_trasa, pid_opiekuna, czy_opiekun;

    /// Walidacja wszystkich argumentów
    if (bezpieczny_strtol(argv[1], &wiek, MIN_WIEK, MAX_WIEK) != 0 ||
        bezpieczny_strtol(argv[2], &powtorna, 0, 1) != 0 ||
        bezpieczny_strtol(argv[3], &poprz_trasa, 1, 2) != 0 ||
        bezpieczny_strtol(argv[4], &pid_opiekuna, 0, INT_MAX) != 0 ||
        bezpieczny_strtol(argv[5], &czy_opiekun, 0, 1) != 0) {
        fprintf(stderr, "ERROR: Nieprawidlowe argumenty\n");
        return 1;
    }

    /// Rejestracja handlerów sygnałów - zwiedzający sterowany wyłącznie sygnałami!
    signal(SIGUSR1, obsluga_sigusr1);
    signal(SIGRTMIN + 0, obsluga_sigrtmin0);
    signal(SIGRTMIN + 1, obsluga_sigrtmin1);
    signal(SIGRTMIN + 2, obsluga_sigrtmin2);
    signal(SIGUSR2, obsluga_sigusr2);
    signal(SIGALRM, obsluga_alarm);
    signal(SIGTERM, obsluga_sigterm);
    signal(SIGINT, SIG_IGN);

    INIT_SEMAFOR_LOG();

    pid_t moj_pid = getpid();

    loguj_wiadomoscf("START: wiek=%d powtorna=%d poprz=%d opiekun=%d czy_opiekun=%d",
        wiek, powtorna, poprz_trasa, pid_opiekuna, czy_opiekun);

    /// Sprawdź czy opiekun faktycznie istnieje (może się zdążył skończyć)
    if (pid_opiekuna > 0 && !czy_proces_zyje(pid_opiekuna)) {
        loguj_wiadomoscf("WARN: Opiekun PID=%d nie istnieje podczas startu", pid_opiekuna);
    }

    /// KROK 1: Idę do kasjera po bilet
    loguj_wiadomosc("STATE: Ide do kasjera");

    int msgid_kasjer = podlacz_msg_helper(KLUCZ_MSG_KASJER);
    if (msgid_kasjer == -1) {
        loguj_wiadomosc("SHUTDOWN: Nie mozna podlaczyc kolejki kasjera");
        return 0;
    }

    /// Wypełnij prośbę o bilet
    WiadomoscKasjer zadanie;
    zadanie.mtype = powtorna ? TYP_MSG_POWTORNA : TYP_MSG_ZADANIE;  /// Powtórne mają priorytet!
    zadanie.pid_zwiedzajacego = moj_pid;
    zadanie.wiek = wiek;
    zadanie.powtorna_wizyta = powtorna;
    zadanie.poprzednia_trasa = poprz_trasa;
    zadanie.pid_opiekuna = pid_opiekuna;
    zadanie.czy_opiekun = czy_opiekun;

    if (msgsnd(msgid_kasjer, &zadanie, sizeof(WiadomoscKasjer) - sizeof(long), 0) == -1) {
        if (errno == EIDRM) {
            loguj_wiadomosc("SHUTDOWN: Kolejka kasjera usunieta");
            return 0;
        }
        loguj_wiadomoscf("ERROR: msgsnd kasjer: %s", strerror(errno));
        return 0;
    }

    /// KROK 2: Czekam na odpowiedź kasjera (max TIMEOUT_ODPOWIEDZ_BILET sekund)
    loguj_wiadomosc("STATE: Czekam na bilet");

    WiadomoscOdpowiedz odpowiedz;
    int otrzymano = 0;

    alarm_otrzymany = 0;
    alarm(TIMEOUT_ODPOWIEDZ_BILET);

    /// msgrcv z mtype=moj_pid - dostanę tylko swoją odpowiedź
    ssize_t wynik = msgrcv(msgid_kasjer, &odpowiedz, sizeof(WiadomoscOdpowiedz) - sizeof(long),
        moj_pid, 0);

    alarm(0);

    if (wynik != -1) {
        otrzymano = 1;
    }
    else if (errno == EINTR) {
        if (alarm_otrzymany) {
            loguj_wiadomoscf("TIMEOUT: Brak odpowiedzi od kasjera (%ds)", TIMEOUT_ODPOWIEDZ_BILET);
            return 0;
        }
        if (sigterm_otrzymany) {
            loguj_wiadomosc("SHUTDOWN: SIGTERM podczas oczekiwania na bilet");
            return 0;
        }
        loguj_wiadomosc("WARN: msgrcv przerwany sygnalem, koncze");
        return 0;
    }
    else if (errno == EIDRM) {
        loguj_wiadomosc("SHUTDOWN: Kolejka kasjera usunieta");
        return 0;
    }
    else {
        loguj_wiadomoscf("ERROR: msgrcv odpowiedz: %s", strerror(errno));
        return 0;
    }

    if (!otrzymano) {
        loguj_wiadomosc("ERROR: Nie udalo sie otrzymac biletu");
        return 0;
    }

    /// Sprawdź decyzję kasjera
    if (odpowiedz.decyzja == DECYZJA_ODRZUCONY) {
        loguj_wiadomosc("REJECT: Odrzucony przez kasjera");
        return 0;
    }

    int trasa = odpowiedz.przydzielona_trasa;
    if (trasa < 1 || trasa > 2) {
        loguj_wiadomoscf("ERROR: Nieprawidlowa przydzielona trasa: %d", trasa);
        return 0;
    }

    loguj_wiadomoscf("TICKET: Przydzielono trase %d", trasa);

    /// KROK 3: Dołączam do kolejki przewodnika
    loguj_wiadomosc("STATE: Dolaczam do kolejki przewodnika");

    int msgid_przewodnik = podlacz_msg_helper(trasa == 1 ? KLUCZ_MSG_PRZEWODNIK1 : KLUCZ_MSG_PRZEWODNIK2);
    if (msgid_przewodnik == -1) {
        loguj_wiadomosc("SHUTDOWN: Nie mozna podlaczyc kolejki przewodnika");
        return 0;
    }

    WiadomoscPrzewodnik wiadomosc_przew;
    wiadomosc_przew.mtype = TYP_MSG_ZWIEDZAJACY;
    wiadomosc_przew.pid_zwiedzajacego = moj_pid;
    wiadomosc_przew.wiek = wiek;

    if (msgsnd(msgid_przewodnik, &wiadomosc_przew, sizeof(WiadomoscPrzewodnik) - sizeof(long), 0) == -1) {
        if (errno == EIDRM) {
            loguj_wiadomosc("SHUTDOWN: Kolejka przewodnika usunieta");
            return 0;
        }
        loguj_wiadomoscf("ERROR: msgsnd przewodnik: %s", strerror(errno));
        return 0;
    }

    /// KROK 4: Czekam na sygnały od przewodnika - MASZYNA STANÓW
    loguj_wiadomosc("STATE: W kolejce czekam na grupe");

    /// Ustawiamy alarm na MAX_CZAS_W_KOLEJCE - jeśli za długo w kolejce, kończymy
    alarm_otrzymany = 0;
    alarm(MAX_CZAS_W_KOLEJCE);

    /// Blokujemy wszystkie sygnały i czekamy na nie przez sigsuspend
    sigset_t maska, stara_maska;
    sigemptyset(&maska);
    sigaddset(&maska, SIGRTMIN + 0);
    sigaddset(&maska, SIGRTMIN + 1);
    sigaddset(&maska, SIGRTMIN + 2);
    sigaddset(&maska, SIGUSR1);
    sigaddset(&maska, SIGUSR2);
    sigaddset(&maska, SIGTERM);
    sigaddset(&maska, SIGALRM);

    sigprocmask(SIG_BLOCK, &maska, &stara_maska);

    /// STAN 1: Czekam aż przewodnik zbierze grupę
    while (!odwolano && !w_grupie && !moze_wyjsc && !sigterm_otrzymany && !alarm_otrzymany) {
        sigsuspend(&stara_maska);  /// Sleep z atomowym odblokowaniem sygnałów
    }

    if (alarm_otrzymany) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomoscf("TIMEOUT: Za dlugo w kolejce (%ds), koncze", MAX_CZAS_W_KOLEJCE);
        return 0;
    }

    /// Wyłączamy alarm - jesteśmy w grupie!
    alarm(0);

    if (sigterm_otrzymany) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("SHUTDOWN: SIGTERM przed rozpoczeciem wycieczki");
        return 0;
    }

    if (odwolano) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("CANCEL: Przed rozpoczeciem wycieczki");
        return 0;
    }

    if (w_grupie) {
        loguj_wiadomosc("STATE: Zebrano do grupy");
    }

    /// STAN 2: Czekam aż przewodnik powie "idźcie przez kładkę"
    while (!odwolano && !na_kladce && !moze_wyjsc && !sigterm_otrzymany) {
        sigsuspend(&stara_maska);
    }

    if (sigterm_otrzymany) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("SHUTDOWN: SIGTERM po zebraniu grupy");
        return 0;
    }

    if (odwolano) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("CANCEL: Po zebraniu grupy");
        return 0;
    }

    if (na_kladce) {
        loguj_wiadomosc("STATE: Przechodze kladke (wejscie)");
    }

    /// STAN 3: Czekam aż przewodnik powie "zaczynamy zwiedzanie"
    while (!odwolano && !zwiedzam && !moze_wyjsc && !sigterm_otrzymany) {
        sigsuspend(&stara_maska);
    }

    if (sigterm_otrzymany) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("SHUTDOWN: SIGTERM podczas przechodzenia kladki");
        return 0;
    }

    if (odwolano) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("CANCEL: Podczas przechodzenia kladki");
        return 0;
    }

    if (zwiedzam) {
        loguj_wiadomoscf("STATE: Zwiedzam trase %d", trasa);
    }

    /// STAN 4: Zwiedzam - czekam aż przewodnik powie "możecie wyjść" (SIGUSR2)
    while (!odwolano && !moze_wyjsc && !sigterm_otrzymany) {
        sigsuspend(&stara_maska);
    }

    if (sigterm_otrzymany) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("SHUTDOWN: SIGTERM podczas zwiedzania");
        return 0;
    }

    if (odwolano) {
        sigprocmask(SIG_SETMASK, &stara_maska, NULL);
        loguj_wiadomosc("CANCEL: Awaryjnie podczas zwiedzania");
        return 0;
    }

    /// STAN 5: WYJŚCIE - przeszedłem kładkę wyjściową
    if (moze_wyjsc) {
        loguj_wiadomosc("STATE: Przechodze kladke (wyjscie)");
        sleep(1);  /// Krótka przerwa
        loguj_wiadomosc("COMPLETE: Opuscilem jaskinie");
    }

    sigprocmask(SIG_SETMASK, &stara_maska, NULL);
    return 0;
}