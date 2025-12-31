#include "common.h"
#include <stdarg.h>

/// ===============================
/// ZMIENNE GLOBALNE – IDENTYFIKATORY IPC
/// ===============================
/// Globalne identyfikatory zasobów IPC.
/// Trzymane globalnie, aby możliwe było ich
/// poprawne usunięcie w cleanup_resources()
/// oraz w obsłudze sygnału SIGINT.
int shmid = -1;
int semid = -1;
int msgid_kasjer = -1;
int msgid_przewodnik1 = -1;
int msgid_przewodnik2 = -1;


/// ===============================
/// FUNKCJE OPERUJĄCE NA SEMAFORACH
/// ===============================

/// Operacja P (wait) na semaforze.
/// Zmniejsza wartość semafora o 1.
/// Jeśli semafor ma wartość 0, proces zostaje
/// zablokowany do momentu jego zwolnienia.
/// Wykorzystywana do synchronizacji procesów
/// oraz ochrony sekcji krytycznych.
void sem_wait(int semid, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;   /// Numer semafora w zestawie
    op.sem_op = -1;         /// Operacja P
    op.sem_flg = 0;         /// Oczekiwanie blokujące
    
    if (semop(semid, &op, 1) == -1) {
        perror("semop wait");
    }
}

/// Operacja V (signal) na semaforze.
/// Zwiększa wartość semafora o 1.
/// Odblokowuje jeden z procesów
/// oczekujących na dany semafor.
void sem_signal(int semid, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;   /// Numer semafora
    op.sem_op = 1;          /// Operacja V
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("semop signal");
    }
}

/// Nieblokująca próba wykonania operacji P.
/// Jeśli semafor jest zajęty, funkcja natychmiast
/// zwraca błąd zamiast blokować proces.
/// Używane w sytuacjach, gdzie blokada
/// mogłaby prowadzić do zakleszczeń.
int sem_trywait(int semid, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = -1;
    op.sem_flg = IPC_NOWAIT;   /// Tryb nieblokujący
    
    if (semop(semid, &op, 1) == -1) {
        if (errno == EAGAIN) {
            /// Semafor chwilowo niedostępny
            return -1;
        }
        perror("semop trywait");
        return -1;
    }
    return 0;
}

/// Ustawienie początkowej wartości semafora.
/// Wywoływane wyłącznie podczas inicjalizacji
/// systemu, przed uruchomieniem procesów.
void sem_init_value(int semid, int sem_num, int value) {
    union semun arg;
    arg.val = value;
    
    if (semctl(semid, sem_num, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }
}


/// ===============================
/// FUNKCJE LOGOWANIA
/// ===============================

/// Zapis pojedynczego komunikatu do pliku logu.
/// Każdy wpis zawiera znacznik czasu, PID procesu
/// oraz nazwę komponentu systemu.
/// Logowanie ułatwia analizę zachowania
/// systemu wieloprocesowego.
void log_message(const char *process, const char *message) {
    int fd = open("jaskinia.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open log file");
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "[%s] [PID:%d] [%s] %s\n", 
             timestamp, getpid(), process, message);
    
    write(fd, buffer, strlen(buffer));
    close(fd);
}

/// Wersja logowania obsługująca formatowanie
/// w stylu printf.
/// Umożliwia dynamiczne tworzenie komunikatów.
void log_formatted(const char *process, const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    log_message(process, message);
}


/// ===============================
/// CZYSZCZENIE ZASOBÓW SYSTEMOWYCH
/// ===============================
/// Funkcja usuwa wszystkie zasoby IPC:
/// - pamięć dzieloną
/// - zestaw semaforów
/// - kolejki komunikatów
/// Zapobiega pozostawieniu „wiszących”
/// zasobów w systemie operacyjnym.
void cleanup_resources() {
    log_message("INIT", "Rozpoczynam czyszczenie zasobów...");
    
    if (shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl IPC_RMID");
        } else {
            log_message("INIT", "Pamięć dzielona usunięta");
        }
    }
    
    if (semid != -1) {
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("semctl IPC_RMID");
        } else {
            log_message("INIT", "Semafory usunięte");
        }
    }
    
    if (msgid_kasjer != -1) {
        if (msgctl(msgid_kasjer, IPC_RMID, NULL) == -1) {
            perror("msgctl IPC_RMID kasjer");
        } else {
            log_message("INIT", "Kolejka kasjera usunięta");
        }
    }
    
    if (msgid_przewodnik1 != -1) {
        if (msgctl(msgid_przewodnik1, IPC_RMID, NULL) == -1) {
            perror("msgctl IPC_RMID przewodnik1");
        } else {
            log_message("INIT", "Kolejka przewodnika 1 usunięta");
        }
    }
    
    if (msgid_przewodnik2 != -1) {
        if (msgctl(msgid_przewodnik2, IPC_RMID, NULL) == -1) {
            perror("msgctl IPC_RMID przewodnik2");
        } else {
            log_message("INIT", "Kolejka przewodnika 2 usunięta");
        }
    }
    
    log_message("INIT", "Czyszczenie zakończone");
}

/// Obsługa sygnału SIGINT (Ctrl+C).
/// Zapewnia kontrolowane zamknięcie systemu
/// oraz zwolnienie wszystkich zasobów IPC.
void sigint_handler(int sig) {
    log_message("INIT", "Otrzymano SIGINT, zamykam system...");
    cleanup_resources();
    exit(0);
}


/// ===============================
/// FUNKCJA GŁÓWNA SYSTEMU
/// ===============================
int main() {
    pid_t pid_straznik, pid_kasjer, pid_przewodnik1, pid_przewodnik2, pid_generator;
    SharedMemory *shm;
    
    /// Usunięcie starego pliku logu,
    /// aby każdy start systemu miał czysty log.
    unlink("jaskinia.log");
    
    log_message("INIT", "=== START SYSTEMU JASKINIA ===");
    
    /// Rejestracja obsługi sygnału SIGINT
    signal(SIGINT, sigint_handler);
    
    /// Utworzenie segmentu pamięci dzielonej
    /// przechowującego globalny stan systemu.
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    log_message("INIT", "Pamięć dzielona utworzona");
    
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        cleanup_resources();
        exit(1);
    }
    
    /// Inicjalizacja struktury pamięci dzielonej
    memset(shm, 0, sizeof(SharedMemory));
    shm->jaskinia_otwarta = 1;
    shm->kierunek_kladki = KIERUNEK_PUSTY;
    shm->timestamp_start = time(NULL);
    
    /// Tworzenie zestawu semaforów
    semid = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("semget");
        cleanup_resources();
        exit(1);
    }
    log_message("INIT", "Semafory utworzone");
    
    /// Inicjalizacja semaforów synchronizujących
    /// ruch zwiedzających i dostęp do zasobów.
    sem_init_value(semid, SEM_MUTEX_KLADKA, 1);
    sem_init_value(semid, SEM_MIEJSCA_KLADKA, K);
    sem_init_value(semid, SEM_MUTEX_TRASA1, 1);
    sem_init_value(semid, SEM_MUTEX_TRASA2, 1);
    sem_init_value(semid, SEM_MUTEX_LOG, 1);
    
    /// Tworzenie kolejek komunikatów
    /// do komunikacji pomiędzy procesami.
    msgid_kasjer = msgget(MSG_KEY_KASJER, IPC_CREAT | 0600);
    if (msgid_kasjer == -1) {
        perror("msgget kasjer");
        cleanup_resources();
        exit(1);
    }
    
    msgid_przewodnik1 = msgget(MSG_KEY_PRZEWODNIK1, IPC_CREAT | 0600);
    if (msgid_przewodnik1 == -1) {
        perror("msgget przewodnik1");
        cleanup_resources();
        exit(1);
    }
    
    msgid_przewodnik2 = msgget(MSG_KEY_PRZEWODNIK2, IPC_CREAT | 0600);
    if (msgid_przewodnik2 == -1) {
        perror("msgget przewodnik2");
        cleanup_resources();
        exit(1);
    }
    
    log_message("INIT", "Kolejki komunikatów utworzone");
    
    /// Odłączenie pamięci dzielonej w procesie INIT
    shmdt(shm);
    
    /// Uruchamianie procesów systemu
    
    /// 1. Strażnik – proces nadrzędny logiki systemu
    pid_straznik = fork();
    if (pid_straznik == -1) {
        perror("fork straznik");
        cleanup_resources();
        exit(1);
    }
    if (pid_straznik == 0) {
        execl("./straznik", "straznik", NULL);
        perror("execl straznik");
        exit(1);
    }
    log_formatted("INIT", "Uruchomiono strażnika (PID: %d)", pid_straznik);
    
    /// 2. Kasjer – obsługa sprzedaży biletów
    pid_kasjer = fork();
    if (pid_kasjer == -1) {
        perror("fork kasjer");
        cleanup_resources();
        exit(1);
    }
    if (pid_kasjer == 0) {
        execl("./kasjer", "kasjer", NULL);
        perror("execl kasjer");
        exit(1);
    }
    log_formatted("INIT", "Uruchomiono kasjera (PID: %d)", pid_kasjer);
    
    /// 3. Przewodnik trasy 1
    pid_przewodnik1 = fork();
    if (pid_przewodnik1 == -1) {
        perror("fork przewodnik1");
        cleanup_resources();
        exit(1);
    }
    if (pid_przewodnik1 == 0) {
        execl("./przewodnik", "przewodnik", "1", NULL);
        perror("execl przewodnik1");
        exit(1);
    }
    log_formatted("INIT", "Uruchomiono przewodnika trasy 1 (PID: %d)", pid_przewodnik1);
    
    /// 4. Przewodnik trasy 2
    pid_przewodnik2 = fork();
    if (pid_przewodnik2 == -1) {
        perror("fork przewodnik2");
        cleanup_resources();
        exit(1);
    }
    if (pid_przewodnik2 == 0) {
        execl("./przewodnik", "przewodnik", "2", NULL);
        perror("execl przewodnik2");
        exit(1);
    }
    log_formatted("INIT", "Uruchomiono przewodnika trasy 2 (PID: %d)", pid_przewodnik2);
    
    /// Krótkie opóźnienie, aby przewodnicy
    /// zdążyli się poprawnie zainicjalizować.
    sleep(1);
    
    /// 5. Generator zwiedzających
    pid_generator = fork();
    if (pid_generator == -1) {
        perror("fork generator");
        cleanup_resources();
        exit(1);
    }
    if (pid_generator == 0) {
        execl("./generator", "generator", NULL);
        perror("execl generator");
        exit(1);
    }
    log_formatted("INIT", "Uruchomiono generator (PID: %d)", pid_generator);
    
    log_message("INIT", "Wszystkie procesy uruchomione, oczekiwanie na zakończenie...");
    
    /// Oczekiwanie na zakończenie strażnika,
    /// który odpowiada za zamknięcie systemu.
    int status;
    waitpid(pid_straznik, &status, 0);
    log_message("INIT", "Strażnik zakończył pracę");
    
    /// Wymuszone zakończenie pozostałych procesów
    log_message("INIT", "Wysyłam sygnały zakończenia do pozostałych procesów...");
    kill(pid_generator, SIGTERM);
    kill(pid_kasjer, SIGTERM);
    kill(pid_przewodnik1, SIGTERM);
    kill(pid_przewodnik2, SIGTERM);
    
    /// Czekanie na zakończenie wszystkich procesów
    sleep(2);
    
    /// Usunięcie ewentualnych procesów zwiedzających
    log_message("INIT", "Oczyszczanie procesów zwiedzających...");
    system("killall zwiedzajacy 2>/dev/null");
    
    sleep(2);
    
    /// Finalne czyszczenie zasobów IPC
    cleanup_resources();
    
    log_message("INIT", "=== KONIEC SYSTEMU JASKINIA ===");
    
    return 0;
}
