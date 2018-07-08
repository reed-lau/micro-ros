#include <unistd.h>
#include <iostream>
#include <string>
#include "node.hpp"

int main(int argc,char *argv[])
{
   node n; 

   auto pub = n.create_publisher("chatter");

   int i = 0;
   while(1)
   {
       std::string msg = "msg" + std::to_string(i); 
       pub->publish(msg); 

       ++i;
       usleep(50000);
   }
}
