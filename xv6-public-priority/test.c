#include "types.h"
#include "user.h"


int main(int argc, char* argv[]){
    int i,k=2, n=atoi(argv[1]);
    int pids[4];
    for (i = 0; i < n; i++) {
        int f = fork();
        if ( f== 0) {
            char *args1[] = {argv[k],"OS611_example.txt",0};
            setpriority(getpid(),atoi(argv[k+1]));
            exec(argv[k],args1);
        }
        else{
            pids[i]=f;
            k+=2;
        }
    }
    for(i=0;i<n;i++){
        wait();
    }
    int *pp = pids;
    avgtimes(pp);
    exit();
}

