#include <iostream>
#include <string>

#include "node.hpp"

bool cb(std::string msg)
{
    std::cout<<msg <<"aaaaa"<<std::endl;

    return true;
}

int main(int argc,char *argv[])
{
   node n; 

   auto sub = n.create_subscription("chatter", cb);
   
   sub->spin();
}
