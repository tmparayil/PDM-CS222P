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

    bool debugFlag = false;
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

    if(debugFlag)
    {
        std::cout<<"encode Record()"<<std::endl;
        std::cout<<"---------------"<<std::endl;
        std::cout<<std::endl;
        std::cout<<"size of record :"<<size<<std::endl;
        std::cout<<"size of nullBits :"<<nullInfo<<std::endl;
    }

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

                    if(dataOffset >= PAGE_SIZE)
                        std::cout<<"Check this, we have an error : "<<dataOffset<<std::endl;

                    if(debugFlag)
                    {
                        std::cout<<"Type int -- not NULL"<<std::endl;
                        std::cout<<"dataOffset after insert : "<<dataOffset<<std::endl;
                        std::cout<<"offset after insert : "<<offset<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);

                    if(debugFlag)
                    {
                        std::cout<<"Type Varchar -- NULL"<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
                }
                break;
            case TypeReal:
                if(!checkNullBit) {
                    memcpy((char*)record + dataOffset,(char*)data + offset, sizeof(int));
                    dataOffset += sizeof(float);
                    offset += sizeof(float);

                    memcpy((char*)record + slotOffset,(char*)&dataOffset, sizeof(int));
                    slotOffset += sizeof(float);

                    if(debugFlag)
                    {
                        std::cout<<"Type Real -- not NULL"<<std::endl;
                        std::cout<<"dataOffset after insert : "<<dataOffset<<std::endl;
                        std::cout<<"offset after insert : "<<offset<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);

                    if(debugFlag)
                    {
                        std::cout<<"Type Varchar -- NULL"<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
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

                    if(debugFlag)
                    {
                        std::cout<<"Type Varchar -- not NULL"<<std::endl;
                        std::cout<<"dataOffset after insert : "<<dataOffset<<std::endl;
                        std::cout<<"offset after insert : "<<offset<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
                } else
                {
                    memcpy((char*)record + slotOffset,(char*)&valNotPresent, sizeof(int));
                    slotOffset += sizeof(int);
                    if(debugFlag)
                    {
                        std::cout<<"Type Varchar -- NULL"<<std::endl;
                        std::cout<<"slotOffset after insert : "<<slotOffset<<std::endl;
                    }
                }
                break;
        }
    }
    memcpy(returnedData,record,size);
    free(record);
    delete[] bitInfo;
}

int RecordBasedFileManager::getEncodedRecordSize(const void *data, const std::vector<Attribute> &recordDescriptor){

    bool debugFlag = false;
    int lastPtr;
    int numberOfSlots = recordDescriptor.size();
    int offset = 0;
    for(int i = numberOfSlots - 1;i >= 0; i--)
    {
        offset = (i * sizeof(int)) + sizeof(int);

        if(debugFlag)
        {
            std::cout<<"attribute : "<<i<<std::endl;
            std::cout<<"offset : "<<offset<<std::endl;
        }
        memcpy((char*)&lastPtr,(char*)data + offset, sizeof(int));
        if(debugFlag)
        {
            std::cout<<"lastPtr : "<<lastPtr<<std::endl;
        }
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

    bool debugFlag = false;

    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);
    int recSize = RecordBasedFileManager::getEncodedRecordSize(data,recordDescriptor);
    int newSize = recSize > 0 ? recSize - sizeof(int) - (recordDescriptor.size() * sizeof(int)) + nullInfo : nullInfo;

    if(debugFlag)
    {
        std::cout<<"decodeRecord"<<std::endl;
        std::cout<<"encoded record size :"<<recSize<<std::endl;
        std::cout<<"new Size :"<<newSize<<std::endl;
    }

    void* record = malloc(newSize);

    for(int i=0;i<recordDescriptor.size();i++)
    {
        int temp;
        memcpy((char*)&temp,(char*)data + sizeof(int) + (sizeof(int) * i), sizeof(int));
        int bitShift = CHAR_BIT - 1 - i%CHAR_BIT;

        if(debugFlag)
        {
            std::cout<<recordDescriptor[i].name<<" : "<<temp<<"  attribute offset values"<<std::endl;
        }
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
    bool debugFlag = false;

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

    if(debugFlag)
    {
        std::cout<<"reading from page : "<<page<<std::endl;
    }

    // Currently we get page data from above block and then read page again
    // We have page in memory, which we can try to re-use below.
    // Reading slot and record offsets
    void* buffer = malloc(PAGE_SIZE);
    fileHandle.readPage(page,buffer);

    int ptrOffsets = PAGE_SIZE - (2* sizeof(int));
    int recordOffset , slotOffset;

    // We get slotOffset and recordOffset from buffer
    memcpy((char*)&recordOffset, (char*)buffer + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)buffer + ptrOffsets + sizeof(int), sizeof(int));

    int insertOffset = -1;
    int totalSlot = ((ptrOffsets - slotOffset ) / (2 * sizeof(int)));

    int slotNumber = nextAvailableSlot(buffer,ptrOffsets, slotOffset);

    if(debugFlag)
    {
        std::cout<<"next avail slot : "<<slotNumber<<std::endl;
        std::cout<<"record offset : "<<recordOffset<<std::endl;
        std::cout<<"slot offset : "<<slotOffset<<std::endl;
    }

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

    if(debugFlag)
    {
        std::cout<<"RID details"<<std::endl;
        std::cout<<rid.pageNum << "\t"<<rid.slotNum<<std::endl;
    }

    // Inserting record into page at recordOffset
    memcpy((char*)buffer + recordOffset,(char*)record,newSize);

    // Insert new record slot in slot directory
    memcpy((char*)buffer + insertOffset,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + insertOffset + sizeof(int),(char*)&newSize, sizeof(int));

    recordOffset += newSize;
    int freeSpace = slotOffset - recordOffset;

    if(debugFlag)
    {
        std::cout<<"updated record ptr : "<<recordOffset<<std::endl;
        std::cout<<"updated slot ptr : "<<slotOffset<<std::endl;
        std::cout<<"updated free space : "<<freeSpace<<std::endl;
    }

    // Updating slot-record pointers after insert
    memcpy((char*)buffer + ptrOffsets,(char*)&recordOffset, sizeof(int));
    memcpy((char*)buffer + ptrOffsets + sizeof(int),(char*)&slotOffset, sizeof(int));


    memcpy((char*)buffer,(char*)&freeSpace, sizeof(int));

    fileHandle.writePage(page,buffer);
    free(buffer);
    free(record);
    return 0;
}

// Current assumption is that when reading a pointer record -> pageNum , slotNum is the order
RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {

    bool debugFlag = false;

    if(debugFlag)
    {
        std::cout<<std::endl;
        std::cout<<"read Record()"<<std::endl;
        std::cout<<"--------------"<<std::endl;
        std::cout<<std::endl;
        std::cout<<"RID details"<<std::endl;
        std::cout<<rid.pageNum << "\t"<<rid.slotNum<<std::endl;
    }

    int offset = PAGE_SIZE - (2 * sizeof(int)) - (rid.slotNum * 2 * sizeof(int));
    void* buffer = malloc(PAGE_SIZE);
    int slotOffset , length;
    fileHandle.readPage(rid.pageNum,buffer);

    memcpy((char*)&slotOffset,(char*)buffer + offset, sizeof(int));
    memcpy((char*)&length,(char*)buffer + offset + sizeof(int), sizeof(int));

    if(debugFlag)
    {
        std::cout<<"slot details"<<std::endl;
        std::cout<<slotOffset<<std::endl;
        std::cout<<length<<std::endl;
    }


    if(length == -1 && slotOffset == -1)
    {
        if(debugFlag)
        {
            std::cout<<"Trying to read deleted record"<<std::endl;
        }
        return -1;
    } else if (slotOffset > 0 && length == -1)
    {
        if(debugFlag)
        {
            std::cout<<"pointer check"<<std::endl;
        }
        RID newRid;
        memcpy((char*)&newRid.pageNum,(char*)buffer + slotOffset, sizeof(int));
        memcpy((char*)&newRid.slotNum,(char*)buffer + slotOffset + sizeof(int), sizeof(int));
        free(buffer);

        if(debugFlag)
        {
            std::cout<<"new RID :"<<newRid.pageNum<<"\t"<<newRid.slotNum<<std::endl;
            std::cout<<std::endl;
        }
        readRecord(fileHandle,recordDescriptor,newRid,data);
    } else {

        int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
        int newSize = length - sizeof(int) - (recordDescriptor.size() * sizeof(int)) + nullInfo;
        void* record = malloc(newSize);
        RecordBasedFileManager::decodeRecord(fileHandle,recordDescriptor,(char*)buffer + slotOffset,record);

        if(debugFlag)
        {
            std::cout<<"decode done"<<std::endl;
        }
        memcpy(data,record,newSize);
        free(record);
        free(buffer);
    }
    return 0;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    bool debugFlag = false;

    if(debugFlag)
    {
        std::cout<<std::endl;
        std::cout<<"delete record()"<<std::endl;
        std::cout<<"----------------"<<std::endl;
        std::cout<<std::endl;
        std::cout<<"RID details"<<std::endl;
        std::cout<<rid.pageNum << "\t"<<rid.slotNum<<std::endl;
        std::cout<<std::endl;
    }

    // if pagenumber doesn't exist, return -1
    int totalPages = fileHandle.getNumberOfPages();
    if(rid.pageNum >= totalPages)
        return -1;

    //get the page
    void* page = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, page);


    //get total number of slots
    int ptrOffsets = PAGE_SIZE - (2* sizeof(int));
    int recordOffset , slotOffset;

    memcpy((char*)&recordOffset, (char*)page + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)page + ptrOffsets + sizeof(int), sizeof(int));
    int totalNumSlots = (ptrOffsets - slotOffset ) / (2 * sizeof(int));

    if(debugFlag)
    {
        std::cout<<"record offset : "<<recordOffset<<std::endl;
        std::cout<<"slot offset : "<<slotOffset<<std::endl;
        std::cout<<"total slots : "<<totalNumSlots<<std::endl;
    }

    int freeSpace;
    memcpy(&freeSpace, (char*)page, sizeof(int));

    //get the offset for the record to be deleted, offset and length for the record and update them to new length 0 and offset -1
    //Make the necessary changes and get the reqd info

    int offset = PAGE_SIZE - (2 * sizeof(int)) - ((rid.slotNum) * 2 * sizeof(int));
    int recordStart, recordLength, newLength = -1, markRecPtr = -1;

    memcpy(&recordStart, (char*) page + offset , sizeof(int));
    memcpy(&recordLength, (char*) page + offset + sizeof(int), sizeof(int));

    if(debugFlag)
    {
        std::cout<<"record start : "<<recordStart<<std::endl;
        std::cout<<"record length : "<<recordLength<<std::endl;
    }

    if(recordLength == -1 && recordStart == -1)
    {
        if(debugFlag) {
            std::cout << "record is already deleted." << std::endl;
            std::cout << "RID : " << rid.pageNum << " " << rid.slotNum << std::endl;
        }
        return -1;
    }
    // Handle tombstones
    else if(recordLength == -1 && recordStart > 0)
    {
        RID newRID;
        memcpy((char*)&newRID.pageNum,(char*)page + recordStart, sizeof(int));
        memcpy((char*)&newRID.slotNum,(char*)page + recordStart+ sizeof(int), sizeof(int));

        if(debugFlag)
        {
            std::cout<<"Tombstone delete!!!!!!!"<<std::endl;
            std::cout<<"new RID :"<<newRID.pageNum<<"  "<<newRID.slotNum<<std::endl;
            std::cout<<std::endl;
        }

        memcpy((char*) page + offset , (char*)&markRecPtr, sizeof(int));
        memcpy((char *) page + offset + sizeof(int), (char*)&newLength, sizeof(int));

        if(rid.slotNum < totalNumSlots) {
            int recEnd = recordStart + (2 * sizeof(int));
            memmove((char *) page + recordStart, (char *) page + recEnd, recordOffset - recEnd);

            //updating the offsets of the remaining slots
            for (int i = rid.slotNum + 1; i <= totalNumSlots; i++) {
                offset -= (2 * sizeof(int));
                int recPtr;
                memcpy((char *) &recPtr, (char *) page + offset, sizeof(int));

                if (debugFlag) {
                    std::cout << "slot : " << i << std::endl;
                    std::cout << "record ptr : " << recPtr << std::endl;
                }
                // If record is deleted, no need to update offset.
                if (recPtr > recordStart) {
                    recPtr -= (2 * sizeof(int));
                    memcpy((char *) page + offset, (char *) &recPtr, sizeof(int));

                    if (debugFlag) {
                        std::cout << "after updating, record ptr : " << recPtr << std::endl;
                    }
                }
            }

            recordOffset -= (2 * sizeof(int));
            memcpy((char *) page + ptrOffsets, (char *) &recordOffset, sizeof(int));

            //Update free space info
            freeSpace += (2 * sizeof(int));
            memcpy((char *) page, (char *) &freeSpace, sizeof(int));

            if (debugFlag) {
                std::cout << "Updated record offset : " << recordOffset << std::endl;
                std::cout << "Updated free space : " << freeSpace << std::endl;
            }
        }
      
            deleteRecord(fileHandle,recordDescriptor,newRID);
    } else
    {
        int neg = -1;
        memcpy((char*) page + offset , (char*)&neg, sizeof(int));
        memcpy((char *) page + offset + sizeof(int), (char*)&neg, sizeof(int));

        //move records and slots if the record num to be deleted is more than one and not the last record
        int slotct = rid.slotNum;
        if (slotct < totalNumSlots) {
            int recEnd = recordStart + recordLength;
            memmove((char*) page + recordStart, (char*)page + recEnd, recordOffset - recEnd);

            //updating the offsets of the remaining slots
            for (int i = slotct + 1 ; i <= totalNumSlots; i++) {
                offset -= 2 * sizeof(int);
                int recPtr;
                memcpy((char*)&recPtr, (char*) page + offset, sizeof(int));

                if(debugFlag)
                {
                    std::cout<<"slot : "<<i<<std::endl;
                    std::cout<<"record ptr : "<<recPtr<<std::endl;
                }
                // If record is deleted, no need to update offset.
                if (recPtr > recordStart) {
                    recPtr -= recordLength;
                    memcpy((char*)page + offset,(char*)&recPtr, sizeof(int));

                    if(debugFlag)
                    {
                        std::cout<<"after updating, record ptr : "<<recPtr<<std::endl;
                    }
                }
            }
        }
        recordOffset -= recordLength;
        memcpy((char *) page + ptrOffsets, (char*)&recordOffset, sizeof(int));

        //Update free space info
        freeSpace += recordLength;
        memcpy((char*)page, (char*)&freeSpace, sizeof(int));

        if(debugFlag)
        {
            std::cout<<"Updated record offset : "<<recordOffset<<std::endl;
            std::cout<<"Updated free space : "<<freeSpace<<std::endl;
        }
    }

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
                    std::cout<<recordDescriptor[itr].name<<": "<<tempInt<<" ";
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
                    std::cout<<recordDescriptor[itr].name<<": "<<tempFloat<<" ";
                    offset += sizeof(float);
                } else
                {
                    std::cout<<recordDescriptor[itr].name<<": NULL"<<" ";
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
                    std::cout<<recordDescriptor[itr].name<<": ";
                    std::cout<<tempString<<" ";
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

    bool debugFlag = false;

    if(debugFlag)
    {
        std::cout<<std::endl;
        std::cout<<"Update record()"<<std::endl;
        std::cout<<rid.pageNum<<" : Page : Slot : "<<rid.slotNum<<std::endl;
        std::cout<<"----------------"<<std::endl;
    }

    //if pageNum doesn't exist return -1
    int totalPages = fileHandle.getNumberOfPages();
    if(rid.pageNum >= totalPages)
        return -1;

    //get the current size and start of the record
    void* page = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, page);

    int ptrOffsets = PAGE_SIZE - 2* sizeof(int), recordStart, curr_recordLength;
    int offset = PAGE_SIZE - (2* sizeof(int))  - (rid.slotNum * 2 * sizeof(int));

    //getting record and slot offsets
    int recordOffset , slotOffset;
    memcpy((char*)&recordOffset, (char*)page + ptrOffsets, sizeof(int));
    memcpy((char*)&slotOffset, (char*)page + ptrOffsets + sizeof(int), sizeof(int));
    int totalNumSlots = (ptrOffsets - slotOffset ) / (2 * sizeof(int));

    memcpy(&recordStart, (char *) page + offset , sizeof(int));
    memcpy(&curr_recordLength, (char *) page + offset + sizeof(int), sizeof(int));


    if(debugFlag)
    {
        std::cout<<recordStart<<" : Record Start"<<std::endl;
        std::cout<<curr_recordLength<<" : Record Length"<<std::endl;
        std::cout<<recordOffset<<" : Record Offset"<<std::endl;
        std::cout<<slotOffset<<" : Slot Offset"<<std::endl;
    }
    //If Tombstone
    if(curr_recordLength == -1 && recordStart > 0)
    {
        RID newRid;
        memcpy((char*)&newRid.pageNum,(char*)page + recordStart, sizeof(int));
        memcpy((char*)&newRid.slotNum,(char*)page + recordStart + sizeof(int), sizeof(int));

        updateRecord(fileHandle,recordDescriptor,data,newRid);
    }
    else if(curr_recordLength == -1 && recordStart == -1)
    {
        if(debugFlag)
        {
            std::cout<<"Trying to update a deleted record"<<std::endl;
        }
        return -1;
    } else
    {
        //get the size required for the updated record
        int updatedRecordSize = getRecSize(data, recordDescriptor);
        int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);
        int newSize = updatedRecordSize - nullInfo + sizeof(int)+ (recordDescriptor.size()* sizeof(int)); // + version no
        int difference = newSize - curr_recordLength;
        int freeSpace;
        memcpy(&freeSpace, (char*)page, sizeof(int));

        if(debugFlag)
        {
            std::cout<<updatedRecordSize<<" : Input record size"<<std::endl;
            std::cout<<nullInfo<<" : Null Bit Length"<<std::endl;
            std::cout<<newSize<<" : New Size of record"<<std::endl;
            std::cout<<difference<<" :Difference in size"<<std::endl;
            std::cout<<freeSpace<<" :Free Space on page"<<std::endl;
        }

        //preparing updated record to insert using encode function
        void* record = malloc(newSize);
        RecordBasedFileManager::encodeRecord(fileHandle,recordDescriptor,data,record);

        if(difference <= 0){
            // updated record requires equal or lesser space
            memcpy((char*) page + recordStart, (char*)record, newSize);
            int recEndPrev = recordStart + curr_recordLength;
            int recEndCurr = recordStart + newSize;

            if(debugFlag)
            {
                std::cout<<"Record details"<<std::endl;
                std::cout<<"---------------"<<std::endl;
                std::cout<<recEndPrev<<" : Prev record end"<<std::endl;
                std::cout<<recEndCurr<<" : Curr record end"<<std::endl;
                std::cout<<(recordOffset - recEndPrev)<<" : shifted these many bytes by : "<<(recEndPrev - recEndCurr)<<std::endl;
            }

            if(recordOffset - recEndPrev == 0)
            {
                if(debugFlag)
                {
                    std::cout<<"Last record on page : "<<rid.pageNum<<std::endl;
                    std::cout<<"New end of record of slot :"<<rid.slotNum<<" is :"<<recEndCurr<<std::endl;
                    std::cout<<"New record offset : "<<(recordOffset + difference);
                }
            } else
            {
                memmove((char*) page + recEndCurr, (char*)page + recEndPrev, recordOffset - recEndPrev);
            }

            offset = PAGE_SIZE - (2 * sizeof(int)) - (rid.slotNum * 2 * sizeof(int));
            //changing the current record's length in the slot entry
            memcpy((char *) page + offset + sizeof(int), &newSize, sizeof(int));



            //updating the offsets of the remaining slots
            offset = PAGE_SIZE - (2 * sizeof(int)) - (2 * sizeof(int)*rid.slotNum);

            if (rid.slotNum < totalNumSlots) {
                for (int i = rid.slotNum + 1; i <= totalNumSlots; i++) {
                    offset -= (2 * sizeof(int));
                    int recPtr;int length;
                    memcpy((char*)&recPtr, (char*) page + offset, sizeof(int));
                    memcpy((char*)&length, (char*) page + offset + sizeof(int), sizeof(int));

                    if(debugFlag)
                    {
                        std::cout<<i<<" : slot number"<<std::endl;
                        std::cout<<recPtr<<" : record pointer in slot"<<std::endl;
                        std::cout<<length<<" : length in slot"<<std::endl;
                    }
                    // If record is deleted, no need to update offset.
                    if (recPtr > recordStart) {
                        recPtr += difference;
                        memcpy((char *) page + offset, (char *) &recPtr, sizeof(int));

                        if(debugFlag)
                        {
                            std::cout<<recPtr<<" : updated record pointer in slot"<<std::endl;
                        }
                    }
                }
            }

            // difference is a negative value. Hence addition
            recordOffset += difference;
            memcpy((char *) page + ptrOffsets, &recordOffset, sizeof(int));

            // difference is < 0
            freeSpace -= difference;
            memcpy((char*)page, (char*)&freeSpace, sizeof(int));

            if(debugFlag)
            {
                std::cout<<recordOffset<<" : record offset updated"<<std::endl;
                std::cout<<freeSpace<<" : Free space updated"<<std::endl;
            }
        }else{
            if(freeSpace >= difference){

                //shift right the contents of the next records and changing the length of the current slot
                int recEndPrev = recordStart + curr_recordLength;
                int recEndCurr = recordStart + newSize;

                if(debugFlag)
                {
                    std::cout<<"Record details"<<std::endl;
                    std::cout<<"---------------"<<std::endl;
                    std::cout<<recEndPrev<<" : Prev record end"<<std::endl;
                    std::cout<<recEndCurr<<" : Curr record end"<<std::endl;
                    std::cout<<(recordOffset - recEndPrev)<<" : shifted these many bytes by "<<difference<<std::endl;
                }

                if(recordOffset - recEndPrev == 0)
                {
                    if(debugFlag)
                    {
                        std::cout<<"Last record on page : "<<rid.pageNum<<std::endl;
                        std::cout<<"New end of record of slot :"<<rid.slotNum<<" is :"<<recEndCurr<<std::endl;
                        std::cout<<"New record offset : "<<(recordOffset - difference);
                    }
                } else
                {
                    memmove((char*) page + recEndCurr, (char*)page + recEndPrev, recordOffset - recEndPrev);
                }

                offset = PAGE_SIZE - (2 * sizeof(int)) - (rid.slotNum * 2 * sizeof(int));
                memcpy((char *) page + offset + sizeof(int), &newSize, sizeof(int));


                //updating the offsets of the remaining slots

                if (rid.slotNum < totalNumSlots) {
                    for (int i = rid.slotNum + 1; i <= totalNumSlots; i++) {
                        offset -= 2 * sizeof(int);
                        int recPtr; int length;
                        memcpy((char *) &recPtr, (char *) page + offset, sizeof(int));
                        memcpy((char *) &length, (char *) page + offset + sizeof(int), sizeof(int));
                        if(debugFlag)
                        {
                            std::cout<<i<<" : slot number"<<std::endl;
                            std::cout<<recPtr<<" : record pointer in slot"<<std::endl;
                            std::cout<<length<<" : length in slot"<<std::endl;
                        }
                        // If record is deleted, no need to update offset.
                        if (recPtr > recordStart ) {
                            recPtr += difference;
                            memcpy((char *) page + offset, (char *) &recPtr, sizeof(int));
                            if(debugFlag)
                            {
                                std::cout<<recPtr<<" : updated record pointer in slot"<<std::endl;
                            }
                        }
                    }
                }
                recordOffset += difference;
                memcpy((char *) page + ptrOffsets, &recordOffset, sizeof(int));
                //updating the current record
                memcpy((char*) page + recordStart, (char*)record, newSize);

                freeSpace -= difference;
                memcpy((char*)page, (char*)&freeSpace, sizeof(int));

                if(debugFlag)
                {
                    std::cout<<recordOffset<<" : record offset updated"<<std::endl;
                    std::cout<<freeSpace<<" : Free space updated"<<std::endl;
                }

            }else{

                offset = PAGE_SIZE - (2 * sizeof(int)) - (2 * sizeof(int)*rid.slotNum);
                //free space not available...store record elsewhere
                //length of the record -1
                int recPtr = -1;
                memcpy((char *)page + offset + sizeof(int),(char*)&recPtr, sizeof(int));

                //left shifting the other records -> curr record -> 8 Bytes
                int bytesForPtrRec = 2* sizeof(int);
                int recEndPrev = recordStart + curr_recordLength;
                int recEndCurr = recordStart + bytesForPtrRec;
                difference = curr_recordLength - bytesForPtrRec;

                if(debugFlag)
                {
                    std::cout<<"Tombstone!!!!!"<<std::endl;
                    std::cout<<"Record details"<<std::endl;
                    std::cout<<"---------------"<<std::endl;
                    std::cout<<recEndPrev<<" : Prev record end"<<std::endl;
                    std::cout<<recEndCurr<<" : Curr record end"<<std::endl;
                    std::cout<<(recordOffset - recEndPrev)<<" : shifted these many bytes by : "<<difference<<std::endl;
                }
                if(recordOffset - recEndPrev == 0)
                {
                    if(debugFlag)
                    {
                        std::cout<<"Last record on page : "<<rid.pageNum<<std::endl;
                        std::cout<<"New end of record of slot :"<<rid.slotNum<<" is :"<<recEndCurr<<std::endl;
                        std::cout<<"New record offset : "<<(recordOffset - difference);
                    }
                } else
                {
                    memmove((char*)page + recEndCurr, (char*)page + recEndPrev, recordOffset - recEndPrev);
                }

                //updating the offsets of other records in slots
                int offsetForSlots = curr_recordLength - bytesForPtrRec;
                if (rid.slotNum < totalNumSlots) {
                    for (int i = rid.slotNum + 1; i <= totalNumSlots; i++) {
                        offset -= (2 * sizeof(int));
                        int recPtr, recLength;
                        memcpy((char *)&recPtr, (char *) page + offset, sizeof(int));
                        memcpy((char *)&recLength, (char *) page + offset + sizeof(int), sizeof(int));
                        if(debugFlag)
                        {
                            std::cout<<i<<" : slot number"<<std::endl;
                            std::cout<<recPtr<<" : record pointer in slot"<<std::endl;
                            std::cout<<recLength<<" : record length in slot"<<std::endl;
                        }
                        // If record is deleted, no need to update offset.
                        if (recPtr > recordStart) {
                            recPtr -= offsetForSlots;
                            memcpy((char *)page + offset, (char*)&recPtr, sizeof(int));
                            if(debugFlag)
                            {
                                std::cout<<recPtr<<" : updated record pointer in slot"<<std::endl;
                            }
                        }
                    }
                }

                //updating last inserted record pointer
                recordOffset -= offsetForSlots;
                memcpy((char *) page + ptrOffsets, (char*)&recordOffset, sizeof(int));

                //freespace update
                freeSpace += offsetForSlots;
                memcpy((char*)page, (char*)&freeSpace, sizeof(int));

                if(debugFlag)
                {
                    std::cout<<recordOffset<<" : record offset updated"<<std::endl;
                    std::cout<<freeSpace<<" : Free space updated"<<std::endl;
                }

                //find space to store the updated record and store the ptr here -> insertRecord
                RID updatedRID;
                insertRecord(fileHandle, recordDescriptor, data, updatedRID);
                int updatedRID_pagenum = updatedRID.pageNum, updatedRId_slotnum = updatedRID.slotNum;

                memcpy((char*) page + recordStart, (char*)&updatedRID_pagenum, sizeof(int));
                memcpy((char*)page + recordStart + sizeof(int),(char*)&updatedRId_slotnum, sizeof(int));

                if(debugFlag)
                {
                    std::cout<<updatedRID_pagenum<<" : new RID pageNum"<<std::endl;
                    std::cout<<updatedRId_slotnum<<" : new RID slotNum"<<std::endl;
                }
            }
        }
        fileHandle.writePage(rid.pageNum, page);
        free(record);
    }

    free(page);
    return 0;
}


RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    bool debugFlag = false;
    void* buffer = malloc(PAGE_SIZE);
    bool skipFlag = false;

    if(debugFlag)
    {
        std::cout<<"RID : "<<rid.pageNum<<" : "<<rid.slotNum<<std::endl;
    }

    int nullInfo = 1;
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);

    if(fileHandle.readPage(rid.pageNum,buffer) != 0)
    {
        free(buffer);
        return -1;
    }

    int offset = PAGE_SIZE - (2 * sizeof(int)) - (rid.slotNum * 2 * sizeof(int));
    int recordOffset,length;

    memcpy((char*)&recordOffset, (char*)buffer + offset, sizeof(int));
    memcpy((char*)&length, (char*)buffer + offset + sizeof(int), sizeof(int));

    if(debugFlag)
    {
        std::cout<<"Null info : "<<nullInfo<<std::endl;
        std::cout<<"offset : "<<offset<<std::endl;
        std::cout<<"record offset : "<<recordOffset<<std::endl;
        std::cout<<"length : "<<length<<std::endl;
        std::cout<<"attribute name : "<<attributeName<<std::endl;
    }

    if(recordOffset == -1 && length == -1)
        return -1;
    else if(recordOffset > 0 && length == -1)
    {
        RID newRID;
        memcpy((char*)&newRID.pageNum,(char*)buffer + recordOffset, sizeof(int));
        memcpy((char*)&newRID.slotNum,(char*)buffer + recordOffset + sizeof(int), sizeof(int));
        readRecord(fileHandle,recordDescriptor,newRID,data);
    } else
    {
        int attr = -1;
        int type = -1;
        for(int i=0;i<recordDescriptor.size();i++)
        {
            if(recordDescriptor[i].name == attributeName) {
                attr = i;
                type = recordDescriptor[i].type;

                if(debugFlag)
                {
                    std::cout<<"Attr details : "<<attr<< " :  " << type<<std::endl;
                }

                // BitInfo formation logic
                int temp;
                memcpy((char *) &temp, (char *) buffer + recordOffset + sizeof(int) + (sizeof(int) * i), sizeof(int));

                if(debugFlag)
                {
                    std::cout<<"Temp value : "<<temp<<std::endl;
                }
                int bitShift = 7;
                if (temp == -1) {
                    bitInfo[0] = bitInfo[0] | (1 << bitShift);
                    skipFlag = true;
                }
            }
        }

        memcpy((char*)data,bitInfo,nullInfo);
        delete[] bitInfo;
        if(skipFlag)
        {
            free(buffer);
            return 0;
        }

        int attribOffset;

        if(attr == 0)
        {
            attribOffset = sizeof(int) * (recordDescriptor.size() + 1);
        } else if(attr > 0)
        {
            attr--;
            int offset = sizeof(int) + (attr* sizeof(int));
            memcpy((char*)&attribOffset,(char*)buffer + recordOffset + offset, sizeof(int));
            while(attribOffset == -1)
            {
                if(attr == 0)
                {
                    attribOffset = sizeof(int) * (recordDescriptor.size() + 1);
                    break;
                }
                attr--;
                offset = sizeof(int) + (attr* sizeof(int));
                memcpy((char*)&attribOffset,(char*)buffer + recordOffset + offset, sizeof(int));
            }
        }

        if(debugFlag)
        {
            std::cout<<"Attrib offset : "<<attribOffset<<std::endl;
        }

        if(type == TypeReal || type == TypeInt)
        {
            memcpy((char*)data + nullInfo,(char*)buffer + recordOffset + attribOffset, sizeof(int));
        }
        else if(type == TypeVarChar)
        {
            int length;
            memcpy((char*)&length,(char*)buffer + recordOffset + attribOffset, sizeof(int));
            memcpy((char*)data + nullInfo,(char*)buffer + recordOffset + attribOffset, sizeof(int) + length);
        }
    }

    free(buffer);
    return 0;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {

    //Read total number of pages
    //For each page -> read total number of slots
    //For each record -> check condition
    //If link -> store in HashSet

    rbfm_ScanIterator.fileHandle = fileHandle;
    rbfm_ScanIterator.recordDescriptor = recordDescriptor;
    rbfm_ScanIterator.comparisonOperator = compOp;
    rbfm_ScanIterator.attributeNames = attributeNames;


    getConditionAttributeDetails(recordDescriptor,conditionAttribute,rbfm_ScanIterator);
    if(value != NULL)
        getConditionValue(rbfm_ScanIterator,value);

    rbfm_ScanIterator.currRID.pageNum = 0;
    rbfm_ScanIterator.currRID.slotNum = 1;

    return 0;
}

/**
 * returns -2 if there is no attribute specified in the scan operation
 * returns -1 if current record does not have any record which matches the given attribute name
 * returns index of attribute for correct scenario :: starts from 0
 *
 * @param recordDescriptor
 * @param conditionAttribute
 * @param rbfmScanIterator
 * @return
 */
RC RecordBasedFileManager::getConditionAttributeDetails(const std::vector<Attribute> &recordDescriptor, const std::string &conditionAttribute, RBFM_ScanIterator &rbfmScanIterator) {

    rbfmScanIterator.conditionAttributeId = -1;

    for(int i=0;i < recordDescriptor.size(); i++)
    {
        if(recordDescriptor[i].name == conditionAttribute)
        {
            rbfmScanIterator.conditionAttributeId = i;
            rbfmScanIterator.conditionAttributeType = recordDescriptor[i].type;
            break;
        }
    }

    return 0;
}

/**
 * Storing the value to rbfm_ScanIterator.value
 * For int/float -> malloc 4B and then store the value
 * For varchar -> malloc 4B + length of varchar and store the offset + string
 *
 * @param rbfm_ScanIterator
 * @param value
 * @return
 */
RC RecordBasedFileManager::getConditionValue(RBFM_ScanIterator &rbfm_ScanIterator, const void *value) {

    // Storing the string value with the length offset
    if(rbfm_ScanIterator.conditionAttributeType == TypeVarChar)
    {
        int length;
        memcpy((char*)&length,(char*)value, sizeof(int));
        rbfm_ScanIterator.value = malloc(length + sizeof(int));
        memcpy(rbfm_ScanIterator.value,value,(length + sizeof(int)));
    } else
    {
        rbfm_ScanIterator.value = malloc(sizeof(int));
        memcpy(rbfm_ScanIterator.value,value, sizeof(int));
    }
}

RC RBFM_ScanIterator::mappingRecord(const std::vector<Attribute> recordDescriptor, const void *record, void *data, const std::vector<std::string> attributeNames) {

    bool debugFlag = false;
    int nullInfo = ceil((double) attributeNames.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);

    if(debugFlag)
    {
        std::cout<<std::endl;
        std::cout<<"mapping record()"<<std::endl;
        std::cout<<"----------------"<<std::endl;
        std::cout<<"Null info : "<<nullInfo<<std::endl;
    }
    int offset = 0;
    int dataOffset = nullInfo;
    int j = 0 , i =0;
    while(j < attributeNames.size())
    {
        if(i == recordDescriptor.size())
            i = 0;

        if(debugFlag)
        {
            std::cout<<std::endl;
            std::cout<<"for mapping attr pos "<<j<<" in total "<<attributeNames.size()<<std::endl;
            std::cout<<"Attribute comparison"<<std::endl;
            std::cout<<"attribute name : "<<attributeNames[j]<<std::endl;
            std::cout<<"record desc name : "<<recordDescriptor[i].name<<std::endl;
        }

        if(attributeNames[j] == recordDescriptor[i].name)
        {
            offset = sizeof(int) + (sizeof(int) * i);
            int temp;

            if(debugFlag)
            {
                std::cout<<"i index : "<<i<<std::endl;
                std::cout<<"offset : "<<offset<<std::endl;
            }

            memcpy((char*)&temp,(char*)record + offset, sizeof(int));
            int bitShift = CHAR_BIT - 1 - i%CHAR_BIT;
            if(temp == -1)
            {
                bitInfo[j/CHAR_BIT] = bitInfo[j / CHAR_BIT] | (1 << (bitShift));
                j++;
            }

            int attribOffset;
            if(offset == sizeof(int))
            {
                attribOffset = sizeof(int) + recordDescriptor.size() * sizeof(int);
            } else
            {
                offset = offset - sizeof(int);
                memcpy((char*)&attribOffset,(char*)record + offset, sizeof(int));
            }

            if(debugFlag)
            {
                std::cout<<"offset : "<<offset<<std::endl;
                std::cout<<"attrib offset : "<<attribOffset<<std::endl;
            }

            while(attribOffset == -1)
            {
                int prevOffset = offset - sizeof(int);
                if(prevOffset == sizeof(int))
                {
                    attribOffset = sizeof(int) + (recordDescriptor.size()* sizeof(int));
                    break;
                }
                prevOffset -= sizeof(int);
                memcpy((char*)&attribOffset,(char*)record + prevOffset, sizeof(int));
            }

            if(debugFlag)
            {
                std::cout<<"attrib offset : "<<attribOffset<<std::endl;
            }


            if(recordDescriptor[i].type == TypeInt || recordDescriptor[i].type == TypeReal)
            {

                int temp;
                memcpy((char*)&temp,(char*)record + attribOffset, sizeof(int));

                if(debugFlag)
                {
                    std::cout<<"Value for Int and Real"<<std::endl;
                    std::cout<<"value : "<<temp<<std::endl;
                    std::cout<<"data Offset : "<<dataOffset<<std::endl;
                }

                memcpy((char*)data + dataOffset,(char*)record + attribOffset, sizeof(int));
                dataOffset += sizeof(int);
            }
            else if(recordDescriptor[i].type == TypeVarChar)
            {
                int length;
                memcpy((char*)&length,(char*)record + attribOffset, sizeof(int));
                //copying length and string to data

                // debug stmts
                char* tempString = new char[length+1];
                memcpy((char*)tempString,(char*)record + attribOffset + sizeof(int),length);
                tempString[length] = '\0';
                if(debugFlag)
                {
                    std::cout<<"Value for varchar"<<std::endl;
                    std::cout<<"value : "<<tempString<<std::endl;
                    std::cout<<"length : "<<length<<std::endl;
                    std::cout<<"data Offset : "<<dataOffset<<std::endl;
                }
                delete [] tempString;
                memcpy((char*)data + dataOffset,(char*)record + attribOffset, sizeof(int) + length);
                // TBD
                dataOffset += (sizeof(int) + length);
            }
            j++;
        }
        i++;
    }
    // copying the null info
    memcpy((char*)data,(char*)bitInfo,nullInfo);

    delete[] bitInfo;
    return 0;
}

/**
 * Checks if current page of RID is readable
 * Check if slot number provided is valid for the page
 *     -> if not, return RBFM_EOF
 *     -> if yes, read the record and check if for the attribute to be compared, the value matches the condition
 * @param rid
 * @param data
 * @return
 */
RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data) {

    void* buffer = malloc(PAGE_SIZE);
    int MAX_RC_SIZE = PAGE_SIZE - (5 * sizeof(int));
    void* record = malloc(MAX_RC_SIZE);

    int totalPages = fileHandle.getNumberOfPages();

    if(fileHandle.readPage(currRID.pageNum,buffer) != 0)
    {
        free(record);
        free(buffer);
        return RBFM_EOF;
    }

    int ptrOffsets = PAGE_SIZE - (2 * sizeof(int));
    int slotOffset;
    memcpy((char*)&slotOffset,(char*)buffer + ptrOffsets, sizeof(int));

    int numOfSlots = (ptrOffsets - slotOffset) / (2 * sizeof(int));

    while(!validSlot(buffer,record))
    {
        currRID.slotNum++;
        // >= for the corner case where the first assigned slot in a page = total number of slots in the page
        if(currRID.slotNum >= numOfSlots)
        {
            currRID.pageNum++;
            // >= for the corner case where the first assigned page number = total number of pages
            if(currRID.pageNum >= totalPages)
            {
                free(buffer);
                free(record);
                return RBFM_EOF;
            }
            currRID.slotNum = 1;
            fileHandle.readPage(currRID.pageNum,buffer);
        }
    }

    rid.pageNum = currRID.pageNum;
    rid.slotNum = currRID.slotNum;

    currRID.slotNum++;
    if(currRID.slotNum > numOfSlots)
    {
        currRID.pageNum++;
        if(currRID.pageNum > totalPages)
        {
            free(buffer);
            free(record);
            return RBFM_EOF;
        }
        currRID.slotNum = 1;
    }

    mappingRecord(recordDescriptor,record,data,attributeNames);

    free(buffer);
    free(record);
    return 0;
}



RC RBFM_ScanIterator::validSlot(const void *buffer, void *record) {

    if(!ridExistsInPage(buffer))
    {
        return false;
    }
    //Read the record in given page for the given RID
    readRecordOnPageForRID(buffer,recordDescriptor,record);

    return satisfyCondition(record);
}



bool RBFM_ScanIterator::ridExistsInPage(const void* buffer)
{
    int slotNum = currRID.slotNum;
    int ptrOffsets = PAGE_SIZE - (2 * sizeof(int));
    int slotOffset;
    memcpy((char*)&slotOffset,(char*)buffer + ptrOffsets + sizeof(int), sizeof(int));

    int totalSlots = (ptrOffsets - slotOffset) / (2 * sizeof(int));

    if(slotNum > totalSlots)
    {
        return false;
    }

    int offset = ptrOffsets - (2 * slotNum * sizeof(int));
    int length , recordOffset;
    memcpy((char*)&recordOffset,(char*)buffer + offset, sizeof(int));
    memcpy((char*)&length,(char*)buffer + offset + sizeof(int), sizeof(int));

    if(recordOffset == -1 && length == -1)
        return false;
    else if(recordOffset > 0 && length == -1)
            return false;

    return true;
}


RC RBFM_ScanIterator::readRecordOnPageForRID(const void* buffer,const std::vector<Attribute> recordDescriptor,void* record)
{
    int slotNum = currRID.slotNum;
    int ptrOffsets = PAGE_SIZE - (2 * sizeof(int));
    int slotOffset;
    memcpy((char*)&slotOffset,(char*)buffer + ptrOffsets + sizeof(int), sizeof(int));

    int totalSlots = (ptrOffsets - slotOffset) / (2 * sizeof(int));

    if(slotNum > totalSlots)
    {
        return false;
    }

    int offset = ptrOffsets - (2 * slotNum * sizeof(int));
    int length , recordOffset;
    memcpy((char*)&recordOffset,(char*)buffer + offset, sizeof(int));
    memcpy((char*)&length,(char*)buffer + offset + sizeof(int), sizeof(int));

    memcpy(record,(char*)buffer + recordOffset,length);
    return 0;
}

bool RBFM_ScanIterator::satisfyCondition(const void *record) {

    if(comparisonOperator == NO_OP)
    {
        return true;
    }

    if(conditionAttributeType == TypeInt)
    {
        int recordValue;
        int offset = sizeof(int) + (conditionAttributeId * sizeof(int));
        int attribOffset;
        memcpy((char*)&attribOffset,(char*)record + offset, sizeof(int));
        if(attribOffset == -1)
            return false;



        memcpy((char*)&recordValue,(char*)record + attribOffset - sizeof(int), sizeof(int));
        int compareValue;
        memcpy((char*)&compareValue,value, sizeof(int));
        return checkConditionInt(recordValue,compareValue,comparisonOperator);
    }
    else if(conditionAttributeType == TypeReal)
    {
        float recordValue;
        int offset = sizeof(int) + (conditionAttributeId * sizeof(int));
        int attribOffset;
        memcpy((char*)&attribOffset,(char*)record + offset, sizeof(int));
        if(attribOffset == -1)
            return false;

        memcpy((char*)&recordValue,(char*)record + attribOffset - sizeof(int), sizeof(int));
        float compareValue;
        memcpy((char*)&compareValue,value, sizeof(int));
        return checkConditionFloat(recordValue,compareValue,comparisonOperator);
    }
    else if(conditionAttributeType == TypeVarChar)
    {
        // If conditionAttributeId = 0 --> first attribute
        if(conditionAttributeId == 0)
        {
            int offset = sizeof(int);
            int attribOffset;
            memcpy((char*)&attribOffset,(char*)record + offset, sizeof(int));
            // NULL check
            if(attribOffset == -1)
                return false;

            offset = recordDescriptor.size()* sizeof(int) + sizeof(int);
            int recordLength;
            memcpy((char*)&recordLength,(char*)record + offset, sizeof(int));
            char* tempString = new char[recordLength+1];
            memcpy(tempString,(char*)record + offset + sizeof(int), recordLength);
            tempString[recordLength] = '\0';
            int length;
            memcpy((char*)&length,(char*)value, sizeof(int));
            char* compareString = new char[length+1];
            memcpy(compareString,(char*)value + sizeof(int),length);
            compareString[length] = '\0';

            return checkConditionChar(tempString,compareString,comparisonOperator);
        }
        else
        {
            int offset = sizeof(int) + (conditionAttributeId * sizeof(int));
            int attribOffset;
            memcpy((char*)&attribOffset,(char*)record + offset, sizeof(int));

            if(attribOffset == -1)
                return false;

            int prevOffset = offset - sizeof(int);
            memcpy((char*)&attribOffset,(char*)record + prevOffset, sizeof(int));

            while(attribOffset == -1)
            {
                if(prevOffset == sizeof(int))
                {
                    attribOffset = sizeof(int) + (recordDescriptor.size()* sizeof(int));
                    break;
                }
                prevOffset -= sizeof(int);
                memcpy((char*)&attribOffset,(char*)record + prevOffset, sizeof(int));
            }

            int recordLength;
            memcpy((char*)&recordLength,(char*)record + attribOffset, sizeof(int));

            char* tempString = new char[recordLength+1];
            memcpy(tempString,(char*)record + attribOffset + sizeof(int), recordLength);
            tempString[recordLength] = '\0';

            int length;
            memcpy((char*)&length,(char*)value, sizeof(int));
            char* compareString = new char[length+1];
            memcpy(compareString,(char*)value + sizeof(int),length);
            compareString[length] = '\0';

//            std::cout<<tempString<<":\t:"<<compareString<<std::endl;
            return checkConditionChar(tempString,compareString,comparisonOperator);
        }
    }

    return -1;
}

bool RBFM_ScanIterator::checkConditionInt(int recordValue, int compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return (recordValue == compareValue);
    else if(comparisonOperator == LT_OP)
        return (recordValue < compareValue);
    else if(comparisonOperator == LE_OP)
        return (recordValue <= compareValue);
    else if(comparisonOperator == GT_OP)
        return (recordValue > compareValue);
    else if(comparisonOperator == GE_OP)
        return  (recordValue >= compareValue);
    else if(comparisonOperator == NE_OP)
        return (recordValue != compareValue);
}

bool RBFM_ScanIterator::checkConditionFloat(float recordValue, float compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return (recordValue == compareValue);
    else if(comparisonOperator == LT_OP)
        return (recordValue < compareValue);
    else if(comparisonOperator == LE_OP)
        return (recordValue <= compareValue);
    else if(comparisonOperator == GT_OP)
        return (recordValue > compareValue);
    else if(comparisonOperator == GE_OP)
        return  (recordValue >= compareValue);
    else if(comparisonOperator == NE_OP)
        return (recordValue != compareValue);
}

bool RBFM_ScanIterator::checkConditionChar(char *recordValue, char *compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return strcmp(recordValue,compareValue) == 0;
    else if(comparisonOperator == LT_OP)
        return strcmp(recordValue,compareValue) < 0;
    else if(comparisonOperator == LE_OP)
        return strcmp(recordValue,compareValue) <= 0;
    else if(comparisonOperator == GT_OP)
        return strcmp(recordValue,compareValue) > 0;
    else if(comparisonOperator == GE_OP)
        return strcmp(recordValue,compareValue) >= 0;
    else if(comparisonOperator == NE_OP)
        return strcmp(recordValue,compareValue) != 0;
}
