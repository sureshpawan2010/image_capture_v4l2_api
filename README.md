# V4L2 API

Video4Linux, V4L for short, is a collection of device drivers and an API for supporting real-time video capture on Linux systems. V4L2 is the second version of V4L.

Capturing a frame involves 6 steps. We can move step-by-step.

1. **Initialize the device** by setting various options like image format.
2. **Request for a buffer** from the device by the method of memory mapping, user pointer, or DMA. We will be using the `mmap` approach.
3. **Query the allotted buffer** from the device to get memory details.
4. **Start streaming**.
5. **Queue the buffer** to the device and get the frame after the device writes to the buffer.
6. **Dequeue the buffer** and stop streaming.

