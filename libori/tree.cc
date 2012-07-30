/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <string>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "tree.h"
#include "util.h"
#include "scan.h"
#include "repo.h"
#include "largeblob.h"
#include "debug.h"

using namespace std;

/********************************************************************
 *
 *
 * AttrMap
 *
 *
 ********************************************************************/

AttrMap::AttrMap()
{
}

void AttrMap::setFromFile(const std::string &filename)
{
    struct stat sb;

    if (stat(filename.c_str(), &sb) < 0) {
	perror("stat failed in Tree::addObject");
	assert(false);
    }

    struct passwd *upw = getpwuid(sb.st_uid);
    struct group *ggr = getgrgid(sb.st_gid);
    setAs<size_t>(ATTR_FILESIZE, sb.st_size);
    setAs<mode_t>(ATTR_PERMS, sb.st_mode & ~S_IFDIR & ~S_IFREG);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;
    setAs<time_t>(ATTR_CTIME, sb.st_ctime);
    setAs<time_t>(ATTR_MTIME, sb.st_mtime);
}

void AttrMap::setCreation(mode_t perms)
{
    struct passwd *upw = getpwuid(geteuid());
    struct group *ggr = getgrgid(getegid());
    setAs<size_t>(ATTR_FILESIZE, 0);
    setAs<mode_t>(ATTR_PERMS, perms);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;

    time_t currTime = time(NULL);
    setAs<time_t>(ATTR_CTIME, currTime);
    setAs<time_t>(ATTR_MTIME, currTime);
}

void AttrMap::mergeFrom(const AttrMap &other)
{
    for (const_iterator it = other.begin();
            it != other.end();
            it++) {
        attrs[(*it).first] = (*it).second;
    }
}

template <> const char *
AttrMap::getAs<const char *>(const std::string &attrName) {
    assert(attrs.find(attrName) != attrs.end());
    return attrs[attrName].c_str();
}


/********************************************************************
 *
 *
 * TreeEntry
 *
 *
 ********************************************************************/

TreeEntry::TreeEntry()
{
    type = Null;
}

TreeEntry::TreeEntry(EntryType type, size_t size, std::string user,
        std::string group, std::string objHash)
    : type(type), hash(objHash)
{
    assert(type != Null);
    assert(hash != "");

    time_t currTime = time(NULL);

    attrs.setAs<size_t>(ATTR_FILESIZE, size);
    attrs.setAs<mode_t>(ATTR_PERMS, 0755);
    attrs.attrs[ATTR_USERNAME] = user;
    attrs.attrs[ATTR_GROUPNAME] = group;
    attrs.setAs<time_t>(ATTR_CTIME, currTime);
    attrs.setAs<time_t>(ATTR_MTIME, currTime);
}

TreeEntry::~TreeEntry()
{
}

TreeEntry
TreeEntry::fromFile(const std::string &filename,
        const std::string &hash,
        const std::string &largeHash)
{
    struct stat sb;

    if (stat(filename.c_str(), &sb) < 0) {
	perror("stat failed in Tree::addObject");
	assert(false);
    }

    TreeEntry::EntryType type;
    if (sb.st_mode & S_IFDIR) {
	type = TreeEntry::Tree;
    } else {
	type = TreeEntry::Blob;
    }

    struct passwd *upw = getpwuid(sb.st_uid);
    struct group *ggr = getgrgid(sb.st_gid);
    TreeEntry entry = TreeEntry(type, sb.st_size, upw->pw_name, ggr->gr_name,
            hash);

    if (largeHash != "") {
        assert(entry.type != TreeEntry::Tree);
        entry.type = TreeEntry::LargeBlob;
        entry.largeHash = largeHash;
    }

    return entry;
}

void
TreeEntry::extractToFile(const std::string &filename, Repo *src)
{
    assert(type != Null);
    Object::sp o(src->getObject(hash));
    if (type == Blob) {
        bytestream::ap bs(o->getPayloadStream());
        bs->copyToFile(filename);
    }
    else if (type == LargeBlob) {
        ::LargeBlob lb(src);
        lb.fromBlob(o->getPayload());
        lb.extractFile(filename);
    }
    else {
        NOT_IMPLEMENTED(false);
    }
}

bool
TreeEntry::hasBasicAttrs()
{
    const char *names[] = {ATTR_FILESIZE, ATTR_PERMS, ATTR_USERNAME,
        ATTR_GROUPNAME, ATTR_CTIME, ATTR_MTIME};
    for (size_t i = 0; i < sizeof(names) / sizeof(const char *); i++) {
        LOG("Checking attr %s", names[i]);
        if (attrs.attrs.find(names[i]) == attrs.attrs.end()) return false;
    }
    return true;
}


/********************************************************************
 *
 *
 * Tree
 *
 *
 ********************************************************************/

Tree::Tree()
{
}

Tree::~Tree()
{
}

void
Tree::addObject(const char *path, const string &objId, const string &lgObjId)
{
    TreeEntry e = TreeEntry::fromFile(path, objId, lgObjId);
    string filename = StrUtil_Basename(path);
    tree[filename] = e;
    printf("Tree::addObject\n");
}


const string
Tree::getBlob() const
{
    /*string blob = "";
    map<string, TreeEntry>::const_iterator it;

    for (it = tree.begin(); it != tree.end(); it++)
    {
        if ((*it).second.type == TreeEntry::Tree) {
            blob += "tree";
        } else if ((*it).second.type == TreeEntry::Blob) {
            blob += "blob";
        } else if ((*it).second.type == TreeEntry::LargeBlob) {
            blob += "lgbl";
        } else {
            assert(false);
        }
	blob += " ";
	blob += (*it).second.hash;
        if ((*it).second.type == TreeEntry::LargeBlob) {
            blob += (*it).second.largeHash;
        }
	blob += " ";
	blob += (*it).first;
	blob += "\n";
    }

    return blob;*/
    strwstream ss;
    size_t size = tree.size();
    ss.write(&size, sizeof(size_t));

    for (map<string, TreeEntry>::const_iterator it = tree.begin();
            it != tree.end();
            it++) {

        if ((*it).second.type == TreeEntry::Tree) {
            ss.write("tree", 4);
        } else if ((*it).second.type == TreeEntry::Blob) {
            ss.write("blob", 4);
        } else if ((*it).second.type == TreeEntry::LargeBlob) {
            ss.write("lgbl", 4);
        } else {
            assert(false);
        }
        ss.writePStr((*it).second.hash);
        if ((*it).second.type == TreeEntry::LargeBlob)
            ss.writePStr((*it).second.largeHash);
        ss.writePStr((*it).first);
        
        size_t asize = (*it).second.attrs.attrs.size();
        ss.write(&asize, sizeof(size_t));
        for (AttrMap::const_iterator ait = (*it).second.attrs.attrs.begin();
                ait != (*it).second.attrs.attrs.end();
                ait++) {
            ss.writePStr((*ait).first);
            ss.writePStr((*ait).second);
        }
    }

    return ss.str();
}

void
Tree::fromBlob(const string &blob)
{
    /*string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	TreeEntry entry = TreeEntry();
	if (line.substr(0, 5) == "tree ") {
	    entry.type = TreeEntry::Tree;
	} else if (line.substr(0, 5) == "blob ") {
	    entry.type = TreeEntry::Blob;
	} else if (line.substr(0, 5) == "lgbl ") {
	    entry.type = TreeEntry::LargeBlob;
	} else {
	    assert(false);
	}

	// XXX: entry.mode

        entry.hash = line.substr(5, 64);
        if (entry.type == TreeEntry::LargeBlob) {
            entry.largeHash = line.substr(69, 64);
	    tree[line.substr(134)] = entry;
        } else {
            entry.largeHash = "";
	    tree[line.substr(70)] = entry;
        }
    }*/
    strstream ss(blob);
    size_t num_entries = ss.readNext<size_t>();
    
    for (size_t i = 0; i < num_entries; i++) {
        TreeEntry entry;
        char type[5] = {'\0'};
        ss.read((uint8_t*)type, 4);
        if (strcmp(type, "tree") == 0) {
            entry.type = TreeEntry::Tree;
        }
        else if (strcmp(type, "blob") == 0) {
            entry.type = TreeEntry::Blob;
        }
        else if (strcmp(type, "lgbl") == 0) {
            entry.type = TreeEntry::LargeBlob;
        }
        else {
            assert(false);
        }

        ss.readPStr(entry.hash);
        if (entry.type == TreeEntry::LargeBlob)
            ss.readPStr(entry.largeHash);
        string path;
        ss.readPStr(path);

        size_t num_attrs = ss.readNext<size_t>();
        for (size_t i_a = 0; i_a < num_attrs; i_a++) {
            string attrName, attrValue;
            ss.readPStr(attrName);
            ss.readPStr(attrValue);
            entry.attrs.attrs[attrName] = attrValue;
        }

        tree[path] = entry;
    }
}

void
_recFlatten(
        const std::string &prefix,
        const Tree *t,
        Tree::Flat *rval,
        Repo *r
        )
{
    for (Tree::Flat::const_iterator it = t->tree.begin();
            it != t->tree.end();
            it++) {
        const TreeEntry &te = (*it).second;
        rval->insert(make_pair(prefix + (*it).first, te));
        if (te.type == TreeEntry::Tree) {
            // Recurse further
            Tree subtree = r->getTree(te.hash);
            _recFlatten(prefix + (*it).first + "/",
                    &subtree, rval, r);
        }
    }
}

Tree::Flat
Tree::flattened(Repo *r) const
{
    Tree::Flat rval;
    _recFlatten("/", this, &rval, r);
    return rval;
}

size_t _num_path_components(const std::string &path)
{
    if (path.size() == 0) return 0;
    size_t cnt = 0;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '/')
            cnt++;
    }
    return 1 + cnt;
}

bool _tree_gt(const std::string &t1, const std::string &t2)
{
    return _num_path_components(t1) > _num_path_components(t2);
}

void _addTreeBackrefs(const std::string &thash, const Tree &t, Repo *r)
{
    for (map<string, TreeEntry>::const_iterator it = t.tree.begin();
            it != t.tree.end();
            it++) {
        const TreeEntry &te = (*it).second;
        r->addBackref(thash, te.hash);
        if (te.type == TreeEntry::Tree) {
            Tree subtree = r->getTree(te.hash);
            _addTreeBackrefs(te.hash, subtree, r);
        }
    }
}

Tree
Tree::unflatten(const Flat &flat, Repo *r)
{
    map<string, Tree> trees;
    for (Flat::const_iterator it = flat.begin();
            it != flat.end();
            it++) {
        const TreeEntry &te = (*it).second;
        if (te.type == TreeEntry::Tree) {
            if (trees.find((*it).first) == trees.end()) {
                trees[(*it).first] = Tree();
            }
        }
        else if (te.type == TreeEntry::Blob ||
                te.type == TreeEntry::LargeBlob) {
            string treename = StrUtil_Dirname((*it).first);
            if (trees.find(treename) == trees.end()) {
                trees[treename] = Tree();
            }
            string basename = StrUtil_Basename((*it).first);
            trees[treename].tree[basename] = te;
        }
        else {
            assert(false && "TreeEntry is type Null");
        }
    }

    // Get the tree hashes, update parents, add to Repo
    vector<string> tree_names;
    for (map<string, Tree>::const_iterator it = trees.begin();
            it != trees.end();
            it++) {
        tree_names.push_back((*it).first);
    }
    // Update leaf trees (no child directories) first
    std::sort(tree_names.begin(), tree_names.end(), _tree_gt);

    for (size_t i = 0; i < tree_names.size(); i++) {
        const string &tn = tree_names[i];
        if (tn.size() == 0) continue;
        string blob = trees[tn].getBlob();
        string hash = Util_HashString(blob);

        // Add to Repo
        r->addBlob(Object::Tree, blob);

        // Add to parent
        TreeEntry te = (*flat.find(tree_names[i])).second;
        te.hash = hash;
        te.type = TreeEntry::Tree;
        assert(te.hasBasicAttrs());

        string parent = StrUtil_Dirname(tn);
        trees[parent].tree[StrUtil_Basename(tn)] = te;
    }

    r->addBlob(Object::Tree, trees[""].getBlob());

    // Update backrefs
    _addTreeBackrefs(trees[""].hash(), trees[""], r);

    return trees[""];
}

std::string
Tree::hash() const
{
    return Util_HashString(getBlob());
}
