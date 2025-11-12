/*
 * If you are including the plugin into your code using static build, you
 * can simplify it by just including this file, which will include all the
 * related code in one step without you having to get involved in the detail.
 */

#define LWS_PLUGIN_STATIC

#include "../crypto/chacha.c"
#include "../crypto/ed25519.c"
#include "../crypto/fe25519.c"
#include "../crypto/ge25519.c"
#include "../crypto/poly1305.c"
#include "../crypto/sc25519.c"
#include "../crypto/smult_curve25519_ref.c"
#include "../kex-25519.c"
#include "../sshd.c"
#include "../telnet.c"
