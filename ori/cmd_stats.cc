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

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <iostream>
#include <iomanip>

#include "debug.h"
#include "util.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

/*
 * Print repository statistics.
 */
int
cmd_stats(int argc, const char *argv[])
{
    uint64_t commits = 0;
    uint64_t trees = 0;
    uint64_t blobs = 0;
    uint64_t danglingBlobs = 0;
    uint64_t blobRefs = 0;
    uint64_t largeBlobs = 0;
    uint64_t purgedBlobs = 0;
    map<string, map<string, Object::BRState> > refs;
    map<string, map<string, Object::BRState> >::iterator it;

    refs = repository.getRefCounts();

    for (it = refs.begin(); it != refs.end(); it++) {
        Object::Type type = repository.getObjectType((*it).first);

        switch (type) {
        case Object::Commit:
        {
            commits++;
            break;
        }
        case Object::Tree:
        {
            trees++;
            break;
        }
        case Object::Blob:
        {
            blobs++;
            if ((*it).second.size() == 0) {
                danglingBlobs++;
            } else {
                blobRefs += (*it).second.size();
            }
            break;
        }
        case Object::LargeBlob:
        {
            largeBlobs++;
            break;
        }
        case Object::Purged:
        {
            purgedBlobs++;
            break;
        }
        default:
        {
            NOT_IMPLEMENTED(false);
            return 1;
        }
        }
    }

    cout << left << setw(40) << "Commits" << commits << endl;
    cout << left << setw(40) << "Trees" << trees << endl;
    cout << left << setw(40) << "Blobs" << blobs << endl;
    cout << left << setw(40) << "  Dangling Blobs" << danglingBlobs << endl;
    cout << left << setw(40) << "  Dedup Ratio"
         << fixed << setprecision(2)
         << 100.0 * (float)blobs/(float)blobRefs << "%" << endl;
    cout << left << setw(40) << "Large Blobs" << largeBlobs << endl;
    cout << left << setw(40) << "Purged Blobs" << purgedBlobs << endl;

    return 0;
}
