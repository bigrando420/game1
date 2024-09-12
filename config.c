

#if CONFIGURATION == RELEASE
#define LOOP_SOUND
#endif

#if CONFIGURATION == DEBUG

// we can turn on/off whatever we want in here, while having the release override always be valid.
#define DEV_TESTING
// #define LOOP_SOUND
#define DISABLE_O2

#endif