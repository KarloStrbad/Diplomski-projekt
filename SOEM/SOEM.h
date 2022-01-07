#include "osal/osal_defs.h"
#include "soem/ethercat.h"

int hal_ethernet_open(void);
void hal_ethernet_close(void);
int hal_ethernet_send(unsigned char *data, int len);
int hal_ethernet_recv(unsigned char **data);
