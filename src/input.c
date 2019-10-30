#include "input.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdbool.h>
#include <pthread.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/limits.h>


static const char* EVDEV_NAME = "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick";


typedef struct go2_input
{
    int fd;
    struct libevdev* dev;
    go2_gamepad_t current_state;
    go2_gamepad_t pending_state;
    pthread_mutex_t gamepadMutex;
    pthread_t thread_id;
} go2_input_t;


static void* input_task(void* arg)
{
    const float VALUE_MAX = 512.0f;
    go2_input_t* input = (go2_input_t*)arg;

    if (!input->dev) return NULL;


	while (true)
	{
		/* EAGAIN is returned when the queue is empty */
		struct input_event ev;
		int rc = libevdev_next_event(input->dev, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
		if (rc == 0)
		{
#if 0
			printf("Gamepad Event: %s-%s(%d)=%d\n",
			       libevdev_event_type_get_name(ev.type),
			       libevdev_event_code_get_name(ev.type, ev.code), ev.code,
			       ev.value);
#endif

            if (ev.type == EV_KEY)
			{
                go2_button_state_t state = ev.value ? ButtonState_Pressed : ButtonState_Released;

                switch (ev.code)
                {
                    case BTN_DPAD_UP:
                        input->pending_state.dpad.up = state;
                        break;
                    case BTN_DPAD_DOWN:
                        input->pending_state.dpad.down = state;
                        break;
                    case BTN_DPAD_LEFT:
                        input->pending_state.dpad.left = state;
                        break;
                    case BTN_DPAD_RIGHT:
                        input->pending_state.dpad.right = state;
                        break;

                    case BTN_EAST:
                        input->pending_state.buttons.a = state;
                        break;
                    case BTN_SOUTH:
                        input->pending_state.buttons.b = state;
                        break;
                    case BTN_NORTH:
                        input->pending_state.buttons.x = state;
                        break;
                    case BTN_WEST:
                        input->pending_state.buttons.y = state;
                        break;

                    case BTN_TL:
                        input->pending_state.buttons.top_left = state;
                        break;                    
                    case BTN_TR:          
                        input->pending_state.buttons.top_right = state;
                        break;

                    case BTN_TRIGGER_HAPPY1:
                        input->pending_state.buttons.f1 = state;
                        break;
                    case BTN_TRIGGER_HAPPY2:
                        input->pending_state.buttons.f2 = state;
                        break;
                    case BTN_TRIGGER_HAPPY3:
                        input->pending_state.buttons.f3 = state;
                        break;
                    case BTN_TRIGGER_HAPPY4:
                        input->pending_state.buttons.f4 = state;
                        break;
                    case BTN_TRIGGER_HAPPY5:
                        input->pending_state.buttons.f5 = state;
                        break;
                    case BTN_TRIGGER_HAPPY6:
                        input->pending_state.buttons.f6 = state;
                        break;
                }
            }
            else if (ev.type == EV_ABS)
            {
                switch (ev.code)
                {
                    case ABS_X:
                        input->pending_state.thumb.x = ev.value / VALUE_MAX;
                        break;
                    case ABS_Y:
                        input->pending_state.thumb.y = ev.value / VALUE_MAX;
                        break;
                }
            }
            else if (ev.type == EV_SYN)
            {
                pthread_mutex_lock(&input->gamepadMutex);
    
                input->current_state = input->pending_state;

                pthread_mutex_unlock(&input->gamepadMutex); 
            }
        }
    }
}

go2_input_t* go2_input_create()
{
	int rc = 1;

    go2_input_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        goto out;
    }

    memset(result, 0, sizeof(*result));


    result->fd = open(EVDEV_NAME, O_RDONLY);
    if (result->fd < 0)
    {
        printf("Joystick: No gamepad found.\n");
    }
    else
    {    
        rc = libevdev_new_from_fd(result->fd, &result->dev);
        if (rc < 0) {
            printf("Joystick: Failed to init libevdev (%s)\n", strerror(-rc));
            goto err_00;
        }

        memset(&result->current_state, 0, sizeof(result->current_state));
        memset(&result->pending_state, 0, sizeof(result->pending_state));
    
    
        // printf("Input device name: \"%s\"\n", libevdev_get_name(result->dev));
        // printf("Input device ID: bus %#x vendor %#x product %#x\n",
        //     libevdev_get_id_bustype(result->dev),
        //     libevdev_get_id_vendor(result->dev),
        //     libevdev_get_id_product(result->dev));

        if(pthread_create(&result->thread_id, NULL, input_task, (void*)result) < 0)
        {
            printf("could not create input_task thread\n");
            goto err_01;
        }
    }

    return result;


err_01:
    libevdev_free(result->dev);

err_00:
    close(result->fd);
    free(result);

out:
    return NULL;
}

void go2_input_destroy(go2_input_t* input)
{
    // TODO: Kill thread and join

    libevdev_free(input->dev);
    close(input->fd);
    free(input);
}

void go2_input_read(go2_input_t* input, go2_gamepad_t* outGamepadState)
{
    pthread_mutex_lock(&input->gamepadMutex);
    
    *outGamepadState = input->current_state;        

    pthread_mutex_unlock(&input->gamepadMutex);  
}