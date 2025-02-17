#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>

#include "../rbf/rbfm.h"

#include "../ix/ix.h"

# define RM_EOF (-1)  // end of a scan operator

// RM_ScanIterator is an iterator to go through tuples
class RM_ScanIterator {


public:
    RM_ScanIterator() = default;
    RBFM_ScanIterator rbfmScanIterator;
    ~RM_ScanIterator() = default;

    // "data" follows the same format as RelationManager::insertTuple()
    RC getNextTuple(RID &rid, void *data);

    RC close();
};

// RM_IndexScanIterator is an iterator to go through index entries
class RM_IndexScanIterator {
public:

    IX_ScanIterator ixScanIterator;
    RM_IndexScanIterator() {};    // Constructor
    ~RM_IndexScanIterator() {};    // Destructor

    // "key" follows the same format as in IndexManager::insertEntry()
    RC getNextEntry(RID &rid, void *key);    // Get next matching entry
    RC close();
    // Terminate index scan

    std::string tableName;
    Attribute attribute;

    IXFileHandle ixFileHandle;

};

// Relation Manager
class RelationManager {
public:
    static RelationManager &instance();



    RC createCatalog();

    RC deleteCatalog();

    RC createTable(const std::string &tableName, const std::vector<Attribute> &attrs);

    RC deleteTable(const std::string &tableName);

    RC getAttributes(const std::string &tableName, std::vector<Attribute> &attrs);

    RC insertTuple(const std::string &tableName, const void *data, RID &rid);

    RC deleteTuple(const std::string &tableName, const RID &rid);

    RC updateTuple(const std::string &tableName, const void *data, const RID &rid);

    RC readTuple(const std::string &tableName, const RID &rid, void *data);

    // Print a tuple that is passed to this utility method.
    // The format is the same as printRecord().
    RC printTuple(const std::vector<Attribute> &attrs, const void *data);

    RC readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    // Do not store entire results in the scan iterator.
    RC scan(const std::string &tableName,
            const std::string &conditionAttribute,
            const CompOp compOp,                  // comparison type such as "<" and "="
            const void *value,                    // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RM_ScanIterator &rm_ScanIterator);

// Extra credit work (10 points)
    RC addAttribute(const std::string &tableName, const Attribute &attr);

    RC dropAttribute(const std::string &tableName, const std::string &attributeName);

    // QE IX related
    RC createIndex(const std::string &tableName, const std::string &attributeName);

    RC destroyIndex(const std::string &tableName, const std::string &attributeName);

    // indexScan returns an iterator to allow the caller to go through qualified entries in index
    RC indexScan(const std::string &tableName,
                 const std::string &attributeName,
                 const void *lowKey,
                 const void *highKey,
                 bool lowKeyInclusive,
                 bool highKeyInclusive,
                 RM_IndexScanIterator &rm_IndexScanIterator);

protected:
    RelationManager();                                                  // Prevent construction
    ~RelationManager();                                                 // Prevent unwanted destruction
    RelationManager(const RelationManager &);                           // Prevent construction by copying
    RelationManager &operator=(const RelationManager &);                // Prevent assignment

    RC initTables(std::vector<Attribute>& recordDescriptor);
    RC initColumns(std::vector<Attribute>& recordDescriptor);
    RC initTableRecord(void* record);
    RC initColumnRecord(void* record);
    RC prepareColumnRecord(int tableId,Attribute attribute,void* record,int pos);
    int findNextId(FileHandle& fileHandle,const std::vector<Attribute>& recordDescriptor);
    RC prepareTableRecord(const int tableId,const std::string& tableName,const std::string& fileName,const int version, void* record);
    bool FileExists(const std::string &fileName);
    void initIndexRecord(std::vector<Attribute> &recordDescriptor);
    RC getTableId(std::string tablename);
    void prepareIndexRecord(int recordsize, char *bitinfo, int tableId, const std::string &colName,const std::string &fileName, void *record);
    RC getIndices(const std::string &tableName, std::vector<std::string> &attrs);
    RC insertIntoIndices(std::string tableName,std:: vector<Attribute> &recordDescriptor,const void* data,const RID &rid);
    RC deleteIndices(FileHandle &fileHandle, std::vector<Attribute> & recordDesc, const std::string &tableName, const RID &rid);
private:
    static RelationManager *_relation_manager;
};

#endif