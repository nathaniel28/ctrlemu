#include <assert.h>
#include <fcntl.h>
#include <stdio.h> // TODO will be unused
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

#include <linux/uinput.h>

/*


type EV_ABS
	code ABS_HAT0Y -1 = up
	code ABS_HAT0Y +1 = down
	code ABS_HAT0X -1 = left
	code ABS_HAT0X +1 = right
type EV_KEY
	1 = pressed
	code BTN_NORTH = triangle
	code BTN_SOUTH = cross
	code BTN_WEST = square
	code BTN_EAST = circle
	code BTN_TL = upper left trigger
	code BTN_TR = upper right trigger
	code BTN_SELECT = left switch
	code BTN_START = right switch
	code BTN_MODE = center switch


*/

// https://www.kernel.org/doc/html/latest/input/input.html
// https://docs.kernel.org/input/uinput.html

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
#define PRODUCT 0x6365

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

	struct uinput_setup usetup;
	memset(&usetup, 0, sizeof usetup);
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = VENDOR;
	usetup.id.product = PRODUCT;
	static_assert(sizeof NAME <= sizeof usetup.name);
	memcpy(&usetup.name, NAME, sizeof NAME);

	int err;
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
		sleep(1);
		emit(uinput_fd, EV_KEY, BTN_NORTH, 1);
		emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
		sleep(1);
		emit(uinput_fd, EV_KEY, BTN_NORTH, 0);
		emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
	}

	printf("\nshutting down\n");

	ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
	return 0;
}
