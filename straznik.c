#include "common.h"
/// Dołączenie wspólnego nagłówka projektu.
/// Zawiera:
/// - definicje struktur pamięci dzielonej
/// - klucze IPC
/// - stałe konfiguracyjne
/// - deklaracje funkcji logujących

volatile sig_atomic_t keep_running = 1;
/// Flaga sterująca główną pętlą programu.
/// Typ sig_atomic_t zapewnia bezpieczny dostęp w kontekście obsługi sygnałów.
/// Wartość 1 oznacza dalsze działanie procesu.

void sigterm_handler(int sig) {
    /// Procedura obsługi sygnału SIGTERM.
    /// Po odebraniu sygnału ustawia flagę zakończenia pracy procesu.
    keep_running = 0;
}

int main() {
    int shmid, semid;
    /// shmid – identyfikator segmentu pamięci dzielonej
    /// semid – identyfikator zestawu semaforów

    SharedMemory *shm;
    /// Wskaźnik na strukturę przechowywaną w pamięci dzielonej.
    /// Zawiera wspólny stan całego systemu.

    time_t start_time, current_time;
    /// start_time – czas rozpoczęcia działania jaskini
    /// current_time – aktualny czas systemowy
    
    signal(SIGTERM, sigterm_handler);
    /// Rejestracja procedury obsługi sygnału SIGTERM
    
    log_message("STRAZNIK", "Start procesu strażnika");
    /// Zapis informacji o uruchomieniu procesu strażnika do logów
    
    /// ===== PODŁĄCZENIE DO PAMIĘCI DZIELONEJ =====
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0);
    /// Uzyskanie dostępu do istniejącego segmentu pamięci dzielonej

    if (shmid == -1) {
        perror("shmget");
        /// Wypisanie błędu systemowego w przypadku niepowodzenia
        exit(1);
        /// Zakończenie programu – bez pamięci dzielonej dalsze działanie
        /// nie jest możliwe
    }
    
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    /// Dołączenie segmentu pamięci dzielonej do przestrzeni adresowej procesu

    if (shm == (void *)-1) {
        perror("shmat");
        /// Obsługa błędu dołączenia pamięci
        exit(1);
    }
    
    /// ===== PODŁĄCZENIE DO SEMAFORÓW =====
    semid = semget(SEM_KEY, SEM_COUNT, 0);
    /// Uzyskanie dostępu do zestawu semaforów synchronizujących procesy

    if (semid == -1) {
        perror("semget");
        /// Błąd pobrania semaforów
        exit(1);
    }
    
    start_time = shm->timestamp_start;
    /// Odczyt czasu rozpoczęcia działania jaskini z pamięci dzielonej

    shm->pid_straznik = getpid();
    /// Zapis PID procesu strażnika w pamięci dzielonej
    
    log_formatted("STRAZNIK", 
                  "Jaskinia otwarta. Zamknięcie za %d sekund", Tk);
    /// Informacja o planowanym czasie zamknięcia jaskini
    
    /// ===== OCZEKIWANIE NA UPŁYW CZASU Tk =====
    while (keep_running) {
        /// Pętla główna procesu strażnika
        /// Działa do momentu upłynięcia czasu Tk
        /// lub otrzymania sygnału SIGTERM

        current_time = time(NULL);
        /// Pobranie aktualnego czasu systemowego

        int elapsed = difftime(current_time, start_time);
        /// Obliczenie liczby sekund, które upłynęły od startu

        if (elapsed >= Tk) {
            /// Warunek zakończenia pracy jaskini
            break;
        }
        
        sleep(1);
        /// Uśpienie procesu na 1 sekundę
    }
    
    log_message("STRAZNIK", 
                "Czas Tk osiągnięty, zamykam jaskinię...");
    /// Informacja o rozpoczęciu procedury zamykania
    
    /// ===== ZAMKNIĘCIE JASKINI =====
    shm->jaskinia_otwarta = 0;
    /// Ustawienie flagi zamknięcia jaskini w pamięci dzielonej
    /// Blokuje możliwość wejścia nowych osób
    
    /// ===== POWIADOMIENIE PRZEWODNIKÓW =====
    if (shm->pid_przewodnik1 > 0) {
        /// Sprawdzenie, czy przewodnik 1 jest aktywny
        log_formatted("STRAZNIK",
                      "Wysyłam SIGUSR1 do przewodnika 1 (PID: %d)",
                      shm->pid_przewodnik1);
        kill(shm->pid_przewodnik1, SIGUSR1);
        /// Sygnał informujący przewodnika 1 o zakończeniu pracy
    }
    
    if (shm->pid_przewodnik2 > 0) {
        /// Analogicznie dla przewodnika 2
        log_formatted("STRAZNIK",
                      "Wysyłam SIGUSR2 do przewodnika 2 (PID: %d)",
                      shm->pid_przewodnik2);
        kill(shm->pid_przewodnik2, SIGUSR2);
    }
    
    /// ===== OCZEKIWANIE NA OPRÓŻNIENIE TRAS =====
    log_message("STRAZNIK", 
                "Oczekiwanie na opróżnienie tras...");
    
    int wait_cycles = 0;
    /// Licznik czasu oczekiwania (w sekundach)

    while ((shm->osoby_na_trasie1 > 0 || 
            shm->osoby_na_trasie2 > 0) &&
            wait_cycles < 120) {
        /// Pętla oczekiwania na zakończenie zwiedzania
        /// Maksymalny czas oczekiwania: 120 sekund

        sleep(1);
        wait_cycles++;
        
        if (wait_cycles % 10 == 0) {
            /// Co 10 sekund wypisywany jest aktualny stan tras
            log_formatted("STRAZNIK",
                          "Trasa 1: %d osób, Trasa 2: %d osób",
                          shm->osoby_na_trasie1,
                          shm->osoby_na_trasie2);
        }
    }
    
    log_message("STRAZNIK", 
                "Trasy opróżnione, strażnik kończy pracę");
    /// Końcowy komunikat procesu
    
    shmdt(shm);
    /// Odłączenie segmentu pamięci dzielonej
    
    return 0;
    /// Poprawne zakończenie programu
}
