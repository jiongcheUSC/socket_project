#include "common.h"

int main(int argc ,char *argv[])
{
	return client("client A",SERVER_M_TCP_PORT_A,argc,argv);
}
