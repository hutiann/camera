/*
 * * V4L2 video capture example
 * *
 * * This program can be used and distributed without restrictions.
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h> /* for videodev2.h */
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define RGB(b,g,r) (unsigned char[4]){(b),(g),(r),0}

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720

#define HDMI_WIDTH 1280
#define HDMI_HEIGHT 720

#define CAPTURE_PIXELFORMAT		(V4L2_PIX_FMT_YUYV)
#define CAPTURE_FIELD		(V4L2_FIELD_NONE)
//#define CAPTURE_FIELD					(V4L2_FIELD_INTERLACED_TB)

#define __MYDEBUG__
#ifdef __MYDEBUG__ 
#define MYDEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
#else 
#define MYDEBUG(format,...) 
#endif 

/************!!!*********************/
// static io_method io = IO_METHOD_MMAP;   P48

using namespace cv;

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

static io_method io = IO_METHOD_MMAP;

struct timeval tvv;
struct timeval ts;

struct buffer {
	void * start;
	size_t length;
};
static const char * vin_devname[] = 
{
	"/dev/video0",		// VIN0
	"/dev/video1",		// VIN1
	"/dev/video2",		// VIN2
	"/dev/video3"		// VIN3
};
static int bp=-1;
static int tty=-1;
struct fb_var_screeninfo  vinfo;
struct fb_fix_screeninfo  finfo;
long screensize=0;
char *fbp = 0;
long location = 0;
        
#define CAMERA_CNT 4
static int camera_fd[CAMERA_CNT];
int maxfd = 0;
struct buffer buffers[CAMERA_CNT][4];
int flag = 1;

static unsigned int n_buffers = 0;
//      static FILE *fp=NULL;
static void errno_exit (const char * s)
{
	fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));
	exit (EXIT_FAILURE);

}
static int xioctl (int fd,int request,void * arg)
{
	int r;
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;
}

void fb_putpixel(int x,int y,unsigned char pixel[4]){
	unsigned long position=0;
	position=((unsigned long)x+((unsigned long)y)*(vinfo.xres))*4ul;
	lseek(bp,position,SEEK_SET);
	write(bp,pixel,4);
}
static void clear(){
	int x,y;
	for (x=0;x<vinfo.xres;x++)
		for (y=0;y<vinfo.yres;y++)
		{
			*(fbp+y*finfo.line_length+x*4)=0;
			*(fbp+y*finfo.line_length+x*4+1)=0;
			*(fbp+y*finfo.line_length+x*4+2)=0;
			*(fbp+y*finfo.line_length+x*4+3)=0;
		}

}

static void process_image (const void * p,size_t length, unsigned int camera_id)
{
	printf("Enter process_image, camera_id = %d\n", camera_id);
	char *tmp_hdmi=NULL;
	unsigned char *tmp_camera=NULL;
	int i = 0;

	if(p == NULL || length ==0)
		return;
	
	Mat out(CAMERA_HEIGHT,CAMERA_WIDTH,CV_8UC2);
	Mat rgb(CAMERA_HEIGHT,CAMERA_WIDTH,CV_8UC4);
	Mat display(CAMERA_HEIGHT/2,CAMERA_WIDTH/2,CV_8UC4);
	memcpy(out.data,p,length);
	cvtColor(out,rgb,COLOR_YUV2BGRA_YUY2);
	
	resize(rgb, display, Size(CAMERA_WIDTH/2, CAMERA_HEIGHT/2));
	tmp_camera = display.data;
	switch(camera_id)
	{

		case 0:
			tmp_hdmi = fbp;
			break;
		case 1:
			tmp_hdmi = fbp + HDMI_WIDTH*2;
			break;
		case 2:
			tmp_hdmi = fbp + HDMI_WIDTH*4*HDMI_HEIGHT/2;
			break;
		case 3:
			tmp_hdmi = fbp + HDMI_WIDTH*4*HDMI_HEIGHT/2 + HDMI_WIDTH*2;
			break;
		default:
			break;
	}
	for(i=0;i<CAMERA_HEIGHT/2;i++)
	{
		memcpy(tmp_hdmi, tmp_camera, CAMERA_WIDTH*2);
		tmp_hdmi +=HDMI_WIDTH*4;
		tmp_camera +=CAMERA_WIDTH*2;
	}

}

static int read_frame (unsigned int camera_id)
{
	struct v4l2_buffer buf;
	unsigned int i;

	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
		
	//DQBUF move video buffer to mmap address 
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_DQBUF, &buf)) {


		switch (errno) {
		case EAGAIN:
			return 0;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			errno_exit ("VIDIOC_DQBUF");
		}
	}
	assert (buf.index < n_buffers);
	
	process_image (buffers[camera_id][buf.index].start,buffers[camera_id][buf.index].length, camera_id);
	//clean video buffer
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_QBUF, &buf))
		errno_exit ("VIDIOC_QBUF");

	
	return 0;
}

static void mainloop (void)
{
	printf("Enter mainloop\n");
	unsigned int count;
	count = 1000;
	fd_set fds;
	struct timeval tv;
	int r;
	int i;	
		/* Timeout. */

	while (1) 
	{
//		gettimeofday(&tvv,NULL);
		
		/*fd_set is a struct actualy a
		 * Long type array ,every Long type 
		 * is a handle*/

		//ensure video device is ready!
		for( i = 0; i < CAMERA_CNT; i++)
		{
		#if 0
			FD_ZERO (&fds);//turn zero to fds
			FD_SET (camera_fd[i], &fds);//add device handle to fds
			tv.tv_sec = 1; //second
			tv.tv_usec = 0; //u second 10^-6s	
			//r = select (maxfd + 1, &fds, NULL, NULL, &tv);
		
			if (-1 == r) {
				if (EINTR == errno)
					continue;
				//errno_exit ("select");
			}
			if (0 == r) {
				fprintf (stderr, "select timeout\n");
				//exit (EXIT_FAILURE);
			}
		#endif	
			if (read_frame (i))
				break;
			if(flag)
			{
				flag = 0;
				sleep(1);
			}
		/* EAGAIN - continue select loop. */
		}
		sleep(0.05);
//		gettimeofday(&ts,NULL);
//		printf("timestamp: %ld ms \n", (ts.tv_sec * 1000 + ts.tv_usec / 1000) - (tvv.tv_sec * 1000 + tvv.tv_usec / 1000));

	}
}
static void stop_capturing (void)
{
	enum v4l2_buf_type type;
	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;


	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		for(int i = 0; i < CAMERA_CNT; i++)
		{
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl (camera_fd[i], VIDIOC_STREAMOFF, &type))
				errno_exit ("VIDIOC_STREAMOFF");
		}
		break;
	}
}


static void start_capturing (unsigned char camera_id)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		CLEAR (buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (-1 == xioctl (camera_fd[camera_id], VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_STREAMON, &type))
		errno_exit ("VIDIOC_STREAMON");



		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (camera_fd[camera_id], VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");
		
	}
	printf("finished start_capturing %d\n", camera_id);
}

static void uninit_device (void)
{
	unsigned int i;

		for(int camera_id = 0; camera_id < CAMERA_CNT; camera_id++)
		{
		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap (buffers[camera_id][i].start, buffers[camera_id][i].length))
				errno_exit ("munmap");

		if (-1 == munmap (fbp, screensize))
			errno_exit ("munmap");
		}

	//free (buffers[4][4]);
}
#if 0
static void init_read (unsigned int buffer_size)
{
	buffers = (buffer *)calloc (1, sizeof (*buffers));
	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);
	if (!buffers[0].start) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
}
#endif
/*
 *
 *struct v4l2_requestbuffers {
 *	__u32			count; //number of buff request
 *	__u32			type;  //buf type (v4l2_format)
 *	__u32			memory;	// set field to mmap
 *	__u32			reserved[2];
 *};
 *
 * struct v4l2_buffer {
 *	__u32			index;
 *	__u32			type;
 *	__u32			bytesused;
 *	__u32			flags;
 *	__u32			field;
 *	struct timeval		timestamp;
 *	struct v4l2_timecode	timecode;
 *	__u32			sequence;
 *	__u32			memory; //memory location
 *	union {
 *		__u32           offset;
 *		unsigned long   userptr;
 *		struct v4l2_plane *planes;
 *		__s32		fd;
 *	} m;
 *	__u32			length;
 *	__u32			reserved2;
 *	__u32			reserved;
 *};   
 *
 **/
static void init_mmap (unsigned char camera_id)
{
	struct v4l2_requestbuffers req;
	CLEAR (req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	char *dev_name;
	dev_name = (char*)vin_devname[camera_id];
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					 "memory mapping\n", dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}
	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on %s\n",
				 dev_name);
		exit (EXIT_FAILURE);
	}
	printf("req buf count:%d\n",req.count);

	if (!buffers) 
	{
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
		CLEAR (buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		if (-1 == xioctl (camera_fd[camera_id], VIDIOC_QUERYBUF, &buf))
			errno_exit ("VIDIOC_QUERYBUF");
		buffers[camera_id][n_buffers].length = buf.length;
		MYDEBUG("buf.length: %d", buf.length);

		buffers[camera_id][n_buffers].start =
			mmap (NULL /* start anywhere */,
				  buf.length,
				  PROT_READ | PROT_WRITE /* required */,
				  MAP_SHARED /* recommended */,
				  camera_fd[camera_id], buf.m.offset);
		if (MAP_FAILED == buffers[camera_id][n_buffers].start)
			errno_exit ("mmap");
	}
	printf("finished init_mmap %d\n", camera_id);
}
#if 0
static void init_userp (unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;
	CLEAR (req);
	req.count = 2;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;
	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					 "user pointer i/o\n", dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}
	//buffers = (buffer *)calloc (4, sizeof (*buffers));
	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = malloc (buffer_size);
		if (!buffers[n_buffers].start) {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
}
#endif
/*
 *
 *cap --->driver |name of driver
 *    --->card   |name of device
 *    --->bus_info| location of the device in the system
 *    --->version | kernel verison
 *    --->capabilities| 0x00000001 support video capture interface
 *                    | 0x00000002 support video output interface
 *                    | 0x00000004 support store images directly in video memory
 *                    | 0x00020000 device has audio input outputs
 *                    | 0x01000000 support read()/write() I\O methods
 *                    | 0x04000000 support streaming I/O methods
 *
 *cropcap video crop and scaling abilities
 *       ---> type    | V4L2_BUF_TYPE_VIDEO_CAPTURE
 *                    | V4L2_BUF_TYPE_VIDEO_OUTPUT
 *                    | V4L2_BUF_TYPE_VIDEO_OVERLAY
 *       --->(v4l2_rect) bounds | Defines window
 *       --->(v4l2_rect) defrect| default cropping rectangel
 *       --->(v4l2_fract)pixelaspect| pixel aspect(y/x)
 *fmt  vidio format
 *       ---> tpye    |same as above
 *       ---> unio( v4l2_pix_format format_mplane window vbi_format sdr_format raw_data )fmt |set format
 *
 * */

static void init_device (unsigned int camera_id)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno) 
		{
			fprintf (stderr, "%s is no V4L2 device\n",(char*)vin_devname[camera_id]);
			exit (EXIT_FAILURE);
		} else 
		{
			errno_exit ("VIDIOC_QUERYCAP");
		}
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf (stderr, "%s is no video capture device\n",(char*)vin_devname[camera_id]);
		exit (EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		fprintf (stderr, "%s does not support streaming i/o\n",(char*)vin_devname[camera_id]);
		exit (EXIT_FAILURE);
	}

	/* Select video input, video standard and tune here. */
	#if 0
	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (0 == xioctl (camera_fd[camera_id], VIDIOC_CROPCAP, &cropcap)) 
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;
		crop.c.left = 180;
		crop.c.top = 240;
		crop.c.width = CAMERA_WIDTH;
		crop.c.height = CAMERA_HEIGHT;
		
		if (-1 == xioctl (camera_fd[camera_id], VIDIOC_S_CROP, &crop)) 
		{
			switch (errno)
			{
			case EINVAL:
				/* Cropping not supported. */
				printf("Cropping not support!\n");
				break;
			default:
				printf("Cropping faild!\n");
				/* Errors ignored. */
				break;
			}
		}
	} 
	else 
	{
		printf("get cropcap error!\n");
		/* Errors ignored. */
	}
	#endif
	CLEAR (fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = CAMERA_WIDTH;
	fmt.fmt.pix.height = CAMERA_HEIGHT;
	fmt.fmt.pix.pixelformat =  CAPTURE_PIXELFORMAT;
	fmt.fmt.pix.field		= 		CAPTURE_FIELD;	
	
	if (-1 == xioctl (camera_fd[camera_id], VIDIOC_S_FMT, &fmt))
		errno_exit ("VIDIOC_S_FMT");
	/* Note VIDIOC_S_FMT may change width and height. */
	/* Buggy driver paranoia. */
	/*set bytes per line | image size = width * height *2*/
	min = fmt.fmt.pix.width * 2;
	printf("bytes per line:%d,width*2:%d\n",fmt.fmt.pix.bytesperline,min);
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	printf("image size:%d,width*height*2:%d\n",fmt.fmt.pix.sizeimage,min);
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;
	printf("finished init_device %d\n", camera_id);
}



static void close_device (void)
{
	for(int i = 0; i < CAMERA_CNT; i++)
	{
	if (-1 == close (camera_fd[i]))
		errno_exit ("close");
	}
	if (-1 == close (bp))
		errno_exit ("close");
	if( -1 ==close(tty))
		errno_exit ("close");

}
/*
 *
 *
 * stat  --->st_dev    |device num
 *       --->st_ino    |node    
 *       --->st_mode   |file type read/write permition
 *       --->st_nlink  |file hard link number
 *       --->st_uid gid|user id group ip
 *       --->st_size   |file size byte number
 *       --->st_blksize|file system I/O buffer size
 *       --->st_blocks |block number
 *       --->st_atime ctime mtime |last access, last change property, last modify
 *
 *
 * */

static void open_device (unsigned int camera_id)
{
	struct stat st;
	char *dev_name;

	dev_name = (char*)vin_devname[camera_id];
	if (-1 == stat (dev_name, &st)) {
		fprintf (stderr, "Cannot identify ’%s’: %d, %s\n",
				 dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", dev_name);
		exit (EXIT_FAILURE);
	}
	
	camera_fd[camera_id] = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
	
	if (-1 == camera_fd[camera_id]) {
		fprintf (stderr, "Cannot open ’%s’: %d, %s\n",
				 dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	printf("finished open_device %d\n", camera_id);
	
}
static void usage (FILE * fp,int argc,char ** argv)
{
	fprintf (fp,
			 "Usage: %s [options]\n\n"
			 "Options:\n"
			 "-d | --device name Video device name [/dev/video]\n"
			 "-h | --help Print this message\n"
			 "-m | --mmap Use memory mapped buffers\n"
			 "-r | --read Use read() calls\n"
			 "-u | --userp Use application allocated buffers\n"
			 "",
			 argv[0]);
}
static const char short_options [] = "d:hmru";

static const struct option

long_options [] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "mmap", no_argument, NULL, 'm' },
	{ "read", no_argument, NULL, 'r' },
	{ "userp", no_argument, NULL, 'u' },
	{ 0, 0, 0, 0 }
};



int main (int argc,
		  char ** argv)
{
	//dev_name = argv[1];
#if 1	
	bp = open ("/dev/fb0",O_RDWR);
	if (bp < 0)
	{
		printf("Error : Can not open framebuffer device/n");
		exit(1);
	}
	if (ioctl(bp,FBIOGET_FSCREENINFO,&finfo))
	{
		printf("Error reading fixed information/n");
		exit(2);
	}
	if (ioctl(bp,FBIOGET_VSCREENINFO,&vinfo))
	{
		printf("Error reading variable information/n");
		exit(3);
	}
	vinfo.xres = HDMI_WIDTH;
	vinfo.yres = HDMI_HEIGHT;
	if (ioctl(bp,FBIOPUT_VSCREENINFO,&vinfo))
	{
		printf("Error setting variable information/n");
		exit(3);
	}
         
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	fbp =(char *) mmap (0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, bp,0);
	if ( fbp == MAP_FAILED )
	{
		printf ("Error: failed to map framebuffer device to memory./n");
		close(bp);
		exit (4);
	}
#endif	
	// graphics or text-> ioctl(tty_fd,KDSETMODE,KD_TEXT);
	tty=open("/dev/tty1",O_RDWR);
	ioctl(tty,KDSETMODE,KD_GRAPHICS);
	
	/* undo  press key to stop capture!!!!!!!!*/

// 	key = open("/dev/input/event",O_RDONLY);
#if 0
	for (;;) {
		int index;
		int c;
		c = getopt_long (argc, argv,
						 short_options, long_options,
						 &index);
		if (-1 == c)
			break;


		switch (c) {
		case 0: /* getopt_long() flag */
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'h':
			usage (stdout, argc, argv);
			exit (EXIT_SUCCESS);
		case 'm':
			io = IO_METHOD_MMAP;
			break;
		case 'r':
			io = IO_METHOD_READ;
			break;
		case 'u':
			io = IO_METHOD_USERPTR;
			break;
		default:
			usage (stderr, argc, argv);
			exit (EXIT_FAILURE);
		}
	}
#endif	
	unsigned char i;
	for( i = 0; i < CAMERA_CNT; i++)
	{
		open_device(i);
		init_device(i);			
		init_mmap (i);
		start_capturing(i);
		if( maxfd < camera_fd[i])
		{
			maxfd = camera_fd[i];
		}
	}

	mainloop ();
	stop_capturing ();
	clear();
	uninit_device ();
	close_device ();
	exit (EXIT_SUCCESS);

	return 0;
}
