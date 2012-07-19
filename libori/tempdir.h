#ifndef __TEMPDIR_H__
#define __TEMPDIR_H__

#include <set>
#include <vector>
#include <string>
#include <tr1/memory>

#include "repo.h"
#include "commit.h"
#include "debug.h"
#include "index.h"

class TempDir : public Repo
{
public:
    TempDir(const std::string &dirpath);
    ~TempDir();

    typedef std::tr1::shared_ptr<TempDir> sp;

    std::string pathTo(const std::string &file);
    
    // Repo implementation
    std::string getHead() { NOT_IMPLEMENTED(false); }
    Object::sp getObject(const std::string &objId);
    bool hasObject(const std::string &objId);
    std::set<ObjectInfo> listObjects();
    std::vector<Commit> listCommits() { NOT_IMPLEMENTED(false); }
    int addObjectRaw(const ObjectInfo &info, bytestream *bs);

    std::string dirpath;

private:
    Index index;
};

#endif