#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef VERSION
#define VERSION "1.0"
#endif

#include <bcl.h>

#define PIN_DATA BC_GPIO_P17
#define PIN_VCC BC_GPIO_P16
#define PIN_GND BC_GPIO_P15
#define SYMBOL 640
#define CMD_UP 0x2
#define CMD_STOP 0x1
#define CMD_DOWN 0x4
#define CMD_PROG 0x8

typedef enum
{
    SHUTTER_ID1 = 0x121301,
    SHUTTER_ID2 = 0x121302,
    SHUTTER_ID3 = 0x121303,
    SHUTTER_ID4 = 0x121304,
    SHUTTER_ID5 = 0x121305
} shutter_id_type;

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

void somfy_cmd(uint64_t *id, const char *topic, void *value, void *param);

// subscribe table, format: topic, expect payload type, callback, user param
static const bc_radio_sub_t subs[] = {
    {"somfy/s1/cmd/up", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID1},
    {"somfy/s1/cmd/stop", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID1},
    {"somfy/s1/cmd/down", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID1},
    {"somfy/s1/cmd/prog", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID1},

    {"somfy/s2/cmd/up", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID2},
    {"somfy/s2/cmd/stop", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID2},
    {"somfy/s2/cmd/down", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID2},
    {"somfy/s2/cmd/prog", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID2},

    {"somfy/s3/cmd/up", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID3},
    {"somfy/s3/cmd/stop", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID3},
    {"somfy/s3/cmd/down", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID3},
    {"somfy/s3/cmd/prog", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID3},

    {"somfy/s4/cmd/up", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID4},
    {"somfy/s4/cmd/stop", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID4},
    {"somfy/s4/cmd/down", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID4},
    {"somfy/s4/cmd/prog", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID4},

    {"somfy/s5/cmd/up", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID5},
    {"somfy/s5/cmd/stop", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID5},
    {"somfy/s5/cmd/down", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID5},
    {"somfy/s5/cmd/prog", BC_RADIO_SUB_PT_INT, somfy_cmd, (void *) SHUTTER_ID5}
};

#endif // _APPLICATION_H
