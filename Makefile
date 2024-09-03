all:
	@echo "COMPILING SOURCE CODE...."
	gcc capture_image_using_v4l2_api.c -o bin/output
	@echo "RUNNING APP BINARY...."
	./bin/output
	@echo "CONVERTING IMAGE TO JPG...."
	ffmpeg -f rawvideo -pixel_format yuyv422 -video_size 640x480 -i images/output.yuy -frames:v 1 images/output.jpg

clean:
	rm -rf bin/output images/output.yuy images/output.jpg


