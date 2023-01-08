#include "common.h"

int main(int argc ,char *argv[])
{
	return client("client B",SERVER_M_TCP_PORT_B,argc,argv);
}

