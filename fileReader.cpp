#include <string>
#include <fstream>
#include "graph.h"

using namespace std;

class FileReader {    
    private:
        string filename;

    public:
        FileReader(string filename) {
            this->filename = filename;
        }

        void readFile(Graph& g) {
            ifstream myfile(this->filename);
            int v1, v2;

            if (myfile.is_open())
            {
                while (myfile >> v1 >> v2)
                {
                    g.addEdge(v1, v2);
                }
                myfile.close();
            }
        }
};

