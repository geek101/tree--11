// -*- C++ -*-

#include <fstream>
#include <stack>
#include <cstdint>
#include <unordered_map>
#include <list>
#include <memory>
using namespace std;

struct node
{
    int id_;
    struct node *left_;
    struct node *right_;
    char* descr_;
};

typedef struct node node_t;

enum class Status : std::int8_t
{
    NONE = 0,         /// invalid
    NODE_WAIT = 1,    /// node is waiting.
    NONNODE_WAIT = 2, /// non node is waiting.
    FILLED = 3        /// Found a line with this node_id
};

typedef struct hash_ref
{
    node_t** nodePtr_;
    Status status_;
} href_t;

typedef list<hash_ref *> nodeList_t;
typedef unordered_map<int, nodeList_t*> hashMap_t;

class BuildTree
{
public:
    BuildTree();
    BuildTree(string& fname, bool complete_tree=true, bool dup_ids=false);
    virtual ~BuildTree();

    int decodeFile();
    void printBFS() const;
    void printDFS() const;

    void setMaxFileSize(const unsigned int fsize); /// in Bytes.

private:
    void printDFSRecur(node_t *root) const;
    int markParentFilled(node_t *n);
    int insertHashMap(node_t &n, node_t **holder, Status s);
    int checkHashMap(node_t *n, node_t **holder, node_t *parent);

    int processNode(node_t *n, node_t **holder,
                    node_t *parent); /// Helper to process new Node
    node_t *parseLine(shared_ptr<string> line);             /// Helper to process the line.
    void freeN(node_t *n);
    void decomission();
    int fileCheck(const string& fname);          /// is File and check limit.

    node_t *decodedTree_;          /// The decoded tree.
    /// Helps with late inserts and error checks.
    /// the list helps us maintain order of parsing.
    hashMap_t  insertMap_;
    int wait_count_;              /// if does not become zero then we have
                                  /// bad input
    string fname_;                /// Input filename
    fstream inFile_;              /// Input File Stream
    unsigned int maxFSize_;       /// Overrides the default
    bool complete_tree_;          /// support for partial!
    bool duplicate_ids_;          /// duplicate node id support.
};


