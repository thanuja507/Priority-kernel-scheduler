#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

int main(){
    head(4,"-n 2 OS611_example.txt OS611_example.txt");
    getstats(getpid());
    exit();
}