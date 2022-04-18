#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <queue>
#include <atomic>
#include "graph.h"
#include "omp.h"

#include "grppi/pipeline.h"
#include "grppi/farm.h"
#include "grppi/dyn/dynamic_execution.h"

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <ff/farm.hpp>
#include <ff/parallel_for.hpp>

using namespace grppi;

using namespace std;

using namespace ff;
const int SIZE = 100000000;

// Create a graph with given vertices,
// and maintain an adjacency vector
Graph::Graph(int vertices) {
    this->numVertices = vertices;
    this->adjvectors = new vector<int>[vertices];
    this->visited = new int[SIZE];
}
//print Graph
void Graph::printGraph() {
  vector<int>::iterator i;
  for (int d = 0; d < this->numVertices; ++d) {
    cout << "\n Vertex " << d << ":";
    for (i = adjvectors[d].begin(); i != adjvectors[d].end(); ++i) 
      cout << " -> vertex: " << *i;
  }
}
// Add edges and value to the graph
void Graph::addEdge(int src, int dest) {
  adjvectors[src].push_back(dest);
}
// BFS algorithm
int Graph::performBfsSearch(int startVertex, int searchValue) {
    queue<int> q;//c++ stl queue initialized here
    visited[startVertex] = 1;//mark the very first vertex as visited
    q.push(startVertex);//insert the very first vertex in queue
    int countOccurence = 0;

    while(!q.empty()){//as far as there is a vertex value in queue, go on
        startVertex = q.front();//retrieve vertex value from queue

        // cout << "t: " << startVertex << endl;

        q.pop();//delete that vertex value from queue
        for(auto n : this->adjvectors[startVertex]){//if vertex 1 is poped from queue, then we are dealing with all vertexes connected to vertex 1 in this pass
            if(visited[n] != 1){//if the vertex has not yet been visited, go on
                visited[n] = 1;//mark vertex as visited  
                if(startVertex == searchValue)
                    countOccurence++;

                q.push(n);//insert vertex value to queue
            }
        }
    }

    return countOccurence;
}

// BFS algorithm
int Graph::performBfsSearchOpenMP(int startVertex, int searchValue, int threadNum) {
    queue<int> q;//c++ stl queue initialized here
    visited[startVertex] = 1;//mark the very first vertex as visited
    q.push(startVertex);//insert the very first vertex in queue
    int countOccurence = 0;


    while(!q.empty()){//as far as there is a vertex value in queue, go on
        startVertex = q.front();//retrieve vertex value from queue

        if(startVertex == searchValue)
            countOccurence++;

        q.pop();//delete that vertex value from queue
        #pragma omp parallel for lastprivate(countOccurence) num_threads(threadNum)
        for(auto n : this->adjvectors[startVertex]){//if vertex 1 is poped from queue, then we are dealing with all vertexes connected to vertex 1 in this pass
            if(visited[n] != 1){//if the vertex has not yet been visited, go on
                visited[n] = 1;//mark vertex as visited 
                #pragma omp critical
                q.push(n);//insert vertex value to queue
            }
        }
    }
    
    return countOccurence;
}

void Graph::performBfsSearchGRPPI(int startVertex, int searchValue, int threadNum) {
    queue<int> q;//c++ stl queue initialized here
    visited[startVertex] = 1;//mark the very first vertex as visited
    q.push(startVertex);//insert the very first vertex in queue
    int countOccurence = 0;

    pipeline(grppi::parallel_execution_native{threadNum},
	    [&] () mutable -> std::optional<int> {
            if(!q.empty()) {
                startVertex = q.front();//retrieve vertex value from queue
                if(startVertex == searchValue)
                    countOccurence++;
                q.pop();//delete that vertex value from queue 
            } else {
                return {};
            }
      
            return {startVertex};
        },
	    farm(2, [&] (int startVertex) {
            for(auto n : this->adjvectors[startVertex]){//if vertex 1 is poped from queue, then we are dealing with all vertexes connected to vertex 1 in this pass
                if(visited[n] != 1){//if the vertex has not yet been visited, go on
                    visited[n] = 1;//mark vertex as visited
                    q.push(n);//insert vertex value to queue
                }
            }

            cout << "queue size: " << q.size();

            return countOccurence;
        }),
	    [] (int n) {
            cout << "DONE!!! " << n << endl;
        });   
}

void Graph::performBfsSearchFastFlow(int startVertex, int searchValue, int threadNum) {
    queue<int> q;//c++ stl queue initialized here
    visited[startVertex] = 1;//mark the very first vertex as visited
    q.push(startVertex);//insert the very first vertex in queue
    int countOccurence = 0;

    ParallelFor pf(threadNum);

    auto map = [&](const int i) {
        if(visited[i] != 1){//if the vertex has not yet been visited, go on
            visited[i] = 1;//mark vertex as visited  
            if(startVertex == searchValue)
                countOccurence++;

            q.push(i);//insert vertex value to queue
        }
    };

    while(!q.empty()){//as far as there is a vertex value in queue, go on
        startVertex = q.front();//retrieve vertex value from queue
        // cout << "vertex: " << startVertex << " " << endl;
        cout << "startVertex: " << startVertex << " " << endl;  

        q.pop();//delete that vertex value from queue
 
        pf.parallel_for(0,this->adjvectors[startVertex].size(), map, threadNum);

    }

}