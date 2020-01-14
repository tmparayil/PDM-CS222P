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

    if(fileHandle.file->good() != 0)
        return -1;

    /**
     * Initial offset is ( directory page + Page num ) * PAGE SIZE - (slot number)
     */
    int offset = (rid.pageNum+1)*PAGE_SIZE -  (rid.slotNum);
    fileHandle.file->seekg(offset,fileHandle.file->beg);

    int curr_slotOffset;
    int curr_slotNumber;
    int prev_slotOffset;

    fileHandle.file->read((char*)&curr_slotNumber, sizeof(int));
    fileHandle.file->seekg(sizeof(int),std::ios_base::cur);
    fileHandle.file->read((char*)&curr_slotOffset, sizeof(int));
    fileHandle.file->seekg(sizeof(int),std::ios_base::cur);
    fileHandle.file->read((char*)&prev_slotOffset, 2 * sizeof(int));

    if(rid.slotNum != curr_slotNumber)
    {
        std::cout<<"Incorrect row ID returned. check slot numbers"<<std::endl;
        return -1;
    }

    int length = curr_slotOffset - prev_slotOffset;
    void* page = malloc(length);

    fileHandle.file->seekg(prev_slotOffset,std::ios_base::beg);
    fileHandle.file->read((char*)page,length);
    memcpy(data,page,length);
    free(page);
    return 0;
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
                printData += sizeof(int);
                break;
            case TypeReal:
                std::cout<<recordDescriptor[itr].name<<":\s"<<(float*)printData<<"\s";
                printData += sizeof(float);
                break;
            case TypeVarChar:
                int* length = (int*)printData;
                printData += 4;
                std::cout<<recordDescriptor[itr].name<<":\s";
                for(int i=0;i<*length;i++)
                {
                    std::cout<<*printData;
                    printData += sizeof(char);
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



