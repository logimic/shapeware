#include <Shaper.h>
#include <Trace.h>
#include <iostream>

TRC_INIT_MNAME("startup");

int main(int argc, char** argv)
{
  std::cout << "startup ... " << std::endl;
  shapeInit(argc, argv);
  int retval = shapeRun();
  return retval;
}
