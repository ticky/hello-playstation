#include <debug.h>
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dmaKit.h>
#include <gsKit.h>
#include <osd_config.h>

#include "libpad.h"

#include "gskit-coordinate-hack.h"

GSGLOBAL *gsGlobal;
GSFONTM *gsFontM;

// BIOS font characters are stored as 26x26px
// gsKit_fontm_print(gsGlobal, gsFontM, 0.0f, 0.0f, 1, rgbaBlueFont, "XX\nXX");
// gsKit_fontm_print(gsGlobal, gsFontM, 26.0f, 26.0f, 1, rgbaWhiteFont, "X");
const float gsFontSize = 26.0f;

static char padBuf[256];
struct padButtonStatus buttons;
u32 paddata;
u32 old_pad = 0;
u32 new_pad;

u64 rgbaBlack = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
u64 rgbaPurple = GS_SETREG_RGBAQ(0x89, 0x3f, 0xf4, 0x00, 0x00);
u64 rgbaOSDPurple = GS_SETREG_RGBAQ(0x3f, 0x33, 0x33, 0x80, 0x00);

u64 rgbaBlueFont = GS_SETREG_RGBAQ(0x20, 0x20, 0x80, 0x80, 0x00);
u64 rgbaWhiteTransparentFont = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x60, 0x00);
u64 rgbaWhiteFont = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);

char romver[16];
static bool is_running = true;
static bool confirm_is_circle = true;

enum operation_mode {
  MENU = 0,
  GRAPHICS_MODES,
  FONT_INFO,
  FONT_REPERTOIRE,
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
  while (padGetReqState(port, 0) != PAD_RSTAT_COMPLETE)
    gsKit_sync_flip(gsGlobal);

  /* Wait for pad to be stable. */
  while (padGetState(port, 0) != PAD_STATE_STABLE)
    gsKit_sync_flip(gsGlobal);
}

void mode_menu() {
  gsFontM->Align = GSKIT_FALIGN_CENTER;

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsGlobal->Width / 2.0f, gsFontSize * 0.5f, 1,
                    rgbaWhiteFont,
                    "Hello PlayStation");

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsGlobal->Width / 2.0f, gsFontSize * 3.0f, 1,
                    menu_index == GRAPHICS_MODES ? rgbaBlueFont : rgbaWhiteFont,
                    "Graphics Modes");
  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsGlobal->Width / 2.0f, gsFontSize * 4.0f, 1,
                    menu_index == FONT_INFO ? rgbaBlueFont : rgbaWhiteFont,
                    "OSD Font Info");
  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsGlobal->Width / 2.0f, gsFontSize * 5.0f, 1,
                    menu_index == FONT_REPERTOIRE ? rgbaBlueFont : rgbaWhiteFont,
                    "Font Repertoire");

  gsFontM->Align = GSKIT_FALIGN_LEFT;

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsFontSize * 8.5f, (gsGlobal->Height - gsFontSize * 1.5f), 1,
                    rgbaWhiteFont,
                    confirm_is_circle ? "\f0090 Enter" : "\f0062 Enter");

  if (new_pad & PAD_UP) {
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

  if (new_pad & PAD_SELECT) {
    is_running = false;
  }
}

void mode_graphics_modes() {
  // EXTREMELY basic graphics mode switching
  // Height values are those used in `gsKit_init_global`
  if (new_pad & PAD_SQUARE) {
    switch (gsGlobal->Mode) {
      case GS_MODE_PAL: {
        gsGlobal->Mode = GS_MODE_NTSC;
        gsGlobal->Height = 448;
        gsKit_vram_clear(gsGlobal);
        gsKit_init_screen(gsGlobal);
        break;
      }

      case GS_MODE_NTSC: {
        gsGlobal->Mode = GS_MODE_PAL;
        gsGlobal->Height = 512;
        gsKit_vram_clear(gsGlobal);
        gsKit_init_screen(gsGlobal);
        break;
      }
    }
  }

  char mode[128];

  snprintf((char *)mode, 128,
           "GS Mode #%d\n"
           "%s, %s, %s\n"
           "DrawField:   %d\n"
           "Framebuffer: %d\f0062%d\n"
           "Display:     %d\f0062%d\n",
           gsGlobal->Mode,
           gsGlobal->Aspect == GS_ASPECT_4_3 ? "4:3" : "16:9",
           gsGlobal->Interlace == GS_INTERLACED ? "Interlaced" : "Non-Interlaced",
           gsGlobal->Field == GS_FRAME ? "Frame" : "Field",
           gsGlobal->DrawField,
           gsGlobal->Width,
           gsGlobal->Height,
           gsGlobal->DW,
           gsGlobal->DH);

  // Draw a checkerboard pattern across the the frame buffer
  for (int i = 0; i < gsGlobal->Width - gsGlobal->Height; i += 2) {
    gsKit_prim_line(gsGlobal, i + 0.0f, 0.0f, i + gsGlobal->Height * 1.0f, gsGlobal->Height * 1.0f, 2, rgbaBlack);
  }

  // Draw some nice text using the BIOS font
  gsKit_fontm_print(gsGlobal, gsFontM,
                    15.5f, 22.5f, 2,
                    rgbaWhiteFont,
                    mode);

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsFontSize * 0.5f, (gsGlobal->Height - gsFontSize * 1.5f), 2,
                    rgbaWhiteFont,
                    "\f0095 Switch Mode  \f0097 Return");

  // Triangle to exit
  if (new_pad & PAD_TRIANGLE) {
    operation_mode = MENU;
  }
}

void mode_font_info() {
  char info[256];

  snprintf((char *)info, 256,
           "OSD Font Info\n"
           "ROM Version: %s\n"
           "Entries:     %d\n",
           strlen(romver) == 0 ? "Invalid" : romver,
           gsFontM->Header.num_entries);

  gsKit_fontm_print(gsGlobal, gsFontM,
                    15.0f, 22.0f, 1,
                    rgbaWhiteFont,
                    info);

  for (int i = 0; i < GS_FONTM_PAGE_COUNT; ++i) {
    GSTEXTURE fontTexture = *gsFontM->Texture[i];

    snprintf((char *)info, 256,
             "Page %d: %dx%d, Px. Fmts: %#02x, %#02x\n",
             i,
             fontTexture.Width,
             fontTexture.Height,
             fontTexture.PSM,
             fontTexture.ClutPSM);

    gsKit_fontm_print_scaled(gsGlobal, gsFontM,
                             15.0f, 22.0f + gsFontSize * 3 + i * gsFontSize * 0.6f, 1, 0.6f,
                             rgbaWhiteFont,
                             info);
  }

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsFontSize * 7.5f, (gsGlobal->Height - gsFontSize * 1.5f), 1,
                    rgbaWhiteFont,
                    "\f0097 Return");

  // Triangle to exit
  if (new_pad & PAD_TRIANGLE) {
    operation_mode = MENU;
  }
}

int font_repertoire_offset = 0;

void mode_font_repertoire() {
  char sample[6];

  // snprintf((char *)sample, 10,
  //          "W: %d H:%d", (*gsFontM->Texture)->Width, (*gsFontM->Texture)->Height);

  // Minimum is 0.35f, any lower and it'll crash
  float charScale = 0.5f;
  float charSize = charScale * gsFontSize;

  int charactersPerLine = (gsGlobal->Width / charSize) - 5;
  int linesPerScreen = (gsGlobal->Height - (gsFontSize * 2)) / charSize;

  for (int i = 0; i < charactersPerLine * linesPerScreen && (i + font_repertoire_offset) < gsFontM->Header.num_entries && i < 10000; ++i) {
    if (((i + font_repertoire_offset) % charactersPerLine * charSize) == 0) {
      snprintf(sample, 6, "%04d", (i + font_repertoire_offset));
      gsKit_fontm_print_scaled(gsGlobal, gsFontM, 0, (i / charactersPerLine * charSize), 1, charScale, rgbaWhiteTransparentFont, sample);
    }
    snprintf(sample, 6, "\f%04d", (i + font_repertoire_offset));
    gsKit_fontm_print_scaled(gsGlobal, gsFontM,
                             5 * charSize + (i % charactersPerLine * charSize), (i / charactersPerLine * charSize), 1, charScale,
                             rgbaWhiteFont,
                             sample);
  }

  gsKit_fontm_print(gsGlobal, gsFontM,
                    gsFontSize * 0.5f, (gsGlobal->Height - gsFontSize * 1.5f), 1,
                    rgbaWhiteFont,
                    "\f0798\f0799 Change Page\t\f0097 Return");

  if (new_pad & PAD_UP) {
    if (font_repertoire_offset >= charactersPerLine * linesPerScreen) {
      font_repertoire_offset -= charactersPerLine * linesPerScreen;
    }
  } else if (new_pad & PAD_DOWN) {
    if (font_repertoire_offset < gsFontM->Header.num_entries / charactersPerLine * linesPerScreen) {
      font_repertoire_offset += charactersPerLine * linesPerScreen;
    }
  }

  // Triangle to exit
  if (new_pad & PAD_TRIANGLE) {
    font_repertoire_offset = 0;
    operation_mode = MENU;
  }
}

int main(int argc, char *argv[]) {
  // Get the ROM name
  GetRomName(romver);

  // Configure which button we display as the "confirm" button
  // We accept both, Gran Turismo style, but advertise by the console region
  if (strlen(romver) > 0 && (romver[4] == 'E' || romver[4] == 'A' || romver[4] == 'H')) {
    confirm_is_circle = false;
  }

  // Set up GSKit so we can do fancy graphics
  gsGlobal = gsKit_init_global();
  // gsGlobal->Mode = GS_MODE_PAL;

  // Set up the GSKit ROM font
  gsFontM = gsKit_init_fontm();

  // Set up the DMA Controller's "GIF" channel so the Emotion Engine can talk to
  // the Graphics Synthesiser
  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
              D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
  dmaKit_chan_init(DMA_CHANNEL_GIF);
  // TODO: Can I work out what the rest of those parameters mean?

  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

  // Initialise the screen
  gsKit_init_screen(gsGlobal);

  // Upload the ROM font to the Graphics Synthesiser
  gsKit_fontm_upload(gsGlobal, gsFontM);

  // Set GSKit's draw buffer (list of drawing commands) to "ONESHOT" mode
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
  if ((ret = padPortOpen(0, 0, padBuf)) == 0) {
    printf("padOpenPort failed: %d\n", ret);
    SleepThread();
  }

  waitPadReady(0);

  while (is_running) {
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

    // For some reason we can't clear the GS_PERSISTENT queue,
    // so just do this every frame instead
    gsKit_clear(gsGlobal, rgbaPurple);

    switch(operation_mode) {
      case MENU: {
        mode_menu();
        break;
      }

      case GRAPHICS_MODES: {
        mode_graphics_modes();
        break;
      }

      case FONT_INFO: {
        mode_font_info();
        break;
      }

      case FONT_REPERTOIRE: {
        mode_font_repertoire();
        break;
      }

      default: {
        printf("Unhandled operation_mode: %d\n", operation_mode);
        operation_mode = MENU;
        break;
      }
    }

    // Execute the draw buffer commands, and flip the framebuffer on vblank
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
    gsKit_TexManager_nextFrame(gsGlobal);
  }

  // destroy the ROM font manager and GSKit so we're being kind about memory
  gsKit_free_fontm(gsGlobal, gsFontM);
  gsKit_deinit_global(gsGlobal);

  Exit(0);

  return 0;
}
