// -*- C++ -*-

#include "build_tree.h"
#include <iostream>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[])
{
    string fname("");
    bool complete = true;
    bool dup_ids = false;
    bool got_file = false;
    int c;
    while ((c = getopt (argc, argv, "hdif:")) != -1)
    switch (c) {
    case 'f': got_file = true; fname = optarg; break;
    case 'd': dup_ids = true; break;
    case 'i': complete = false; break;
    case '?':
    case 'h':
    default:
        cerr << "usage: " << argv[0]
             << "[ -f <filename> -i(support incomplete tree) "
             << "-d(support duplicate ids]" << endl;
        return(-1);
    }

    if (!got_file) {
        cerr << "usage: " << argv[0]
             << "[ -f <filename> -i(support incomplete tree) "
             << "-d(support duplicate ids]" << endl;
        return(-1);
    }

    BuildTree bt(fname, complete, dup_ids);
    if (bt.decodeFile() < 0) {
        cerr << "Error decoding file." << endl;
        return(-1);
    }

    bt.printBFS();

    bt.printDFS();

    return(0);
}
