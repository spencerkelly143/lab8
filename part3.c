 #include <stdio.h>
 #include <string.h>
 #include <errno.h>
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <unistd.h>
 #include <math.h>
 #include <pthread.h>
 #include <signal.h>
 #include "address_map_arm.h"
 #include <linux/input.h>
 #define TAU 6.28318531
 #define SAMPLE_RATE 8000
 #define KEY_RELEASED 0
 #define KEY_PRESSED 1
 #define LAG 0.0004
 volatile sig_atomic_t stop = 0;
 void * LW_virtual;
 double freq[] = {261.626,277.183,293.665,311.127,329.628,349.228,369.994,391
 .995,415.305,440.000,466.164,493.883,523.251};
 int num_notes;
 int notes[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

 int open_physical (int);
 void * map_physical (int,unsigned int, unsigned int);
 void close_physical(int);
 int unmap_physical(void *, unsigned int);
 int get_note(int);
 double tone_volume[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

 void sigintHandler(int sig){
     stop =1;
 }

 void *audio_thread(){
     volatile int * aud_ptr;
     int fd = -1;
     int nth_sample;
     int vol = 0x7FFFFFFF;
     int tone;
     int wslc,wsrc;
     int i;

     /*char str[14];


     strcpy(str,argv[1]);

     for(i=0;i<strlen(str);i++){
         if(((int) *(str+i))-48 == 1){
             *(notes+num_notes) = i;
             num_notes++;
         }
     }*/

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
     nth_sample = 0;
     while(!stop){
         vol = 0x7FFFFFFF/13;
         nth_sample = (nth_sample+1)%(100*SAMPLE_RATE);
         tone = 0;
         for(i=0;i<13;i++){
             if(notes[i])
                 tone_volume[i] = 1.00;
             tone = tone+tone_volume[i]*vol*sin(freq[i]*TAU*nth_sample/SAMPLE_RATE);
             tone_volume[i] = (tone_volume[i]-LAG>0) ? tone_volume[i]-LAG : 0;

         }


         while((*(aud_ptr+1)>>24 & 0xFF) < 40){
             tone = tone;
         }

         *(aud_ptr+2) = tone;
         *(aud_ptr+3) = tone;
     }
 }

 int main(int argc,char *argv[]){
     int fd = -1;
     int i,key,err;
     struct input_event ev;
     pthread_t tid;

     num_notes = 0;
     signal(SIGINT,sigintHandler);

     int event_size = sizeof(struct input_event);

     if((err = pthread_create(&tid, NULL, &audio_thread,NULL)) != 0){
         printf("pthread_create failed");
         return -1;
     }

     if((fd=open(argv[1], O_RDONLY | O_NONBLOCK))==-1){
         printf("could not open keyboard");
         return -1;
     }

     while(!stop){
         if(read(fd,&ev,event_size)<event_size){
             continue;
         }
         if(ev.type==EV_KEY && ((ev.value==0) || (ev.value==1))){
             if(ev.value==KEY_PRESSED){
                 if((key=get_note((int) ev.code))!=-1){
                     notes[key] = 1;
                     num_notes++;
                 }
             }
             if(ev.value==KEY_RELEASED){
                 if((key=get_note((int) ev.code))!=-1){
                     notes[key] = 0;
                     num_notes--;
                 }
             }
         }
     }

     pthread_cancel(tid);

     pthread_join(tid,NULL);

     unmap_physical(LW_virtual,LW_BRIDGE_SPAN);
     close_physical(fd);

     return 0;
 }

 int get_note(int key){
     if(key == 0x0010){
         return 0;
     } else if(key == 0x0003){
         return 1;
     } else if(key == 0x0011){
         return 2;
     }else if(key==0x004){
         return 3;
     } else if(key == 0x0012){
         return 4;
     } else if(key == 0x0013){
         return 5;
     }else if (key == 0x006){
         return 6;
     } else if (key == 0x0014){
         return 7;
     } else if (key == 0x007){
         return 8;
     } else if (key==0x0015){
         return 9;
     } else if (key==0x0008){
         return 10;
     } else if (key == 0x0016){
         return 11;
     } else if(key == 0x0017){
         return 12;
     }
     return -1;
 }
 int open_physical (int fd){
     if (fd == -1) // check if already open
         if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1){
             printf ("ERROR: could not open \"/dev/mem\"...\n");
             return (-1);
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
         printf("Error: munmap failed ... \n");
         return (-1);
     }
     return 0;
 }

