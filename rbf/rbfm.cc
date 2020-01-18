#include "rbfm.h"
#include "pfm.h"

#include <vector>
#include <cstring>
#include <climits>
#include <iostream>
#include <cmath>

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    PagedFileManager& pfm = PagedFileManager::instance();
    if(pfm.createFile(fileName.c_str()) != 0)
        return -1;
    FileHandle fileHandle;
    pfm.openFile(fileName.c_str(),fileHandle);
    RecordBasedFileManager::initialise(fileHandle,-1);
    return 0;
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    return PagedFileManager::instance().destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    return PagedFileManager::instance().openFile(fileName,fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return PagedFileManager::instance().closeFile(fileHandle);
}

int RecordBasedFileManager::getRecSize(const void *data, const std::vector<Attribute> &recordDescriptor) {

    bool checknull;
    uint32_t RecSize = 0;
    uint32_t nullinfosize = ceil((double) recordDescriptor.size() / 8);
    u_int32_t bytesforNullIndic = ceil((double) recordDescriptor.size() / CHAR_BIT);

    RecSize += bytesforNullIndic;

    unsigned char *null_Fields_Indicator = (unsigned char *) malloc(nullinfosize);
    memcpy(null_Fields_Indicator, (char *) data, nullinfosize);

    for (int i = 0; i < recordDescriptor.size(); i++) {
        checknull = null_Fields_Indicator[i / 8] & (1 << (7 - i % 8));
        if (!checknull) {
            switch (recordDescriptor[i].type) {
                case TypeInt:
                case TypeReal:
                    RecSize += recordDescriptor[i].length;
                    break;
                case TypeVarChar:
                    int temp = 0;
                    memcpy(&temp, (char *) data + RecSize, sizeof(int));
                    RecSize += temp + sizeof(int);
                    break;
            }
        }
    }
    free(null_Fields_Indicator);
    return RecSize;
}

/**
 *
 * This method initialises a new page with the structure.
 * Returns the free space in the new page
 * PFM appendPage() is invoked.
 *
 * @param fileHandle
 * @param pageNum
 * @return int
 */
void RecordBasedFileManager::setPage(FileHandle &fileHandle) {

    void* buffer = malloc(PAGE_SIZE);
    int offset = PAGE_SIZE - 2 * sizeof(int);
    int recordOffset = sizeof(int);
    int slotOffset = offset;
    int freeSpace = (slotOffset - recordOffset);
    memcpy((char*)buffer,(char*)&freeSpace, sizeof(int));
    memcpy((char*)buffer + offset,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + offset + sizeof(int),(char*)&slotOffset, sizeof(int));

    fileHandle.appendPage(buffer);
    free(buffer);
}

/**
 * Initialise hidden directory for pageNum = -1
 * -> set HeaderValue
 * -> update Total pages to 0
 * Initialise a page in the hidden directory for all other values
 * -> set free size for that page
 * -> update total pages
 * @param fileHandle
 * @param pageNum
 */
void RecordBasedFileManager::initialise(FileHandle &fileHandle,int pageNum) {
    RecordBasedFileManager::setPage(fileHandle);
}

/**
 * Insert a record into the file
 *
 * Get record size. Check hidden directory if free space available in a  page.
 * If yes -> insert record in that page
 * If no ->
 *  -> -> has hidden page been initialised ? If no : ERROR
 *  -> -> if no pages have free space, append new page and insert record
 *
 * @param fileHandle
 * @param recordDescriptor
 * @param data
 * @param rid
 * @return
 */
RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    uint32_t recSize = getRecSize(data,recordDescriptor);
    int totalPages=fileHandle.getNumberOfPages();

    int freeSize = 0;
    bool gotPage = false;
    int page = 0;
    if(totalPages == 0)
    {
        RecordBasedFileManager::initialise(fileHandle,0);
    }
    else{
        void* aPage = malloc(PAGE_SIZE);
        // Looping through all the pages in hidden directory
        for(int i=0;i<totalPages;i++)
        {
            fileHandle.readPage(i,aPage);
            memcpy((char*)&freeSize,(char*)aPage, sizeof(int));

            if(freeSize >= recSize + 2* sizeof(int))
            {
                page = i;
                gotPage = true;
                break;
            }
        }
        free(aPage);

        // If no page in the directory has enough free space for the record
        if(!gotPage)
        {
            RecordBasedFileManager::initialise(fileHandle,totalPages + 1);
            page = totalPages;
        }
    }

    // Currently we get page data from above block and then read page again
    // We have page in memory, which we can try to re-use below.
    // Reading slot and record offsets
    void* buffer = malloc(PAGE_SIZE);
    fileHandle.readPage(page,buffer);

    int ptrOffsets = PAGE_SIZE - 2* sizeof(int);
    int recordOffset , slotOffset;

    memcpy((char*)&recordOffset, (char*)buffer + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)buffer + ptrOffsets + sizeof(int), sizeof(int));

    int slotNumber = (ptrOffsets - slotOffset ) / (2 * sizeof(int));
    slotNumber++;

    //Updating RID of record
    rid.pageNum = page;
    rid.slotNum = slotNumber;

    // Insert new record slot in slot directory
    memcpy((char*)buffer + slotOffset - sizeof(int),(char*)&recSize, sizeof(int));
    memcpy((char*)buffer + slotOffset - 2 * sizeof(int),(char*)&recordOffset, sizeof(int));
    slotOffset -= 2 * sizeof(int);

    // Inserting record
    memcpy((char*)buffer + recordOffset,(char*)data,recSize);
    recordOffset += recSize;

    // Updating slot-record pointers after insert
    memcpy((char*)buffer + ptrOffsets,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + ptrOffsets + sizeof(int),(char*)&slotOffset, sizeof(int));

    int freeSpace = slotOffset - recordOffset;
    memcpy((char*)buffer,(char*)&freeSpace, sizeof(int));

    fileHandle.writePage(page,buffer);
    free(buffer);
    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {

    int offset = PAGE_SIZE - 2 * sizeof(int) - rid.slotNum * 2 * sizeof(int);
    void* buffer = malloc(PAGE_SIZE);
    int slotOffset , length;
    fileHandle.readPage(rid.pageNum,buffer);

    memcpy((char*)&slotOffset,(char*)buffer + offset, sizeof(int));
    memcpy((char*)&length,(char*)buffer + offset + sizeof(int), sizeof(int));

    memcpy(data,(char*)buffer + slotOffset , length);
    free(buffer);
    return 0;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {

    int offset = 0;
    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];

    memset(bitInfo, 0 , nullInfo);
    memcpy((char*)bitInfo,(char*)data, nullInfo);
    offset += nullInfo;
    for(int itr = 0 ;itr < recordDescriptor.size();itr++) {
        int bitShift = CHAR_BIT - 1 - itr%CHAR_BIT;
        int checkNullBit = bitInfo[itr / CHAR_BIT] & (1 << (bitShift));
        switch(recordDescriptor[itr].type)
        {
            case TypeInt:
                int tempInt;
                if(!checkNullBit)
                {
                    memcpy((char*)&tempInt,(char*)data + offset, sizeof(int));
                    std::cout<<recordDescriptor[itr].name<<":\t"<<tempInt<<"\t";
                    offset += sizeof(int);
                } else
                {
                    std::cout<<recordDescriptor[itr].name<<":\tNULL"<<"\t";
                }
                break;
            case TypeReal:
                float tempFloat;
                if(!checkNullBit) {
                    memcpy((char *) &tempFloat, (char *) data + offset, sizeof(float));
                    std::cout<<recordDescriptor[itr].name<<":\t"<<tempFloat<<"\t";
                    offset += sizeof(float);
                } else
                {
                    std::cout<<recordDescriptor[itr].name<<":\tNULL"<<"\t";
                }
                break;
            case TypeVarChar:
                int length;
                if(!checkNullBit)
                {
                    memcpy((char*)&length,(char*)data + offset, sizeof(int));
                    offset += sizeof(int);

                    char* tempString = new char[length+1];
                    memcpy(tempString,(char*)data + offset, length);
                    tempString[length] = '\0';
                    std::cout<<recordDescriptor[itr].name<<":\t";
                    std::cout<<tempString<<"\t";
                    offset += length;

                } else
                {
                    std::cout<<recordDescriptor[itr].name<<":\tNULL"<<"\t";
                }
                break;
        }
    }
    std::cout<<std::endl;
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
