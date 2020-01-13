#include "rbfm.h"
#include "pfm.h"

#include <vector>
#include <cstring>
#include <climits>
#include <iostream>

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    PagedFileManager::instance().createFile(fileName);
    return 0;
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    PagedFileManager::instance().destroyFile(fileName);
    return 0;
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    PagedFileManager::instance().openFile(fileName,fileHandle);
    return 0;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    PagedFileManager::instance().closeFile(fileHandle);
    return 0;
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {
    void* page = malloc(PAGE_SIZE);
    if(fileHandle.readPage(rid.pageNum,page) != 0)
        return -1;

    // Do we want to directly find offset of record/slots and then allocate memory based on record length ?
//    int offset = rid.pageNum*PAGE_SIZE;
//    fileHandle.file->seekp(offset,fileHandle.file->beg);

    // Review below code after insert record design
    char* charPage = (char*)page;
    // Find offset of the record pointer
    char* record = charPage + PAGE_SIZE - (rid.slotNum + 1)*8;
    char* prevRecord = record + 8;

    slots* prevSlot = (slots*)prevRecord;
    int prevOffset = prevSlot->offset;

    slots* slot = (slots*)record;
    int offset = slot->offset;
    int rowId = slot->slotNumber;
    int length = prevOffset - offset;

    /**
     * At this point, we have :
     * start offset of record -> prevOffset
     * length of record -> length
     * rowId -> rid.slotNum
     * we shift the file pointer of charPage to point to record
     */
    charPage += prevOffset;

    //Assuming we use 8 bits to store null values for attributes
    charPage += 1;
    //
    if(rowId == rid.slotNum)
    {
        memcpy(data, charPage, length);
        free(page);
        return 0;
    }

    return -1;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {

    char* printData = (char*)data;
    for(int itr = 0 ;itr < recordDescriptor.size();itr++) {
        switch(recordDescriptor[itr].type)
        {
            case TypeInt:
                std::cout<<recordDescriptor[itr].name<<":\s"<<(int*)printData<<"\s";
                printData += 4;
                break;
            case TypeReal:
                std::cout<<recordDescriptor[itr].name<<":\s"<<(float*)printData<<"\s";
                printData += 4;
                break;
            case TypeVarChar:
                int* length = (int*)printData;
                printData += 4;
                std::cout<<recordDescriptor[itr].name<<":\s";
                for(int i=0;i<*length;i++)
                {
                    std::cout<<*printData;
                    printData++;
                }
                std::cout<<std::endl;
                break;
        }
    }
    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    return -1;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
    return -1;
}



