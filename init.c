#include "common.h"

/// ================= GLOBALNE ZMIENNE =================
/// ID zestawu semaforów – globalne, żeby cleanup miał do nich dostęp
int semid = -1;

/// ================= OPERACJE SEMAFOROWE =================

/// Funkcja blokująca – czeka aż semafor będzie dostępny
void sem_wait(int semid, int sem_num) {
    struct sembuf op = { sem_num, -1, 0 };

    if (semop(semid, &op, 1) == -1) {
        perror("sem_wait");
        exit(1);
    }
}

/// Funkcja odblokowująca – zwiększa wartość semafora
void sem_signal(int semid, int sem_num) {
    struct sembuf op = { sem_num, 1, 0 };

    if (semop(semid, &op, 1) == -1) {
        perror("sem_signal");
        exit(1);
    }
}

/// Próba opuszczenia semafora bez blokowania
/// Zwraca -1, jeśli się nie da (EAGAIN)
int sem_trywait(int semid, int sem_num) {
    struct sembuf op = { sem_num, -1, IPC_NOWAIT };

    if (semop(semid, &op, 1) == -1) {
        if (errno == EAGAIN)
            return -1;

        perror("sem_trywait");
        exit(1);
    }
    return 0;
}

/// Ustawia początkową wartość konkretnego semafora
void sem_init_value(int semid, int sem_num, int value) {
    union semun arg;
    arg.val = value;

    if (semctl(semid, sem_num, SETVAL, arg) == -1) {
        perror("sem_init_value");
        exit(1);
    }
}

/// ================= CLEANUP =================
/// Usuwa zestaw semaforów z systemu
void cleanup(void) {
    if (semid != -1) {
        semctl(semid, 0, IPC_RMID);
        printf("INIT: semafory usunięte\n");
    }
}

/// ================= OBSŁUGA SIGINT =================
/// Reakcja na Ctrl+C – sprzątanie i wyjście
void sigint_handler(int sig) {
    (void)sig;

    printf("\nINIT: SIGINT – sprzątanie i koniec\n");
    cleanup();
    exit(0);
}

/// ================= MAIN =================
/// Program init istnieje aktualnie tylko po to,
/// żeby stworzyć semafory i trzymać je przy życiu
int main(void) {
    /// Rejestracja obsługi Ctrl+C
    signal(SIGINT, sigint_handler);

    /// Tworzenie zestawu semaforów
    semid = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    /// Inicjalizacja wartości semaforów
    sem_init_value(semid, SEM_MUTEX_KLADKA, 1);
    sem_init_value(semid, SEM_MIEJSCA_KLADKA, K);
    sem_init_value(semid, SEM_MUTEX_TRASA1, 1);
    sem_init_value(semid, SEM_MUTEX_TRASA2, 1);

    printf("INIT: semafory utworzone i gotowe\n");
    printf("INIT: proces żyje – Ctrl+C kończy\n");

    /// INIT śpi, ale trzyma semafory w systemie
    while (1)
        pause();

    return 0;
}
