#ifndef _INPUT_H
#define _INPUT_H


#include <sys/time.h>
#include <sys/types.h>
#include "standard-headers/linux/types.h"

#include "standard-headers/linux/input-event-codes.h"


struct input_event {
#if (HOST_LONG_BITS != 32 || !defined(__USE_TIME_BITS64)) && !defined(__KERNEL__)
	struct timeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#else
	unsigned long __sec;
#if defined(__sparc__) && defined(__arch64__)
	unsigned int __usec;
	unsigned int __pad;
#else
	unsigned long __usec;
#endif
#define input_event_sec  __sec
#define input_event_usec __usec
#endif
	uint16_t type;
	uint16_t code;
	int32_t value;
};


#define EV_VERSION		0x010001


struct input_id {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
};

struct input_absinfo {
	int32_t value;
	int32_t minimum;
	int32_t maximum;
	int32_t fuzz;
	int32_t flat;
	int32_t resolution;
};

struct input_keymap_entry {
#define INPUT_KEYMAP_BY_INDEX	(1 << 0)
	uint8_t  flags;
	uint8_t  len;
	uint16_t index;
	uint32_t keycode;
	uint8_t  scancode[32];
};

struct input_mask {
	uint32_t type;
	uint32_t codes_size;
	uint64_t codes_ptr;
};

#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */
#define EVIOCGID		_IOR('E', 0x02, struct input_id)	/* get device ID */
#define EVIOCGREP		_IOR('E', 0x03, unsigned int[2])	/* get repeat settings */
#define EVIOCSREP		_IOW('E', 0x03, unsigned int[2])	/* set repeat settings */

#define EVIOCGKEYCODE		_IOR('E', 0x04, unsigned int[2])        /* get keycode */
#define EVIOCGKEYCODE_V2	_IOR('E', 0x04, struct input_keymap_entry)
#define EVIOCSKEYCODE		_IOW('E', 0x04, unsigned int[2])        /* set keycode */
#define EVIOCSKEYCODE_V2	_IOW('E', 0x04, struct input_keymap_entry)

#define EVIOCGNAME(len)		_IOC(_IOC_READ, 'E', 0x06, len)		/* get device name */
#define EVIOCGPHYS(len)		_IOC(_IOC_READ, 'E', 0x07, len)		/* get physical location */
#define EVIOCGUNIQ(len)		_IOC(_IOC_READ, 'E', 0x08, len)		/* get unique identifier */
#define EVIOCGPROP(len)		_IOC(_IOC_READ, 'E', 0x09, len)		/* get device properties */

#define EVIOCGMTSLOTS(len)	_IOC(_IOC_READ, 'E', 0x0a, len)

#define EVIOCGKEY(len)		_IOC(_IOC_READ, 'E', 0x18, len)		/* get global key state */
#define EVIOCGLED(len)		_IOC(_IOC_READ, 'E', 0x19, len)		/* get all LEDs */
#define EVIOCGSND(len)		_IOC(_IOC_READ, 'E', 0x1a, len)		/* get all sounds status */
#define EVIOCGSW(len)		_IOC(_IOC_READ, 'E', 0x1b, len)		/* get all switch states */

#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + (ev), len)	/* get event bits */
#define EVIOCGABS(abs)		_IOR('E', 0x40 + (abs), struct input_absinfo)	/* get abs value/limits */
#define EVIOCSABS(abs)		_IOW('E', 0xc0 + (abs), struct input_absinfo)	/* set abs value/limits */

#define EVIOCSFF		_IOW('E', 0x80, struct ff_effect)	/* send a force effect to a force feedback device */
#define EVIOCRMFF		_IOW('E', 0x81, int)			/* Erase a force effect */
#define EVIOCGEFFECTS		_IOR('E', 0x84, int)			/* Report number of effects playable at the same time */

#define EVIOCGRAB		_IOW('E', 0x90, int)			/* Grab/Release device */
#define EVIOCREVOKE		_IOW('E', 0x91, int)			/* Revoke device access */

#define EVIOCGMASK		_IOR('E', 0x92, struct input_mask)	/* Get event-masks */

#define EVIOCSMASK		_IOW('E', 0x93, struct input_mask)	/* Set event-masks */

#define EVIOCSCLOCKID		_IOW('E', 0xa0, int)			/* Set clockid to be used for timestamps */


#define ID_BUS			0
#define ID_VENDOR		1
#define ID_PRODUCT		2
#define ID_VERSION		3

#define BUS_PCI			0x01
#define BUS_ISAPNP		0x02
#define BUS_USB			0x03
#define BUS_HIL			0x04
#define BUS_BLUETOOTH		0x05
#define BUS_VIRTUAL		0x06

#define BUS_ISA			0x10
#define BUS_I8042		0x11
#define BUS_XTKBD		0x12
#define BUS_RS232		0x13
#define BUS_GAMEPORT		0x14
#define BUS_PARPORT		0x15
#define BUS_AMIGA		0x16
#define BUS_ADB			0x17
#define BUS_I2C			0x18
#define BUS_HOST		0x19
#define BUS_GSC			0x1A
#define BUS_ATARI		0x1B
#define BUS_SPI			0x1C
#define BUS_RMI			0x1D
#define BUS_CEC			0x1E
#define BUS_INTEL_ISHTP		0x1F
#define BUS_AMD_SFH		0x20

#define MT_TOOL_FINGER		0x00
#define MT_TOOL_PEN		0x01
#define MT_TOOL_PALM		0x02
#define MT_TOOL_DIAL		0x0a
#define MT_TOOL_MAX		0x0f

#define FF_STATUS_STOPPED	0x00
#define FF_STATUS_PLAYING	0x01
#define FF_STATUS_MAX		0x01



struct ff_replay {
	uint16_t length;
	uint16_t delay;
};

struct ff_trigger {
	uint16_t button;
	uint16_t interval;
};

struct ff_envelope {
	uint16_t attack_length;
	uint16_t attack_level;
	uint16_t fade_length;
	uint16_t fade_level;
};

struct ff_constant_effect {
	int16_t level;
	struct ff_envelope envelope;
};

struct ff_ramp_effect {
	int16_t start_level;
	int16_t end_level;
	struct ff_envelope envelope;
};

struct ff_condition_effect {
	uint16_t right_saturation;
	uint16_t left_saturation;

	int16_t right_coeff;
	int16_t left_coeff;

	uint16_t deadband;
	int16_t center;
};

struct ff_periodic_effect {
	uint16_t waveform;
	uint16_t period;
	int16_t magnitude;
	int16_t offset;
	uint16_t phase;

	struct ff_envelope envelope;

	uint32_t custom_len;
	int16_t *custom_data;
};

struct ff_rumble_effect {
	uint16_t strong_magnitude;
	uint16_t weak_magnitude;
};

struct ff_effect {
	uint16_t type;
	int16_t id;
	uint16_t direction;
	struct ff_trigger trigger;
	struct ff_replay replay;

	union {
		struct ff_constant_effect constant;
		struct ff_ramp_effect ramp;
		struct ff_periodic_effect periodic;
		struct ff_condition_effect condition[2]; /* One for each axis */
		struct ff_rumble_effect rumble;
	} u;
};


#define FF_RUMBLE	0x50
#define FF_PERIODIC	0x51
#define FF_CONSTANT	0x52
#define FF_SPRING	0x53
#define FF_FRICTION	0x54
#define FF_DAMPER	0x55
#define FF_INERTIA	0x56
#define FF_RAMP		0x57

#define FF_EFFECT_MIN	FF_RUMBLE
#define FF_EFFECT_MAX	FF_RAMP


#define FF_SQUARE	0x58
#define FF_TRIANGLE	0x59
#define FF_SINE		0x5a
#define FF_SAW_UP	0x5b
#define FF_SAW_DOWN	0x5c
#define FF_CUSTOM	0x5d

#define FF_WAVEFORM_MIN	FF_SQUARE
#define FF_WAVEFORM_MAX	FF_CUSTOM


#define FF_GAIN		0x60
#define FF_AUTOCENTER	0x61

#define FF_MAX_EFFECTS	FF_GAIN

#define FF_MAX		0x7f
#define FF_CNT		(FF_MAX+1)

#endif /* _INPUT_H */
