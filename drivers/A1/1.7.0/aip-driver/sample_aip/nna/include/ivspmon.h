#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#if 0//FPGA
#define OPENPMON                                        \
    char perf_array[48] = {0};                          \
    int fperf = open("/proc/jz/pmon/perform",O_RDWR);   \
    if(fperf <= 0)                                      \
        printf("open pmon error!\n");

#define CLOSEFS close(fperf)

#define START0 memset(perf_array,0,48); write(fperf,"perf0",5);
#define START1 memset(perf_array,0,48); write(fperf,"perf1",5);
#define START2 memset(perf_array,0,48); write(fperf,"perf2",5);
#define START3 memset(perf_array,0,48); write(fperf,"perf3",5);
#define START4 memset(perf_array,0,48); write(fperf,"perf4",5);
#define START5 memset(perf_array,0,48); write(fperf,"perf5",5);
#define START6 memset(perf_array,0,48); write(fperf,"perf6",5);
#define START7 memset(perf_array,0,48); write(fperf,"perf7",5);
#define START8 memset(perf_array,0,48); write(fperf,"perf8",5);
#define START9 memset(perf_array,0,48); write(fperf,"perf9",5);
#define START10 memset(perf_array,0,48); write(fperf,"perf10",6);
#define START48 memset(perf_array,0,48); write(fperf,"perf48",6);
#define START49 memset(perf_array,0,48); write(fperf,"perf49",6);
#define START50 memset(perf_array,0,48); write(fperf,"perf50",6);
#define START51 memset(perf_array,0,48); write(fperf,"perf51",6);
#define START52 memset(perf_array,0,48); write(fperf,"perf52",6);
#define START53 memset(perf_array,0,48); write(fperf,"perf53",6);
#define START54 memset(perf_array,0,48); write(fperf,"perf54",6);
#define START55 memset(perf_array,0,48); write(fperf,"perf55",6);
#define START56 memset(perf_array,0,48); write(fperf,"perf56",6);
#define START57 memset(perf_array,0,48); write(fperf,"perf57",6);
#define START58 memset(perf_array,0,48); write(fperf,"perf58",6);
#define START59 memset(perf_array,0,48); write(fperf,"perf59",6);
#define STOP()                                  \
    lseek(fperf,1,SEEK_SET);                    \
    read(fperf,perf_array,47);                  \
    write(fperf,"perfstop",8);                  \
    printf("%s",perf_array);

#else//palladium
#define PMON_SIZE 200
#define STR_SIZE 50
#define OPENPMON                                               \
    char* perf_array=(char*)malloc(PMON_SIZE*STR_SIZE);        \
    printf("size=%d\n",PMON_SIZE*STR_SIZE);                    \
    int fperf = open("/proc/jz/pmon/perform",O_RDWR);          \
    if(fperf <= 0)                                             \
    {                                                          \
        printf("open pmon error!\n");                          \
    }                                                          \
    FILE* out_txt = fopen("time.txt", "w");                    \
    if(!out_txt){                                              \
        printf("open out.txt failed\n");                       \
    }

#define CLOSEFS                                   \
    if(1){                                        \
        close(fperf);                             \
        fclose(out_txt);                          \
        free(perf_array);                         \
    }

#define START0 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8080",8);
#define START1 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8181",8);
#define START2 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8282",8);
#define START3 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8383",8);
#define START4 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8484",8);
#define START5 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8585",8);
#define START6 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8686",8);
#define START7 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8787",8);
#define START8 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8888",8);
#define START9 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8989",8);
#define START10 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8a8a",8);
#define START11 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8b8b",8);
#define START12 memset(perf_array,0,STR_SIZE*PMON_SIZE); write(fperf,"perf8c8c",8);
#define STOP()                                  \
    lseek(fperf,0,SEEK_SET);                    \
    read(fperf,perf_array,STR_SIZE*PMON_SIZE);  \
    write(fperf,"perf0000",8);                  \
    fprintf(out_txt,"%s",perf_array);

#endif

