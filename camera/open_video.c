#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
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
#include <linux/fb.h>
#include <linux/kd.h>

static int bp=-1;
static int tty=-1;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long screensize=0;
char *fbp=0;
void *myshow(unsigned char *img,int width,int height){

	int i,j;
	for(i=0;i<height;i++)
		for(j=0;j<width;j++)
		{

			*(fbp+i*finfo.line_length+j*4)=*img;
			img++;
			*(fbp+i*finfo.line_length+j*4+1)=*img;
			img++;
			*(fbp+i*finfo.line_length+j*4+2)=*img;
			img++;
			*(fbp+i*finfo.line_length+j*4+3)=0;

		}
	
	
	getchar();

}


using namespace cv;  
  
int main(int argc,char** argv)  
{  
 
         bp=open ("/dev/fb0",O_RDWR);
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
         vinfo.xres = 1280;
	 vinfo.yres = 800;
	 if (ioctl(bp,FBIOPUT_VSCREENINFO,&vinfo))
	 {
		  printf("Error setting variable information/n");
		  exit(3);
	 }
         
	 printf("virtual size x: %d,y:%d\n    xres:%d,yres%d\n",vinfo.xres_virtual,vinfo.yres_virtual,\
			 vinfo.xres,vinfo.yres);
	 screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	 fbp =(char *) mmap (0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, bp,0);
	 if ( fbp == MAP_FAILED )
	 {
		printf ("Error: failed to map framebuffer device to memory./n");
		close(bp);
		exit (4);
	 }
	// graphics or text-> ioctl(tty_fd,KDSETMODE,KD_TEXT);
	tty=open("/dev/tty1",O_RDWR);
	ioctl(tty,KDSETMODE,KD_GRAPHICS);

	if(argv[1]){
		printf("Open video :%s\n",argv[1]);
	}
	else{
		printf("print a video path for argv[1] \n");
		return -1;
	}	
		

	VideoCapture capture(argv[1]);

	if(!capture.isOpened())return 1;
        Mat frame ,out; 	
	while(1){
		if(!capture.read(frame))
			break;
		resize(frame,out,Size(1280,800));
	
		myshow(out.data,out.cols,out.rows);	
		Mat element = getStructuringElement(MORPH_RECT, Size(15, 15));  
		erode(out,out, element);  
		myshow(out.data,out.cols,out.rows);	

	}      
        //myshow(srcImage.data,srcImage.cols,srcImage.rows);
        // imshow("Source image", srcImage);  
 
    return 0;  
}  
