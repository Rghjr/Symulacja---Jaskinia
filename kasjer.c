#include "common.h"

/// Flaga sterująca pętlą główną procesu kasjera
/// Zmieniana asynchronicznie przez handler sygnału
volatile sig_atomic_t keep_running = 1;

/// Handler sygnału SIGTERM
/// Ustawia flagę zakończenia pracy procesu
void sigterm_handler(int sig) {
    keep_running = 0;
}

int main() {
    /// Identyfikatory zasobów IPC
    int shmid, semid, msgid;

    /// Wskaźnik na pamięć współdzieloną
    SharedMemory *shm;

    /// Struktura żądania od zwiedzającego
    MessageKasjer msg_req;

    /// Struktura odpowiedzi wysyłanej do zwiedzającego
    MessageOdpowiedz msg_resp;
    
    /// Rejestracja obsługi sygnału zakończenia procesu
    signal(SIGTERM, sigterm_handler);
    
    /// Log startu procesu kasjera
    log_message("KASJER", "Start procesu kasjera");
    
    /// Pobranie identyfikatora pamięci współdzielonej
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    
    /// Dołączenie pamięci współdzielonej do przestrzeni adresowej procesu
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    
    /// Pobranie zestawu semaforów
    semid = semget(SEM_KEY, SEM_COUNT, 0);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }
    
    /// Otwarcie kolejki komunikatów kasjera
    msgid = msgget(MSG_KEY_KASJER, 0);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    
    /// Zapis PID kasjera w pamięci współdzielonej
    shm->pid_kasjer = getpid();
    
    /// Log gotowości do obsługi zwiedzających
    log_message("KASJER", "Kasjer gotowy do obsługi");
    
    /// Główna pętla pracy kasjera
    /// Działa dopóki proces nie dostanie SIGTERM
    /// i jaskinia jest otwarta
    while (keep_running && shm->jaskinia_otwarta) {

        /// Próba odebrania żądania z kolejki komunikatów
        /// Tryb nieblokujący (IPC_NOWAIT)
        if (msgrcv(msgid, &msg_req, sizeof(MessageKasjer) - sizeof(long), 
                   MSG_TYPE_REQUEST, IPC_NOWAIT) == -1) {

            /// Brak wiadomości – chwilowy sleep i ponowna próba
            if (errno == ENOMSG) {
                usleep(100000);
                continue;
            }

            /// Przerwanie przez sygnał – wracamy do pętli
            if (errno == EINTR) {
                continue;
            }

            /// Inny błąd odbioru komunikatu
            perror("msgrcv kasjer");
            continue;
        }
        
        /// Log odebranego żądania
        log_formatted("KASJER", "Otrzymano żądanie od PID %d, wiek: %d, powtórna: %d",
                     msg_req.pid_zwiedzajacego, msg_req.wiek, msg_req.powtorna_wizyta);
        
        /// Domyślna decyzja – odrzucenie
        int decyzja = DECYZJA_ODRZUCONY;

        /// Numer przydzielonej trasy (1 lub 2)
        int trasa = 0;
        
        /// Dzieci poniżej 3 lat
        /// Wstęp darmowy, ale tylko z opiekunem
        if (msg_req.wiek < 3) {
            if (!msg_req.ma_opiekuna) {
                log_formatted("KASJER", "ODRZUCONO: Dziecko < 3 lat bez opiekuna (PID: %d)", 
                             msg_req.pid_zwiedzajacego);
                decyzja = DECYZJA_ODRZUCONY;
            } else {
                /// Dziecko z opiekunem – tylko trasa 2
                decyzja = DECYZJA_TRASA2;
                trasa = 2;
            }
        }

        /// Dzieci w wieku 3–7 lat
        /// Zawsze wymagany opiekun, tylko trasa 2
        else if (msg_req.wiek >= 3 && msg_req.wiek < 8) {
            if (!msg_req.ma_opiekuna) {
                log_formatted("KASJER", "ODRZUCONO: Dziecko < 8 lat bez opiekuna (PID: %d)", 
                             msg_req.pid_zwiedzajacego);
                decyzja = DECYZJA_ODRZUCONY;
            } else {
                decyzja = DECYZJA_TRASA2;
                trasa = 2;
            }
        }

        /// Osoby powyżej 75 roku życia
        /// Bezpieczna opcja – tylko trasa 2
        else if (msg_req.wiek > 75) {
            decyzja = DECYZJA_TRASA2;
            trasa = 2;
        }

        /// Zwiedzający na powtórnej wizycie
        /// Dostaje inną trasę niż poprzednio
        else if (msg_req.powtorna_wizyta) {
            if (msg_req.poprzednia_trasa == 1) {
                decyzja = DECYZJA_TRASA2;
                trasa = 2;
            } else {
                decyzja = DECYZJA_TRASA1;
                trasa = 1;
            }

            /// Log decyzji dla powtórnej wizyty
            log_formatted("KASJER", "Powtórna wizyta: poprzednia trasa %d, nowa trasa %d (PID: %d)",
                         msg_req.poprzednia_trasa, trasa, msg_req.pid_zwiedzajacego);
        }

        /// Osoby w wieku 8–75 lat
        /// Losowy przydział jednej z tras
        else {
            trasa = (rand() % 2) + 1;
            decyzja = (trasa == 1) ? DECYZJA_TRASA1 : DECYZJA_TRASA2;
        }
        
        /// Przygotowanie odpowiedzi do zwiedzającego
        /// Typ komunikatu = PID odbiorcy
        msg_resp.mtype = msg_req.pid_zwiedzajacego;
        msg_resp.decyzja = decyzja;
        msg_resp.przydzielona_trasa = trasa;
        
        /// Wysłanie odpowiedzi przez kolejkę komunikatów
        if (msgsnd(msgid, &msg_resp, sizeof(MessageOdpowiedz) - sizeof(long), 0) == -1) {
            perror("msgsnd odpowiedz");
            continue;
        }
        
        /// Log akceptacji zwiedzającego
        if (decyzja != DECYZJA_ODRZUCONY) {
            log_formatted("KASJER", "ZAAKCEPTOWANO: PID %d, wiek: %d -> trasa %d",
                         msg_req.pid_zwiedzajacego, msg_req.wiek, trasa);
        }
    }
    
    /// Log zakończenia pracy kasjera
    log_message("KASJER", "Kasjer kończy pracę");
    
    /// Odłączenie pamięci współdzielonej
    shmdt(shm);

    /// Normalne zakończenie procesu
    return 0;
}
