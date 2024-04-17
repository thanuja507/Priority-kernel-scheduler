//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
// ###########################################################################
int 
sys_hello(void){
  cprintf("Hello command is getting executed in kernel mode\n");
  return 0;
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int 
startswith(char *s1,char *s2){
  int len = strlen(s2);
  return strncmp(s1,s2,len);
}

// Conduct a case-sensitive comparison.
int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

// Perform a case-insensitive comparison
int
stricmp(const char *p, const char *q)
{
  while(*p){
    char a,b;
    a=*p;
    b=*q;
  if(a>64 && a<91){
	a+=32;
	}
  if(b>64 && b<91){
	b+=32;
	}
  if(a!=b){
    break;
  }
    p++, q++;
  }
  return (uchar)*p - (uchar)*q;
}

// Open the file specified by filename, read data into the buffer from it.
int open_read(char *filename,char *buffer){
    struct file *f;
    struct inode *ip;
    int fd,r;
    begin_op();
    if((ip = namei(filename)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
      if(f)
        fileclose(f);
      iunlockput(ip);
      end_op();
      return -1;
    }
    iunlock(ip);
    end_op();
    f->ip = ip;
    f->off = 0;
    ilock(f->ip);
    if((r = readi(f->ip, buffer, f->off, 512)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
}

int 
sys_uniq(void){
  cprintf("Uniq command is getting executed in kernel mode\n");
    int flag_c = 1, flag_d = 1, flag_i = 1;
    char *argv1,*argv2,file[30];
    argstr(0, &argv1);
    argstr(1, &argv2);
    if(startswith(argv1,"-")==0){ // Verify if the first argument is an option.
        flag_c = strcmp(argv1,"-c");
        flag_d = strcmp(argv1,"-d");
        flag_i = strcmp(argv1,"-i");
        strcpy(file,argv2);
    }
    else{
        strcpy(file,argv2);
    }
    char buffer[512];
    if(strlen(file)>0){
        open_read(file,buffer); // Open the file specified by filename, read data into the buffer from it.
    }
    // else{
    //     fd=0;
    // }
    // while ((n = read(fd, buffer, sizeof(buffer))) > 0);
    char *p= buffer;
    char curr[128],prev[128],data[512];
    int i =0,line_counts[10],counter=-1,j=0;
    while(*p){
        curr[i]=*p;
        if(*p=='\n'){
            i+=1;
            curr[i]='\0';
            if(flag_c==0 || flag_d==0){ // Verify if -c or -d flags are set.
                if(strcmp(curr,prev)!=0){
                    int temp =0;
                    while(temp<i){
                        data[j]=curr[temp];  // Copy the current line to the data buffer.
                        temp++;
                        j++;
                    }
                    counter+=1; // Increments a counter to monitor distinct lines.
                    line_counts[counter]=1; // Updates an array line_counts to count the instances of each separate line
                }
                else{
                    line_counts[counter]+=1;
                }
            }
            else if(flag_i==0){
                if(stricmp(curr,prev)!=0){ // Perform a case-insensitive comparison
                  cprintf("%s",curr);
                }
                i=-1;
            }
            else{
                if(strcmp(curr,prev)!=0){ // Conduct a case-sensitive comparison.
                  cprintf("%s",curr);
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
                cprintf("\t%d ",line_counts[i]);
                i++;
            }
            cprintf("%c",q); // Data array is printed
            if(*q=='\n' && i<=counter){
              cprintf("\t%d ",line_counts[i]); // Corresponding count is printed along with the data line. 
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
                cprintf("%c",q); // Lines that appear more than once in the data array are printed.
            }
            if(*q=='\n'){
                i+=1;
            }
            q++;
        }
    }
  return 0;
}

// Function to read and print a specified number of lines from a fd.
void read_print(char *filename,int n_lines){
    char buffer[512];
    int cnt=0;
    int n = open_read(filename,buffer); // Open the file, read data into buffer.
    buffer[n]='\0';
    char *p = buffer;
    while(*p && cnt<n_lines){
        cprintf("%c",p);
        if(*p=='\n'){
            cnt+=1; // Count the lines that came across.
        }
        p++;
    }
}

// Converts strings that represent integers into actual integer values. 
int
atoi(const char *s)
{
  int n;
  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

int
sys_head(){
  cprintf("Head command is getting executed in kernel mode\n");
  char *arg;
  int argc,i=0,flag=0,flag_w=0;
  argint(0,&argc); // Extract the number of arguments.
  argstr(1,&arg); // Extract the command-line argument string.
  char *argv[argc];
  char *p=arg;
  while (*p) {
    if (*p == ' ') {
        *p = '\0'; // Replace space with a null terminator to separate arguments.
        flag = 0;
        p++;
    }
    if (flag == 0) {
        argv[i] = p;
        i++;
        flag = 1;
    }
   p++;
  }

  int n_lines = 14,argp=0;

  if(startswith(argv[0],"-")==0){ // Verify if the first argument starts with a '-' character which indicates the number of lines.
        n_lines= atoi(argv[1]);
        argp=2;
    }
    flag = argc-argp > 1 ? 1 : 0; // Verify whether there are multiple input files.
    while(argp<argc){
        flag_w=1;
        if(flag==1){
            cprintf("==> %s <==\n",argv[argp]); // Print file header(arrows) if more than one file are being processed.
        }
        // int fd = open(argv[argp],O_RDONLY);
        read_print(argv[argp],n_lines);
        argp++;
    }
    if(flag_w!=1){
        read_print(0,n_lines);
    }
  return 0;
}