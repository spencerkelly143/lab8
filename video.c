 #include <linux/fs.h> // struct file, struct file_operations
 #include <linux/init.h> // for __init, see code
 #include <linux/module.h> // for module init and exit macros
 #include <linux/miscdevice.h> // for misc_device_register and struct miscdev
 #include <linux/uaccess.h> // for copy_to_user, see code
 #include <asm/io.h> // for mmap
 #include "address_map_arm.h"

 // Declare global variables needed to use the pixel buffer
 void *LW_virtual; // used to access FPGA light-weight bridge
 volatile int * pixel_ctr_ptr; // virtual address of pixel buffer controller
 volatile int * char_ctr_ptr;
 int ONCHIP_virtual;
 int SDRAM_virtual;
 int CHAR_virtual;
 int back_buffer;
 int resolution_x, resolution_y, char_res_x,char_res_y; // VGA screen size
 void draw_line(int, int, int, int, short int);
 void plot_pixel(int, int, short int);
 void draw_box(int,int,int,int,short int);
 void get_screen_specs(volatile int *,int);
 void wait_for_vsync(void);
 void clear_screen(void);
 void erase(void);
 static int device_open (struct inode *, struct file *);
 static int device_release (struct inode *, struct file *);
 static ssize_t device_read (struct file *, char *, size_t, loff_t *);
 static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

 /* Code to initialize the video driver */
 static struct file_operations video_fops = {
     .owner = THIS_MODULE,
     .read = device_read,
     .write = device_write,
     .open = device_open,
     .release = device_release
 };

 #define SUCCESS 0
 #define DEV_NAME "video"

 static struct miscdevice video = {
     .minor = MISC_DYNAMIC_MINOR,
     .name = DEV_NAME,
     .fops = &video_fops,
     .mode = 0666
 };



 char firstThree[4];
 char firstFour[5];
 char firstFive[6];


 static int video_registered = 0;
 static char video_msg[64];
 int buffer_check;

 static int __init start_video(void){
     int err = misc_register(&video);

     if (err < 0) {
         printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME);
     } else {
         printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME);
         video_registered = 1;
     }

     LW_virtual = ioremap_nocache(0xFF200000,0x00005000);
     if(LW_virtual==0)
         printk(KERN_ERR "Error: ioremap_nocache returned NULL\n");

     pixel_ctr_ptr = (unsigned int*) (LW_virtual + PIXEL_BUF_CTRL_BASE);
     char_ctr_ptr = (unsigned int *) (LW_virtual + 0xFF203030);

     get_screen_specs (pixel_ctr_ptr,0);
     get_screen_specs(char_ctr_ptr,1);
     ONCHIP_virtual = (int) ioremap_nocache(FPGA_ONCHIP_BASE,FPGA_ONCHIP_SPAN);
     SDRAM_virtual = (int) ioremap_nocache(SDRAM_BASE,SDRAM_SPAN);

     CHAR_virtual = (int) ioremap_nocache(FPGA_CHAR_BASE, FPGA_CHAR_SPAN);

     buffer_check = *pixel_ctr_ptr;
     if(*pixel_ctr_ptr ==  SDRAM_BASE){
         *(pixel_ctr_ptr+1) = FPGA_ONCHIP_BASE;
         back_buffer = ONCHIP_virtual;
     }else{
         *(pixel_ctr_ptr+1)=SDRAM_BASE;
         back_buffer = SDRAM_virtual;
     }

     if(ONCHIP_virtual==0)
         printk(KERN_ERR "Error: ioremap_nocache for onchip returned NULL\n");
     //if(SDRAM_virtual==0)
     //  printk(KERN_ERR "Error: ioremap_nocache for sdram returned NULL\n");
     clear_screen();
     return 0;
 }

 void get_screen_specs(volatile int * ptr, int isCharBuf){
     if(isCharBuf){
         resolution_x = *(ptr+2) & 0xFFFF;
         resolution_y = (*(ptr+2)>>16)&0xFFFF;
     }else{
         char_res_x = *(ptr+2)&0xFFFF;
         char_res_y = (*(ptr+2)>>16)&0xFFFF;
     }
     return;
 }

 void write_text(int x,int y, char * str){
     int i;
     for(i=0;i<strlen(str)&&(i+x<79)&& y<59;i++){
         *(short int *)(CHAR_virtual + ((y)<<7)+(x+i)) = *(str+i);
     }
     return;
 }
 void erase(){
     memset((void*) CHAR_virtual,0x0000, FPGA_CHAR_SPAN);
     return;
 }
 void clear_screen(){
     if(back_buffer==(int) SDRAM_virtual)
         memset( (void*) SDRAM_virtual,0x0000,FPGA_ONCHIP_SPAN);
     else
         memset( (void*) ONCHIP_virtual,0x0000,FPGA_ONCHIP_SPAN);
     return;
 }

 void wait_for_vsync(){
     register volatile int status;
     *pixel_ctr_ptr = 1;

     status = *(pixel_ctr_ptr + 3);

     while((status&0x01) != 0){
         status = *(pixel_ctr_ptr+3);
     }
     if(*(pixel_ctr_ptr+1)== (unsigned int) SDRAM_BASE)
         back_buffer = (int) SDRAM_virtual;
     else
         back_buffer = (int) ONCHIP_virtual;

     return;
 }

 void plot_pixel(int x, int y, short int color){

     *(short int *)(back_buffer + (y<<10)+(x<<1)) = color;

     return;
 }

 void draw_box(int x0,int x1,int y0,int y1, short int color){
     int temp, i;

     if(x0>x1){
         temp = x0;
         x0 = x1;
         x1 = temp;
     }
     for(i=x0;i<x1;i++){
         draw_line(i,i,y0,y1,color);
     }
     return;
 }

 void draw_line(int x0,int x1,int y0, int y1, short int color){
     int is_steep = (abs(y1-y0)> abs(x1-x0));

     int temp0,temp1;
     if(is_steep){
         temp0 = x0;
         x0 = y0;
         y0 = temp0;
         temp1 = x1;
         x1=y1;
         y1=temp1;
     }
     if(x0>x1){
         temp0 = x0;
         x0 = x1;
         x1 = temp0;
         temp1 = y0;
         y0=y1;
         y1=temp1;
     }
     int deltax = x1-x0;
     int deltay = abs(y1-y0);
     int error = -(deltax/2);
     int y=y0;
     int y_step;
     if(y0<y1){
         y_step = 1;
     }else{
         y_step = -1;
     }
     int x;
     for(x=x0;x<x1;x++){
         if(is_steep){
             plot_pixel(y,x,color);
         } else{
             plot_pixel(x,y,color);
         }
         error = error+deltay;
         if(error>=0){
             y = y + y_step;
             error = error-deltax;
         }
     }
     return;
 }

 static void __exit stop_video(void){
     /* unmap the physical-to-virtual mappings */
     iounmap (LW_virtual);
     iounmap ((void *) ONCHIP_virtual);
     iounmap((void *) SDRAM_virtual);

     /* Remove the device from the kernel */
     if(video_registered){
         misc_deregister(&video);
         printk(KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
     }
 }
 static int device_open(struct inode *inode, struct file *file){
     return SUCCESS;
 }
 static int device_release(struct inode *inode, struct file *file){
     return 0;
 }

 static ssize_t device_read(struct file *filp, char *buffer,size_t length, loff_t *offset){

     size_t bytes;


     sprintf(video_msg,"%s \n %s \n",firstFive,firstFour);
     sprintf(video_msg,"%d %d\n", resolution_x,resolution_y);

     bytes = strlen(video_msg) - *offset;

     bytes = bytes > length ? length : bytes;

     if(bytes)
         (void) copy_to_user(buffer,&video_msg[*offset],bytes);
     *offset = bytes;
     return bytes;
 }

 static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset){
     size_t bytes;
     bytes = length;

     char pixelInfo[bytes];

     (void) copy_from_user(pixelInfo, buffer,bytes);
     pixelInfo[bytes] = '\0';

     strncpy(firstThree,pixelInfo,3);
     strncpy(firstFour,pixelInfo,4);
     strncpy(firstFive,pixelInfo,5);

     firstThree[4] = '\0';
     firstFour[5] = '\0';
     firstFive[6] = '\0';

     char pixelStr[] = "pixel";
     char lineStr[] = "line";
     char syncStr[] = "sync";
     char boxStr[] = "box";
     char clearStr[] = "clear";
     char textStr[] = "text";
     char eraseStr[] = "erase";
     short int color;

     if(strcmp(firstFive,clearStr)==0)
         clear_screen();
     else if(strcmp(firstFive,pixelStr)==0){
         int x;
         int y;
         sscanf(pixelInfo,"pixel %d,%d %hx", &x,&y,&color);
         plot_pixel(x,y,color);
     }else if(strcmp(firstFour,textStr)==0){
         int x,y;
         char str[79];
         sscanf(pixelInfo, "text %d,%d %s",&x,&y,str);
         write_text(x,y,str);
     } else if(strcmp(firstFour,lineStr)==0){
         int x1,x2,y1,y2;
         sscanf(pixelInfo,"line %d,%d %d,%d %hx",&x1,&y1,&x2,&y2,&color);
         draw_line(x1,x2,y1,y2,color);
     } else if(strcmp(firstFour,syncStr)==0){
         wait_for_vsync();
     } else if(strcmp(firstThree,boxStr)==0){
         int x1,x2,y1,y2;
         sscanf(pixelInfo,"box %d,%d %d,%d %hx",&x1,&y1,&x2,&y2,&color);
         draw_box(x1,x2,y1,y2,color);
     } else if(strcmp(firstFive,eraseStr)==0){
         erase();
     }

     return bytes;
 }

 MODULE_LICENSE("GPL");
 module_init (start_video);
 module_exit (stop_video);

