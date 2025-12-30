#include "common.h"

/// Flaga sterująca główną pętlą programu.
/// Ustawiana na 0 po otrzymaniu sygnału SIGTERM,
/// co powoduje bezpieczne zakończenie pracy generatora.
volatile sig_atomic_t keep_running = 1;

/// Obsługa sygnału SIGTERM.
/// Funkcja ustawia flagę keep_running na 0,
/// sygnalizując programowi konieczność zakończenia działania.
void sigterm_handler(int sig) {
    keep_running = 0;
}

int main() {
    /// Identyfikator segmentu pamięci dzielonej
    int shmid;

    /// Wskaźnik na strukturę pamięci dzielonej
    SharedMemory *shm;
    
    /// Rejestracja obsługi sygnału SIGTERM
    signal(SIGTERM, sigterm_handler);
    
    /// Inicjalizacja generatora liczb losowych
    /// z użyciem aktualnego czasu i PID procesu
    srand(time(NULL) ^ getpid());
    
    /// Log informujący o starcie generatora zwiedzających
    log_message("GENERATOR", "Start generatora zwiedzających");
    
    /// Pobranie dostępu do istniejącej pamięci dzielonej
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    
    /// Dołączenie segmentu pamięci dzielonej do przestrzeni adresowej procesu
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    
    /// Zapis PID generatora w pamięci dzielonej
    /// umożliwiający innym procesom identyfikację generatora
    shm->pid_generator = getpid();
    
    /// Licznik wygenerowanych zwiedzających
    int licznik = 0;
    
    /// Główna pętla generatora:
    /// działa dopóki nie nadejdzie SIGTERM
    /// oraz dopóki jaskinia jest otwarta
    while (keep_running && shm->jaskinia_otwarta) {

        /// Losowanie wieku zwiedzającego w zakresie 1–80 lat
        int wiek = (rand() % 80) + 1;

        /// Losowanie informacji o powtórnej wizycie
        /// na podstawie zdefiniowanego prawdopodobieństwa
        int powtorna_wizyta = (rand() % 100) < REPEAT_CHANCE ? 1 : 0;

        /// Losowanie poprzedniej trasy (istotne dla powtórnych wizyt)
        int poprzednia_trasa = (rand() % 2) + 1;
        
        /// Utworzenie nowego procesu reprezentującego zwiedzającego
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork zwiedzajacy");
            sleep(1);
            continue;
        }
        
        if (pid == 0) {
            /// Proces potomny – przygotowanie argumentów
            /// i uruchomienie programu zwiedzającego
            char wiek_str[16], powtorna_str[16], trasa_str[16];
            snprintf(wiek_str, sizeof(wiek_str), "%d", wiek);
            snprintf(powtorna_str, sizeof(powtorna_str), "%d", powtorna_wizyta);
            snprintf(trasa_str, sizeof(trasa_str), "%d", poprzednia_trasa);
            
            execl("./zwiedzajacy", "zwiedzajacy",
                  wiek_str, powtorna_str, trasa_str, NULL);

            /// Jeśli execl się nie powiedzie – błąd krytyczny procesu dziecka
            perror("execl zwiedzajacy");
            exit(1);
        }
        
        /// Inkrementacja licznika wygenerowanych zwiedzających
        licznik++;
        
        /// Co 10 wygenerowanych zwiedzających zapis do logów
        if (licznik % 10 == 0) {
            log_formatted("GENERATOR",
                          "Wygenerowano %d zwiedzających", licznik);
        }
        
        /// Losowe opóźnienie pomiędzy generowaniem kolejnych zwiedzających
        int delay = GENERATOR_MIN_DELAY +
                    (rand() % (GENERATOR_MAX_DELAY - GENERATOR_MIN_DELAY + 1));
        sleep(delay);
    }
    
    /// Log końcowy informujący o zakończeniu pracy generatora
    log_formatted("GENERATOR",
                  "Generator kończy pracę (wygenerowano %d zwiedzających)",
                  licznik);
    
    /// Odłączenie pamięci dzielonej
    shmdt(shm);

    return 0;
}
