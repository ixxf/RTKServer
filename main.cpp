#include "RTKServer.h"
#include  <iostream>

int main(int argc, char *argv[])
{
    RTK::RTKServer server;
    server.start();
    return 0;
}
