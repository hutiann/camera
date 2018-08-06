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
	vinfo.yres = 720;
	 if (ioctl(bp,FBIOPUT_VSCREENINFO,&vinfo))
	 {
		  printf("Error seting variable information/n");
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
	tty=open("/dev/tty2",O_RDWR);
	ioctl(tty,KDSETMODE,KD_GRAPHICS);

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
  
int main(int argc,char *argv[])  
{  
    Mat srcImage = imread(argv[1]);  
    Mat src ,dstImage, edge, grayImage;  
    resize(srcImage ,src,Size(1280,720));
    
  //  myshow(src.data,src.cols,src.rows);
  imshow("Source image", srcImage);  
      
    /* 腐蚀   */
//    Mat element = getStructuringElement(MORPH_RECT, Size(15, 15));  
  //  erode(src, dstImage, element);  
    //imwrite("/media/erode.jpg", dstImage);  
    // myshow(dstImage.data,dstImage.cols,dstImage.rows);
   
    /* 均值滤波   
    blur(srcImage, dstImage, Size(7, 7));  
    imwrite("blur.jpg", dstImage);  
    *//* 边缘检查   
    dstImage.create(srcImage.size(), srcImage.type());  
    cvtColor(srcImage, grayImage, COLOR_BGR2GRAY);  
    blur(srcImage, edge, Size(3,3));  
    Canny(edge, edge, 3, 9, 3);  
    imwrite("canny.jpg", edge);  
   */
    //imshow("Canny", edge);  


    waitKey(0);  
      
    return 0;  
}  
