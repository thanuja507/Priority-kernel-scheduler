#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

int main()
{
    uniq("-c","OS611_example.txt");
    // uniq("","");
    // procstat(getpid());
    getstats(getpid());
    exit();
}