#include<vector>
#include<thread>
#include<iostream>
#include<algorithm>
#include<atomic>

using namespace std;

int main()
{
    vector<int>Q(128);
    vector<int>R(1024);

    atomic<int> head = 0;
    atomic<int> tail = 0;


    thread producer([&]()
    {
        int idx =0;

        while(idx < 1024)
        {
            if(head.load() == (tail.load() + 1)%128)
            {
                continue;
            }

            Q[tail.load()]=idx;
            idx++;
            tail.store((tail.load() + 1) % 128);
        }
    });

    thread consumer([&]()
    {
        int idx=0;
        while (idx < 1024)
        {
            if(head.load() == tail.load())
            {
                continue;
            }

            R[idx++] = Q[head];
            head.store((head.load() + 1) % 128);
        }
    });


    consumer.join();
    producer.join();


    for(int i=0;i<1024;i++)
    {
        if(i!=R[i])
        {
            cout<<"error\n"<<i;
            break;
        }
            
    }

    return 0;
}
