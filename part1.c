
 #include <stdio.h>
 #include <string.h>
 #include <errno.h>
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <unistd.h>
 #include <math.h>
 #include "address_map_arm.h"

 #define TAU 6.28318531
 #define SAMPLE_RATE 8000

 int open_physical (int);
 void * map_physical (int,unsigned int, unsigned int);
 void close_physical(int);
 int unmap_physical(void *, unsigned int);

 int main(int argc,char *argv){
     volatile int * aud_ptr;
     int fd = -1;
     void *LW_virtual;
     int nth_sample;
                                
     void *LW_virtual;
     int nth_sample;
     double freq[] = {261.626,277.183,293.665,311.127,329.628,349.228,369.994,391.995,415.305,440.000,466.164,493.883,523.251};
     int vol = 0x7FFFFFFF;
     int tone;
     int wslc,wsrc;
     if((fd=open_physical(fd))==-1)
         return (-1);
     if(!(LW_virtual = map_physical(fd,LW_BRIDGE_BASE,LW_BRIDGE_SPAN)))
         return (-1);

     aud_ptr = (int *) (LW_virtual + AUDIO_BASE);
     *aud_ptr = 0x8;
     *aud_ptr = 0x3;
     *aud_ptr = *aud_ptr&0xFFFFFFF7;
     wslc = (*(aud_ptr+1)>>24 &0xFF);
     wsrc = (*(aud_ptr+1)>>16 & 0xFF);
     int t;
     for(nth_sample = 0; nth_sample<SAMPLE_RATE*3.9 ; nth_sample++){

         tone = vol*sin(freq[nth_sample/2400]*TAU*nth_sample/SAMPLE_RATE);
         while((*(aud_ptr+1)>>24 & 0xFF) < 40){
                                                           
         tone = vol*sin(freq[nth_sample/2400]*TAU*nth_sample/SAMPLE_RATE);
         while((*(aud_ptr+1)>>24 & 0xFF) < 40){
             tone = vol*sin(freq[nth_sample/2400]*TAU*nth_sample/SAMPLE_RATE);
         }

         *(aud_ptr+2) = tone;
         *(aud_ptr+3) = tone;
     }
     printf("%d\n",t);
     unmap_physical(LW_virtual,LW_BRIDGE_SPAN);
     close_physical(fd);
     return 0;
 }

 int open_physical (int fd){
     if (fd == -1) // check if already open
         if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1){
             printf ("ERROR: could not open \"/dev/mem\"...\n");
             return (-1);
         }
     return fd;
 }
                                                         
     return fd;
 }
 /* Close /dev/mem to give access to physical addresses */
 void close_physical (int fd){
     close (fd);
 }
 /* Establish a virtual address mapping for the physical addresses starting
 * at base and extending by span bytes */
 void* map_physical(int fd, unsigned int base, unsigned int span){
     void *virtual_base;
     // Get a mapping from physical addresses to virtual addresses
     virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED,fd, base);
     if (virtual_base == MAP_FAILED){
         printf ("ERROR: mmap() failed...\n");
         close (fd);
         return (NULL);
     }
     return virtual_base;
 }

 int unmap_physical(void * virtual_base, unsigned int span){
     if(munmap(virtual_base,span)!=0){
                                                           
 int unmap_physical(void * virtual_base, unsigned int span){
     if(munmap(virtual_base,span)!=0){
         printf("Error: munmap failed ... \n");
         return (-1);
     }
     return 0;
 }

