#pragma once
/* Call only from the task owning the NO_SYS lwIP stack. */
void camera_stream_init(void);
void camera_stream_poll(void);
