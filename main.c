#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// https://docs.kernel.org/input/uinput.html
#include <linux/uinput.h>

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

void handle_code(int to_fd, int code, int val) {
	if (val == 2) {
		// key is being held
		return;
	}
	const int pressed = 32767;
	int otype, ocode, oval;
	switch (code) {
	case KEY_LEFT:
		otype = EV_ABS,	ocode = ABS_HAT0X, oval = -pressed;
		break;
	case KEY_RIGHT:
		otype = EV_ABS, ocode = ABS_HAT0X, oval = +pressed;
		break;
	case KEY_UP:
		otype = EV_ABS, ocode = ABS_HAT0Y, oval = -pressed;
		break;
	case KEY_DOWN:
		otype = EV_ABS, ocode = ABS_HAT0Y, oval = +pressed;
		break;
	case KEY_LEFTSHIFT:
		otype = EV_KEY, ocode = BTN_WEST, oval = 1;
		break;
	case KEY_Z:
		otype = EV_KEY, ocode = BTN_SOUTH, oval = 1;
		break;
	case KEY_X:
		otype = EV_KEY, ocode = BTN_EAST, oval = 1;
		break;
	case KEY_LEFTCTRL:
		otype = EV_KEY, ocode = BTN_NORTH, oval = 1;
		break;
	case KEY_SPACE:
		otype = EV_KEY, ocode = BTN_TR,	oval = 1;
		break;
	case KEY_ESC:
		otype = EV_KEY, ocode = BTN_SELECT, oval = 1;
		break;
	default:
		return;
	}
	if (val == 0) {
		oval = 0;
	}
	emit(to_fd, otype, ocode, oval);
	emit(to_fd, EV_SYN, SYN_REPORT, 0);
}

int main() {
	struct sigaction sa;
	sa.sa_handler = interrupt_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("failed to establish signal handler\n");
		return 1;
	}

	int uinput_fd = open("/dev/uinput", O_WRONLY|O_NONBLOCK);
	if (uinput_fd == -1) {
		printf("failed to open /dev/uinput\n");
		return 1;
	}
	// TODO: find keyboard instead of assuming it's in event3
	int keyboard_fd = open("/dev/input/event3", O_RDONLY);
	if (keyboard_fd == -1) {
		close(uinput_fd);
		printf("failed to open keyboard\n");
		return 1;
	}

	struct uinput_setup usetup;
	memset(&usetup, 0, sizeof usetup);
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = VENDOR;
	usetup.id.product = PRODUCT;
	static_assert(sizeof NAME <= sizeof usetup.name);
	memcpy(&usetup.name, NAME, sizeof NAME);

	int err = 0;
	if (
		(err = ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY))
		|| (err = ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS))
	) {
		printf("failed to set evbits\n");
		return err;
	}
	for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
		err = ioctl(uinput_fd, UI_SET_KEYBIT, keys[i]);
		if (err) {
			printf("failed to set keys\n");
			return err;
		}
	}
	for (size_t i = 0; i < sizeof abss / sizeof *abss; i++) {
		err = ioctl(uinput_fd, UI_SET_ABSBIT, abss[i]);
		if (err) {
			printf("failed to set abss\n");
			return err;
		}
	}
	if (
		(err = ioctl(uinput_fd, UI_DEV_SETUP, &usetup))
		|| (err = ioctl(uinput_fd, UI_DEV_CREATE))
	) {
		printf("failed to create device\n");
		return err;
	}

	while (!stop) {
		struct input_event ie[64];
		ssize_t got = read(keyboard_fd, ie, sizeof ie);
		if (got < (ssize_t) sizeof(struct input_event)) {
			if (!stop) {
				printf("expected at least one input event\n");
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
	close(uinput_fd);
	close(keyboard_fd);
	return err;
}
