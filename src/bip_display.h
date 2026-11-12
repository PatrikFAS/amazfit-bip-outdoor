#ifndef BIP_DISPLAY_H
#define BIP_DISPLAY_H

#include <libbip.h>

/* navi_demo sets these flags when registering its screen. Reasserting
 * them on the app timer kept Navigator open on the original Bip. */
static inline void bip_keep_screen_open(void)
{
    set_display_state_value(8, 1);
    set_display_state_value(2, 1);
}

#endif
