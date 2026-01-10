#include "common.h"

int main() {
    unlink("jaskinia_common.log");
    unlink("jaskinia_kasjer.log");
    unlink("jaskinia_przewodnik1.log");
    unlink("jaskinia_przewodnik2.log");
    unlink("jaskinia_generator.log");
    unlink("jaskinia_zwiedzajacy.log");

    execl("./straznik", "straznik", NULL);
    perror("execl straznik");
    return 1;
}