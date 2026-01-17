#include "common.h"

/// Punkt wejœcia do systemu
/// 1. Usuwa stare logi
/// 2. Uruchamia stra¿nika (który zajmuje siê reszt¹)
int main() {
    /// Czyœcimy logi z poprzednich uruchomieñ
    unlink("jaskinia_common.log");
    unlink("jaskinia_kasjer.log");
    unlink("jaskinia_przewodnik1.log");
    unlink("jaskinia_przewodnik2.log");
    unlink("jaskinia_generator.log");
    unlink("jaskinia_zwiedzajacy.log");

    /// Zamieñ siê w stra¿nika - exec() zastêpuje ten proces
    execl("./straznik", "straznik", NULL);
    perror("execl straznik");  /// To siê wykona tylko jeœli exec failed
    return 1;
}