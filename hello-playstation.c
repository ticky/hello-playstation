#include <debug.h>
#include <sifrpc.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // Initialise the Remote Procedure Call handler,
  // so the Emotion Engine CPU can talk to the Input Output Processor
  SifInitRpc(0);

  // This initiates the debug screen space so we can print text to it
  init_scr();

  while (1) {
    // Print to the debug screen
    scr_printf("Hello, PlayStation 2! ");

    // Print to the debug/serial interface; only really useful on an emulator
    printf("Hello, PlayStation 2 ");
    sleep(1);
  }

  return 0;
}
