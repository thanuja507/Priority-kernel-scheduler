#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

void read_print(int fd, int n_lines){
    char buffer[512];
    int cnt=0,n=0;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0);
    char *p = buffer;
    while(*p && cnt<n_lines){
        printf(1,"%c",*p);
        if(*p=='\n'){
            cnt+=1;
        }
        p++;
    }
}

int main(int argc, char *argv[]){

    printf(1,"Head command is getting executed in user mode\n");

    int n_lines = 14,argp=1;

    if(startswith(argv[1],"-")==0){
        n_lines= atoi(argv[2]);
        argp=3;
    }
    int flag = argc-argp > 1 ? 1 : 0 , flag_w=0;
    while(argp<argc){
        flag_w=1;
        if(flag==1){
            printf(1,"==> %s <==\n",argv[argp]);
        }
        int fd = open(argv[argp],O_RDONLY);
        read_print(fd,n_lines);
        argp++;
    }
    if(flag_w!=1){
        read_print(0,n_lines);
    }
    // procstat(getpid());
    getstats(getpid());
    exit();
}