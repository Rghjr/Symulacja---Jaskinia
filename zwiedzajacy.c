#include "common.h"

/// Flaga ustawiana sygnałem SIGUSR1 – zwiedzający został odrzucony
volatile sig_atomic_t odrzucony = 0;

/// Flaga ustawiana sygnałem SIGUSR2 – zwiedzający może zakończyć wizytę
volatile sig_atomic_t moze_wyjsc = 0;

/// Obsługa sygnału SIGUSR1 – informacja o odrzuceniu zwiedzającego
void sigusr1_handler(int sig) {
    odrzucony = 1;
}

/// Obsługa sygnału SIGUSR2 – informacja o zakończeniu wycieczki
void sigusr2_handler(int sig) {
    moze_wyjsc = 1;
}

int main(int argc, char* argv[]) {

    /// Sprawdzenie poprawności liczby argumentów wejściowych
    if (argc != 4) {
        fprintf(stderr, "Użycie: %s <wiek> <powtorna_wizyta> <poprzednia_trasa>\n", argv[0]);
        exit(1);
    }

    /// Pobranie parametrów zwiedzającego z linii poleceń
    int wiek = atoi(argv[1]);
    int powtorna_wizyta = atoi(argv[2]);
    int poprzednia_trasa = atoi(argv[3]);

    /// Identyfikatory zasobów IPC
    int shmid, semid, msgid_kasjer, msgid_przewodnik;

    /// Wskaźnik do pamięci współdzielonej
    SharedMemory* shm;

    /// Struktury komunikatów IPC
    MessageKasjer msg_req;
    MessageOdpowiedz msg_resp;
    MessagePrzewodnik msg_przewodnik;

    /// Rejestracja obsługi sygnałów
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    /// PID bieżącego procesu zwiedzającego
    pid_t moj_pid = getpid();

    /// Pobranie segmentu pamięci współdzielonej
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    /// Podłączenie do pamięci współdzielonej
    shm = (SharedMemory*)shmat(shmid, NULL, 0);
    if (shm == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    /// Sprawdzenie, czy jaskinia jest otwarta
    if (!shm->jaskinia_otwarta) {
        log_formatted("ZWIEDZAJACY", "PID %d: Jaskinia zamknięta, rezygnuję", moj_pid);
        shmdt(shm);
        exit(0);
    }

    /// Pobranie zestawu semaforów
    semid = semget(SEM_KEY, SEM_COUNT, 0);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    /// Pobranie kolejki komunikatów kasjera
    msgid_kasjer = msgget(MSG_KEY_KASJER, 0);
    if (msgid_kasjer == -1) {
        perror("msgget kasjer");
        exit(1);
    }

    /// Log informacji o zwiedzającym
    log_formatted(
        "ZWIEDZAJACY",
        "PID %d: Wiek %d, %s",
        moj_pid,
        wiek,
        powtorna_wizyta ? "powtórna wizyta" : "pierwsza wizyta"
    );

    /// Przygotowanie komunikatu żądania do kasjera
    msg_req.mtype = MSG_TYPE_REQUEST;
    msg_req.pid_zwiedzajacego = moj_pid;
    msg_req.wiek = wiek;
    msg_req.powtorna_wizyta = powtorna_wizyta;
    msg_req.poprzednia_trasa = poprzednia_trasa;

    /// Dzieci poniżej 8 lat traktowane są jako posiadające opiekuna
    /// W tej wersji opiekun jest symulowany jako proces nadrzędny
    if (wiek < 8) {
        msg_req.ma_opiekuna = 1;
        msg_req.pid_opiekuna = getppid();
    }
    else {
        msg_req.ma_opiekuna = 0;
        msg_req.pid_opiekuna = 0;
    }

    /// Wysłanie żądania do kasjera
    if (msgsnd(
        msgid_kasjer,
        &msg_req,
        sizeof(MessageKasjer) - sizeof(long),
        0
    ) == -1) {
        perror("msgsnd żądanie");
        shmdt(shm);
        exit(1);
    }

    log_formatted("ZWIEDZAJACY", "PID %d: Wysłano żądanie do kasjera", moj_pid);

    /// Oczekiwanie na odpowiedź kasjera
    if (msgrcv(
        msgid_kasjer,
        &msg_resp,
        sizeof(MessageOdpowiedz) - sizeof(long),
        moj_pid,
        0
    ) == -1) {
        perror("msgrcv odpowiedź");
        shmdt(shm);
        exit(1);
    }

    /// Obsługa decyzji o odrzuceniu
    if (msg_resp.decyzja == DECYZJA_ODRZUCONY) {
        log_formatted("ZWIEDZAJACY", "PID %d: Odrzucony przez kasjera", moj_pid);
        shmdt(shm);
        exit(0);
    }

    /// Pobranie przydzielonej trasy
    int przydzielona_trasa = msg_resp.przydzielona_trasa;

    log_formatted(
        "ZWIEDZAJACY",
        "PID %d: Zaakceptowany, trasa %d",
        moj_pid,
        przydzielona_trasa
    );

    /// Wybór kolejki komunikatów odpowiedniego przewodnika
    if (przydzielona_trasa == 1) {
        msgid_przewodnik = msgget(MSG_KEY_PRZEWODNIK1, 0);
    }
    else {
        msgid_przewodnik = msgget(MSG_KEY_PRZEWODNIK2, 0);
    }

    if (msgid_przewodnik == -1) {
        perror("msgget przewodnik");
        shmdt(shm);
        exit(1);
    }

    /// Przygotowanie komunikatu do przewodnika
    msg_przewodnik.mtype = MSG_TYPE_ZWIEDZAJACY;
    msg_przewodnik.pid_zwiedzajacego = moj_pid;
    msg_przewodnik.wiek = wiek;

    /// Zgłoszenie zwiedzającego do kolejki przewodnika
    if (msgsnd(
        msgid_przewodnik,
        &msg_przewodnik,
        sizeof(MessagePrzewodnik) - sizeof(long),
        0
    ) == -1) {
        perror("msgsnd przewodnik");
        shmdt(shm);
        exit(1);
    }

    log_formatted(
        "ZWIEDZAJACY",
        "PID %d: Dołączam do kolejki trasy %d",
        moj_pid,
        przydzielona_trasa
    );

    /// Oczekiwanie na zakończenie wycieczki, odrzucenie lub zamknięcie jaskini
    while (!moze_wyjsc && !odrzucony && shm->jaskinia_otwarta) {
        sleep(1);
    }

    /// Obsługa zakończenia wizyty
    if (odrzucony) {
        log_formatted(
            "ZWIEDZAJACY",
            "PID %d: Wycieczka odwołana (zamknięcie)",
            moj_pid
        );
    }
    else {
        log_formatted(
            "ZWIEDZAJACY",
            "PID %d: Wycieczka zakończona, wychodzę",
            moj_pid
        );
    }

    /// Odłączenie pamięci współdzielonej
    shmdt(shm);

    return 0;
}
