#include "ix.h"
#include <iostream>

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

RC IndexManager::createFile(const std::string &fileName) {
    PagedFileManager& pfm = PagedFileManager::instance();
    if(pfm.createFile(fileName.c_str()) != 0)
        return -1;
    return 0;
}

RC IndexManager::destroyFile(const std::string &fileName) {
    return PagedFileManager::instance().destroyFile(fileName);

}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {
    return PagedFileManager::instance().openFile(fileName,ixFileHandle.fileHandle);
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
    return PagedFileManager::instance().closeFile(ixFileHandle.fileHandle);
}
/*
RC IndexManager::compareInt(void *entry, void *recordOnPage){
    int entryKey, entrySlotNum, entryPageNum;
    int recordKey, recordSlotNum, recordPageNum;
    memcpy(&entryKey,(char *)entry, sizeof(int));
    memcpy(&entryPageNum,(char *)entry + sizeof(int), sizeof(int));
    memcpy(&entrySlotNum,(char *)entry + 2 * sizeof(int), sizeof(int));

    memcpy(&recordKey,(char *)recordOnPage, sizeof(int));
    memcpy(&recordPageNum,(char *)recordOnPage + sizeof(int), sizeof(int));
    memcpy(&recordSlotNum,(char *)recordOnPage + 2 * sizeof(int), sizeof(int));
    if(recordKey < entryKey or recordPageNum  ?? entryPageNum)
        return 1;
    return -1

}
*/
RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {




    return -1;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    return -1;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {
    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {

    //check if a B+ tree exists
    int rootKeysCount = getRoot(ixFileHandle), newNode = 0;
    if(rootKeysCount < 0) {
        std::cout<<"B+ tree doesn't exist"<<std::endl;
        return;
    }

    // Start with root -> pageNum -> 0
    printCurrentNode(ixFileHandle, attribute, 0, newNode);

}
int getRoot(IXFileHandle &ixFileHandle){

    //No root if the file has no pages, return -1
    if(!ixFileHandle.fileHandle.getNumberOfPages())
        return -1;

    //open file and get the number of keys on root page and return the same
    int rootPageNum = 0, offset = 10;
    void *pageData = malloc(PAGE_SIZE);
    ixFileHandle.fileHandle.readPage(0, pageData);
    memcpy(&rootPageNum, (char *)pageData + offset + sizeof(int), sizeof(int));
    free(pageData);
    return rootPageNum;

}
bool IndexManager::isLeaf(const void *page) const{

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[1] == 'L') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool IndexManager::isInter(const void *page) const{

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[0] == 'I') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}
void IndexManager:: printCurrentNode(IXFileHandle &ixFileHandle, const Attribute &attribute, int pageNum, int newNode) const{


    //read the page and get the number of records on that page
    void *pageData = malloc(PAGE_SIZE);
    ixFileHandle.fileHandle.readPage(pageNum, pageData);
    int headerOffset = 10, numOfKeysCount = 0;
    memcpy(&numOfKeysCount, (char*)pageData + headerOffset - sizeof(int), sizeof(int) );




    //getting the type of the current node
    bool isleaf;
    isleaf = isLeaf(pageData);




    std::cout<<"{";

    //check if it's the root in which case print the start of the json string
    if(pageNum == getRoot(ixFileHandle) && !isleaf){
        std::cout << "\n";
    }
    std::cout << "\"keys\": [";

    //get the number of records on that page
    int numRecords;
    memcpy(&numRecords, (char*)pageData + 6, sizeof(int));

    RID currRID;
    //if leaf node print the data too

    if(isleaf){
        switch(attribute.type){
            case TypeInt:
                    for (int i = 0; i < numRecords; i++)
                    {
                        int key;

                        //header + P0 + 3*i*sizeof(int)
                        memcpy(&key, (char*)pageData + 10  + 2* sizeof(int) + 3*i*sizeof(int), sizeof(int));
                        memcpy(&currRID.pageNum, (char*)pageData + 10 + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                        memcpy(&currRID.slotNum, (char*)pageData + 10 + 4* sizeof(int) + 3* sizeof(int)*i, sizeof(int));


                        if (!newNode)
                        {
                            newNode = true;
                        }

                            if (newNode && i > 0)
                                std::cout << "]\", ";
                            std::cout << "\"" ;
                            std::cout << key <<  ": [(" << currRID.pageNum << ", "<< currRID.slotNum << ")";
                    }
                     break;
            case TypeReal:
                for (int i = 0; i < numRecords; i++)
                {
                    float key;

                    //header + P0 + 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + headerOffset  + 2* sizeof(int) + 3*i*sizeof(int), sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    memcpy(&currRID.slotNum, (char*)pageData + headerOffset + 4* sizeof(int) + 3* sizeof(int)*i, sizeof(int));


                    if (!newNode)
                    {
                        newNode = true;
                    }

                    if (newNode && i > 0)
                        std::cout << "]\", ";
                    std::cout << "\"" ;
                    std::cout << key <<  ": [(" << currRID.pageNum << ", "<< currRID.slotNum << ")";
                }

                break;

            case TypeVarChar:

                int offset = 0;
                for (int i = 0; i < numRecords; i++) {

                        int keyLength;
                        std::string key;
                        //get the length of the record
                        memcpy(&keyLength, (char*) pageData + headerOffset + offset, sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset + offset + sizeof(int) + keyLength, sizeof(int));
                    memcpy(&currRID.slotNum,(char*)pageData + headerOffset + offset + 2*sizeof(int) + keyLength, sizeof(int));

                    for(int j = 0; j < keyLength; j++){
                            char temp;
                            memcpy(&temp, (char*)pageData + headerOffset + offset + sizeof(int) + j* sizeof(char),
                                   sizeof(char) );
                            key += temp;
                        }

                    offset += sizeof(int) + keyLength + 2* sizeof(int);
                    if (!newNode)
                    {
                        newNode = true;
                    }

                    if (newNode && i > 0)
                        std::cout << "]\", ";
                    std::cout << "\"" ;
                    std::cout << key <<  ": [(" << currRID.pageNum << ", "<< currRID.slotNum << ")";

                    }

                break;

        }
        std::cout << "]";

        
    }
    //if non leaf node print the key values
    else{

        switch(attribute.type){
            case TypeInt:

                for (int i = 0; i < numRecords; i++) // key_2 to key_m
                {
                    int key;

                    //header + P0 + 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + 10  + 2* sizeof(int) + 3*i*sizeof(int), sizeof(int));
                    if (i > 0)
                        std::cout << ", ";
                    std::cout << "\"" << key << "\"";
                }
                // Print m+1 children
                std::cout << "],\n\"children\": [\n";
                for (int i = 0; i <= numRecords; i++) // child_1 to child_m
                {
                    if (i > 0)
                        std::cout << ",\n";
                    int pageNum;
                    memcpy(&pageNum, (char*)pageData + 10 + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    if (pageNum >=0)
                        printCurrentNode(ixFileHandle, attribute, pageNum, newNode);
                    else
                        std::cout << "{\"keys\": [\"\"]}"; // Empty child
                }


                break;
            case TypeReal:

                for (int i = 0; i < numRecords; i++) // key_2 to key_m
                {
                    float key;

                    //header + P0 + 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + 10  + 2* sizeof(int) + 3*i*sizeof(int), sizeof(int));
                    if (i > 0)
                        std::cout << ", ";
                    std::cout << "\"" << key << "\"";
                }
                // cout m + 1 children
                std::cout << "],\n\"children\": [\n";
                for (int i = 0; i <= numRecords; i++) // child_1 to child_m
                {
                    if (i > 0)
                        std::cout << ",\n";
                    int pageNum;
                    memcpy(&pageNum, (char*)pageData + headerOffset + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    if (pageNum >=0)
                        printCurrentNode(ixFileHandle, attribute, pageNum, newNode);
                    else
                        std::cout << "{\"keys\": [\"\"]}"; // Empty child
                }


                break;

            case TypeVarChar:
                int offset = 0;
                std::vector<int> keyLengthVec;
                for (int i = 0; i < numRecords; i++) {
                    int keyLength;
                    std::string key;
                    //get the length of the record
                    memcpy(&keyLength, (char*) pageData + headerOffset + offset, sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset + offset + sizeof(int) + keyLength, sizeof(int));
                    memcpy(&currRID.slotNum,(char*)pageData + headerOffset + offset + 2*sizeof(int) + keyLength, sizeof(int));


                    keyLengthVec.push_back(keyLength);
                    for(int j = 0; j < keyLength; j++){
                        char temp;
                        memcpy(&temp, (char*)pageData + headerOffset + offset + sizeof(int) + j* sizeof(char),
                               sizeof(char) );
                        key += temp;
                    }  offset += sizeof(int) + keyLength + 2* sizeof(int);

                    std::cout << "\"" << key << "\"";


                } // cout m + 1 children
                std::cout << "],\n\"children\": [\n";
                for(int i = 0; i <= numRecords; i++) // child_1 to child_m
                {
                    int pageNum;
                    if (i > 0)
                        std::cout << ",\n";
                    //header + length of key + keydata
                    memcpy(&pageNum, (char*) pageData + headerOffset + sizeof(int)+keyLengthVec[i], sizeof(int));
                    if (pageNum >=0)
                        printCurrentNode(ixFileHandle, attribute, pageNum, newNode);
                    else
                        std::cout << "{\"keys\": [\"\"]}"; // Empty child

                }


                break;
        }
        std::cout << "]";

    }
    free(pageData);
    if (getRoot(ixFileHandle) == pageNum && !isleaf)
       std:: cout << "\n";
    std::cout << "}";



}

IX_ScanIterator::IX_ScanIterator() {
}

IX_ScanIterator::~IX_ScanIterator() {
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {
    return -1;
}

RC IX_ScanIterator::close() {
    return -1;
}

IXFileHandle::IXFileHandle() {
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
}

IXFileHandle::~IXFileHandle() {
}

RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {

    void* buffer = malloc(PAGE_SIZE);
    file->seekg(0,std::ios_base::beg);
    file->read((char*)buffer,PAGE_SIZE);

    int readPage , writePage , appendPage;

    memcpy((char*)&readPage,(char*)buffer + sizeof(int) + sizeof(int), sizeof(int));
    memcpy((char*)&writePage,(char*)buffer + sizeof(int) + 2 * sizeof(int), sizeof(int));
    memcpy((char*)&appendPage,(char*)buffer + sizeof(int) + 3 * sizeof(int), sizeof(int));

    readPageCount = readPage;
    writePageCount = writePage;
    appendPageCount = appendPage;

    memcpy((char*)buffer + sizeof(int) + sizeof(int),(char*)&readPage, sizeof(int));
    memcpy((char*)buffer + sizeof(int) + 2 * sizeof(int),(char*)&writePage, sizeof(int));
    memcpy((char*)buffer + sizeof(int) + 3 * sizeof(int),(char*)&appendPage, sizeof(int));

    file->seekp(0,std::ios_base::beg);
    file->write((char*)buffer,PAGE_SIZE);
    file->seekp(0,std::ios_base::beg);

    free(buffer);
    return 0;
}

