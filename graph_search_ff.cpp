#include <string>
#include <iostream>

#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <cstddef>
#include <math.h>
#include <string>
#include <functional>
#include "graph.cpp"
#include "fileReader.cpp"
#include "utimer.cpp"

#include <ff/ff.hpp>
#include <ff/farm.hpp>

using namespace std;
using namespace ff;

const int VERTEX_NUM = 100000000;

struct Emitter: ff_monode_t<int> {
    Emitter(Graph& g, int &startVertex, int &threadNum, long &count): 
    g(g), startVertex(startVertex), threadNum(threadNum), count(count) {}
    int* svc(int *task) {
        g.visited[startVertex] = 1;//mark the very first vertex as visited

        if(task == nullptr) {
            #ifdef PROGRESSPRINT
                std::cout << "EMITTER: " << get_my_id() << " SENT: " << startVertex << "\n";
            #endif

            {
                lock_guard<mutex> lg(mu);

                ff_send_out(new int(startVertex));
                count--;
            }

            return GO_ON;
        } else {
            int &t = *task;
            {
                lock_guard<mutex> lg(mu);

                turn = (turn + 1) % threadNum;
                ff_send_out_to(new int(t), turn);

                delete task;
                
                count--;
            }    

            if(count == 0)
                return EOS;

            return GO_ON;
        }
    }

    int turn=0;
    mutex mu;
    Graph g;
    int startVertex;    
    int threadNum;
    long &count;
};

struct Worker: ff_node_t<int> {
    Worker(Graph &g, long &count): g(g), count(count) {}
    int* svc(int *task) { 
        int &startVertex = *task; 
            #ifdef PROGRESSPRINT
                std::cout << "WORKER: " << get_my_id() << " PUSHED: " << startVertex << "\n";
            #endif
            {
                lock_guard<mutex> lg(mu);

                for(auto n : g.adjvectors[startVertex]){//if vertex 1 is poped from queue, then we are dealing with all vertexes connected to vertex 1 in this pass
                    if(g.visited[n] != 1){//if the vertex has not yet been visited, go on
                        g.visited[n] = 1;//mark vertex as visited
                        count++;
                        ff_send_out(new int(n));
                    }
                }

                delete task;
            }
        
        return GO_ON;
    }

    int countOccurence = 0;
    mutex mu;
    Graph g;
    long &count;
};

struct Collector: ff_minode_t<int> {
    Collector(Graph &g, int &searchValue):
     g(g), searchValue(searchValue) {}
    int* svc(int *task) { 
        int &t = *task;

        #ifdef PROGRESSPRINT
            std::cout << "COLLECTOR: " << get_my_id() << " ACCEPTED: " << t << "\n";
        #endif

        if(t == searchValue)
            countOccurence++;

        ff_send_out(new int(t));

        delete task;

        return GO_ON;
    }

    void svc_end() { 
        #ifdef PROGRESSPRINT
            std::cout << "OCCURENCES FOUND: " << countOccurence << "\n";
        #endif
        cout << "countOccurence " << countOccurence << endl;
    }

    int countOccurence = 0;
    Graph &g;
    int searchValue;
};

void performBfsSearchFastFlow(Graph &g, int startVertex, int searchValue, int threadNum) {
    long count = 1;
    
    Emitter emitter(ref(g), startVertex, threadNum, ref(count));
    Collector collector(ref(g), searchValue);
    
    std::vector<std::unique_ptr<ff_node>> W;
    for(int i=0;i<threadNum;++i) 
        W.push_back(make_unique<Worker>(ref(g), ref(count)));

    ff_Farm<int> farm(std::move(W), emitter, collector);
    farm.wrap_around();   // this call creates feedbacks from Workers to the Emitter

    long countTime;
    {
        utimer tt("TIME COMPLETION (ff): ", &countTime);
        if (farm.run_and_wait_end()<0) {
            error("running farm");
        }
    }
}


int main(int argc, char *argv[])
{
    string filename = argv[1];
    int startNode = stoi(argv[2]);
    int searchValue = stoi(argv[3]);
    int threadNum = stoi(argv[4]);
    
    Graph g(VERTEX_NUM);
    FileReader fr(filename);
    fr.readFile(g);
 
    performBfsSearchFastFlow(ref(g), startNode, searchValue, threadNum);
    
    return 0;
}