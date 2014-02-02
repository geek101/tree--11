// -*- C++ -*-

/**
 * Copyright (c) 2014 Powell Molleti
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file:build_tree.cc
 * Implementation for decoding a tree from encoded text file.
 * Uses HashMap to get O(1) insert into the tree since the assumption is
 * any number of lines have to processed.
 * Few things are unclear hence following options have been provided.
 * -d : support duplicate node_ids (look into test_cycle/)
 * -i : support incomplete trees  (look into test_completeonly/)
 */

#include "build_tree.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <istream>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <queue>

// #define NDEBUG
#include <cassert>
using namespace std;

const unsigned int maxFSize = 100*1024*1024; /// in Bytes, 100Mb default
const unsigned int maxLineSize = 1024;       /// in Char count

/**
 * Constructor
 */
BuildTree::BuildTree()
    : decodedTree_(NULL),
      wait_count_(0),
      maxFSize_(maxFSize),
      complete_tree_(true),
      duplicate_ids_(false)
{
}

/**
 * Constructor with options!.
 */
BuildTree::BuildTree(string& fname, bool complete_tree, bool dup_ids)
    : decodedTree_(NULL),
      wait_count_(0),
      maxFSize_(maxFSize),
      complete_tree_(complete_tree),
      duplicate_ids_(dup_ids)
{
    if (fname.length() == 0)
    {
        cerr << "Invalid fname" << endl;
        return;
    }

    fname_ = fname;
}

/**
 * Destructor.
 */
BuildTree::~BuildTree()
{
    if (inFile_.is_open())
    {
        inFile_.close();
    }

    decomission();
}

/**
 * Call to delete the allocated HashMap and the decoded tree.
 */
void
BuildTree::decomission()
{
    hashMap_t::iterator it = insertMap_.begin();
    for (; it != insertMap_.end(); ++it) {
        nodeList_t& node_list = *it->second;
        nodeList_t::iterator lit = node_list.begin();
        for (; lit != node_list.end(); ++lit) {
            href_t* ln = *lit;
            if (ln->status_ == Status::NODE_WAIT)
            {
                node_t *t = (node_t *)ln->nodePtr_;
                freeN(t);
            }
            delete ln;
        }
        delete &node_list;
    }
    insertMap_.clear();

    if (!decodedTree_)
        return;

    queue<node_t *> q;
    q.push(decodedTree_);

    while (q.size() != 0) {
        node_t *t = q.front();
        q.pop();
        if (t->left_)
            q.push(t->left_);
        if (t->right_)
            q.push(t->right_);
        if (t->descr_) {
            delete t->descr_;
            t->descr_ = NULL;
        }
        delete t;
    }
    return;
}

void
BuildTree::setMaxFileSize(const unsigned int fsize)
{
    maxFSize_ = fsize;
}

/**
 * Helps with stopping bad filenames and files that exceed the limit
 * we expect.
 */
int
BuildTree::fileCheck(const string& fname)
{
    struct stat sbuf;
    memset(&sbuf, 0, sizeof(sbuf));
    if (::stat(fname.c_str(), &sbuf) == -1) {
        cerr << "stat Error for fname : " << fname << " "
             << strerror(errno) << endl;
        return(-1);
    }

    if (!S_ISREG(sbuf.st_mode)) {
        cerr << "fname : " << fname
             << " is not a regular file" << endl;
        return(-1);
    }

    if (sbuf.st_size > maxFSize_) {
        cerr << "fname : " << fname
             << " size : " << sbuf.st_size
             << " exceeded max size : " << maxFSize_ << endl;
        return(-1);
    }
    return 0;
}

/**
 * Insert a new node into the hashMap, here except for the first time
 * rest of the nodes will reside in either NODE_WAIT(with description)
 * and NONNODE_WAIT (leaves waiting for nodes with description)
 * This along with checkHashMap helps with O(1) inserts for the
 * tree.
 */
int
BuildTree::insertHashMap(node_t &n, node_t **holder, Status s)
{
    // first time.
    href_t *href = new href_t;
    memset(href, 0, sizeof(href_t));
    href->nodePtr_ = holder;

    if (insertMap_.empty()) {
        // first insert set the root.
        decodedTree_ = &n;
        href->nodePtr_ = &decodedTree_;
        s = Status::FILLED;
    }

    nodeList_t *nlist = NULL;
    hashMap_t::iterator it = insertMap_.find(n.id_);
    if (it != insertMap_.end()) {
        nlist = it->second;
    } else {
        nlist = new nodeList_t;
        std::pair<int, nodeList_t *>ins_item(n.id_, nlist);
        insertMap_.insert(ins_item);
    }

    if (s == Status::NODE_WAIT) {
        href->nodePtr_ = (node_t **)&n; ///XXX: Hack :(
    }

    if (s != Status::FILLED)
    {
        wait_count_++;
    }
    href->status_ = s;
    nlist->push_back(href);
    return(0);
}

/**
 * Helps with reducing wait_count when root gets shifted.
 */
int
BuildTree::markParentFilled(node_t *n)
{
    hashMap_t::iterator it = insertMap_.find(n->id_);
    if (it == insertMap_.end())
        return(-1);

     nodeList_t& node_list = *it->second;
     nodeList_t::iterator lit = node_list.begin();
     for (; lit != node_list.end(); ++lit) {
         href_t* ln = *lit;
         if (ln->status_ == Status::NODE_WAIT)
         {
             ln->status_ = Status::FILLED;
             wait_count_--;
             return(0);
         }
     }

     return(-1);
}

/**
 * Main method for stiching disjoint trees together as more info
 * comes in.
 * Using a list of nodes in the HashMap to help us support same integer
 * used in many nodes.
 */
int
BuildTree::checkHashMap(node_t *n, node_t** holder, node_t *parent)
{
    hashMap_t::iterator it = insertMap_.find(n->id_);
    if (it != insertMap_.end()) {
        // found something.
        nodeList_t& node_list = *it->second;
        nodeList_t::iterator lit = node_list.begin();
        bool is_filled = false;
        for (; lit != node_list.end(); ++lit) {
            href_t* ln = *lit;
            // hit, check if this in wait state.
            if (ln->status_ == Status::FILLED &&
                ln->nodePtr_ != &decodedTree_)
                continue; // not interesting spoken for.

            // this is wait state.
            // lets insert this into the tree and mark it as filled.

            /* two cases here.
             * 1. non node found a node.
             * 2. node found a non node.
             */

            switch(ln->status_) {
            case Status::NODE_WAIT: {
                // check we are not asked to handle an unfilled node.
                if (n->descr_ != NULL || holder == NULL) {
                    if (duplicate_ids_) {
                        return(-1);
                    }

                    cerr << "NODE_WAIT: " << n->descr_
                         << " : found for node_id "
                         << n->id_ << endl;
                    return -EINVAL;
                }

                delete n;
                node_t *tn = (node_t *)ln->nodePtr_;
                *holder = tn;
                ln->nodePtr_ = holder;
                wait_count_--;
                break;
            }
            case Status::NONNODE_WAIT: {
                // check we have a node
                if (n->descr_ == NULL) {
                    if (duplicate_ids_) {
                        return(-1);
                    }
                    cerr << "NONENODE_WAIT: " << n->id_ << endl;
                    return -EINVAL;
                }

                node_t *tfree = *ln->nodePtr_;
                *ln->nodePtr_ = n;
                delete tfree;
                wait_count_--;
                break;
            }
            case Status::FILLED: {
                // This is only possible if ln is pointed by root so adjust
                // the root.
                if (n->descr_ != NULL) {
                    // if we are replacing root then we better be a
                    // non node.
                    cerr << "FILLED: " << n->descr_
                         << " : descr found for node_id "
                         << n->id_ << endl;
                    return -EINVAL;
                }

                if (!parent) {
                    // if we are taking ownership of root then we better
                    // have one.
                    cerr << "PARENT: not found for node_id "
                         << n->id_ << endl;
                    return -EINVAL;
                }

                delete n;
                *holder = *ln->nodePtr_; // lets take current root.
                decodedTree_ = parent; // point to new root.

                if (markParentFilled(parent) < 0) {
                    cerr << "Could not mark FILLED for parent : "
                         << parent->id_ << endl;
                    return -EINVAL;
                }

                break;
            }
            default:
                cerr << "node status is : " << (int)ln->status_ << endl;
                assert(false);
            }

            ln->status_ = Status::FILLED;
            is_filled = true;
            break;
        }

        if (!is_filled) {
            if (duplicate_ids_) {
                return(-1);
            }

            cerr << "node not filled : " << n->id_ << endl;
            return(-EINVAL);
        }

        return(0);
    }

    return(-1); // not found
}

/**
 * Consume each node and place at the right place in the tree
 * or store it for future use.
 */
int
BuildTree::processNode(node_t *n, node_t **holder, node_t *parent)
{
    // first check if we have an existing entry for n->id_ in this.
    int ret = checkHashMap(n, holder, parent);

    Status s = Status::FILLED;
    if (ret < 0) { // not found.
        if (ret == -EINVAL)
            return(ret);

        s = Status::NODE_WAIT;
        if (n->descr_ == NULL) {
            s = Status::NONNODE_WAIT;
        }

        insertHashMap(*n, holder, s);
   }

    // now try the left and right.
    if (n->left_) {
        ret = processNode(n->left_, &n->left_, n);
        if (ret == -EINVAL) {
            return(ret);
        }
    }

    if (n->right_) {
        ret = processNode(n->right_, &n->right_, n);
        if (ret == -EINVAL) {
            return(ret);
        }
    }

    return(0);
}

/**
 * Parse each line, tried to use some c++ specific stuff instead of plain
 * C-Style parsing. C-Style parsing could avoid extra copies
 * Will return a node left and right child if present.
 */
node_t *
BuildTree::parseLine(shared_ptr<string> line)
{
    if (line->length() >= maxLineSize) {
        cerr << "exceeded size : " << maxLineSize << endl;
        return NULL;
    }

    node_t *n = NULL;
    istringstream in_line(*line);
    in_line.seekg(0, in_line.end);
    unsigned int end_pos = in_line.tellg();
    in_line.seekg(0, in_line.beg);
    int c = 0; (void)c;
    bool firstData = false; // did we get through the first item?
    int leaf_count = 0;
    for (array<char, maxLineSize> a;
         in_line.getline(&a[0], maxLineSize, ' '); ) {
        bool set_descr = false;
        int node_id = 0;
        if (strlen(&a[0]) == 0 || a[0] == ' ') {
            continue;
        }

        int sval = sscanf(&a[0], "%d", &node_id);

        if (!firstData) {
            if (!sval) {
                cerr << "Cannot parse line" << endl;
                return NULL;
            }

            n = new node_t;
            memset(n, 0, sizeof(node_t));
            n->id_ = node_id;
            firstData = true;
            continue;
        }

        if (sval) {
            node_t **item = (n->left_ != NULL ? &n->right_ : &n->left_);
            if (*item != NULL) {
                // both are taken care of so add this to description
                set_descr = true;
            } else {
                *item = new node_t;
                memset(*item, 0, sizeof(node_t));
                (*item)->id_ = node_id;
                leaf_count++;
            }
        }

        // subsequent data.
        if (!sval || set_descr) {
            // if we only support complete tree then add the left node to
            // description.
            // this word is not a number
            // lets store the current position in the stream and
            // rewind and store description and bail.
            array<char, maxLineSize> *buf = new array<char, maxLineSize>;
            memset(&(*buf)[0], 0, maxLineSize);

            int offset = 0;
            if (complete_tree_ && leaf_count == 1) {
                // fold the leaf id into the description.
                sprintf(&(*buf)[0], "%d ", n->left_->id_);
                delete n->left_;
                n->left_ = NULL;
                offset = strlen(&(*buf)[0]);
            }

            memcpy(&(*buf)[offset], &a[0], strlen(&a[0]));

            if (in_line.eof()) {
                n->descr_ = &(*buf)[0];
                break;
            }

            char *buf_fwd = &(*buf)[offset] + strlen(&a[0]);
            *buf_fwd = ' ';
            unsigned int cur_pos = in_line.tellg();

            in_line.seekg(cur_pos, ios_base::beg);
            in_line.read(++buf_fwd, end_pos - cur_pos);
            n->descr_ = &(*buf)[0];
            break;
        }
    }

    if (n->descr_ == NULL) {
        // user did not entry anything but we need a dummy value cannt manage
        // null pointer for this field.
        array<char, 1> *buf = new array<char, 1>;
        memset(&(*buf)[0], 0, 1);
        n->descr_ = &(*buf)[0];
    }
    return n;
}

void
BuildTree::freeN(node_t *n)
{
    if (!n)
        return;

    if (n->left_)
        delete n->left_;
    if (n->right_)
        delete n->right_;
    if (n->descr_)
        delete n->descr_;

    n->left_ = n->right_ = NULL;
    n->descr_ = NULL;
    delete n;
    return;
}
/**
 * Main method that interfaces external world. Use this to start
 * decoding.
 */
int BuildTree::decodeFile()
{
    if (fileCheck(fname_) < 0)
        return -1;

    // open the fstream and start the big loop!.
    inFile_.open(fname_.c_str(), fstream::in);
    if (!inFile_) {
        // TODO: Print error.
        cerr << fname_ << " : error in open " << endl;
        return -1;
    }

    int line_count = 0;
    shared_ptr<string> line(new string); // not sure what getline does!.
    while(!inFile_.eof()) {
        line->clear();
        getline(inFile_, *line);
        if (line->length() == 0)
            continue;

        line_count++;

        // we have a line parse it get a node.
        node_t *n = parseLine(line);
        if (!n) {
            cerr << line_count << " : Error line - " << *line << endl;
            freeN(n);
            continue;
        }

        if (processNode(n, NULL, NULL) < 0) {
            cerr << line_count << " : Error line - " << *line << endl;
            inFile_.close();
            freeN(n);
            return(-1);
        }
    }

    if (wait_count_ > 0) {
        cerr << "Error - unresolved node count : " << wait_count_ << endl;
        inFile_.close();
        return(-1);
    }

    // After all the effort if the root is empty then no go.
    if (decodedTree_ == NULL) {
        cerr << "Error - could not build any tree!" << endl;
        inFile_.close();
        return(-1);
    }

    inFile_.close();
    return 0;
}

void
BuildTree::printBFS() const
{
    if (!decodedTree_)
        return;

    queue<node_t *> q;
    q.push(decodedTree_);

    while (q.size() != 0) {
        node_t *t = q.front();
        q.pop();
        if (t->left_)
            q.push(t->left_);
        if (t->right_)
            q.push(t->right_);
        cout << t->descr_ << " ";
    }

    cout << endl;
    return;
}

void
BuildTree::printDFSRecur(node_t *root) const
{
    if (!root)
        return;

    printDFSRecur(root->left_);
    cout << root->descr_ << " ";
    printDFSRecur(root->right_);
    return;
}

void
BuildTree::printDFS() const
{
    printDFSRecur(decodedTree_);
    cout << endl;
    return;
}


