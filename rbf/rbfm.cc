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


void RecordBasedFileManager::encodeRecord(FileHandle &fileHandle,const std::vector<Attribute> &recordDescriptor,const void* data,void* returnedData){

    int valNotPresent = -1;
    int offset = 0;
    int size = getRecSize(data,recordDescriptor);
    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);
    memcpy((char*)bitInfo,(char*)data, nullInfo);
    offset += nullInfo;
    //Updating the size of the record to include offsets and remove nullBitInfo
    size = size - nullInfo + sizeof(int) + recordDescriptor.size()* sizeof(int);
    void* record = malloc(size);
    int version = 1;
    memcpy((char*)record,(char*)&version, sizeof(int));

    // 1st 4B int -> version number
    int dataOffset = sizeof(int) + recordDescriptor.size()* sizeof(int);
    int slotOffset = sizeof(int);

    for(int itr = 0 ;itr < recordDescriptor.size();itr++) {
        int bitShift = CHAR_BIT - 1 - itr % CHAR_BIT;
        int checkNullBit = bitInfo[itr / CHAR_BIT] & (1 << (bitShift));
        switch(recordDescriptor[itr].type)
        {
            case TypeInt:
                if(!checkNullBit)
                {
                    memcpy((char*)record + dataOffset,(char*)data + offset, sizeof(int));
                    dataOffset += sizeof(int);
                    offset += sizeof(int);
                    memcpy((char*)record + slotOffset,(char*)&dataOffset, sizeof(int));
                    slotOffset += sizeof(int);
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);
                }
                break;
            case TypeReal:
                if(!checkNullBit) {
                    memcpy((char*)record + dataOffset,(char*)data + offset, sizeof(int));
                    dataOffset += sizeof(float);
                    offset += sizeof(float);

                    memcpy((char*)record + slotOffset,(char*)&dataOffset, sizeof(int));
                    slotOffset += sizeof(float);
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);
                }
                break;
            case TypeVarChar:
                int length;
                if(!checkNullBit)
                {
                    memcpy((char*)&length,(char*)data + offset, sizeof(int));
                    offset += sizeof(int);

                    memcpy((char*)record + dataOffset,(char*)&length, sizeof(int));
                    dataOffset += sizeof(int);
                    memcpy((char*)record + dataOffset,(char*)data + offset,length);
                    dataOffset += length;
                    offset += length;

                    memcpy((char*)record + slotOffset,(char*)&dataOffset, sizeof(int));
                    slotOffset += sizeof(int);
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);
                }
                break;
        }
    }
    memcpy(returnedData,record,size);
    free(record);
    delete[] bitInfo;
}

int RecordBasedFileManager::getEncodedRecordSize(const void *data, const std::vector<Attribute> &recordDescriptor){

    int lastPtr;
    int numberOfSlots = recordDescriptor.size();
    int offset = 0;
    for(int i = numberOfSlots - 1;i >= 0; i--)
    {
        offset = (i * sizeof(int)) + sizeof(int);
        memcpy((char*)&lastPtr,(char*)data + offset, sizeof(int));
        if(lastPtr != -1)
        {

            return lastPtr;
        }
    }
    return 0;
}

// Find the next available slot for inserting new record in the page
// If -1 is returned, create a new slot. Else re-use existing slot number returned
int RecordBasedFileManager::nextAvailableSlot(void* aPage,int slotPtrEnd, int slotPtrStart)
{
    int ret = -1;
    int slotOffset = (2* sizeof(int));
    int offset , length;
    int numOfSlots = (slotPtrEnd - slotPtrStart) / (2 * sizeof(int));
    for(int i = 0;i < numOfSlots;i++)
    {
        memcpy((char*)&offset,(char*)aPage + slotPtrEnd - slotOffset, sizeof(int));
        memcpy((char*)&length,(char*)aPage + slotPtrEnd - slotOffset + sizeof(int), sizeof(int));
        slotOffset += (2 * sizeof(int));
        if(offset == -1 && length == -1)
        {
            ret = (i+1);
            break;
        }
    }

    return ret;
}

void RecordBasedFileManager::decodeRecord(FileHandle &fileHandle,const std::vector<Attribute> &recordDescriptor,const void* data,void* returnedData){

    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);
    int recSize = RecordBasedFileManager::getEncodedRecordSize(data,recordDescriptor);
    int newSize = recSize > 0 ? recSize - sizeof(int) - (recordDescriptor.size() * sizeof(int)) + nullInfo : nullInfo;
    void* record = malloc(newSize);

    for(int i=0;i<recordDescriptor.size();i++)
    {
        int temp;
        memcpy((char*)&temp,(char*)data + sizeof(int) + (sizeof(int) * i), sizeof(int));
        int bitShift = CHAR_BIT - 1 - i%CHAR_BIT;
        if(temp == -1)
            bitInfo[i/CHAR_BIT] = bitInfo[i / CHAR_BIT] | (1 << (bitShift));
    }
    int dataSize = recSize > 0 ? recSize - sizeof(int) - (recordDescriptor.size() * sizeof(int)) : 0;

    memcpy((char*)record,bitInfo,nullInfo);
    memcpy((char*)record + nullInfo,(char*)data + sizeof(int) + (recordDescriptor.size() * sizeof(int)),dataSize);
    memcpy(returnedData,record,newSize);
    free(record);
    delete[] bitInfo;
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
    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
    int newSize = recSize - nullInfo + sizeof(int)+ recordDescriptor.size()* sizeof(int);
    void* record = malloc(newSize);

    RecordBasedFileManager::encodeRecord(fileHandle,recordDescriptor,data,record);
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
        for(int i=totalPages - 1;i >= 0;i--)
        {
            fileHandle.readPage(i,aPage);
            memcpy((char*)&freeSize,(char*)aPage, sizeof(int));

            if(freeSize >= newSize + 2* sizeof(int))
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

    // We get slotOffset and recordOffset from buffer
    memcpy((char*)&recordOffset, (char*)buffer + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)buffer + ptrOffsets + sizeof(int), sizeof(int));

    int insertOffset = -1;
    int totalSlot = ((ptrOffsets - slotOffset ) / (2 * sizeof(int)));

    int slotNumber = nextAvailableSlot(buffer,ptrOffsets, slotOffset);
    std::cout<<"insert slot"<<std::endl;
    std::cout<<slotNumber<<std::endl;

    if(slotNumber == -1)
    {
        slotNumber = totalSlot + 1;
        insertOffset = slotOffset - (2 * sizeof(int));
        slotOffset -= (2 * sizeof(int));
    }
    else{
        // slot offset remains the same
        insertOffset = ptrOffsets - (slotNumber * 2 * sizeof(int));
     }

    rid.pageNum = page;
    rid.slotNum = slotNumber;

    std::cout<<"RID details"<<std::endl;
    std::cout<<rid.pageNum << "\t"<<rid.slotNum<<std::endl;

    // Inserting record into page at recordOffset
    memcpy((char*)buffer + recordOffset,(char*)record,newSize);

    // Insert new record slot in slot directory
    memcpy((char*)buffer + insertOffset,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + insertOffset + sizeof(int),(char*)&newSize, sizeof(int));

    recordOffset += newSize;

    // Updating slot-record pointers after insert
    memcpy((char*)buffer + ptrOffsets,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + ptrOffsets + sizeof(int),(char*)&slotOffset, sizeof(int));

    int freeSpace = slotOffset - recordOffset;
    memcpy((char*)buffer,(char*)&freeSpace, sizeof(int));

    fileHandle.writePage(page,buffer);
    free(buffer);
    free(record);
    return 0;
}

// Current assumption is that when reading a pointer record -> pageNum , slotNum is the order
RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {

//    std::cout<<"RID details"<<std::endl;
//    std::cout<<rid.pageNum << "\t"<<rid.slotNum<<std::endl;

    int offset = PAGE_SIZE - 2 * sizeof(int) - rid.slotNum * 2 * sizeof(int);
    void* buffer = malloc(PAGE_SIZE);
    int slotOffset , length;
    fileHandle.readPage(rid.pageNum,buffer);

    memcpy((char*)&slotOffset,(char*)buffer + offset, sizeof(int));
    memcpy((char*)&length,(char*)buffer + offset + sizeof(int), sizeof(int));

//    std::cout<<"slot details"<<std::endl;
//    std::cout<<slotOffset<<std::endl;
//    std::cout<<length<<std::endl;

    if(length == -1 && slotOffset == -1)
    {
        std::cout<<"Trying to read deleted record"<<std::endl;
        return -1;
    } else if (slotOffset > 0 && length == -1)
    {
        std::cout<<"pointer check"<<std::endl;
        RID newRid;
        memcpy((char*)&newRid.pageNum,(char*)buffer + slotOffset, sizeof(int));
        memcpy((char*)&newRid.slotNum,(char*)buffer + slotOffset + sizeof(int), sizeof(int));
        free(buffer);
        RecordBasedFileManager::readRecord(fileHandle,recordDescriptor,newRid,data);
    } else {

        int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
        int newSize = length - sizeof(int) - (recordDescriptor.size() * sizeof(int)) + nullInfo;
        void* record = malloc(newSize);
        RecordBasedFileManager::decodeRecord(fileHandle,recordDescriptor,(char*)buffer + slotOffset,record);

        memcpy(data,record,newSize);
        free(record);
        free(buffer);
    }
    return 0;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    // if pagenumber doesn't exist, return -1
    int totalPages = fileHandle.getNumberOfPages();
    if(rid.pageNum >= totalPages)
        return -1;

    //get the page
    void* page = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, page);


    //get total number of slots
    int ptrOffsets = PAGE_SIZE - 2* sizeof(int);
    int recordOffset , slotOffset;

    memcpy((char*)&recordOffset, (char*)page + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)page + ptrOffsets + sizeof(int), sizeof(int));
    int totalNumSlots = (ptrOffsets - slotOffset ) / (2 * sizeof(int));

    int freeSpace;
    memcpy(&freeSpace, (char*)page, sizeof(int));

    //get the offset for the record to be deleted, offset and length for the record and update them to new length 0 and offset -1
    //Make the necessary changes and get the reqd info

    int offset = ptrOffsets - (rid.slotNum) * 2 * sizeof(int);
    int recordStart, recordLength, newLength = -1, markRecPtr = -1;

    memcpy(&recordStart, (char *) page + offset , sizeof(int));
    memcpy(&recordLength, (char *) page + offset, sizeof(int));

    memcpy((char*) page + offset , &markRecPtr, sizeof(int));
    memcpy((char *) page + offset + sizeof(int), &newLength, sizeof(int));

    //move records and slots if the record num to be deleted is more than one and not the last record
    int slotct = totalNumSlots - (rid.slotNum);

    if (slotct < totalNumSlots) {
        int recEnd = recordStart + recordLength;
        memmove((char*) page + recordStart, (char*)page + recEnd, recordOffset - recEnd);

        //updating the offsets of the remaining slots
        for (int i = slotct + 1 ; i <= totalNumSlots; i++) {
            offset -= 2 * sizeof(int);
            int recPtr, reclength;
            memcpy((char*)&recPtr, (char*) page + offset, sizeof(int));
            memcpy((char*)&reclength,(char*)page + offset + sizeof(int), sizeof(int));

            // If record is deleted, no need to update offset.
            if (recPtr != -1 && reclength != -1) {
                recPtr -= recordLength;
                memcpy((char*)page + offset,(char*)&recPtr, sizeof(int));
            }
        }
    } else {
         recordOffset -= recordLength;
         memcpy((char *) page + ptrOffsets, &recordOffset, sizeof(int));
    }

    //Update free space info
    freeSpace += recordLength;
    memcpy((char*)page, (char*)&freeSpace, sizeof(int));

    int res = fileHandle.writePage(rid.pageNum, page);
    free(page);
    return res;
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
                     delete[] tempString;
                 } else
                 {
                     std::cout<<recordDescriptor[itr].name<<":\tNULL"<<"\t";
                 }
                 break;
         }
     }
     std::cout<<std::endl;
     delete[] bitInfo;
     return 0;
 }

 RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const void *data, const RID &rid) {
     return -1;
 }

 RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                          const RID &rid, const std::string &attributeName, void *data) {
     void* buffer = malloc(PAGE_SIZE);
     int offset = -1;
     int type = -1;
     RecordBasedFileManager::readRecord(fileHandle,recordDescriptor,rid,buffer);
     for(int i = 0; i< recordDescriptor.size();i++)
     {
         if(attributeName == recordDescriptor[i].name)
         {
             offset = sizeof(int) + (i * sizeof(int));
             type = recordDescriptor[i].type;
             break;
         }
     }

     if(type == TypeInt || type == TypeReal)
         memcpy(data,(char*)buffer + offset, sizeof(int));
     else
         {
             int length;
             memcpy((char*)&length,(char*)buffer + offset, sizeof(int));
             memcpy(data,(char*)buffer + offset + sizeof(int),length);
         }
     free(buffer);
     return 0;
 }

 RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                 const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                 const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
     return -1;
 }
