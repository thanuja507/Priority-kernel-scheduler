#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

void print_line(char *line){
    while(*line){
        printf(1,"%c",*line);
        line++;
    }
}

int main(int argc, char *argv[]){

    printf(1,"Uniq command is getting executed in user mode\n");

    int flag_c = 1, flag_d = 1, flag_i = 1;
    char file[30];

    if(startswith(argv[1],"-")==0){
        flag_c = strcmp(argv[1],"-c");
        flag_d = strcmp(argv[1],"-d");
        flag_i = strcmp(argv[1],"-i");
        if(argc==3){
        strcpy(file,argv[2]);
        }
    }
    else{
        if(argc==2){
        strcpy(file,argv[1]);
        }
    }
    char buffer[512];
    int fd,n;
    if(strlen(file)>0){
        fd = open(file,O_RDONLY);
    }
    else{
        fd=0;
    }
    while ((n = read(fd, buffer, sizeof(buffer))) > 0);
    char *p= buffer;
    char curr[128],prev[128],data[512];
    int i =0,line_counts[10],counter=-1,j=0;
    while(*p){
        curr[i]=*p;
        if(*p=='\n'){
            i+=1;
            curr[i]='\0';
            if(flag_c==0 || flag_d==0){
                if(strcmp(curr,prev)!=0){
                    int temp =0;
                    while(temp<i){
                        data[j]=curr[temp];
                        temp++;
                        j++;
                    }
                    counter+=1;
                    line_counts[counter]=1;
                }
                else{
                    line_counts[counter]+=1;
                }
            }
            else if(flag_i==0){
                if(stricmp(curr,prev)!=0){
                print_line(curr);
                }
                i=-1;
            }
            else{
                if(strcmp(curr,prev)!=0){
                print_line(curr);
                }
            }
        strcpy(prev,curr);
        i=-1;
        }
        i++;
        p++;
    }
    if(flag_c==0){
        data[j]='\0';
        char *q=data;
        int i =0;
        while(*q){
            if(i==0){
                printf(1,"\t%d ",line_counts[i]);
                i++;
            }
            printf(1,"%c",*q);
            if(*q=='\n' && line_counts[i]!=0){
                printf(1,"\t%d ",line_counts[i]);
                i++;
            }
            q++;
        }
    }
    if(flag_d==0){
        data[j]='\0';
        char *q=data;
        int i =0;
        while(*q){
            if(line_counts[i]>1){
                printf(1,"%c",*q);
            }
            if(*q=='\n'){
                i+=1;
            }
            q++;
        }
    }
    // procstat(getpid());
    getstats(getpid());
exit();
}