#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

virConnectPtr local_connect(void)
{
    virConnectPtr conn;
    conn = virConnectOpen("qemu:///system");
    if (conn == NULL) {
	perror("virConnectOpen");
	exit(-1);
    }
    return conn;
}

int main (int argc, char** argv)
{
  printf("- vCPU scheduler - \n");
  printf("Connecting to Libvirt... \n");
  local_connect();
  printf("Connected! \n");
  return 0;
}
