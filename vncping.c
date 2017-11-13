#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <stdarg.h>

void die(int retcode,const char *msgfmt, ...) {
  va_list ap;
  
  va_start(ap,msgfmt);
  vfprintf(stderr,msgfmt,ap);
  va_end(ap);
  exit(retcode);
}

static char RFB_VERSION[] = "RFB 003.003\n";

int main(int argc, char **argv) {
  struct hostent *phe;
  struct sockaddr_in host_addr;
  char *s_hostname;
  int s_host_port, host_fd, i;
  char buf[256];
  if (argc != 3) die(__LINE__,"Usage:\n\t%s host port\n", argv[0]);

  s_hostname = argv[1];
  s_host_port = atoi(argv[2]);

  fprintf(stderr,"Connecting to %s, port %d\n", s_hostname, s_host_port);

  phe = gethostbyname(s_hostname);
  if (phe == NULL) die(__LINE__,"gethostbyname(%s): %s\n", s_hostname, strerror(errno));

  host_addr.sin_family = AF_INET;
  memcpy(&host_addr.sin_addr.s_addr, phe->h_addr, phe->h_length);
  host_addr.sin_port = htons((unsigned short)s_host_port);

  host_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (host_fd == -1) die(__LINE__,"socket: %s\n", strerror(errno));
  if (connect(host_fd, (struct sockaddr *)&host_addr,
                sizeof(host_addr)) != 0) die(__LINE__,"connect: %s\n", strerror(errno));
  i = read(host_fd,buf,sizeof(buf)-1);
  if (i == -1 || i == 0) die(__LINE__,"Error reading RFB protocol version (%d)\n", i);
  buf[i] = 0;
  fprintf(stderr,"Server protocol: %s\n", buf);
  write(host_fd,RFB_VERSION, sizeof(RFB_VERSION)-1);
  i = read(host_fd,buf,sizeof(buf)-1);
  if (i != 4)  die(__LINE__,"Error reading Security word (%d)\n", i);
  if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1)
    die(__LINE__,"Unsupported security type: %02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
  write(host_fd,"\1",1);
  read(host_fd,buf,sizeof(buf)-1);
  close(host_fd);
  exit(0);
}
