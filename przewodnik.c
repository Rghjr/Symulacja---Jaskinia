#include "common.h"

/// Flaga informująca o otrzymaniu sygnału zamknięcia trasy
/// Ustawiana w handlerze SIGUSR1 / SIGUSR2
volatile sig_atomic_t zamkniecie_otrzymane = 0;

/// Flaga sterująca główną pętlą programu
/// Zerowana po otrzymaniu SIGTERM (grzeczne zakończenie procesu)
volatile sig_atomic_t keep_running = 1;

/// Handler sygnału użytkownika (SIGUSR1 lub SIGUSR2)
/// Informuje przewodnika o konieczności zakończenia przyjmowania nowych grup
void sigusr_handler(int sig) {
    zamkniecie_otrzymane = 1;
}

/// Handler SIGTERM
/// Powoduje zakończenie głównej pętli i poprawne wyjście z programu
void sigterm_handler(int sig) {
    keep_running = 0;
}

int main(int argc, char* argv[]) {

    /// Sprawdzenie poprawności argumentów wejściowych
    /// Program wymaga numeru trasy (1 lub 2)
    if (argc != 2) {
        fprintf(stderr, "Użycie: %s <numer_trasy>\n", argv[0]);
        exit(1);
    }

    int numer_trasy = atoi(argv[1]);

    /// Walidacja numeru trasy
    if (numer_trasy != 1 && numer_trasy != 2) {
        fprintf(stderr, "Numer trasy musi być 1 lub 2\n");
        exit(1);
    }

    /// Identyfikatory zasobów IPC
    int shmid, semid, msgid;

    /// Wskaźnik do pamięci współdzielonej
    SharedMemory* shm;

    /// Struktura komunikatu odbieranego od zwiedzających
    MessagePrzewodnik msg;

    /// Nazwa procesu używana w systemie logowania
    char process_name[32];
    snprintf(process_name, sizeof(process_name), "PRZEWODNIK%d", numer_trasy);

    /// Rejestracja obsługi sygnałów
    /// Każda trasa reaguje na inny sygnał użytkownika
    if (numer_trasy == 1) {
        signal(SIGUSR1, sigusr_handler);
    }
    else {
        signal(SIGUSR2, sigusr_handler);
    }

    /// Obsługa sygnału zakończenia procesu
    signal(SIGTERM, sigterm_handler);

    log_formatted(process_name, "Start przewodnika trasy %d", numer_trasy);

    /// Podłączenie do segmentu pamięci współdzielonej
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    shm = (SharedMemory*)shmat(shmid, NULL, 0);
    if (shm == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    /// Podłączenie do zestawu semaforów
    semid = semget(SEM_KEY, SEM_COUNT, 0);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    /// Podłączenie do odpowiedniej kolejki komunikatów
    /// oraz zapis PID przewodnika w pamięci współdzielonej
    if (numer_trasy == 1) {
        msgid = msgget(MSG_KEY_PRZEWODNIK1, 0);
        shm->pid_przewodnik1 = getpid();
    }
    else {
        msgid = msgget(MSG_KEY_PRZEWODNIK2, 0);
        shm->pid_przewodnik2 = getpid();
    }

    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    /// Parametry zależne od wybranej trasy
    int max_osoby = (numer_trasy == 1) ? N1 : N2;
    int czas_zwiedzania = (numer_trasy == 1) ? T1 : T2;
    int sem_mutex_trasa = (numer_trasy == 1) ? SEM_MUTEX_TRASA1 : SEM_MUTEX_TRASA2;

    log_formatted(process_name,
        "Przewodnik gotowy (max osób: %d, czas: %ds)",
        max_osoby, czas_zwiedzania);

    /// Główna pętla pracy przewodnika
    /// Działa dopóki:
    /// - proces nie otrzyma SIGTERM
    /// - jaskinia jest otwarta lub trwa obsługa ostatniej grupy
    while (keep_running && (shm->jaskinia_otwarta || !zamkniecie_otrzymane)) {

        /// Jeśli otrzymano sygnał zamknięcia przed zebraniem grupy
        /// nie przyjmuj nowych zwiedzających
        if (zamkniecie_otrzymane) {
            log_formatted(process_name,
                "Otrzymano sygnał zamknięcia, nie przyjmuję nowych grup");
            break;
        }

        /// Bufor PID-ów zwiedzających w aktualnej grupie
        pid_t grupa[max_osoby];
        int liczba_w_grupie = 0;

        log_formatted(process_name,
            "Zbieranie grupy (max %d osób)...", max_osoby);

        /// Zbieranie grupy maksymalnie przez 5 sekund
        time_t start_collecting = time(NULL);
        while (liczba_w_grupie < max_osoby &&
            difftime(time(NULL), start_collecting) < 5 &&
            !zamkniecie_otrzymane) {

            /// Odbiór komunikatu od zwiedzającego (nieblokujący)
            if (msgrcv(msgid, &msg,
                sizeof(MessagePrzewodnik) - sizeof(long),
                MSG_TYPE_ZWIEDZAJACY, IPC_NOWAIT) != -1) {

                grupa[liczba_w_grupie++] = msg.pid_zwiedzajacego;

                log_formatted(process_name,
                    "Dołączył zwiedzający PID %d (%d/%d)",
                    msg.pid_zwiedzajacego,
                    liczba_w_grupie, max_osoby);
            }
            else {
                usleep(100000);   /// Krótka pauza (100 ms)
            }
        }

        /// Jeśli nie udało się zebrać żadnej osoby – spróbuj ponownie
        if (liczba_w_grupie == 0) {
            usleep(500000);
            continue;
        }

        /// Jeśli w trakcie zbierania grupy otrzymano sygnał zamknięcia
        /// grupa jest odrzucana
        if (zamkniecie_otrzymane) {
            log_formatted(process_name,
                "Sygnał zamknięcia podczas zbierania, odrzucam grupę %d osób",
                liczba_w_grupie);

            /// Powiadom zwiedzających o zakończeniu (sygnał)
            for (int i = 0; i < liczba_w_grupie; i++) {
                kill(grupa[i], SIGUSR1);
            }
            break;
        }

        log_formatted(process_name,
            "Zebrano grupę %d osób, rozpoczynam wycieczkę",
            liczba_w_grupie);

        /// Oczekiwanie na zwolnienie kładki
        sem_wait(semid, SEM_MUTEX_KLADKA);
        while (shm->osoby_na_kladce > 0 ||
            shm->kierunek_kladki == KIERUNEK_WYJSCIE) {

            sem_signal(semid, SEM_MUTEX_KLADKA);
            usleep(100000);
            sem_wait(semid, SEM_MUTEX_KLADKA);
        }

        /// Ustawienie kierunku ruchu na wejście
        shm->kierunek_kladki = KIERUNEK_WEJSCIE;
        sem_signal(semid, SEM_MUTEX_KLADKA);

        log_formatted(process_name,
            "Kładka wolna, wpuszczam grupę...");

        /// Przeprowadzanie zwiedzających przez kładkę
        for (int i = 0; i < liczba_w_grupie; i++) {

            sem_wait(semid, SEM_MIEJSCA_KLADKA);

            sem_wait(semid, SEM_MUTEX_KLADKA);
            shm->osoby_na_kladce++;
            sem_signal(semid, SEM_MUTEX_KLADKA);

            usleep(200000);   /// Symulacja przechodzenia

            sem_wait(semid, SEM_MUTEX_KLADKA);
            shm->osoby_na_kladce--;
            sem_signal(semid, SEM_MUTEX_KLADKA);

            sem_signal(semid, SEM_MIEJSCA_KLADKA);
        }

        /// Aktualizacja liczby osób znajdujących się na trasie
        sem_wait(semid, sem_mutex_trasa);
        if (numer_trasy == 1) {
            shm->osoby_na_trasie1 += liczba_w_grupie;
            log_formatted(process_name,
                "Na trasie 1: %d osób", shm->osoby_na_trasie1);
        }
        else {
            shm->osoby_na_trasie2 += liczba_w_grupie;
            log_formatted(process_name,
                "Na trasie 2: %d osób", shm->osoby_na_trasie2);
        }
        sem_signal(semid, sem_mutex_trasa);

        /// Reset kierunku kładki
        sem_wait(semid, SEM_MUTEX_KLADKA);
        shm->kierunek_kladki = KIERUNEK_PUSTY;
        sem_signal(semid, SEM_MUTEX_KLADKA);

        /// Symulacja zwiedzania trasy
        log_formatted(process_name,
            "Grupa zwiedza trasę %d (%ds)...",
            numer_trasy, czas_zwiedzania);

        sleep(czas_zwiedzania);

        /// Wypuszczanie grupy z jaskini (analogicznie jak wejście)
        log_formatted(process_name,
            "Zwiedzanie zakończone, wypuszczam grupę...");

        sem_wait(semid, SEM_MUTEX_KLADKA);
        while (shm->osoby_na_kladce > 0 ||
            shm->kierunek_kladki == KIERUNEK_WEJSCIE) {

            sem_signal(semid, SEM_MUTEX_KLADKA);
            usleep(100000);
            sem_wait(semid, SEM_MUTEX_KLADKA);
        }

        shm->kierunek_kladki = KIERUNEK_WYJSCIE;
        sem_signal(semid, SEM_MUTEX_KLADKA);

        for (int i = 0; i < liczba_w_grupie; i++) {

            sem_wait(semid, SEM_MIEJSCA_KLADKA);

            sem_wait(semid, SEM_MUTEX_KLADKA);
            shm->osoby_na_kladce++;
            sem_signal(semid, SEM_MUTEX_KLADKA);

            usleep(200000);

            sem_wait(semid, SEM_MUTEX_KLADKA);
            shm->osoby_na_kladce--;
            sem_signal(semid, SEM_MUTEX_KLADKA);

            sem_signal(semid, SEM_MIEJSCA_KLADKA);

            /// Powiadomienie zwiedzającego o możliwości opuszczenia jaskini
            kill(grupa[i], SIGUSR2);
        }

        /// Aktualizacja liczby osób na trasie po wyjściu grupy
        sem_wait(semid, sem_mutex_trasa);
        if (numer_trasy == 1) {
            shm->osoby_na_trasie1 -= liczba_w_grupie;
        }
        else {
            shm->osoby_na_trasie2 -= liczba_w_grupie;
        }
        sem_signal(semid, sem_mutex_trasa);

        sem_wait(semid, SEM_MUTEX_KLADKA);
        shm->kierunek_kladki = KIERUNEK_PUSTY;
        sem_signal(semid, SEM_MUTEX_KLADKA);

        log_formatted(process_name,
            "Grupa opuściła jaskinię");

        if (zamkniecie_otrzymane) {
            break;
        }
    }

    log_formatted(process_name,
        "Przewodnik kończy pracę");

    /// Odłączenie pamięci współdzielonej
    shmdt(shm);

    return 0;
}
