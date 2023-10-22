#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// https://docs.kernel.org/input/uinput.html
#include <linux/uinput.h>

#include "parser.h"

// keys and abss are key codes that this virtual controller is allowed to emit

const int keys[] = {
	BTN_NORTH,
	BTN_SOUTH,
	BTN_EAST,
	BTN_WEST,
	BTN_TR,
};

const int abss[] = {
	ABS_HAT0X,
	ABS_HAT0Y,
};

#define NAME "nh-virtual-controller"
#define VENDOR 0x4E48
#define PRODUCT 0x7663

static volatile sig_atomic_t stop = 0;
static void interrupt_handler(int sig) {
	(void) sig;
	stop = 1;
}

void emit(int fd, int type, int code, int val) {
	struct input_event ie;
	ie.type = type;
	ie.code = code;
	ie.value = val;
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	ssize_t put = write(fd, &ie, sizeof ie);
	if (put != sizeof ie) {
		printf("failed to write\n");
		stop = 1;
		return;
	}
}

#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3
_Bool holding[4] = {0, 0, 0, 0};
void handle_code(int to_fd, int in_code, int val) {
	if (val == 2) {
		// key is being held
		return;
	}
	int opposite_direction;
	const int analog_press = 32767;
	int otype, ocode, oval;
	// TODO: let user define joystick controlls in keys.conf
	// This is impossible currently because some extra checks need to be
	// done when receiving an un-hold value for an analog output.
	switch (in_code) {
	case KEY_LEFT:
		otype = EV_ABS;	ocode = ABS_HAT0X; oval = -analog_press;
		holding[WEST] = val;
		opposite_direction = EAST;
		break;
	case KEY_RIGHT:
		otype = EV_ABS; ocode = ABS_HAT0X; oval = +analog_press;
		holding[EAST] = val;
		opposite_direction = WEST;
		break;
	case KEY_UP:
		otype = EV_ABS; ocode = ABS_HAT0Y; oval = -analog_press;
		holding[NORTH] = val;
		opposite_direction = SOUTH;
		break;
	case KEY_DOWN:
		otype = EV_ABS; ocode = ABS_HAT0Y; oval = +analog_press;
		holding[SOUTH] = val;
		opposite_direction = NORTH;
		break;
	default:
		if (
			in_code > (int) (sizeof keymap / sizeof *keymap)
			|| keymap[in_code].code == 0
		) {
			return;
		}
		otype = keymap[in_code].type;
		ocode = keymap[in_code].code;
		oval = keymap[in_code].value;
	}
	if (val == 0) {
		if (otype == EV_ABS && holding[opposite_direction]) {
			oval = -oval;
		} else {
			oval = 0;
		}
	}
	emit(to_fd, otype, ocode, oval);
	emit(to_fd, EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv) {
	struct sigaction sa;
	sa.sa_handler = interrupt_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("%s: failed to establish signal handler: %s\n",
		       argv[0], strerror(errno));
		return 1;
	}

	char *key_binds_path = "keys.conf";
	printf("reading keybinds from %s\n", key_binds_path);
	int err = parse(key_binds_path);
	if (err) {
		printf("failed to parse keybinds\n");
		return 1;
	}

	int uinput_fd = open("/dev/uinput", O_WRONLY|O_NONBLOCK);
	if (uinput_fd == -1) {
		printf("%s: failed to open /dev/uinput: %s\n",
		       argv[0], strerror(errno));
		return errno;
	}

	int keyboard_fd = STDIN_FILENO;
	char *input_file_name = "stdin";
	if (argc >= 2) {
		if (argc > 2) {
			printf("%s: expected one argument; ignoring the rest\n",
			       argv[0]);
		}
		keyboard_fd = open(argv[1], O_RDONLY);
		if (keyboard_fd == -1) {
			close(uinput_fd);
			printf("%s: failed to open %s: %s\n",
			       argv[0], argv[1], strerror(errno));
			return errno;
		}
		input_file_name = argv[1];
	}
	printf("%s: reading input from %s\n", argv[0], input_file_name);

	if (
		(err = ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY))
		|| (err = ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS))
	) {
		printf("%s: failed to set evbits: %s\n",
		        argv[0], strerror(errno));
		goto cleanup;
	}

	for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
		err = ioctl(uinput_fd, UI_SET_KEYBIT, keys[i]);
		if (err) {
			printf("%s: failed to set keys: %s\n",
			       argv[0], strerror(errno));
			goto cleanup;
		}
	}
	for (size_t i = 0; i < sizeof abss / sizeof *abss; i++) {
		err = ioctl(uinput_fd, UI_SET_ABSBIT, abss[i]);
		if (err) {
			printf("%s: failed to set abss: %s\n",
			       argv[0], strerror(errno));
			goto cleanup;
		}
	}

	struct uinput_setup usetup;
	memset(&usetup, 0, sizeof usetup);
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = VENDOR;
	usetup.id.product = PRODUCT;
	static_assert(sizeof NAME <= sizeof usetup.name);
	memcpy(&usetup.name, NAME, sizeof NAME);
	if (
		(err = ioctl(uinput_fd, UI_DEV_SETUP, &usetup))
		|| (err = ioctl(uinput_fd, UI_DEV_CREATE))
	) {
		printf("%s: failed to create device: %s\n",
		       argv[0], strerror(errno));
		goto cleanup;
	}

	while (!stop) {
		struct input_event ie[64];
		ssize_t got = read(keyboard_fd, ie, sizeof ie);
		const ssize_t want = sizeof(struct input_event);
		if (got < want) {
			if (!stop) {
				printf("%s: read %ld bytes, expected at least"
				       "%ld\n", argv[0], got, want);
				err = 1;
			}
			break;
		}
		_Bool find_syn_report = 0;
		for (size_t i = 0; i < got / sizeof *ie; i++) {
			if (ie[i].type == EV_SYN) {
				switch (ie[i].code) {
				case SYN_REPORT:
					find_syn_report = 0;
					break;
				case SYN_DROPPED:
					find_syn_report = 1;
					break;
				}
			} else if (!find_syn_report && ie[i].type == EV_KEY) {
				handle_code(uinput_fd, ie[i].code, ie[i].value);
			}
		}
	}

	printf("\nshutting down\n");

	ioctl(uinput_fd, UI_DEV_DESTROY);
cleanup:
	close(uinput_fd);
	close(keyboard_fd);
	return err;
}
