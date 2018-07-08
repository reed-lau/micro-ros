#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <deque>
#include <condition_variable>

class transport
{
public:
   transport() = default;

   bool create(int type, void *info) 
   {
       int fd;

       fd = socket(PF_INET, SOCK_STREAM, 0);
       if (-1==fd)
           return false;

       struct sockaddr_in addr;
       socklen_t len;

       addr.sin_family = PF_INET;
       addr.sin_addr.s_addr = inet_addr("127.0.0.1");
       addr.sin_port = htons(9000);

       len = sizeof(addr);
       int ret = bind(fd, (struct sockaddr*)(&addr), len);

       if ( -1 == ret )
       {
          fprintf(stderr, "bind fail\n");
          return false;
       }

       ret = listen(fd, 32);
       if ( -1 == ret )
       {
           fprintf(stderr, "listen fail\n");         
           return false;
       }

       sockaddr_in addr1;
       socklen_t len1;

       fprintf(stderr, "before accept\n");
       len1 = sizeof(addr1);
       ret = accept(fd, (struct sockaddr*)(&addr1), &len1);        
       if ( -1 == ret )
       {
           fprintf(stderr, "accept fail\n");         
           return false;
       }

       fd_pub = ret;

       fprintf(stderr, "accept=%d\n", ret);

       return true;
   }
   
   bool attach(int type,void *info)
   {
       int fd;

       fd = socket(PF_INET, SOCK_STREAM, 0);
       if (-1==fd)
           return false;

       struct sockaddr_in addr;
       socklen_t len;

       addr.sin_family = PF_INET;
       addr.sin_addr.s_addr = htonl(INADDR_ANY);; 
       addr.sin_port = htons(9001);

       len = sizeof(addr);
       int ret = bind(fd, (struct sockaddr*)(&addr), len);
       if ( -1 == ret )
       {
          fprintf(stderr, "bind fail\n");  
          return false;
       }

       struct sockaddr_in addr1;
       socklen_t len1;

       addr1.sin_family = PF_INET;
       addr1.sin_port   = htons(9000);
       addr1.sin_addr.s_addr = inet_addr("127.0.0.1");

       ret = connect(fd, (struct sockaddr*)(&addr1), len);
       if ( -1 == ret )
       {
          fprintf(stderr, "connect fail\n");  
          return false;
       }

       printf("connect=%d\n", ret);

       fd_sub = fd;

       return true;
   }

   bool start()
   {
        return true;
   } 

   bool stop()
   {
        return true;
   }

   bool publish(const std::string &msg)
   {
        int ret;
        size_t left, sent;   
        char *p;

        size_t size = msg.size(); 

        /* send size field */
        p = (char*)(&size);
        left = sizeof(size_t);
        while ( left > 0 )
        {
           sent = write(fd_pub, p, left);
           left -= sent;
           p += sent;
        }

        /* send data field */
        p = (char*)(msg.c_str());
        left = msg.size();
        while ( left > 0 )
        {
           sent = write(fd_pub, p, left); 
           left -=sent; 
           p += sent;
        }

        return true;
   }

   bool gather(std::string &msg) 
   {
       size_t size; 
       size_t recv, left;

       char *p;

       /* recv size field */
       p = (char*)(&size);
       left = sizeof(size_t);
       while( left > 0)
       {
          recv = read(fd_sub, p, left);
          left -= recv;
          p += recv;
       }

       /* recv data field */
       char buf[4096]; 

       msg.clear();
       left = size;
       while ( left > 0)
       {
           recv = read(fd_sub, buf, 4095);
           left -= recv;
           buf[recv] = 0;
           msg += buf; 
       } 

       if ( size != msg.size() )
       {
           fprintf(stderr, "size not consistant\n");
           return false;
       }

       return true;
   }
   
   /* timeout ms */
   /* 0: ok      */
   /* 1: timeout */
   /*-1: fail    */
   int wait(int64_t timeout)
   {
       std::cv_status ret;

       std::unique_lock<std::mutex> lock(mutex);     
       while( bufs_.empty() )
       {
          ret = cond.wait_for(lock, std::chrono::milliseconds(timeout));
       }

       lock.unlock();

       if ( std::cv_status::timeout == ret)
       {
           return 0;
       } else {
           return 1;
       }

       return -1;
   }

   bool take(std::string &msg)
   {
       std::lock_guard<std::mutex> lock(mutex);
       msg = bufs_.front();
       bufs_.pop_front();
       return true;
   }

   bool start_listen()
   {
       fprintf(stderr, "start_listen\n");
       th_ = std::thread( [this](){
           while(1)
           {
               std::string msg;
               if ( gather(msg) )
               {
                   std::unique_lock<std::mutex> lock(mutex);     
                   bufs_.push_back(msg);
                   // std::cout<<msg<<std::endl;
                   cond.notify_one();  
                   lock.unlock();
               } else {
                   printf("gather else\n");
               }
           }

       });
       return true;
   }
   
private:
   int fd_pub;
   int fd_sub;

   std::deque<std::string> bufs_;

   std::thread th_;
   std::mutex mutex;
   std::condition_variable cond;
};

class publisher
{
public:
    publisher(const std::string &topic)
    {
        topic_ = topic; 
        tr_.create(0, NULL);
    }

    bool publish(const std::string &msg)
    {
         tr_.publish(msg);
         std::cout<<msg<<std::endl;
         return true;
    }

private:
    std::string topic_;
    transport tr_;
};

typedef std::function<bool(std::string)> callback_func;

class subscription
{
friend class node;

public:
    subscription(const std::string &topic, callback_func cb)
    {
         topic_ = topic;
         cb_ = cb;
         tr_.attach(0, NULL);

         tr_.start_listen();
    }

    void spin()
    {
        int ret;
        while(1)
        {
            ret = tr_.wait(1000);
            if ( ret == 0) /* time out */
            {
               // fprintf(stderr, "wait =%d\n", ret);
            } else if ( ret == 1 ) /* data arrived */
            {
               // fprintf(stderr, "data arrived\n");
               std::string msg;
               ret = tr_.take(msg);
               if ( !ret )
                  continue;

               cb_(msg);
              
            } else {
               fprintf(stderr, "wait fail\n");
            }
        }
      
    }

    bool take(std::string &msg)
    {
//        printf("called take in sub\n");
        return tr_.take(msg);
    }

private:
    std::string topic_;
    std::function<bool(std::string)> cb_;
    transport tr_;
};

class node
{
public:
    node(const std::string &name="node" )
    {
        name_ = name; 
    }
    
    publisher *create_publisher(const std::string &topic)
    {
         publisher *p = new publisher(topic);
         return p;
    }
   
    subscription *create_subscription(const std::string &topic, callback_func cb)
    {
         subscription *p = new subscription(topic, cb);
         return p;
    }

    void spin()
    {
    }
private:
    std::string name_;
     
};


