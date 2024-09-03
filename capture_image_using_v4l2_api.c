#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/types.h>

/* Initialize the Device*/
/* We need to set the output image format of the device. It is possible to query possible image format from terminal with v4l2-ctl. */

/**
 * v4l2-ctl -d /dev/video0 --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
        Type: Video Capture

        [0]: 'MJPG' (Motion-JPEG, compressed)
                Size: Discrete 1280x720
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 848x480
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 960x540
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 640x480
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 640x360
                        Interval: Discrete 0.033s (30.000 fps)
        [1]: 'YUYV' (YUYV 4:2:2)
                Size: Discrete 640x480
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 160x120
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 320x180
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 320x240
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 424x240
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 640x360
                        Interval: Discrete 0.033s (30.000 fps)
                Size: Discrete 1280x720
                        Interval: Discrete 0.100s (10.000 fps)
 */
int open_video_device(char *dev, int permission) {
    int fd = open(dev, permission);
    if (fd == -1) {
        perror("Opening video device");
        return -1;
    }
    return fd;
}

/**
 * 
 */
int set_video_device_format(int *fd) {
    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    int res = ioctl(*fd, VIDIOC_S_FMT, &format);
    if (res == -1) {
        perror("Could not set format");
        exit(1);
    }
    return res;
}

/**
 * Requesting a buffer
    In this step, we are informing the device that we are using memory map method for streaming I/O. Here count is the number of buffers.
 */
int request_buffer_from_video_device(int *fd, int count) {
    struct v4l2_requestbuffers reqbuff = {0};
    reqbuff.count = count;
    reqbuff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuff.memory = V4L2_MEMORY_MMAP;
    int res = ioctl(*fd, VIDIOC_REQBUFS, &reqbuff);
    if (res == -1) {
        perror("Unable to request buffer");
        exit(1);
    }
    return res;
}

/**
 * Do the memory mapping
    We have now informed the device about the streaming I/O method. So in order to do the actual mapping we need to get some information like offset and size from the device.
 */
int query_buffer_from_video_device(int *fd, u_int8_t **buffer) {
    struct v4l2_buffer querybuff = {0};
    querybuff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    querybuff.memory = V4L2_MEMORY_MMAP;
    querybuff.index = 0;

    int res = ioctl(*fd, VIDIOC_QUERYBUF, &querybuff);
    if (res == -1) {
        perror("Unable to request the desired buffer");
        return 2;
    }
    *buffer = (u_int8_t *)mmap(NULL, querybuff.length, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, querybuff.m.offset);
    if (*buffer == MAP_FAILED) {
        perror("mmap");
        return 3;
    }
    return querybuff.length;
}
/**
 * start streaming.
 */
int start_video_streaming(int *fd) {
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(*fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON");
        exit(1);
    }
    return 0;
}

int stop_video_streaming(int *fd) {
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(*fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF");
        exit(1);
    }
    return 0;
}
/**
 * Grab the frame and save it.
    To grab a frame, we need to queue the buffer to the device so that it can start writing to it.
 */
int queue_video_buffer(int *fd) {
    struct v4l2_buffer bufd = {0};
    bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufd.memory = V4L2_MEMORY_MMAP;
    bufd.index = 0;
    if (ioctl(*fd, VIDIOC_QBUF, &bufd) == -1) {
        perror("Queue Buffer");
        return 1;
    }
    return bufd.bytesused;
}

int dequeue_video_buffer(int *fd) {
    struct v4l2_buffer bufdd = {0};
    bufdd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufdd.memory = V4L2_MEMORY_MMAP;
    bufdd.index = 0;

    if (ioctl(*fd, VIDIOC_DQBUF, &bufdd) == -1) {
        perror("deQueue Buffer");
        return 1;
    }
    return bufdd.bytesused;
}
/**
 * After queueing the buffer we need to wait till the camera writes to the image so that we can read 
 * from the buffer. We can achieve this by the select() system call. 
 * select will inform us when the camera becomes ready for a read operation. 
 * Now we can save the data to file. Remember that this is raw pixel data. 
 * We can either convert it into any common image format like JPEG or view it using this website(rawpixels.net).
 */
void save_yuyvframe(int *fd, u_int8_t *buffer, u_int32_t size) {
    queue_video_buffer(fd);
    // Wait for io operation
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(*fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2; // set timeout to 2 second
    int r = select(*fd + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        perror("Waiting for Frame");
        exit(1);
    }
    printf("Opening File\n");
    int file = open("images/output.yuy", O_WRONLY | O_CREAT, 0666);
    if (file == -1) {
        perror("Opening output file");
        exit(1);
    }
    write(file, buffer, size); // size is obtained from the query_buffer function
    close(file);
    printf("Closing File\n");
    dequeue_video_buffer(fd);
}
/**
 * 
 */
void close_video_device(int *fd) {
    close(*fd);
}

int main(void) {
    int fd = open_video_device("/dev/video0", O_RDWR);
    if (fd == -1) {
        return 1;
    }
    set_video_device_format(&fd);
    request_buffer_from_video_device(&fd, 1);
    u_int8_t *buff = NULL;
    u_int32_t size;
    size = query_buffer_from_video_device(&fd, &buff);
    start_video_streaming(&fd);
    save_yuyvframe(&fd, buff, size);
    stop_video_streaming(&fd);
    munmap(buff, size);
    close_video_device(&fd);
    return 0;
}
