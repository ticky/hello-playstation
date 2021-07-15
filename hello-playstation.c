#include <debug.h>
#include <sifrpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gsKit.h>
#include <dmaKit.h>

GSGLOBAL *gsGlobal;
GSFONTM *gsFontM;

u64 rgbaBlack = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
u64 rgbaPurple = GS_SETREG_RGBAQ(0x89, 0x3f, 0xf4, 0x00, 0x00);

u64 rgbaWhiteFont = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);

int main(int argc, char *argv[]) {
  char mode[128];
  int frame = 0;

  // Set up GSKit so we can do fancy graphics
  gsGlobal = gsKit_init_global();
  // gsGlobal->Mode = GS_MODE_PAL;

  // Set up the GSKit ROM font
  gsFontM = gsKit_init_fontm();

  // Set up the DMA Controller's "GIF" channel so the Emotion Engine can talk to the Graphics Synthesiser
  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
              D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
  dmaKit_chan_init(DMA_CHANNEL_GIF);
  // TODO: Can I work out what the rest of those parameters mean?

  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

  // Initialise the screen
  gsKit_init_screen(gsGlobal);

  // Upload the ROM font to the Graphics Synthesiser
  gsKit_fontm_upload(gsGlobal, gsFontM);

  // Set GSKit's draw buffer (list of drawing commands) to "PERSISTENT" mode
  gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

  // Clear the graphics context with a nice purple
  gsKit_clear(gsGlobal, rgbaPurple);

  // Draw a single line primitive
  gsKit_prim_line(gsGlobal, 525.0f, 125.0f, 575.0f, 200.0f, 2, rgbaBlack);

  // A "Line Strip" is passed an array(ish) of float pairs denoting points
  float *LineStrip;
  float *LineStripPtr;

  LineStripPtr = LineStrip = malloc(12 * sizeof(float)); 
  *LineStrip++ =  75; // Point 1 X
  *LineStrip++ = 250; // Point 1 Y
  *LineStrip++ = 125; // Point 2 X
  *LineStrip++ = 290; // Point 2 Y
  *LineStrip++ = 100; // Point 3 X
  *LineStrip++ = 350; // Point 3 Y
  *LineStrip++ =  50; // Point 4 X
  *LineStrip++ = 350; // Point 4 Y
  *LineStrip++ =  25; // Point 6 X
  *LineStrip++ = 290; // Point 6 X
  *LineStrip++ =  75; // Point 6 Y
  *LineStrip++ = 250; // Point 6 Y
  gsKit_prim_line_strip(gsGlobal, LineStripPtr, 6, 2, rgbaBlack);

  // Switch the draw buffer back to "ONESHOT" mode
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);

  // Initialise the Remote Procedure Call handler,
  // so the Emotion Engine CPU can talk to the Input Output Processor
  // SifInitRpc(0);

  // This initiates the debug screen space so we can print text to it
  // init_scr();

  while (1) {
    // Draw some nice text using the BIOS font
    gsKit_fontm_print(gsGlobal, gsFontM, 15.0f, 22.0f, 1, rgbaWhiteFont, "Hello, PlayStation 2!");

    gsKit_prim_line(gsGlobal, 0.0f, 0.0f, 575.0f, 200.0f, 2, rgbaBlack);

    // Print to the debug/serial interface; only really useful on an emulator
    // printf("Hello, PlayStation 2 ");

    // Execute the draw buffer commands, and flip the framebuffer on vertical blank
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
    gsKit_TexManager_nextFrame(gsGlobal);

    frame++;
  }

  gsKit_free_fontm(gsGlobal, gsFontM);

  // (hypothetically) destroy GSKit so we're being kind about memory
  gsKit_deinit_global(gsGlobal);

  return 0;
}
