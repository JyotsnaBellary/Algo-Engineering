#include <iostream>
#include "test.hpp"
#include "utils/file_handler.hpp"
#include "data_structure/graph.hpp"
using namespace std;

int main() {
    cout << "Let's start with multiway cut" << endl;
    // FileHandler file_handler;
    // string filePath = "../input_data/example.txt";
    // Graph graph = file_handler.readGraph(filePath);
    test_exact_simple();
}