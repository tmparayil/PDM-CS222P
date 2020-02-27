#include "ix.h"

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

bool fileExists(const std::string &fileName)
{
    std::ifstream f(fileName.c_str());
    if(f.good())
    {
        f.close();
        return true;
    }
    return false;
}

/*
 * 0-4 Root page
 * 4-8 Num of pages
 * 8-12 Read count
 * 12-16 Write count
 * 16-20 Append count
 */
void initialise(std::fstream &file)
{
    void* buffer = malloc(PAGE_SIZE);
    int root = 0;
    int recordNum = 0;
    int readCount = 0;
    int writeCount = 0;
    int appendCount = 1;

    // First time creation of hidden page
    memcpy((char*)buffer,(char*)&root, sizeof(int));
    memcpy((char*)buffer + sizeof(int),(char*)&recordNum, sizeof(int));
    memcpy((char*) buffer + sizeof(int) + sizeof(int),(char*)&readCount, sizeof(int));
    memcpy((char*)buffer + sizeof(int) + 2 * sizeof(int),(char*)&writeCount, sizeof(int));
    memcpy((char*)buffer + sizeof(int) + 3 * sizeof(int),(char*)&appendCount, sizeof(int));

    file.seekp(0,std::ios_base::beg);
    file.write((char*)buffer,PAGE_SIZE);
    file.seekp(0,std::ios_base::beg);
    free(buffer);
}

RC IndexManager::createFile(const std::string &fileName) {
    if(fileExists(fileName))
        return -1;

    std::fstream newFile(fileName,std::ios::out);
    if(!newFile)
    {
        return -1;
    }

    initialise(newFile);
    newFile.close();
    return 0;
}

RC IndexManager::destroyFile(const std::string &fileName) {
    if(!fileExists(fileName))
        return -1;

    if(std::remove(fileName.c_str()) != 0)
        return -1;

    return 0;
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {

    if(ixFileHandle.getFile() != nullptr)
    {
        return -1;
    }

    if(fileExists(fileName)) {
        ixFileHandle.setFile(const_cast<std::string &>(fileName));
        return 0;
    } 

    return -1;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {

    if((ixFileHandle.getFile()) != nullptr)
    {
        ixFileHandle.closeFile();
        return 0;
    }
    return -1;
}



int getRootPage(IXFileHandle &ixFileHandle){
    void* buffer = malloc(PAGE_SIZE);
    int root;
    ixFileHandle.readPage(-1,buffer);
    memcpy((char*)&root,(char*)buffer, sizeof(int));
    free(buffer);
    return root;
}

void IndexManager::setRootInHidden(IXFileHandle &ixFileHandle, int rootNum) {

    void* buffer = malloc(PAGE_SIZE);
    ixFileHandle.readPage(-1,buffer);
    memcpy((char*)buffer,(char*)&rootNum, sizeof(int));
    ixFileHandle.writePage(-1,buffer);
    free(buffer);
    return;
}

// Structure of root Page
// 0-1 -> I
// 1-2 -> L
// 2-6 -> freeSpace
// 6-10 -> numOfSlots
void IndexManager::setRootPage(IXFileHandle &ixFileHandle,void* entry,int length,bool flag) {

    bool debugFlag = false;
    void* page = malloc(PAGE_SIZE);
    char* inter = new char[2];
    inter[0] = 'I';
    inter[1] = 'L';
    memcpy((char*)page,inter, (2* sizeof(char)));
    delete[] inter;

    int slots = 0;
    int freeSpace = PAGE_SIZE - 10 - 4;
    int ptr = -1;

    memcpy((char*)page + (2* sizeof(char)),(char*)&freeSpace, sizeof(int));
    memcpy((char*)page + (2* sizeof(char)) + sizeof(int),(char*)&slots, sizeof(int));

    if(flag)
        memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&ptr, sizeof(int));

    if(!flag)
    {
        char unset = '0';
        memcpy((char*)page + sizeof(char),(char*)&unset, sizeof(char));
        //Need to remove the last 4
        setSpaceOnPage(page,PAGE_SIZE - 10);
        freeSpace = PAGE_SIZE - 10;
    }

    if(length != 0)
    {
        memcpy((char*)page + 10,(char*)entry,length);
        setSpaceOnPage(page,freeSpace - length);
        setSlotOnPage(page,1);

        if(debugFlag)
        {
            std::cout<<length<<std::endl;
            std::cout<<"entry not null"<<std::endl;
            int ptr1,ptr2;
            memcpy((char*)&ptr1,(char*)entry, sizeof(int));
            memcpy((char*)&ptr2,(char*)entry + length - 4, sizeof(int));
            std::cout<<"pointers inserted in root"<<std::endl;
            std::cout<<ptr1<<","<<ptr2<<std::endl;
        }
    }

    ixFileHandle.appendPage(page);

    int currPageNum = ixFileHandle.getNumberOfPages();
    setRootInHidden(ixFileHandle,currPageNum - 1);

    if(debugFlag)
    {
        std::cout<<"New root page : "<<currPageNum - 1<<std::endl;
    }
    free(page);
    return;
}

//Setting sibling pointer as -1
void IndexManager::newLeafPage(void* page) {

    char* inter = new char[2];
    inter[0] = '0';
    inter[1] = 'L';
    memcpy((char*)page,inter, (2* sizeof(char)));
    delete[] inter;
    int slots = 0;
    int newPtr = -1;
    int freeSpace = PAGE_SIZE - 14;
    memcpy((char*)page + (2* sizeof(char)),(char*)&freeSpace, sizeof(int));
    memcpy((char*)page + (2* sizeof(char)) + sizeof(int),(char*)&slots, sizeof(int));
    memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&newPtr, sizeof(int));
    return;
}

void IndexManager::newInterPage(void* page) {

    char* inter = new char[2];
    inter[0] = 'I';
    inter[1] = '0';
    memcpy((char*)page,inter, (2* sizeof(char)));
    delete[] inter;
    int slots = 0;
    int freeSpace = PAGE_SIZE - 10;
    memcpy((char*)page + (2* sizeof(char)),(char*)&freeSpace, sizeof(int));
    memcpy((char*)page + (2* sizeof(char)) + sizeof(int),(char*)&slots, sizeof(int));
    return;
}

bool isRoot(const void *page){

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2);

    if(checkArr[0] == 'I') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool isLeaf(const void *page){

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[1] == 'L') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool isInter(const void *page){

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[0] == 'I') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

int IndexManager::getLengthOfEntry(const void *key, const Attribute &attribute) {

    if(attribute.type == TypeVarChar)
    {
        int length;
        memcpy((char*)&length,(char*)key, sizeof(int));
        return 12 + length;
    }
    else if(attribute.type == TypeReal || attribute.type == TypeInt)
        return 12;
}

int getSpaceOnPage(const void *page) {
    int space;
    memcpy((char*)&space,(char*)page + (2* sizeof(char)), sizeof(int));
    return space;
}

int getSlotOnPage(const void *page) {
    int slot;
    memcpy((char*)&slot,(char*)page + (2* sizeof(char)) + sizeof(int), sizeof(int));
    return slot;
}


void setSpaceOnPage(const void *page,int space) {
    memcpy((char*)page + (2* sizeof(char)),(char*)&space, sizeof(int));
    return;
}

void setSlotOnPage(const void *page,int slot) {
    memcpy((char*)page + (2* sizeof(char))+ sizeof(int),(char*)&slot, sizeof(int));
    return;
}

bool isSpaceAvailable(const void *page, int length) {
    int space = getSpaceOnPage(page);
    if(space >= length)
        return true;

    return false;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {

    int totalPages = ixFileHandle.getNumberOfPages();

    if(totalPages == 0)
    {
        setRootPage(ixFileHandle,NULL,0,true);
    }

    void* page = malloc(PAGE_SIZE);
    int root = getRootPage(ixFileHandle);

    /* std::cout<<"\n\n"<<std::endl;
     std::cout<<"root page : "<<root<<std::endl;
 */
    ixFileHandle.readPage(root,page);

    if(!isRoot(page))
    {
        std::cout<<"not a valid root page"<<std::endl;
        return -1;
    }

    void* returnedChild = malloc(PAGE_SIZE);
    int n1 = -1 , n2 = -1;
    int length = -1;

    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        void* newKey = malloc(12); // sizeof(int) + RID size (2 * sizeof(int))
        memcpy((char*)newKey,(char*)key, sizeof(int));
        memcpy((char*)newKey + sizeof(int),(char*)&rid.pageNum, sizeof(int));
        memcpy((char*)newKey + (2* sizeof(int)),(char*)&rid.slotNum, sizeof(int));

        insertIntoPage(ixFileHandle, attribute, root, newKey, returnedChild,n1,n2,length);
        free(newKey);
    }
    else if(attribute.type == TypeVarChar)
    {
        int leng;
        memcpy((char*)&leng,(char*)key, sizeof(int));
        void* newKey = malloc(12 + leng); // sizeof(int) + length of string + RID size (2 * sizeof(int))
        memcpy((char*)newKey,(char*)key, sizeof(int)+leng);
        memcpy((char*)newKey + sizeof(int)+leng,(char*)&rid.pageNum, sizeof(int));
        memcpy((char*)newKey + leng + (2* sizeof(int)),(char*)&rid.slotNum, sizeof(int));

        insertIntoPage(ixFileHandle,attribute,root,newKey,returnedChild,n1,n2,length);
        free(newKey);
    }

    //std::cout<<"length returned : "<<length<<std::endl;

    if(length != -1)
    {
        //std::cout<<"Length returned : "<<std::endl;
        if(attribute.type == TypeInt || attribute.type == TypeReal)
        {
            void* newKey = malloc(20);
            memcpy((char*)newKey,(char*)&n1, sizeof(int));
            memcpy((char*)newKey + sizeof(int),(char*)returnedChild,length);
            memcpy((char*)newKey + 16,(char*)&n2, sizeof(int));
            setRootPage(ixFileHandle,newKey,20,false);
	    free(newKey);
        }
        else if(attribute.type == TypeVarChar)
        {
            void* newKey = malloc(length + 8);
            memcpy((char*)newKey,(char*)&n1, sizeof(int));
            memcpy((char*)newKey + sizeof(int),(char*)returnedChild,length);
            memcpy((char*)newKey + length + 4,(char*)&n2, sizeof(int));
            setRootPage(ixFileHandle,newKey,length+8,false);
	    free(newKey);
        }
    }
    free(returnedChild);
    free(page);
    return 0;
}

int compareInt(const void *entry, const void *recordOnPage) {

    int entryKey, entrySlotNum, entryPageNum;
    int recordKey, recordSlotNum, recordPageNum;
    memcpy(&entryKey,(char *)entry, sizeof(int));
    memcpy(&entryPageNum,(char *)entry + sizeof(int), sizeof(int));
    memcpy(&entrySlotNum,(char *)entry + 2 * sizeof(int), sizeof(int));
    memcpy(&recordKey,(char *)recordOnPage, sizeof(int));
    memcpy(&recordPageNum,(char *)recordOnPage + sizeof(int), sizeof(int));
    memcpy(&recordSlotNum,(char *)recordOnPage + 2 * sizeof(int), sizeof(int));

    if(entryKey < recordKey)
        return -1;
    else if(entryKey == recordKey && entryPageNum < recordPageNum)
        return -1;
    else if(entryKey == recordKey && entryPageNum == recordPageNum && entrySlotNum < recordSlotNum)
        return -1;
    return 1;
}


int compareReal(const void *entry, const void *recordOnPage) {
    int entryKey, entrySlotNum, entryPageNum;
    int recordKey, recordSlotNum, recordPageNum;
    memcpy(&entryKey,(char *)entry, sizeof(int));
    memcpy(&entryPageNum,(char *)entry + sizeof(int), sizeof(int));
    memcpy(&entrySlotNum,(char *)entry + 2 * sizeof(int), sizeof(int));
    memcpy(&recordKey,(char *)recordOnPage, sizeof(int));
    memcpy(&recordPageNum,(char *)recordOnPage + sizeof(int), sizeof(int));
    memcpy(&recordSlotNum,(char *)recordOnPage + 2 * sizeof(int), sizeof(int));
    if(entryKey < recordKey)
        return -1;
    else if(entryKey == recordKey && entryPageNum < recordPageNum)
        return -1;
    else if(entryKey == recordKey && entryPageNum == recordPageNum && entrySlotNum < recordSlotNum)
        return -1;
    return 1;


}

int compareVarChar(const void *entry, const void *recordOnPage) {

    int recordLength;
    memcpy((char*)&recordLength,(char*)entry, sizeof(int));

    char* tempString = new char[recordLength+1];
    memcpy(tempString,(char*)entry + sizeof(int), recordLength);
    tempString[recordLength] = '\0';

    int length;
    memcpy((char*)&length,(char*)recordOnPage, sizeof(int));
    char* compareString = new char[length+1];
    memcpy(compareString,(char*)recordOnPage + sizeof(int),length);
    compareString[length] = '\0';
    int compare =  strcmp(tempString,compareString);
    if(compare != 0)
    {
	delete[] tempString;
	delete[] compareString;
        return compare;
    } else
    {
	delete[] tempString;
	delete[] compareString;
        int entryPage , entrySlot;
        int recordPage,recordSlot;
        memcpy((char*)&entryPage,(char*)entry + recordLength + sizeof(int), sizeof(int));
        memcpy((char*)&entrySlot,(char*)entry + recordLength + 8, sizeof(int));

        memcpy((char*)&recordPage,(char*)recordOnPage + length + sizeof(int), sizeof(int));
        memcpy((char*)&recordSlot,(char*)recordOnPage + length + 8, sizeof(int));

        if(entryPage < recordPage)
            return -1;
        else if(entryPage == recordPage && entrySlot < recordSlot)
            return -1;
        else
            return 1;
    }
}


void IndexManager::addToPage(void *page, const void *newKey,const Attribute &attribute) {

    //search for location
    int numSlots = getSlotOnPage(page);
    // start of record offset
    int offset = 10;

    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        for(int i=1;i<=numSlots;i++)
        {
            void* temp = malloc(12);
            memcpy((char*)temp,(char*)page + offset,12);
            int compare = compareInt(newKey,temp);
            free(temp);

            if(compare > 0)
            {
                offset += 12;
                continue;
            }
            // if newKey is less than the record on page. Insert newEntry at current offset.
            if(compare < 0)
            {
                break;
            }
        }

        int space = getSpaceOnPage(page);
        int recPtrEnd = PAGE_SIZE - 4 - space;
        int shift = recPtrEnd - offset;

        //offset is at the point of insert.
        if(shift != 0)
            memmove((char*)page + offset + 12, (char*)page + offset,shift);

        memcpy((char*)page + offset,(char*)newKey,12);
        space -= 12;
        setSpaceOnPage(page,space);
        setSlotOnPage(page,numSlots + 1);

        return;
    }
    else if(attribute.type == TypeVarChar)
    {
        for(int i=1;i<=numSlots;i++)
        {
            int length;
            memcpy((char*)&length,(char*)page + offset, sizeof(int));
            void* temp = malloc(12+length);
            memcpy((char*)temp,(char*)page + offset,12+length);
            int compare = compareVarChar(newKey,temp);
            free(temp);

            if(compare == 0)
            {
                offset += length + 12;
                break;
            }
            else if(compare > 0)
            {
                offset += length + 12;
                continue;
            }
                // if newKey is less than the record on page. Insert newEntry at current offset.
            else if(compare < 0)
            {
                break;
            }
        }

        //std::cout<<"offset : "<<offset<<std::endl;

        int space = getSpaceOnPage(page);
        int recPtrEnd = PAGE_SIZE - 4 - space;
        int shift = recPtrEnd - offset;
        //offset is at the point of insert.
        //std::cout<<"Free Space : "<<space<<std::endl;
        // std::cout<<"recPtrEnd : "<<recPtrEnd<<"  :  shift : "<<shift<<std::endl;

        int length;
        memcpy((char*)&length,(char*)newKey, sizeof(int));

        if(shift != 0)
            memmove((char*)page + offset + length + 12, (char*)page + offset,shift);
        memcpy((char*)page + offset,(char*)newKey,12+length);
        space -= (12 + length);
        setSpaceOnPage(page,space);
        setSlotOnPage(page,numSlots + 1);
        return;
    }

}

int IndexManager::splitLeaf(IXFileHandle &ixFileHandle, void *page, void *newPage) {

    // ~ 340 records need to be in curr page.
    // Shifting all records > 170 to new page
    int offset = (170 * 3 * sizeof(int)) + 10;
    int freeSpace = getSpaceOnPage(page);
    int shiftSize = (PAGE_SIZE - 4 - freeSpace) - offset;
    //int newSlot = shiftSize / 12;
    int newSlot = getSlotOnPage(page) - 170;

    int newSpace = PAGE_SIZE - 10 - 4 - shiftSize;
    setSpaceOnPage(newPage,newSpace);
    setSlotOnPage(newPage,newSlot);
    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);

//    std::cout<<std::endl;
//    std::cout<<"Shift size : "<<shiftSize<<std::endl;
//    std::cout<<"Slot on curr page : "<<getSlotOnPage(page)<<std::endl;
//    std::cout<<"Space on curr page : "<<getSpaceOnPage(page)<<std::endl;
//    std::cout<<"Slot on new page : "<<getSlotOnPage(page) - 170<<std::endl;
//    std::cout<<"Space on new page : "<<newSpace<<std::endl;
//    std::cout<<std::endl;

    // Copying sibling pointer from curr page to new page
    int currSiblingPtr;
    memcpy((char*)&currSiblingPtr,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));

    if(currSiblingPtr < 0)
        currSiblingPtr = -1;

    memcpy((char*)newPage + PAGE_SIZE - sizeof(int),(char*)&currSiblingPtr, sizeof(int));
    ixFileHandle.appendPage(newPage);

//    int key;
//    memcpy((char*)&key,(char*)newPage+10, sizeof(int));
//    std::cout<<"Key on new page after split : "<<key<<std::endl;

    int newPageNum = ixFileHandle.getNumberOfPages() - 1;

    //updating curr Page sibling pointer to new page
    memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&newPageNum, sizeof(int));
    freeSpace = PAGE_SIZE - offset - 4;
    setSpaceOnPage(page,freeSpace);
    setSlotOnPage(page,170);

    return newPageNum;
}

int IndexManager::splitLeafVarchar(IXFileHandle &ixFileHandle, void *page, void *newPage) {

    int slots = getSlotOnPage(page);
    int offset = 10;
    int length,ptr;
    int freeSpace = getSpaceOnPage(page);
    for(int i=0;i<slots;i++)
    {
        if(offset >= (PAGE_SIZE/2))
        {
            ptr = i;
            break;
        }
        memcpy((char*)&length,(char*)page + offset, sizeof(int));
        offset += (length + 12);
    }

    int shiftSize = PAGE_SIZE - 4 - freeSpace - offset;

    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);

    setSpaceOnPage(newPage,PAGE_SIZE - 10 - 4 - shiftSize);
    if(ptr != -1)
        setSlotOnPage(newPage,slots - ptr);
    else
        setSlotOnPage(newPage,0);


    int newSpace = PAGE_SIZE - offset - 4;
    setSpaceOnPage(page,newSpace);
    setSlotOnPage(page,ptr);

    // Copying sibling pointer from curr page to new page
    int currSiblingPtr;
    memcpy((char*)&currSiblingPtr,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));

    memcpy((char*)newPage + PAGE_SIZE - sizeof(int),(char*)&currSiblingPtr, sizeof(int));
    ixFileHandle.appendPage(newPage);
    int newPageNum = ixFileHandle.getNumberOfPages() - 1;

    //updating curr Page sibling pointer to new page
    memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&newPageNum, sizeof(int));

    return newPageNum;
}

int findPtrToInsert(const Attribute &attribute, const void *page, const void *newKey) {

    int slotNum = getSlotOnPage(page);
    int offset;
    int ptr = -1, slot = -1;
    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        offset = 10;
        void* temp = malloc(12);
        for(int i = 1;i <= slotNum;i++)
        {
            memcpy((char*)temp,(char*)page + offset + sizeof(int), 12);
            int compare = compareInt(newKey,temp);
            if(compare < 0)
            {
                memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
                slot = i;
                break;
            } else if (compare > 0)
            {
                offset += (12 + sizeof(int));
            }
        }

        free(temp);
        if(slot == -1)
        {
            offset = PAGE_SIZE - getSpaceOnPage(page) - sizeof(int);
            memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
        }

        return ptr;
    }
    else if(attribute.type == TypeVarChar)
    {
        offset = 10;
        for(int i = 1;i <= slotNum;i++)
        {
            int length;
            memcpy((char*)&length,(char*)page + offset+ sizeof(int), sizeof(int));
            void* temp = malloc(length + 12);
            memcpy((char*)temp,(char*)page + offset + sizeof(int), length + 12);
            int compare = compareVarChar(newKey,temp);
            free(temp);

            if(compare == 0)
            {
                // std::cout<<"Find Ptr : compare == 0"<<std::endl;
                offset += (16 + length);
                memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
                slot = (i + 1);
                break;
            }
            else if(compare < 0)
            {
                memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
                slot = i;
                break;
            } else if (compare > 0)
            {
                offset += (16 + length);
            }
        }

        if(slot == -1)
        {
            offset = PAGE_SIZE - getSpaceOnPage(page) - sizeof(int);
            memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
        }
        return ptr;
    }
}

void IndexManager::addToInterPage(void *page, const Attribute &attribute, const void *newChild, int x, int y,
                                  int lenRec) {

    bool debugFlag = false;
    int numSlots = getSlotOnPage(page);

    int tmp;
    memcpy((char*)&tmp,(char*)newChild, sizeof(int));
    if(debugFlag)
    {
        std::cout<<std::endl;
//        std::cout<<tmp<<std::endl;
        std::cout<<x<<" , "<<y<<std::endl;
    }


    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        int offset = 10;

        for(int i=1;i<=numSlots;i++)
        {
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));

            if(temp == x)
            {
                break;
            }
            offset += 16;
        }

        offset += sizeof(int);

        int space = getSpaceOnPage(page);

        int shiftSize = PAGE_SIZE - space - offset;
        memmove((char*)page + offset + lenRec + sizeof(int),(char*)page + offset,shiftSize);
        memcpy((char*)page + offset,(char*)newChild,lenRec);
        memcpy((char*)page + offset + lenRec,(char*)&y, sizeof(int));

        setSpaceOnPage(page,space - (lenRec+ sizeof(int)));
        setSlotOnPage(page,numSlots+1);

        if(debugFlag)
        {
            std::cout<<std::endl;
            std::cout<<"shift : "<<shiftSize<<std::endl;
            std::cout<<getSpaceOnPage(page)<<" , "<<getSlotOnPage(page)<<std::endl;
        }
        return;
    }
    else if (attribute.type == TypeVarChar)
    {
        int offset = 10;
//        std::cout<<"num of slots : "<<numSlots<<std::endl;
        for(int i=1;i<=numSlots;i++)
        {
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));
//            std::cout<<"temp with x :: "<<temp<<" , "<<x<<std::endl;
            if(temp == x)
            {
                break;
            }

            int length;
            memcpy((char*)&length,(char*)page+offset + sizeof(int), sizeof(int));
            offset += (length + 16);
        }

//        int tmp;
//        memcpy((char*)&tmp,(char*)page + offset, sizeof(int));
//        std::cout<<"Tmp value : "<<tmp<<std::endl;

        offset += sizeof(int);

        int space = getSpaceOnPage(page);
        int shiftSize = PAGE_SIZE - space - offset;
        memmove((char*)page + offset + lenRec + sizeof(int),(char*)page + offset,shiftSize);
        memcpy((char*)page + offset,(char*)newChild,lenRec);
        memcpy((char*)page + offset + lenRec,(char*)&y, sizeof(int));
        setSpaceOnPage(page,space - (lenRec+ sizeof(int)));
        setSlotOnPage(page,numSlots+1);

        if(debugFlag)
        {
            std::cout<<std::endl;
            std::cout<<"shift : "<<shiftSize<<std::endl;
            std::cout<<getSpaceOnPage(page)<<" , "<<getSlotOnPage(page)<<std::endl;

            std::cout<<std::endl;
            int offset = 10;
            for(int i=1;i<=numSlots + 1;i++)
            {
                int temp;
                memcpy((char*)&temp,(char*)page + offset, sizeof(int));
                std::cout<<temp<<std::endl;
                int length;
                memcpy((char*)&length,(char*)page+offset + sizeof(int), sizeof(int));
                offset += (length + 16);
            }
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));
            std::cout<<temp<<std::endl;

            std::cout<<std::endl;
        }

        return;
    }
}


int IndexManager::splitInter(IXFileHandle &ixFileHandle, void *page, void *newPage) {

    int newPageNum = -1;
    newInterPage(newPage);

    int offset = 14;int ptr;
    int numSlots = getSlotOnPage(page);
    for(int i=1;i<=numSlots;i++)
    {
        offset += 16;
        if(offset >= (PAGE_SIZE /2 ))
        {
            ptr = i;
            break;
        }
    }

    int space =getSpaceOnPage(page);
    int shiftSize = PAGE_SIZE - space - offset;
    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);

    setSpaceOnPage(newPage,PAGE_SIZE - 10 - shiftSize);
    setSlotOnPage(newPage,numSlots - ptr);


    setSpaceOnPage(page,space + shiftSize);
    setSlotOnPage(page,ptr);

    ixFileHandle.appendPage(newPage);
    newPageNum = ixFileHandle.getNumberOfPages() - 1;
    return newPageNum;
}

int IndexManager::splitInterVarchar(IXFileHandle &ixFileHandle, void *page, void *newPage) {

    int newPageNum = -1;
    newInterPage(newPage);

    int offset = 14;int ptr;
    int numSlots = getSlotOnPage(page);
    for(int i=1;i<=numSlots;i++)
    {
        int length;
        memcpy((char*)&length,(char*)page + offset, sizeof(int));
        offset += length + 16;
        if(offset >= (PAGE_SIZE /2 ))
        {
            ptr = i;
            break;
        }
    }

    int space = getSpaceOnPage(page);
    int shiftSize = PAGE_SIZE - space - offset;
    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);

    setSpaceOnPage(newPage,PAGE_SIZE - 10 - shiftSize);
    setSlotOnPage(newPage,numSlots - ptr);

    setSpaceOnPage(page,space + shiftSize);
    setSlotOnPage(page,ptr);

    ixFileHandle.appendPage(newPage);
    newPageNum = ixFileHandle.getNumberOfPages() - 1;
    return newPageNum;
}

//Returns the key to be pushed up.
//Deletes the key from the page to balance out keys and pointers
int IndexManager::findPushUpKey(void *page, void *newPage, const void *newKey, void *rootKey,const Attribute &attribute,int x,int y) {
    int length = -1;

    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        void* firstRecOnNewPage = malloc(12);
        void* lastRecOnCurrPage = malloc(12);

        int offsetOnCurr = PAGE_SIZE - getSpaceOnPage(page) - 4 - 12;
        memcpy((char*)lastRecOnCurrPage,(char*)page + offsetOnCurr,12);

        memcpy((char*)firstRecOnNewPage,(char*)newPage + 10,12);
        int compare1 = compareInt(newKey,lastRecOnCurrPage);
        int compare2 = compareInt(newKey,firstRecOnNewPage);

        if(compare1 < 0)
        {
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,12);
            addToInterPage(page,attribute,newKey,x,y,12);

            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - 12;
            memmove((char*)newPage + 10,(char*)newPage + 10 + 12,shiftSize);
        }
        else if(compare2 > 0)
        {
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,12);

            int space = getSpaceOnPage(newPage);
            int shiftSize = PAGE_SIZE - space - 10 - 12;

            memmove((char*)newPage + 10,(char*)newPage + 10 + 12,shiftSize);
            setSpaceOnPage(newPage,space + 12);
            setSlotOnPage(newPage,getSlotOnPage(newPage) -1);

            addToInterPage(newPage,attribute,newKey,x,y,12);
        }
        else if(compare1 > 0 && compare2 < 0)
        {
            memcpy((char*)rootKey,(char*)newKey,12);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10;
            memmove((char*)newPage + 14,(char*)newPage + 10,shiftSize);
            memcpy((char*)newPage + 10,(char*)&y, sizeof(int));
        }
        free(firstRecOnNewPage);
        free(lastRecOnCurrPage);
        length = 12;
    }
    else if (attribute.type == TypeVarChar)
    {
        int offset = 10;
        int numSlots = getSlotOnPage(page);
        offset+= sizeof(int);
        for(int i=1;i<numSlots;i++)
        {
            int len;
            memcpy((char*)&len,(char*)page + offset, sizeof(int));
            offset += len + 16;
        }
        int len1;
        memcpy((char*)&len1,(char*)page + offset, sizeof(int));
        void* lastRecOnCurrPage = malloc(len1 + 12);
        memcpy((char*)lastRecOnCurrPage,(char*)page + offset,len1 + 12);

        memcpy((char*)&len1,(char*)newPage + 10, sizeof(int));
        void* firstRecOnNewPage = malloc(len1 + 12);
        memcpy((char*)firstRecOnNewPage,(char*)newPage + 10,len1 + 12);

        int compare1 = compareVarChar(newKey,lastRecOnCurrPage);
        int compare2 = compareVarChar(newKey,firstRecOnNewPage);

        int newKeyLength;
        memcpy((char*)&newKeyLength,(char*)newKey, sizeof(int));
        newKeyLength += 12;

        if(compare1 < 0)
        {
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,len1 + 12);
            addToInterPage(page,attribute,newKey,x,y,len1 + 12);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - len1 - 12;
            memmove((char*)newPage + 10,(char*)newPage + 10 + len1 + 12,shiftSize);
            length = len1 + 12;
        }
        else if(compare2 > 0)
        {
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,len1 + 12);

            int space = getSpaceOnPage(newPage);
            int shiftSize = PAGE_SIZE - space - 10 - len1 - 12;

            memmove((char*)newPage + 10,(char*)newPage + 10 + len1 + 12,shiftSize);
            setSpaceOnPage(newPage,space + len1 + 12);
            setSlotOnPage(newPage,getSlotOnPage(newPage) -1);

            addToInterPage(newPage,attribute,newKey,x,y,newKeyLength);
            length = len1 + 12;
        }
        else if(compare1 > 0 && compare2 < 0)
        {
            memcpy((char*)rootKey,(char*)newKey,newKeyLength);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10;
            memmove((char*)newPage + 14,(char*)newPage + 10,shiftSize);
            memcpy((char*)newPage + 10,(char*)&y, sizeof(int));
            length = newKeyLength;
        }
        free(firstRecOnNewPage);
        free(lastRecOnCurrPage);
    }
    return length;
}


//Actual insert function
void IndexManager::insertIntoPage(IXFileHandle &ixFileHandle, const Attribute &attribute,int currPage,
                                  const void *newKey, void *returnedChild, int &n1, int &n2, int &lengthToRet) {

    //std::cout<<"insert into page : "<<currPage<<std::endl;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readPage(currPage,page);

    int tmp;
    memcpy((char*)&tmp,(char*)newKey, sizeof(int));


    bool debugFlag = false;

    if(isLeaf(page))
    {
        int length = getLengthOfEntry(newKey,attribute);

        if(debugFlag)
        {
            std::cout<<"leaf block"<<std::endl;
            std::cout<<"Length of record : "<<length<<std::endl;
            //std::cout<<"Key : "<<tmp<<std::endl;
            int page1,slot1;
            memcpy((char*)&page1,(char*)newKey + length - 8, sizeof(int));
            memcpy((char*)&slot1,(char*)newKey + length - 4, sizeof(int));
            std::cout<<"RID details : "<<page1<<" :: "<<slot1<<std::endl;
        }

        if(isSpaceAvailable(page,length))
        {
            if(debugFlag)
            {
                std::cout<<"space available in page :: "<<currPage<<std::endl;
                std::cout<<"Space in page : "<<getSpaceOnPage(page)<<std::endl;
                std::cout<<"Slots in page : "<<getSlotOnPage(page)<<std::endl;
            }
            addToPage(page,newKey,attribute);
            ixFileHandle.writePage(currPage,page);
            free(page);
            return;
        }
        else
        {
            if(isRoot(page))
            {
                if(debugFlag)
                {
                    std::cout<<"no space in page :: "<<currPage<<std::endl;
                    std::cout<<"root page"<<std::endl;
                }

                //Unsetting root notation in current page ::  Root -> Inter
                char unset = '0';
                memcpy((char*)page ,(char*)&unset, sizeof(char));
            }

            if(attribute.type == TypeInt || attribute.type == TypeReal)
            {
                void* newPage = malloc(PAGE_SIZE);
                newLeafPage(newPage);

                if(debugFlag)
                {
                    std::cout<<"no space in page :: "<<currPage<<std::endl;
                    int ptr1,ptr2;
                    memcpy((char*)&ptr1,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
                    memcpy((char*)&ptr2,(char*)newPage + PAGE_SIZE - sizeof(int), sizeof(int));
                    std::cout<<"ptr on curr page : "<<ptr1<<std::endl;
                    std::cout<<"ptr on new page : "<<ptr2<<std::endl;
                    std::cout<<"num of slots on new Page: "<<getSlotOnPage(newPage)<<std::endl;
                    std::cout<<"num of slots on curr Page: "<<getSlotOnPage(page)<<std::endl;
                    std::cout<<"space on new Page: "<<getSpaceOnPage(newPage)<<std::endl;
                    std::cout<<"space on curr Page: "<<getSpaceOnPage(page)<<std::endl;

                    std::cout<<std::endl;
                }

                int newPageNum = splitLeaf(ixFileHandle, page,newPage);
                ixFileHandle.writePage(currPage,page);

                if(debugFlag)
                {
                    std::cout<<"after split"<<std::endl;
                    int ptr1,ptr2;
                    memcpy((char*)&ptr1,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
                    memcpy((char*)&ptr2,(char*)newPage + PAGE_SIZE - sizeof(int), sizeof(int));
                    std::cout<<"ptr on curr page : "<<ptr1<<std::endl;
                    std::cout<<"ptr on new page : "<<ptr2<<std::endl;
                    std::cout<<"num of slots on new Page: "<<getSlotOnPage(newPage)<<std::endl;
                    std::cout<<"num of slots on curr Page: "<<getSlotOnPage(page)<<std::endl;
                    std::cout<<"space on new Page: "<<getSpaceOnPage(newPage)<<std::endl;
                    std::cout<<"space on curr Page: "<<getSpaceOnPage(page)<<std::endl;

                    std::cout<<std::endl;
                    void* test = malloc(PAGE_SIZE);
                    ixFileHandle.readPage(newPageNum,test);
                    int offsetTest = 10;


                    for(int i=1;i<=getSlotOnPage(test);i++)
                    {
                        int key;
                        memcpy((char*)&key,(char*)test+offsetTest, sizeof(int));
                        std::cout<<" key : "<<key<<std::endl;
                        offsetTest+=12;
                    }
		    free(test);
                    std::cout<<std::endl;

                }

                n1 = currPage;
                n2 = newPageNum;
                lengthToRet = 12;

                int offset = (170 * 3 * sizeof(int)) + 10;
                // Inserting the new entry into either of these pages
                void* lastRecOnCurrPage = malloc(12);
                memcpy((char*)lastRecOnCurrPage,(char*)page + offset - (3* sizeof(int)),12);

                void* firstRecOnNewPage = malloc(12);
                memcpy((char*)firstRecOnNewPage,(char*)newPage + 10,12);

                if(debugFlag)
                {
                    int tmp1,tmp2;
                    memcpy((char*)&tmp1,(char*)firstRecOnNewPage, sizeof(int));
                    memcpy((char*)&tmp2,(char*)lastRecOnCurrPage, sizeof(int));
                    std::cout<<"first on new page : "<<tmp1<<std::endl;
                    std::cout<<"last on curr page : "<<tmp2<<std::endl;
                }


                int compare1 = compareInt(newKey,lastRecOnCurrPage);
                int compare2 = compareInt(newKey,firstRecOnNewPage);

                if(compare1 < 0)
                {
                    addToPage(page,newKey,attribute);
                    memcpy(returnedChild,firstRecOnNewPage,12);
                    ixFileHandle.writePage(currPage,page);
                }
                else if(compare2 > 0)
                {
                    addToPage(newPage,newKey,attribute);
                    memcpy(returnedChild,firstRecOnNewPage,12);
                    ixFileHandle.writePage(newPageNum,newPage);
                }
                else if(compare1 > 0 && compare2 < 0)
                {
                    int shiftSize = PAGE_SIZE - 4 - getSpaceOnPage(newPage) - 10;
                    //shift all records in new page + 12
                    memmove((char*)newPage + 10 + 12,(char*)newPage + 10 , shiftSize);
                    memcpy((char*)newPage + 10,(char*)newKey,12);
                    memcpy(returnedChild,newKey,12);
                    ixFileHandle.writePage(newPageNum,newPage);
                }
                free(lastRecOnCurrPage);
                free(firstRecOnNewPage);
                free(newPage);
                free(page);
                return;
            }
            else if(attribute.type == TypeVarChar)
            {
                void* newPage = malloc(PAGE_SIZE);
                newLeafPage(newPage);

                if(debugFlag)
                {
                    std::cout<<"no space in page :: "<<currPage<<std::endl;
                    int ptr1,ptr2;
                    memcpy((char*)&ptr1,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
                    memcpy((char*)&ptr2,(char*)newPage + PAGE_SIZE - sizeof(int), sizeof(int));
                    std::cout<<"ptr on curr page : "<<ptr1<<std::endl;
                    std::cout<<"ptr on new page : "<<ptr2<<std::endl;
                    std::cout<<"num of slots on new Page: "<<getSlotOnPage(newPage)<<std::endl;
                    std::cout<<"num of slots on curr Page: "<<getSlotOnPage(page)<<std::endl;
                    std::cout<<"space on new Page: "<<getSpaceOnPage(newPage)<<std::endl;
                    std::cout<<"space on curr Page: "<<getSpaceOnPage(page)<<std::endl;

                    std::cout<<std::endl;
                }

                int newPageNum = splitLeafVarchar(ixFileHandle,page,newPage);
                ixFileHandle.writePage(currPage,page);
                n1 = currPage;
                n2 = newPageNum;

                if(debugFlag)
                {
                    std::cout<<"after split"<<std::endl;
                    int ptr1,ptr2;
                    memcpy((char*)&ptr1,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
                    memcpy((char*)&ptr2,(char*)newPage + PAGE_SIZE - sizeof(int), sizeof(int));
                    std::cout<<"ptr on curr page : "<<ptr1<<std::endl;
                    std::cout<<"ptr on new page : "<<ptr2<<std::endl;
                    std::cout<<"num of slots on new Page: "<<getSlotOnPage(newPage)<<std::endl;
                    std::cout<<"num of slots on curr Page: "<<getSlotOnPage(page)<<std::endl;
                    std::cout<<"space on new Page: "<<getSpaceOnPage(newPage)<<std::endl;
                    std::cout<<"space on curr Page: "<<getSpaceOnPage(page)<<std::endl;

                }

                int offset = 10; int length;

                // To get the start of the last record
                for(int i=1;i<getSlotOnPage(page);i++)
                {
                    memcpy((char*)&length, (char*)page + offset, sizeof(int));
                    offset += (length + 12);
                }

                int compare1 = 0, compare2 = 0;
                memcpy((char*)&length, (char*)page + offset, sizeof(int));
                void* lastRecOnCurrPage = malloc(length + 12);
                memcpy(lastRecOnCurrPage,(char*)page + offset, length + 12);


                memcpy((char*)&length,(char*)newPage+10, sizeof(int));
                void* firstRecOnNewPage = malloc(length + 12);
                memcpy(firstRecOnNewPage,(char*)newPage + 10,length + 12);
                compare2 = compareVarChar(newKey,firstRecOnNewPage);

                compare1 = compareVarChar(newKey,lastRecOnCurrPage);

                if(compare1 < 0)
                {
                    addToPage(page,newKey,attribute);
                    memcpy(returnedChild,firstRecOnNewPage,length + 12);
                    lengthToRet = length + 12;
                    ixFileHandle.writePage(currPage,page);
                }
                else if(compare2 > 0)
                {
                    addToPage(newPage,newKey,attribute);
                    memcpy(returnedChild,firstRecOnNewPage,length + 12);
                    lengthToRet = length + 12;
                    ixFileHandle.writePage(newPageNum,newPage);
                }
                else if(compare1 > 0 && compare2 < 0)
                {
                    int shiftSize = PAGE_SIZE - 4 - getSpaceOnPage(newPage) - 10;
                    int lengthOfRec;
                    memcpy((char*)&lengthOfRec,(char*)newKey, sizeof(int));
                    //shift all records in new page + 12
                    memmove((char*)newPage + 10 + 12 + lengthOfRec,(char*)newPage + 10 , shiftSize);
                    memcpy((char*)newPage + 10,(char*)newKey,lengthOfRec + 12);
                    memcpy(returnedChild,newKey,lengthOfRec + 12);
                    lengthToRet = lengthOfRec + 12;
                    ixFileHandle.writePage(newPageNum,newPage);
                }
//                else if(compare1 > 0 && compare2 == 0)
//                {
//                    int shiftSize = PAGE_SIZE - 4 - getSpaceOnPage(newPage) - 10;
//                    int lengthOfRec;
//                    memcpy((char*)&lengthOfRec,(char*)newKey, sizeof(int));
//                    //shift all records in new page + 12
//                    memmove((char*)newPage + 10 + 12 + lengthOfRec,(char*)newPage + 10 , shiftSize);
//                    memcpy((char*)newPage + 10,(char*)newKey,lengthOfRec + 12);
//                    memcpy(returnedChild,newKey,lengthOfRec + 12);
//                    lengthToRet = lengthOfRec + 12;
//                    ixFileHandle.writePage(newPageNum,newPage);
//                }

//                std::cout<<"Length to ret : "<<lengthToRet<<std::endl;
                free(lastRecOnCurrPage);
                free(firstRecOnNewPage);
                free(newPage);
                free(page);
                return;
            }
        }
    }
    else if(isInter(page))
    {
        int newPageToInsert = findPtrToInsert(attribute,page,newKey);
        void* newChild = malloc(PAGE_SIZE);
        int x=-1 , y=-1, lenRec=0;
        insertIntoPage(ixFileHandle,attribute,newPageToInsert,newKey,newChild,x,y,lenRec);

        if(lenRec == 0)
        {
            free(newChild);
	    free(page);
            return;
        } else
        {
//            std::cout<<"Length to inter node : "<<lenRec<<std::endl;
            if(isSpaceAvailable(page,lenRec))
            {
                addToInterPage(page,attribute,newChild,x,y,lenRec);
                ixFileHandle.writePage(currPage,page);
		free(newChild);
                free(page);
                return;
            } else
            {
                //split into 2 pages
                //check where to add new entry
                // assign values to return call
                void* newPage = malloc(PAGE_SIZE);

                if(attribute.type == TypeInt || attribute.type == TypeReal)
                {
                    int newPageNum = splitInter(ixFileHandle,page,newPage);
                    ixFileHandle.writePage(currPage,page);
                    n1 = currPage; n2 = newPageNum;

                    findPushUpKey(page,newPage,newChild,returnedChild,attribute,x,y);
                    ixFileHandle.writePage(newPageNum,newPage);
                    ixFileHandle.writePage(currPage,page);
                    lengthToRet = 12;
                }
                else if(attribute.type == TypeVarChar)
                {
                    int newPageNum = splitInterVarchar(ixFileHandle,page,newPage);
                    ixFileHandle.writePage(currPage,page);
                    n1 = currPage; n2 = newPageNum;

                    lengthToRet = findPushUpKey(page,newPage,newChild,returnedChild,attribute,x,y);
                    ixFileHandle.writePage(newPageNum,newPage);
                    ixFileHandle.writePage(currPage,page);
                }
                free(newPage);
            }
        }
        free(newChild);
    }
    free(page);
    return;
}

RC IndexManager::getRecordOffsetVarchar(const void *pageData, const RID &rid,std::string key ){
    int startOffset = 10, numOfSlot;
    int headeroffset = 10;
    int length = key.length();
    void *keydata = malloc(12+length);
    void *recData = malloc(12+length);
    memcpy((char*)keydata,(char*)&length, sizeof(int));
    memcpy((char*)keydata+ sizeof(int),(char*)&key, length);
    memcpy((char*)keydata+ sizeof(int)+length,(char*)&rid.pageNum, sizeof(int));
    memcpy((char*)keydata+ 2*sizeof(int)+length,(char*)&rid.slotNum, sizeof(int));
    numOfSlot = getSlotOnPage(pageData);
    int recoff=0;
    for(int i = 0; i < numOfSlot; i++){
        int reclen;
        memcpy(&reclen,(char*)pageData + headeroffset + recoff ,sizeof(int));
        if(reclen == length) {
            memcpy((char *)recData, &reclen, sizeof(int));
            memcpy((char *) recData + sizeof(int), (char *) pageData + headeroffset + recoff + sizeof(int),length);
            memcpy((char *) recData + sizeof(int)+length, (char *) pageData + headeroffset + recoff+length,sizeof(int));
            memcpy((char *) recData + 2*sizeof(int)+length, (char *) pageData + headeroffset + recoff+length+ sizeof(int),sizeof(int));
            if (compareInt(keydata, recData) == 0)
		{
		    free(keydata);
		    free(recData);	
                    return startOffset;
		}
        }
        startOffset += 12+reclen;
    }
    free(keydata);
    free(recData);
    return -1;
}
int isEqualINTReal(const void *entry, const void *recordOnPage) {

    int entryKey, entrySlotNum, entryPageNum;
    int recordKey, recordSlotNum, recordPageNum;
    memcpy(&entryKey,(char *)entry, sizeof(int));
    memcpy(&entryPageNum,(char *)entry + sizeof(int), sizeof(int));
    memcpy(&entrySlotNum,(char *)entry + 2 * sizeof(int), sizeof(int));
    memcpy(&recordKey,(char *)recordOnPage, sizeof(int));
    memcpy(&recordPageNum,(char *)recordOnPage + sizeof(int), sizeof(int));
    memcpy(&recordSlotNum,(char *)recordOnPage + 2 * sizeof(int), sizeof(int));

    if(entryKey == recordKey && entryPageNum == recordPageNum && entrySlotNum == recordSlotNum)
        return 1;
    return -1;
}
RC IndexManager::getRecordOffsetInt(const void *pageData,const RID &rid,int key ){
    int startOffset = 10;
    int headeroffset = 10;
    void *keydata = malloc(12);
    void *recData = malloc(12);
    memcpy((char*)keydata, &key, sizeof(int));
    memcpy((char*)keydata+ sizeof(int), (char*)&rid.pageNum, sizeof(int));
    memcpy((char*)keydata+ 2*sizeof(int), (char*)&rid.slotNum, sizeof(int));
    // std::cout<<"page num is "<<rid.pageNum;
    int numOfSlot = getSlotOnPage(pageData);

    for(int i = 0; i < numOfSlot; i++){
        memcpy((char*)recData,(char*)pageData + startOffset,12);
        if(isEqualINTReal(keydata, recData) == 1) {
            free(keydata);
            free(recData);
            return startOffset;
        }
        startOffset+=12;
    }
    free(keydata);
    free(recData);
    return -1;
}
RC IndexManager::getRecordOffsetReal(const void *pageData, const RID &rid,float key ){
    int startOffset = 10;
    int headeroffset = 10;
    void *keydata = malloc(12);
    void *recData = malloc(12);
    memcpy((char*)keydata, &key, sizeof(int));
    memcpy((char*)keydata+ sizeof(int), (char*)&rid.pageNum, sizeof(int));
    memcpy((char*)keydata+ 2*sizeof(int), (char*)&rid.slotNum, sizeof(int));
    // std::cout<<"page num is "<<rid.pageNum;
    int numOfSlot = getSlotOnPage(pageData);

    for(int i = 0; i < numOfSlot; i++){
        memcpy((char*)recData,(char*)pageData + startOffset,12);
        if(isEqualINTReal(keydata, recData) == 1) {
            free(keydata);
            free(recData);
            return startOffset;
        }
        startOffset+=12;
    }
    free(keydata);
    free(recData);
    return -1;
}
RC IndexManager::getLastRecOffsetVarchar(void *pageData){
    int offset = 10;
    int length = 0;
    int numrec = getSlotOnPage(pageData);
    for(int i = 0; i < numrec; i++){
        memcpy(&length, (char*)pageData+offset, sizeof(int));
        offset += length+12;
    }
    return offset;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {

    int rootpageNum = getRootPage(ixFileHandle);
    if(rootpageNum < 0) {
        return -1;
    }
    return deleteEntryInNode(rootpageNum, ixFileHandle, attribute, key, rid);
}



RC IndexManager :: deleteEntryInNode(unsigned pagenum,IXFileHandle &ixFileHandle,const Attribute &attribute,const void *key,const  RID &rid){
    bool isDeleted = false;
    void *pageData = malloc(PAGE_SIZE);
    ixFileHandle.readPage(pagenum, pageData);
    int numRec = getSlotOnPage(pageData);
    if(isLeaf(pageData)){
        int freeSpace = getSpaceOnPage(pageData);
        int deleteSize = 0;
        //get the startoffset of the entry to be deleted and last recoffset -->>
        int startOffset = 0, lastRecOffset = 0;
        int endOffset;
        switch(attribute.type){
            case TypeReal:
            case TypeInt:
                int keyI;
                lastRecOffset = 10+3* sizeof(int)*numRec;
                memcpy(&keyI, (char*)key, sizeof(int));
                startOffset = getRecordOffsetInt(pageData, rid, keyI);
                if(startOffset == -1)
		{
		    free(pageData);
                    return -1;
		}
                isDeleted = true;
                deleteSize = 3* sizeof(int);
                break;
            case TypeVarChar:
                int length=0;
                std::string keyStr;
                lastRecOffset = getLastRecOffsetVarchar(pageData);
                memcpy(&length, (char*)key, sizeof(int));
                memcpy(&keyStr, (char*)key, length);
                startOffset = getRecordOffsetVarchar(pageData, rid, keyStr);
                if(startOffset == -1)
		{
		    free(pageData);
                    return -1;
		}
                isDeleted = true;
                memcpy(&deleteSize, (char*)pageData + startOffset, sizeof(int));
                deleteSize += 12;
        }
        endOffset = startOffset + deleteSize;
        memmove((char*)pageData + startOffset, (char*)pageData + endOffset, lastRecOffset - endOffset);
        freeSpace += deleteSize;
        setSpaceOnPage(pageData,freeSpace);
        numRec -= 1;
        setSlotOnPage(pageData,numRec);
        ixFileHandle.writePage(pagenum, pageData);
        free(pageData);
        if(isDeleted == true) {
            ixFileHandle.deleteFlag = true;
            return 0;
        }

    }else{
        if(attribute.type == TypeReal || attribute.type == TypeInt) {
            void *newKey = malloc(12);
            memcpy((char*)newKey, (char *) key, sizeof(int));
            memcpy((char*)newKey + sizeof(int), &rid.pageNum, sizeof(int));
            memcpy((char*)newKey + 2* sizeof(int), &rid.slotNum, sizeof(int));
            int nextpagenum = findPtrToInsert(attribute, pageData, newKey);
	    free(newKey);
	    free(pageData);
            deleteEntryInNode(nextpagenum, ixFileHandle, attribute, key, rid);
        }
        else{
            int newKeyLen;
            memcpy((char*)newKeyLen, (char *) key, sizeof(int));
            void *newKey = malloc(newKeyLen + 12);
            memcpy((char*)newKey,  &newKeyLen, sizeof(int));
            memcpy((char*)newKey+ sizeof(int), (char*)key+ sizeof(int), newKeyLen);
            memcpy((char*)newKey+ sizeof(int) + newKeyLen, &rid.pageNum, sizeof(int));
            memcpy((char*)newKey + 2* sizeof(int) + newKeyLen, &rid.slotNum, sizeof(int));
            int nextpagenum = findPtrToInsert(attribute, pageData, newKey);
	    
	    free(newKey);
	    free(pageData);
            deleteEntryInNode(nextpagenum, ixFileHandle, attribute, key, rid);
        }
    }

    return 0;
}

void IndexManager::printCurrentNode(IXFileHandle &ixFileHandle, const Attribute &attribute, int pageNum, int newNode) const
{

    bool DEBUG_VAL = false;
    //read the page and get the number of records on that page
    void *pageData = malloc(PAGE_SIZE);
    ixFileHandle.readPage(pageNum, pageData);
    int headerOffset = 10, numOfKeysCount = 0;
    numOfKeysCount = getSlotOnPage(pageData);
  
    //getting the type of the current node
    bool isleaf, isinter;
    isleaf = isLeaf(pageData);
    isinter = isInter(pageData);
    std::cout<<"{";
    //check if it's the root in which case print the start of the json string
    if(pageNum == getRootPage(ixFileHandle) && !isleaf){
        std::cout << "\n";
    }

    std::cout << "\"keys\": [";

    RID currRID;
    //if leaf node print the data too
    if(isleaf){
        switch(attribute.type){
            case TypeInt:
                if(numOfKeysCount == 0){
                    std::cout<<"\"";
                    break;
                }
                for (int i = 0; i < numOfKeysCount; i++)
                {
                    int key;
                    //header 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + headerOffset + 3*i*sizeof(int), sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset +  sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    memcpy(&currRID.slotNum, (char*)pageData + headerOffset + 2* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    if (!newNode)
                    {
                        newNode = true;
                    }
                    if (newNode && i > 0)
                        std::cout << "]\",";
                    std::cout << "\"" ;

                    std::cout << key <<  ":[(" << currRID.pageNum << ","<< currRID.slotNum << ")]";
                }
                break;
            case TypeReal:
                if(numOfKeysCount == 0){
                    std::cout<<"\"";
                    break;
                }
                for (int i = 0; i < numOfKeysCount; i++)
                {
                    float key;
                    //header 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + headerOffset + 3*i*sizeof(int), sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset +  sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    memcpy(&currRID.slotNum, (char*)pageData + headerOffset + 2* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    if (!newNode)
                    {
                        newNode = true;
                    }
                    if (newNode && i > 0)
                        std::cout << "]\",";
                    std::cout << "\"" ;
                    std::cout << key <<  ":[(" << currRID.pageNum << ","<< currRID.slotNum << ")]";
                }
                break;
            case TypeVarChar:
                if(numOfKeysCount == 0){
                    std::cout<<"\"";
                    break;
                }
                int offset = 0;
                // std::cout<<"num of key count"<<numOfKeysCount<<std::endl;
                for (int i = 0; i < numOfKeysCount; i++) {
                    int keyLength;
                    //get the length of the record
                    memcpy(&keyLength, (char*) pageData + headerOffset + offset, sizeof(int));
                    // std::cout<<"key length"<<keyLength<<std::endl;
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset + offset + sizeof(int) + keyLength, sizeof(int));
                    memcpy(&currRID.slotNum,(char*)pageData + headerOffset + offset + 2*sizeof(int) + keyLength, sizeof(int));

                    char* key = new char[keyLength + 1];
                    memcpy((char*)key, (char*)pageData + headerOffset + offset + sizeof(int),keyLength);
                    key[keyLength] = '\0';

                    offset += sizeof(int) + keyLength + 2* sizeof(int);
                    if (!newNode)
                    {
                        newNode = true;
                    }
                    if (newNode && i > 0)
                        std::cout << "]\",";
                    std::cout << "\"" ;

                    std::cout << key <<  ":[(" << currRID.pageNum << ","<< currRID.slotNum << ")]";
            		    delete[] key;

                }
                break;
        }
        std::cout << "\"]";
    }
        //if non leaf node print the key values
    else if(isinter){
        switch(attribute.type){
            case TypeReal:
            case TypeInt:
                if(numOfKeysCount >= 0)
                {
                    for (int i = 0; i < numOfKeysCount; i++) //to key_m
                    {
                        int key,pagenum,slotnum;
                        //header + P0 + 4*i*sizeof(int)
                        memcpy(&key, (char*)pageData + headerOffset  + sizeof(int) + 4*i*sizeof(int), sizeof(int));

                        memcpy(&pagenum, (char*)pageData + headerOffset  + 2*sizeof(int) + 4*i*sizeof(int), sizeof(int));

                        memcpy(&slotnum, (char*)pageData + headerOffset  + 3*sizeof(int) + 4*i*sizeof(int), sizeof(int));
                        if (i > 0)
                            std::cout << ",";
                        std::cout << "\"" << key << ":[(" << pagenum << ","<< slotnum << ")]\"";
                    }
                    // Print m+1 children
                    std::cout << "],\n\"children\": [\n";
                    int pageID;
                    memcpy(&pageID, (char*)pageData + headerOffset, sizeof(int));

                    if (pageID >= 0)
                        printCurrentNode(ixFileHandle, attribute, pageID, newNode);

                    int ptr1;
                    for (int i = 1; i <= numOfKeysCount; i++){
                        if (i > 0)
                            std::cout << ",\n";

                        memcpy(&ptr1, (char*)pageData + headerOffset  + (4* sizeof(int)*i), sizeof(int));
                        if(DEBUG_VAL) {

                            std::cout << "page id in loop" << ptr1 << std::endl;
                            std::cout << "page number in loop" << pageNum << std::endl;
                        }
                        if (ptr1 >= 0)
                            printCurrentNode(ixFileHandle, attribute, ptr1, newNode);
                        else
                            std::cout << "{\"keys\":[\"\"]}"; // Empty child
                    }
                }
                break;
            case TypeVarChar:
                int offset = 4;
                std::vector<int> keyLengthVec;
                for (int i = 0; i < numOfKeysCount; i++) {
                    int keyLength;
                    //get the length of the record
                    memcpy(&keyLength, (char*) pageData + headerOffset + offset, sizeof(int));

                    //std::cout<<"key length inter"<<keyLength<<std::endl;
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset + offset  + keyLength+sizeof(int) , sizeof(int));

                    memcpy(&currRID.slotNum,(char*)pageData + headerOffset + offset + 2 * sizeof(int)+keyLength, sizeof(int));
                    keyLengthVec.push_back(keyLength+12);

                    char* key = new char[keyLength + 1];
                    memcpy((char*)key, (char*)pageData + headerOffset + offset + sizeof(int),keyLength);
                    key[keyLength] = '\0';

                    offset += keyLength + 3* sizeof(int) + sizeof(int);

                    if (i > 0)
                        std::cout << ",";

                    std::cout << "\"" << key << ":[(" << currRID.pageNum << ","<< currRID.slotNum << ")]\"";
            		    delete[] key;

                } // cout m + 1 children


                std::cout << "],\n\"children\": [\n";

                int pageID;
                memcpy(&pageID, (char*)pageData + headerOffset, sizeof(int));

                if (pageID >= 0)
                    printCurrentNode(ixFileHandle, attribute, pageID, newNode);

                std::cout << ",\n";

                int offsetLoop = 0;
                for(int i = 0; i < numOfKeysCount; i++) // child_1 to child_m
                {
                    int pageNum;
                    offsetLoop+=keyLengthVec[i]+ sizeof(int);
                    if (i > 0)
                        std::cout << ",\n";
                    //header + length of key + keydata
                    memcpy(&pageNum, (char*) pageData + headerOffset + offsetLoop, sizeof(int));
                    //std::cout<<"page number in varchar "<<pageNum<<std::endl;
                    if (pageNum >=0)
                        printCurrentNode(ixFileHandle, attribute, pageNum, newNode);
                    else
                        std::cout << "{\"keys\":[\"\"]}"; // Empty child
                }
                break;
        }
        std::cout << "]";
    }
    free(pageData);
    if (getRootPage(ixFileHandle) == pageNum && !isleaf) {
        std::cout << "\n";
    }
    std::cout << "}";
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const
{
    //check if a B+ tree exists
    bool DEBUG_VAL = false;
    if(DEBUG_VAL) {
        void *page = malloc(PAGE_SIZE);
        ixFileHandle.readPage(2, page);
        int temp, temp2;

        int ptr1,i = 1;
        while(i < 7) {
            memcpy(&ptr1, (char *) page + 10  + (4 * sizeof(int) * i), sizeof(int));
            std::cout<<"here we are:"<<ptr1<<std::endl;
            std::cout<<"calc: "<<14+3*i*4<<std::endl;
            i++;
        }
        memcpy(&temp, (char *) page + 10, sizeof(int));
        memcpy(&temp2, (char *) page + 26, sizeof(int));
        std::cout << "total pages " << ixFileHandle.getNumberOfPages() << std::endl;
        std::cout << "total number of slots " << getSlotOnPage(page) << std::endl;
        std::cout << "p0 is " << temp << std::endl;
        std::cout << "p1  is " << temp2 << std::endl;

        std::cout << "page is leaf " << isLeaf(page) << std::endl;

        std::cout << "page is inter " << isInter(page) << std::endl;

        std::cout << "page is root " << isRoot(page) << std::endl;

    }
    int rootpageNum = getRootPage(ixFileHandle);
    int newNode = 0;
    if(rootpageNum < 0) {
        return;
    }
    // Start with root -> pageNum -> 0
    printCurrentNode(ixFileHandle, attribute, rootpageNum, newNode);
    std::cout<<std::endl;

}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {

    if(ixFileHandle.getFile() == nullptr)
    {
        return -1;
    }

    ix_ScanIterator.ixFileHandle = &ixFileHandle;
    ix_ScanIterator.attribute = &attribute;
    ix_ScanIterator.lowKeyInclusive = lowKeyInclusive;
    ix_ScanIterator.highKeyInclusive = highKeyInclusive;
    ix_ScanIterator.pageNum = -2;
    ix_ScanIterator.page = malloc(PAGE_SIZE);

    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        if(lowKey != NULL)
        {
            ix_ScanIterator.lowKey = malloc(12);
            memcpy((char*)ix_ScanIterator.lowKey,(char*)lowKey, sizeof(int));
            if(lowKeyInclusive)
            {
                int temp = -1;
                memcpy((char*)ix_ScanIterator.lowKey + sizeof(int),(char*)&temp, sizeof(int));
                memcpy((char*)ix_ScanIterator.lowKey + 2*sizeof(int),(char*)&temp, sizeof(int));
            }
        }
	else
	{
		ix_ScanIterator.lowKey = nullptr;
	}
        if(highKey != NULL)
        {
            ix_ScanIterator.highKey = malloc(12);
            memcpy((char*)ix_ScanIterator.highKey,(char*)highKey, sizeof(int));
            if(highKeyInclusive)
            {
                int temp = INT_MAX;
                memcpy((char*)ix_ScanIterator.highKey + sizeof(int),(char*)&temp, sizeof(int));
                memcpy((char*)ix_ScanIterator.highKey + 2*sizeof(int),(char*)&temp, sizeof(int));
            }
        }
	else
	{
		ix_ScanIterator.highKey = nullptr;
	}
    }
    else if(attribute.type == TypeVarChar)
    {
        if(lowKey != NULL)
        {
            // LowKey will be updated. So to handle variable length of records for VARCHAR
            ix_ScanIterator.lowKey = malloc(PAGE_SIZE);
            int length;
            memcpy((char*)&length,(char*)lowKey, sizeof(int));
            memcpy((char*)ix_ScanIterator.lowKey,(char*)lowKey, length + sizeof(int));

            if(lowKeyInclusive)
            {
                int temp = 0;
                memcpy((char*)ix_ScanIterator.lowKey + length + sizeof(int),(char*)&temp, sizeof(int));
                memcpy((char*)ix_ScanIterator.lowKey + length + 2*sizeof(int),(char*)&temp, sizeof(int));
            }
        }
	else
	{
		ix_ScanIterator.lowKey = nullptr;
	}
        if(highKey != NULL)
        {
            int length;
            memcpy((char*)&length,(char*)highKey, sizeof(int));
            ix_ScanIterator.highKey = malloc(length + 12);
            memcpy((char*)ix_ScanIterator.highKey,(char*)highKey, length + sizeof(int));
            if(highKeyInclusive)
            {
                int temp = INT_MAX;
                memcpy((char*)ix_ScanIterator.highKey + length + sizeof(int),(char*)&temp, sizeof(int));
                memcpy((char*)ix_ScanIterator.highKey + length + 2*sizeof(int),(char*)&temp, sizeof(int));
            }
        }
	else
	{
		ix_ScanIterator.highKey = nullptr;
	}
    }
    return 0;
}

IX_ScanIterator::IX_ScanIterator() {
}

IX_ScanIterator::~IX_ScanIterator() {
}

int getFirstPage(const void* temp)
{
    int offset = 10;
    int locn = -1;
    memcpy((char*)&locn,(char*)temp + offset, sizeof(int));
    return locn;
}

int IX_ScanIterator::findFirstLeafPage(void *page) {

    int num = getRootPage(*ixFileHandle);

    if(num == ixFileHandle->getNumberOfPages() -1)
    {
        ixFileHandle->readPage(num,page);
        return num;
    }

    void* temp = malloc(PAGE_SIZE);
    ixFileHandle->readPage(num,temp);
    while(!isLeaf(temp))
    {
        num = getFirstPage(temp);
        ixFileHandle->readPage(num,temp);
    }
    memcpy((char*)page,(char*)temp,PAGE_SIZE);
    free(temp);
    return num;
}

int getNextLeafPage(const void* page)
{
    int offset = PAGE_SIZE - sizeof(int);
    int temp;
    memcpy((char*)&temp,(char*)page + offset, sizeof(int));
    return temp;
}

int findLeafPageTraverse(const void* lowKey,IXFileHandle* ixFileHandle,void* page,const Attribute* attribute,int curr)
{
    if(isLeaf(page))
        return curr;

    int slots = getSlotOnPage(page);

    if(attribute->type==TypeInt || attribute->type == TypeReal)
    {
        int offset = 10;
        offset += sizeof(int);
        void* currKey = malloc(12);
        for(int i=1;i<= slots;i++)
        {
            memcpy((char*)currKey,(char*)page + offset,12);
            int compare = compareInt(lowKey,currKey);
            if(compare < 0)
            {
                free(currKey);
                int ptr;
                memcpy((char*)&ptr,(char*)page + offset - sizeof(int), sizeof(int));
                ixFileHandle->readPage(ptr,page);
                return findLeafPageTraverse(lowKey,ixFileHandle,page,attribute,ptr);
            }
            offset += 16;
        }
        free(currKey);
        int ptr;
        memcpy((char*)&ptr,(char*)page + offset - sizeof(int), sizeof(int));
        ixFileHandle->readPage(ptr,page);
        return findLeafPageTraverse(lowKey,ixFileHandle,page,attribute,ptr);
    }
    else if (attribute->type == TypeVarChar)
    {
        int offset = 10;
        offset += sizeof(int);
        for(int i=1;i<= slots;i++)
        {
            int length;
            memcpy((char*)&length,(char*)page + offset, sizeof(int));
            void* currKey = malloc(length + 12);
            memcpy((char*)currKey,(char*)page + offset,length + 12);
            int compare = compareVarChar(lowKey,currKey);
            if(compare < 0)
            {
                free(currKey);
                int ptr;
                memcpy((char*)&ptr,(char*)page + offset - sizeof(int), sizeof(int));
                ixFileHandle->readPage(ptr,page);
                return findLeafPageTraverse(lowKey,ixFileHandle,page,attribute,ptr);
            }
            free(currKey);
            offset += (length + 16);
        }

        int ptr;
        memcpy((char*)&ptr,(char*)page + offset - sizeof(int), sizeof(int));
        ixFileHandle->readPage(ptr,page);
        return findLeafPageTraverse(lowKey,ixFileHandle,page,attribute,ptr);
    }

    return -5;
}

int findLeafPage(void* lowKey,IXFileHandle* ixFileHandle,const Attribute* attribute)
{
    int root = getRootPage(*ixFileHandle);
    void* page = malloc(PAGE_SIZE);
    ixFileHandle->readPage(root,page);

    int temp = findLeafPageTraverse(lowKey,ixFileHandle,page,attribute,root);
    free(page);
    return temp;
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {	

    if(lowKey == nullptr && pageNum != -2)
    {
        if(ixFileHandle->deleteFlag)
        {
            ixFileHandle->readPage(pageNum,page);
            ixFileHandle->deleteFlag = false;
        }
//        void* page = malloc(PAGE_SIZE);
//        ixFileHandle->readPage(pageNum,page);
        int numSlots = getSlotOnPage(page);

        if(numSlots == 0)
        {
            pageNum = getNextLeafPage(page);
            ixFileHandle->readPage(pageNum,page);
            if(pageNum < 0)
            {
                return IX_EOF;
            } else
            {
                getNextEntry(rid,key);
                return 0;
            }
        }

        if(attribute->type == TypeInt || attribute->type == TypeReal)
        {
            memcpy((char*)key,(char*)page + 10, sizeof(int));
            memcpy((char*)&rid.pageNum,(char*)page + 10 + 4 , sizeof(int));
            memcpy((char*)&rid.slotNum,(char*)page + 10 + 8 , sizeof(int));
            lowKey = malloc(12);
            memcpy((char*)lowKey,(char*)page + 10,12);
        }
        else if(attribute->type == TypeVarChar)
        {
            int length;
            memcpy((char*)&length,(char*)page + 10, sizeof(int));
            memcpy((char*)key,(char*)page + 10, sizeof(int) + length);
            memcpy((char*)&rid.pageNum,(char*)page + 10 + length + 4 , sizeof(int));
            memcpy((char*)&rid.slotNum,(char*)page + 10 + length + 8 , sizeof(int));
            lowKey = malloc(PAGE_SIZE);
            memcpy((char*)lowKey,(char*)page + 10,length + 12);
        }
        return 0;
    }
    else if(lowKey == nullptr && pageNum == -2)
    {
	
//        void* page = malloc(PAGE_SIZE);
        pageNum = findFirstLeafPage(page);
        int numSlots = getSlotOnPage(page);
	
        if(numSlots == 0)
        {
            pageNum = getNextLeafPage(page);
            ixFileHandle->readPage(pageNum,page);
            if(pageNum < 0)
            {
                return IX_EOF;
            } else
            {
                getNextEntry(rid,key);
                return 0;
            }
        }

        if(attribute->type == TypeInt || attribute->type == TypeReal)
        {
            memcpy((char*)key,(char*)page + 10, sizeof(int));
            memcpy((char*)&rid.pageNum,(char*)page + 10 + 4 , sizeof(int));
            memcpy((char*)&rid.slotNum,(char*)page + 10 + 8 , sizeof(int));
            lowKey = malloc(12);
            memcpy((char*)lowKey,(char*)page + 10,12);
        }
        else if(attribute->type == TypeVarChar)
        {
            int length;
            memcpy((char*)&length,(char*)page + 10, sizeof(int));
            memcpy((char*)key,(char*)page + 10, sizeof(int) + length);
            memcpy((char*)&rid.pageNum,(char*)page + 10 + length + 4 , sizeof(int));
            memcpy((char*)&rid.slotNum,(char*)page + 10 + length + 8 , sizeof(int));
            lowKey = malloc(PAGE_SIZE);
            memcpy((char*)lowKey,(char*)page + 10,length + 12);
        }

        return 0;
    }
    else if(lowKey != NULL)
    {
        // Inside a leaf page
        if(pageNum != -2)
        {
            if(ixFileHandle->deleteFlag)
            {
                ixFileHandle->readPage(pageNum,page);
                ixFileHandle->deleteFlag = false;
            }
//            void* page = malloc(PAGE_SIZE);
//            ixFileHandle->readPage(pageNum,page);
            if(attribute->type == TypeInt || attribute->type == TypeReal)
            {
                int numSlots = getSlotOnPage(page);
                int offset = 10;
                void* currKey = malloc(12);
                for(int i=1;i<=numSlots;i++)
                {
                    memcpy((char*)currKey,(char*)page + offset,12);
                    int compare = compareInt(lowKey,currKey);
                    if(compare < 0)
                    {
                        if(highKey != nullptr)
                        {
                            compare = compareInt(currKey,highKey);
                            if(compare < 0) {
                                memcpy((char *) key, (char *) currKey, sizeof(int));
                                memcpy((char *) &rid.pageNum, (char *) currKey + sizeof(int), sizeof(int));
                                memcpy((char *) &rid.slotNum, (char *) currKey + 8, sizeof(int));
                                memcpy((char *)lowKey, (char *) currKey, 12);
                                free(currKey);
                                return 0;
                            }
                        } else
                        {
                            memcpy((char *) key, (char *) currKey, sizeof(int));
                            memcpy((char *) &rid.pageNum, (char *) currKey + sizeof(int), sizeof(int));
                            memcpy((char *) &rid.slotNum, (char *) currKey + 8, sizeof(int));
                            memcpy((char *) lowKey, (char *) currKey, 12);
                            free(currKey);
                            return 0;
                        }

                    }
                    offset+=12;
                }
		free(currKey);

                if(isRoot(page))
                {
                    return IX_EOF;
                }

                pageNum = getNextLeafPage(page);
                ixFileHandle->readPage(pageNum,page);
                if(pageNum < 0)
                {
                    return IX_EOF;
                } else
                {
                    return getNextEntry(rid,key);
                }
            }
            else if(attribute->type == TypeVarChar)
            {
                int numSlots = getSlotOnPage(page);
                int offset = 10;
                for(int i=1;i<=numSlots;i++)
                {
                    int length;
                    memcpy((char*)&length,(char*)page + offset, sizeof(int));
                    void* currKey = malloc(length + 12);
                    memcpy((char*)currKey,(char*)page + offset,length + 12);
                    int compare = compareVarChar(lowKey,currKey);

                    if(compare < 0)
                    {
                        if(highKey != nullptr)
                        {
                            compare = compareVarChar(currKey,highKey);
                            if(compare < 0) {
                                memcpy((char *) key, (char *) currKey, length + sizeof(int));
                                memcpy((char *) &rid.pageNum, (char *) currKey + length + sizeof(int), sizeof(int));
                                memcpy((char *) &rid.slotNum, (char *) currKey + length + 8, sizeof(int));
                                memcpy((char *) lowKey, (char *) currKey, length + 12);
                                free(currKey);
                                return 0;
                            }
                        } else
                        {
                            memcpy((char *) key, (char *) currKey, length + sizeof(int));
                            memcpy((char *) &rid.pageNum, (char *) currKey + length + sizeof(int), sizeof(int));
                            memcpy((char *) &rid.slotNum, (char *) currKey + length + 8, sizeof(int));
                            memcpy((char *) lowKey, (char *) currKey, length + 12);
                            free(currKey);
                            return 0;
                        }

                    }
                    offset+=(length +12);
		    free(currKey);

                }
		
                if(isRoot(page))
                {
                    return IX_EOF;
                }
                pageNum = getNextLeafPage(page);
                ixFileHandle->readPage(pageNum,page);

                if(pageNum < 0)
                {
                    return IX_EOF;
                } else
                {
                    return getNextEntry(rid,key);;
                }
            }
        }
        else // Find PageNum based on LowKey by traversing the B Tree
        {
            pageNum = findLeafPage(lowKey,ixFileHandle,attribute);
            ixFileHandle->readPage(pageNum,page);
            return getNextEntry(rid,key);
        }
    }
}

RC IX_ScanIterator::close() {

    highKey = NULL;
    lowKey = NULL;
    free(page);
    return 0;
}

IXFileHandle::IXFileHandle() {
  file = nullptr;
}

IXFileHandle::~IXFileHandle() {
}

void IXFileHandle::setFile(std::string& fileName) {
    file = new std::fstream(fileName);
}

void IXFileHandle::closeFile()
{
    file->close();
    delete(file);
    file = nullptr;
}

std::fstream* IXFileHandle::getFile() {
    return file;
}

bool IXFileHandle::check_file_stream() {
    if(!file->good())
        return false;

    return true;
}

int IXFileHandle::getNumberOfPages() {
    int pages;
    file->seekg(sizeof(int),std::ios_base::beg);
    file->read((char*)&pages, sizeof(int));
    file->seekg(0,std::ios_base::beg);
    return pages;
}

RC IXFileHandle::readPage(int pageNum, void *data) {

    if(!check_file_stream())
    {
        return -1;
    }

    if(pageNum >= IXFileHandle::getNumberOfPages())
    {
        return -1;
    }

    int offset = (pageNum + 1) * PAGE_SIZE;
    file->seekg(offset,std::ios_base::beg);
    file->read((char*)data, PAGE_SIZE);
    file->seekg(0,std::ios_base::beg);

    if(pageNum != -1)
    {
        int readCounter;
        file->seekg(sizeof(int) + sizeof(int),std::ios_base::beg);
        file->read((char*)&readCounter, sizeof(int));
        readCounter++;
        file->seekp(sizeof(int) + sizeof(int),std::ios_base::beg);
        file->write((char*)&readCounter, sizeof(int));
        file->seekp(0,std::ios_base::beg);
    }

    return 0;
}

RC IXFileHandle::writePage(int pageNum, const void *data) {
    if (!check_file_stream())
        return -1;

    if (pageNum >= IXFileHandle::getNumberOfPages())
        return -1;

    int offset = (pageNum + 1) * PAGE_SIZE;
    file->seekp(offset, std::ios_base::beg);
    file->write((char *)data, PAGE_SIZE);
    file->seekp(0, std::ios_base::beg);

    int writeCounter;
    file->seekg(sizeof(int) + 2 * sizeof(int),std::ios_base::beg);
    file->read((char*)&writeCounter, sizeof(int));
    writeCounter++;
    file->seekp(sizeof(int) + 2 * sizeof(int),std::ios_base::beg);
    file->write((char*)&writeCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);
    return 0;
}

RC IXFileHandle::appendPage(const void *data) {

    if(!check_file_stream())
        return -1;

    file->seekp(0,std::ios_base::end);
    file->write((char*)data, PAGE_SIZE);
    file->seekp(0,std::ios_base::beg);

    //Update total pages as well
    int totalPages = 0;
    int appendCounter;

    file->seekg(sizeof(int),std::ios_base::beg);
    file->read((char*)&totalPages, sizeof(int));
    file->seekg(sizeof(int) + 3 * sizeof(int),std::ios_base::beg);
    file->read((char*)&appendCounter, sizeof(int));
    totalPages++;
    appendCounter++;
    file->seekp(sizeof(int),std::ios_base::beg);
    file->write((char*)&totalPages, sizeof(int));
    file->seekp(sizeof(int) + 3 * sizeof(int),std::ios_base::beg);
    file->write((char*)&appendCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);

    return 0;
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
