// The float coordinates used for the text drawing routines are off by 0.5f,
// I don't like that, so here I force them to be correct in this program
#define gsKit_fontm_print_scaled(gsGlobal, gsFontM, X, Y, Z, scale, colour, String) \
        gsKit_fontm_print_scaled(gsGlobal, gsFontM, 0.5f + X, 0.5f + Y, Z, scale, colour, String)
// Note that gsKit_fontm_print calls gsKit_fontm_print_scaled under the hood,
// so this adjusts both
