
#include <string>
#include <iostream>
#include "graph.cpp"
#include "fileReader.cpp"
#include "utimer.cpp"
#include "myqueue.cpp"

#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <cstddef>
#include <math.h>
#include <string>
#include <functional>


using namespace std;

const int VERTEX_NUM = 100000000;

void delay(std::chrono::milliseconds m) {
    #ifdef ACTIVEWAIT
        auto active_wait = [] (std::chrono::milliseconds ms) {
                    long msecs = ms.count();
                    auto start = std::chrono::high_resolution_clock::now();
                    auto end   = false;
                    while(!end) {
                    auto elapsed = std::chrono::high_resolution_clock::now() - start;
                    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                    if(msec>msecs)
                    end = true;
                    }
                    return;
                    };
        active_wait(m);
    #else
        std::this_thread::sleep_for(m);
    #endif
    return;
}

void source2farm(Graph &g, vector<myqueue<int>> &q, myqueue<int> &srcToFarm, int nw) {
    int turn = 0;
    int startVertex = 0;
    int countIteration = 0;
    while(true) { //as far as there is a vertex value in queue, go on

        startVertex = srcToFarm.front(); //retrieve vertex value from queue

        // cout << "t: " << startVertex << endl;

        srcToFarm.pop();

        countIteration++;
        q[turn].push(startVertex);
        turn = (turn + 1) % nw;

        if(srcToFarm.empty() && countIteration > 1) {
            break;
        }
    }

    #ifdef PROGRESSPRINT
        std::cout << "sent EOS: " << std::this_thread::get_id() << std::endl;
        std::cout << "queue size: " << srcToFarm.size() << std::endl;
    #endif
    
    for(int i=0; i < nw; i++) {
        q[i].push(EOS);
    }
    
    return;
}

void genericfarmstage(Graph &g, myqueue<int> &inq, myqueue<int> &outq, myqueue<int> &srcToFarm, 
                      vector<mutex> &mtx, int &countOccurence, int searchValue) {
    auto e = inq.pop();

    #ifdef PROGRESSPRINT
        cout << "Farm received (thread id): " << std::this_thread::get_id() << " e: " << e << endl;
    #endif

    while(e != EOS) {
        for(auto n : g.adjvectors[e]){ //if vertex 1 is poped from queue, then we are dealing with all vertexes connected to vertex 1 in this pass
            if(g.visited[n] != 1){ //if the vertex has not yet been visited, go on
                mtx[n].lock();
                g.visited[n] = 1; //mark vertex as visited
                mtx[n].unlock();
                if(n == searchValue)
                    countOccurence++;

                srcToFarm.push(n); //insert vertex value to queue
            }
        }
        e = inq.pop();
    }
    if(e == EOS) {
        outq.push(EOS);
    }

    #ifdef PROGRESSPRINT
        cout << "Source emitted to drain: " << std::this_thread::get_id() << endl;
    #endif
      
    return;
}

void drainfarm(Graph &g, int &countOccurence, myqueue<int> &drain, int nw) {
    #ifdef PROGRESSPRINT
        std::cout << "Drain started" << std::endl;
    #endif
    int countEos = 0, e = 0; 
    do {
        if(e == EOS) {
            countEos++;
            if(countEos == nw) {
                // cout << "countEos: " << countEos << endl;
                cout << "Occurence No: " << countOccurence << endl;
                #ifdef PROGRESSPRINT
                    std::cout << "countEos: " << countEos << std::endl;
                #endif
                break;
            } else {
                continue;   // cot first EOS, look for the second one
            }    
        }
        e = drain.pop();
    } while(true);

    #ifdef PROGRESSPRINT
        std::cout << "Drain finished: " << std::this_thread::get_id() << std::endl;
    #endif

    return;
}

void performBfsSearchCThread(Graph &g, int startVertex, int searchValue, int threadNum) {
    myqueue<int> qdrain, srcToFarm;
    vector<myqueue<int>> q(threadNum);

    g.visited[startVertex] = 1;//mark the very first vertex as visited
    srcToFarm.push(startVertex); //insert the very first vertex in queue
    int countOccurence = 0;
    vector<mutex> vectorlock(VERTEX_NUM);
    long count = 0;
    {
        utimer tt("TIME COMPLETION (par): ", &count);

        // std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        // std::thread s0([](){});
        // std::vector<std::thread*> tids(threadNum);
        // for(int i=0; i < threadNum; i++)
        //     tids[i] = new std::thread([](){});
        // std::thread s1([](){});    

        // now start the farm stages threads
        std::thread s0(source2farm, ref(g), ref(q), ref(srcToFarm), threadNum); // this is the emitter, source of the stream of tasks  ;
        std::vector<std::thread*> tids(threadNum);
        for(int i=0; i < threadNum; i++) 
            tids[i] = new std::thread(
                genericfarmstage, ref(g), ref(q[i]), ref(qdrain),
                ref(srcToFarm), ref(vectorlock), ref(countOccurence), searchValue);       
        std::thread s1(drainfarm, ref(g), ref(countOccurence), ref(qdrain), threadNum); // fourth stage (prints results)
        
        s0.join(); 
        for(int i = 0; i < threadNum; i++) 
            tids[i]->join();
        s1.join();
        // std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        // auto millisec = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        // std::cout << " computed in " << millisec << " millisec " << std::endl;
    }
    
 
}

int main(int argc, char *argv[])
{
    string filename = argv[1];
    int startNode = stoi(argv[2]);
    int searchValue = stoi(argv[3]);
    int threadNum = stoi(argv[4]);
    
    Graph g1(VERTEX_NUM);
    Graph g2(VERTEX_NUM);
    FileReader fr(filename);
    fr.readFile(g1);
    long count = 0;
    
    performBfsSearchCThread(ref(g1), startNode, searchValue, threadNum);

    count = 0;
    // fr.readFile(g2);
    // {
    //     utimer tt("TIME COMPLETION (seq): ", &count);
    //     g2.performBfsSearch(startNode, searchValue);
    // };

    cout << "done!!!" << endl;
  
    return 0;
}