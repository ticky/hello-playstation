#include <debug.h>
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <sifrpc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gsKit.h>
#include <dmaKit.h>

#include "libpad.h"

GSGLOBAL *gsGlobal;
GSFONTM *gsFontM;

static char padBuf[256];
struct padButtonStatus buttons;
u32 paddata;
u32 old_pad = 0;
u32 new_pad;

u64 rgbaBlack = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
u64 rgbaPurple = GS_SETREG_RGBAQ(0x89, 0x3f, 0xf4, 0x00, 0x00);

u64 rgbaBlueFont = GS_SETREG_RGBAQ(0x20, 0x20, 0x80, 0x80, 0x00);
u64 rgbaWhiteTransparentFont = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x60, 0x00);
u64 rgbaWhiteFont = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);

enum operation_mode {
  MENU = 0,
  MODE_TEST,
  FONT_TEST,
  MODE_COUNT
};

enum operation_mode operation_mode = MENU;
enum operation_mode menu_index = 1;

void loadModules() {
    int ret;

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        printf("sifLoadModule sio failed: %d\n", ret);
        SleepThread();
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        printf("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }
}

void waitPadReady(int port) {
  /* Wait for request to complete. */
  while(padGetReqState(port, 0) != PAD_RSTAT_COMPLETE)
    gsKit_sync_flip(gsGlobal);

  /* Wait for pad to be stable. */
  while(padGetState(port, 0) != PAD_STATE_STABLE)
    gsKit_sync_flip(gsGlobal);
}

void mode_menu() {
  gsKit_fontm_print(gsGlobal, gsFontM,
                    13.0f, 13.0f, 1,
                    rgbaWhiteFont,
                    "Hello PlayStation");
  gsKit_fontm_print(gsGlobal, gsFontM,
                    13.0f, 52.0f, 1,
                    menu_index == MODE_TEST ? rgbaBlueFont : rgbaWhiteFont,
                    "Graphics Modes");
  gsKit_fontm_print(gsGlobal, gsFontM,
                    13.0f, 78.0f, 1,
                    menu_index == FONT_TEST ? rgbaBlueFont : rgbaWhiteFont,
                    "OSD Font Test");

  if(new_pad & PAD_UP) {
    if (menu_index == 1) {
      menu_index = MODE_COUNT - 1;
    } else {
      menu_index--;
    }
  } else if (new_pad & PAD_DOWN) {
    if (menu_index == MODE_COUNT - 1) {
      menu_index = 1;
    } else {
      menu_index++;
    }
  }

  if (new_pad & PAD_CROSS || new_pad & PAD_CIRCLE) {
    operation_mode = menu_index;
  }
}

void mode_mode_test() {
  // \f794 = ⇧
  char mode[128];

  snprintf((char *)mode, 128,
           "GS Mode #%d\n"
           "%s, %s, %s\n"
           "DrawField: %d\n"
           "Framebuffer: %dx%d\n"
           "Display: %dx%d\n",
           gsGlobal->Mode,
           gsGlobal->Aspect == GS_ASPECT_4_3 ? "4:3" : "16:9",
           gsGlobal->Interlace == GS_INTERLACED ? "Interlaced" : "Non-Interlaced",
           gsGlobal->Field == GS_FRAME ? "Frame" : "Field",
           gsGlobal->DrawField,
           gsGlobal->Width,
           gsGlobal->Height,
           gsGlobal->DW,
           gsGlobal->DH);

  // Draw some nice text using the BIOS font
  gsKit_fontm_print(gsGlobal, gsFontM,
                    15.0f, 22.0f, 1,
                    rgbaWhiteFont,
                    mode);

  // Triangle to exit
  if (new_pad & PAD_TRIANGLE) {
    operation_mode = MENU;
  }
}

void mode_font_test() {
  char sample[10];

  // BIOS font characters are stored as 26x26px
  // gsKit_fontm_print(gsGlobal, gsFontM, 0.0f, 0.0f, 1, rgbaBlueFont, "XX\nXX");
  // gsKit_fontm_print(gsGlobal, gsFontM, 26.0f, 26.0f, 1, rgbaWhiteFont, "X");

  // snprintf((char *)sample, 10,
  //          "W: %d H:%d", (*gsFontM->Texture)->Width, (*gsFontM->Texture)->Height);

  int charactersPerLine = gsGlobal->Width / 13;

  for (int i = 0; i < 800; ++i) {
    snprintf(sample, 10, "\f%d", i);
    gsKit_fontm_print_scaled(gsGlobal, gsFontM,
                             (i % charactersPerLine * 13.0f), (i / charactersPerLine * 13.0f), 1, 0.5f,
                             rgbaWhiteTransparentFont,
                             sample);
  }

  // Triangle to exit
  if (new_pad & PAD_TRIANGLE) {
    operation_mode = MENU;
  }
}

int main(int argc, char *argv[]) {
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
  // gsKit_prim_line(gsGlobal, 525.0f, 125.0f, 575.0f, 200.0f, 2, rgbaBlack);

  // Switch the draw buffer back to "ONESHOT" mode
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);

  // Initialise the Remote Procedure Call handler,
  // so the Emotion Engine CPU can talk to the Input Output Processor
  SifInitRpc(0);

  // This initiates the debug screen space so we can print text to it
  // init_scr();

  // Print to the debug/serial interface; only really useful on an emulator
  printf("Hello, PlayStation 2\n");

  loadModules();
  padInit(0);

  int ret;

  // port is 0 or 1, slot is for multitap
  if((ret = padPortOpen(0, 0, padBuf)) == 0) {
      printf("padOpenPort failed: %d\n", ret);
      SleepThread();
  }

  waitPadReady(0);

  while (1) {
    // enum operation_mode original_mode = operation_mode;

    ret = padGetState(0, 0);
    if ((ret == PAD_STATE_STABLE) || (ret == PAD_STATE_FINDCTP1)) {
      ret = padRead(0, 0, &buttons);

      if (ret != 0) {
        paddata = 0xffff ^ buttons.btns;
        new_pad = paddata & ~old_pad;
        old_pad = paddata;
      }
    }

    switch(operation_mode) {
      case MENU: {
        mode_menu();
        break;
      }

      case MODE_TEST: {
        mode_mode_test();
        break;
      }

      case FONT_TEST: {
        mode_font_test();
        break;
      }

      default: {
        printf("Unhandled operation_mode: %d\n", operation_mode);
        operation_mode = MENU;
        break;
      }
    }

    // if (original_mode != operation_mode) {
    //   // Reset the persistent graphics queue
    //   gsKit_queue_reset(GS_PERSISTENT);
    // }

    // Execute the draw buffer commands, and flip the framebuffer on vertical blank
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
    gsKit_TexManager_nextFrame(gsGlobal);
  }

  // (hypothetically) destroy the ROM font manager and GSKit so we're being kind about memory
  gsKit_free_fontm(gsGlobal, gsFontM);
  gsKit_deinit_global(gsGlobal);

  return 0;
}
