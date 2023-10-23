#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/uinput.h>

#include "parser.h"

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

void handle_code(int to_fd, int in_code, int in_val) {
	if (in_val == 2) {
		// key is being held
		return;
	}
	const key k = keymap[in_code];
	if (
		in_code > (int) (sizeof keymap / sizeof *keymap)
		|| k.code == 0
	) {
		return;
	}
	keymap[in_code].holding = in_val;
	int out_val = k.value;
	if (in_val == 0) {
		if (k.type == EV_ABS && keymap[k.opposite].holding) {
			out_val = keymap[k.opposite].value;
		} else {
			out_val = 0;
		}
	}
	emit(to_fd, k.type, k.code, out_val);
	emit(to_fd, EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv) {
	if (argc >= 4) {
		printf("%s: too many arguments provided, ignoring excess\n",
		       argv[0]);
	}

	struct sigaction sa;
	sa.sa_handler = interrupt_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("%s: failed to establish signal handler: %s\n",
		       argv[0], strerror(errno));
		return 1;
	}

	int uinput_fd = open("/dev/uinput", O_WRONLY|O_NONBLOCK);
	if (uinput_fd == -1) {
		printf("%s: failed to open /dev/uinput: %s\n",
		       argv[0], strerror(errno));
		return errno;
	}

	char *key_binds_path = "keys.conf";
	if (argc >= 3) {
		key_binds_path = argv[2];
	}
	printf("reading keybinds from %s\n", key_binds_path);
	int err = parse(key_binds_path);
	if (err) {
		printf("failed to parse keybinds\n");
		close(uinput_fd);
		return 1;
	}

	int keyboard_fd = STDIN_FILENO;
	char *input_file_name = "stdin";
	if (argc >= 2) {
		keyboard_fd = open(argv[1], O_RDONLY);
		if (keyboard_fd == -1) {
			close(uinput_fd);
			printf("%s: failed to open %s: %s\n",
			       argv[0], argv[1], strerror(errno));
			return errno;
		}
		input_file_name = argv[1];
	}

	if (
		(err = ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY))
		|| (err = ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS))
	) {
		printf("%s: failed to set evbits: %s\n",
		        argv[0], strerror(errno));
		goto cleanup;
	}

	for (size_t i = 0; i < sizeof keymap / sizeof *keymap; i++) {
		if (keymap[i].code == 0) {
			continue;
		}
		int set;
		switch (keymap[i].type) {
		case EV_KEY:
			set = UI_SET_KEYBIT;
			break;
		case EV_ABS:
			set = UI_SET_ABSBIT;
			break;
		default:
			printf("%s: bad type %d\n", argv[0], keymap[i].type);
			goto cleanup;
		}
		err = ioctl(uinput_fd, set, keymap[i].code);
		if (err) {
			printf("%s: failed to set as %d %d: %s\n",
			       argv[0], set, keymap[i].code, strerror(errno));
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

	printf("%s: reading input from %s\n", argv[0], input_file_name);
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
