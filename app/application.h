#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>

typedef enum
{
    STATE_BEGIN = 0,
    STATE_FIRSTFRAME1 = 1,
    STATE_FIRSTFRAME2 = 2,
    STATE_HWSYNC = 3,
    STATE_SWSYNC1 = 4,
    STATE_SWSYNC2 = 5,
    STATE_DATA = 6,
    STATE_LAST = 7,
    STATE_END = 8
} state_type;

#endif // _APPLICATION_H
