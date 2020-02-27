#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>
#include <iostream>
#include <climits>
#include <cstring>

#include "../rbf/rbfm.h"


# define IX_EOF (-1)  // end of the index scan

class IX_ScanIterator;

class IXFileHandle;

class IndexManager {

public:
    static IndexManager &instance();

    // Create an index file.
    RC createFile(const std::string &fileName);

    // Delete an index file.
    RC destroyFile(const std::string &fileName);

    // Open an index and return an ixFileHandle.
    RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

    // Close an ixFileHandle for an index.
    RC closeFile(IXFileHandle &ixFileHandle);

    // Insert an entry into the given index that is indicated by the given ixFileHandle.
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Delete an entry from the given index that is indicated by the given ixFileHandle.
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Initialize and IX_ScanIterator to support a range search
    RC scan(IXFileHandle &ixFileHandle,
            const Attribute &attribute,
            const void *lowKey,
            const void *highKey,
            bool lowKeyInclusive,
            bool highKeyInclusive,
            IX_ScanIterator &ix_ScanIterator);

    // Print the B+ tree in pre-order (in a JSON record format)
    void printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const;

protected:
    void insertIntoPage(IXFileHandle &ixFileHandle,const Attribute &attribute,int currPage,const void* newKey,void* returnedChild,int& n1,int& n2,int& length);
    void setRootPage(IXFileHandle &ixFileHandle,void* entry,int length,bool flag);
    void setRootInHidden(IXFileHandle &ixFileHandle,int rootNum);
    int getLengthOfEntry(const void* key,const Attribute& attribute);
    void addToPage(void* page,const void* newKey,const Attribute &attribute);
    void newLeafPage(void* page);
    void newInterPage(void* page);
    int splitLeaf(IXFileHandle &ixFileHandle,void* page,void* newPage);
    int splitLeafVarchar(IXFileHandle &ixFileHandle,void* page,void* newPage);
    void addToInterPage(void* page,const Attribute &attribute,const void* newChild,int x,int y,int lenRec);
    int splitInter(IXFileHandle &ixFileHandle,void* page,void* newPage);
    int splitInterVarchar(IXFileHandle &ixFileHandle,void* page,void* newPage);
    int findPushUpKey(void* page,void* newPage,const void* newKey,void* rootKey,const Attribute &attribute,int x,int y);
    void printCurrentNode(IXFileHandle &ixFileHandle, const Attribute &attribute, int pageNum, int newNode) const;
    RC getRecordOffsetVarchar(const void *pageData,const RID &rid,std::string key );
    RC getRecordOffsetInt(const void *pageData, const RID &rid,int key );
    RC getRecordOffsetReal(const void *pageData, const RID &rid,float key );
    RC getLastRecOffsetVarchar(void *data);
    RC deleteEntryInNode(unsigned rootpageNum,IXFileHandle &ixFileHandle,const Attribute &attribute,const void *key, const RID &rid);


protected:
    IndexManager() = default;                                                   // Prevent construction
    ~IndexManager() = default;                                                  // Prevent unwanted destruction
    IndexManager(const IndexManager &) = default;                               // Prevent construction by copying
    IndexManager &operator=(const IndexManager &) = default;                    // Prevent assignment

};

class IX_ScanIterator {
public:

    // Constructor
    IX_ScanIterator();

    // Destructor
    ~IX_ScanIterator();

    const Attribute* attribute;
    void* lowKey;
    void* highKey;
    bool lowKeyInclusive;
    bool highKeyInclusive;
    IXFileHandle* ixFileHandle;
    int pageNum;
    void* page;

    // Get next matching entry
    RC getNextEntry(RID &rid, void *key);
    int findFirstLeafPage(void* page);

    // Terminate index scan
    RC close();
};

class IXFileHandle {
public:
    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

    // Put the current counter values of associated PF FileHandles into variables
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);

    RC readPage(int pageNum, void *data);                           // Get a specific page
    RC writePage(int pageNum, const void *data);                    // Write a specific page
    RC appendPage(const void *data);                                    // Append a specific page
    int getNumberOfPages();                                        // Get the number of pages in the file

    std::fstream* file;
    std::fstream* getFile();
    bool deleteFlag;
    void setFile(std::string& fileName);
    void closeFile();

    bool check_file_stream();
};

int getRootPage(IXFileHandle &ixFileHandle);
bool isRoot(const void* page);
bool isLeaf(const void* page);
bool isInter(const void* page);
int getFirstPage(const void* page);
bool isSpaceAvailable(const void* page,int length);
int getSpaceOnPage(const void* page);
void setSpaceOnPage(const void *page,int space);
int getSlotOnPage(const void* page);
void setSlotOnPage(const void *page,int slot);
int compareInt(const void* entry,const void* recordOnPage);
int isEqualINTReal(const void *entry, const void *recordOnPage);
int compareReal(const void* entry,const void* recordOnPage);
int compareVarChar(const void* entry,const void* recordOnPage);
int getNextLeafPage(const void* page);
int findLeafPage(void* lowKey,IXFileHandle* ixFileHandle,const Attribute* attribute);
int findLeafPageTraverse(const void* lowKey,IXFileHandle* ixFileHandle,void* page,const Attribute* attribute,int curr);
int findPtrToInsert(const Attribute &attribute,const void* page,const void* newKey);

#endif