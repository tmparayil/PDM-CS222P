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



int IndexManager::getRootPage(IXFileHandle &ixFileHandle) const {
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
void IndexManager::setRootPage(IXFileHandle &ixFileHandle,void* entry,int length) {

    void* page = malloc(PAGE_SIZE);
    char* inter = new char[2];
    inter[0] = 'I';
    inter[1] = 'L';
    memcpy((char*)page,inter, (2* sizeof(char)));
    delete[] inter;

    int slots = 0;
    int freeSpace = PAGE_SIZE - 10;

    memcpy((char*)page + (2* sizeof(char)),(char*)&freeSpace, sizeof(int));
    memcpy((char*)page + (2* sizeof(char)) + sizeof(int),(char*)&slots, sizeof(int));

    if(entry != NULL)
    {
        std::cout<<"entry not null"<<std::endl;
        memcpy((char*)page + 10,(char*)entry,length);
    }

    ixFileHandle.appendPage(page);

    int currPageNum = ixFileHandle.getNumberOfPages();
    setRootInHidden(ixFileHandle,currPageNum - 1);
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
    int freeSpace = PAGE_SIZE - 10;
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

bool IndexManager::isRoot(const void *page) const{

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[0] == 'I' && checkArr[1] == 'L') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool IndexManager::isLeaf(const void *page) const{

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[1] == 'L'&& checkArr[0] == '0') {
        delete[] checkArr;
        return true;
    }
    delete[] checkArr;
    return false;
}

bool IndexManager::isInter(const void *page) const {

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[0] == 'I' && checkArr[1] == '0') {
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
        memcpy((char*)&length,(char*)key + sizeof(int), sizeof(int));
        return 12 + length;
    }
    else if(attribute.type == TypeReal || attribute.type == TypeInt)
        return 12;
}

int IndexManager::getSpaceOnPage(const void *page) {
    int space;
    memcpy((char*)&space,(char*)page + (2* sizeof(char)), sizeof(int));
    return space;
}

int IndexManager::getSlotOnPage(const void *page) const{
    int slot;
    memcpy((char*)&slot,(char*)page + (2* sizeof(char)) + sizeof(int), sizeof(int));
    return slot;
}


void IndexManager::setSpaceOnPage(const void *page,int space) {
    memcpy((char*)page + (2* sizeof(char)),(char*)&space, sizeof(int));
    return;
}

void IndexManager::setSlotOnPage( const void *page,int slot) {
    memcpy((char*)page + (2* sizeof(char))+ sizeof(int),(char*)&slot, sizeof(int));
    return;
}

bool IndexManager::isSpaceAvailable(const void *page, int length) {
    int space = getSpaceOnPage(page);
    if(space >= length)
        return true;

    return false;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {

    int totalPages = ixFileHandle.getNumberOfPages();

    //std::cout<<"insertEntry()"<<totalPages<<std::endl;

    if(totalPages == 0)
    {
        setRootPage(ixFileHandle,NULL,0);
    }

    void* page = malloc(PAGE_SIZE);
    int root = getRootPage(ixFileHandle);

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
        int length;
        memcpy((char*)&length,(char*)key, sizeof(int));
        void* newKey = malloc(12 + length); // sizeof(int) + length of string + RID size (2 * sizeof(int))
        memcpy((char*)newKey,key, sizeof(int)+length);
        memcpy((char*)newKey + sizeof(int)+length,(char*)rid.pageNum, sizeof(int));
        memcpy((char*)newKey + length + (2* sizeof(int)),(char*)rid.slotNum, sizeof(int));

        insertIntoPage(ixFileHandle,attribute,root,newKey,returnedChild,n1,n2,length);
        free(newKey);
    }

    // If returnedChild is not Null , insert node into root page.
    /* if(length != -1)
     {
         if(getSpaceOnPage(page) > length)
         {
             addToInterPage(page,attribute,returnedChild,n1,n2,length);
         } else
         {
             void* newPage = malloc(PAGE_SIZE);

             if(attribute.type == TypeInt || attribute.type == TypeReal)
             {
                 int newPageNum = splitInter(ixFileHandle,page,newPage);

                 n1 = root; n2 = newPageNum;

                 int offset = PAGE_SIZE - getSpaceOnPage(page) - sizeof(int) - 12;
                 void* lastRecOnCurrPage = malloc(12);
                 memcpy((char*)lastRecOnCurrPage,(char*)page + offset,12);

                 void* firstRecOnNewPage = malloc(12);
                 memcpy((char*)firstRecOnNewPage,(char*)newPage + 10 + sizeof(int),12);

                 int compare1 = compareInt(returnedChild,lastRecOnCurrPage);
                 int compare2 = compareInt(returnedChild,firstRecOnNewPage);

                 int startPtr,endPtr;
                 memcpy((char*)&startPtr,(char*)newPage + 10, sizeof(int));
                 memcpy((char*)&endPtr,(char*)newPage + 10 + 12 + 4, sizeof(int));

                 if(compare1 < 0)
                 {
                     addToInterPage(page,attribute,returnedChild,n1,n2,length);

                     void* newChild = malloc(20);
                     memcpy((char*)newChild,(char*)&startPtr, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)firstRecOnNewPage,12);
                     memcpy((char*)newChild + sizeof(int) + 12,(char*)&endPtr, sizeof(int));

                     //Expects the entire pointer+key combination to be passed
                     setRootPage(ixFileHandle,newChild, 20);
                 }
                 else if(compare2 > 0)
                 {
                     addToInterPage(newPage,attribute,returnedChild,n1,n2,12);

                     void* newChild = malloc( 20);
                     memcpy((char*)newChild,(char*)&startPtr, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)firstRecOnNewPage,12);
                     memcpy((char*)newChild + sizeof(int) + 12,(char*)&endPtr, sizeof(int));
                     setRootPage(ixFileHandle,newChild, 20 );
                 }
                 else if(compare1 > 0 && compare2 < 0)
                 {
                     //This means first pointer in new page is n1.

                     int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - sizeof(int);
                     //shift all records in new page
                     memmove((char*)newPage + 10 + sizeof(int) + 12 + sizeof(int),(char*)newPage + 10 + sizeof(int) , shiftSize);
                     memcpy((char*)newPage + 10 + sizeof(int),(char*)returnedChild,12);
                     memcpy((char*)newPage + 10 + sizeof(int) + 12,(char*)&n2, sizeof(int));
                     ixFileHandle.writePage(newPageNum,newPage);

                     void* newChild = malloc( 20);
                     memcpy((char*)newChild,(char*)&n1, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)returnedChild,12);
                     memcpy((char*)newChild + sizeof(int) + 12,(char*)&n2, sizeof(int));
                     setRootPage(ixFileHandle,newChild, 20 );
                 }

             } else if(attribute.type == TypeVarChar)
             {
                 int newPageNum = splitInterVarchar(ixFileHandle,page,newPage);

                 n1 = root; n2 = newPageNum;

                 int offset = 10;
                 int numSlots = getSlotOnPage(page);
                 offset += sizeof(int);
                 for(int i=1;i<numSlots;i++)
                 {
                     int temp;
                     memcpy((char*)&temp,(char*)page + offset, sizeof(int));
                     offset += (temp + 12);
                 }

                 int length1;
                 memcpy((char*)&length1,(char*)page + offset, sizeof(int));
                 void* lastRecOnCurrPage = malloc(length1 + 12);
                 memcpy((char*)lastRecOnCurrPage,(char*)page + offset,length1 + 12);


                 memcpy((char*)&length1,(char*)newPage + 10 + sizeof(int), sizeof(int));
                 void* firstRecOnNewPage = malloc(length1 + 12);
                 memcpy((char*)firstRecOnNewPage,(char*)newPage + 10 + sizeof(int),length1 + 12);

                 int compare1 = compareInt(returnedChild,lastRecOnCurrPage);
                 int compare2 = compareInt(returnedChild,firstRecOnNewPage);

                 int startPtr,endPtr;
                 memcpy((char*)&startPtr,(char*)newPage + 10, sizeof(int));
                 memcpy((char*)&endPtr,(char*)newPage + 10 + sizeof(int) + length + 12, sizeof(int));

                 if(compare1 < 0)
                 {
                     addToInterPage(page,attribute,returnedChild,n1,n2,length);

                     void* newChild = malloc(length1 + 12 + 8);
                     memcpy((char*)newChild,(char*)&startPtr, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)firstRecOnNewPage,length1 + 12);
                     memcpy((char*)newChild + sizeof(int) + length1 + 12,(char*)&endPtr, sizeof(int));

                     //Expects the entire pointer+key combination to be passed
                     setRootPage(ixFileHandle,newChild, 20);
                 }
                 else if(compare2 > 0)
                 {
                     addToInterPage(newPage,attribute,returnedChild,n1,n2,12);

                     void* newChild = malloc(length1 + 12 + 8);
                     memcpy((char*)newChild,(char*)&startPtr, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)firstRecOnNewPage,length1 + 12);
                     memcpy((char*)newChild + sizeof(int) + length1 + 12,(char*)&endPtr, sizeof(int));

                     //Expects the entire pointer+key combination to be passed
                     setRootPage(ixFileHandle,newChild, 20);
                 }
                 else if(compare1 > 0 && compare2 < 0)
                 {
                     //This means first pointer in new page is n1.

                     int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - sizeof(int);
                     //shift all records in new page
                     memmove((char*)newPage + 10 + sizeof(int) + length + 12 + sizeof(int),(char*)newPage + 10 + sizeof(int) , shiftSize);
                     memcpy((char*)newPage + 10 + sizeof(int),(char*)returnedChild,length + 12);
                     memcpy((char*)newPage + 10 + sizeof(int) + length + 12,(char*)&n2, sizeof(int));
                     ixFileHandle.writePage(newPageNum,newPage);

                     void* newChild = malloc( length + 20);
                     memcpy((char*)newChild,(char*)&n1, sizeof(int));
                     memcpy((char*)newChild + sizeof(int),(char*)returnedChild,length + 12);
                     memcpy((char*)newChild + sizeof(int) + length + 12,(char*)&n2, sizeof(int));
                     setRootPage(ixFileHandle,newChild, length + 20 );
                 }
             }

             //Unsetting root notation in current page ::  Root -> Inter
             char unset = '0';
             memcpy((char*)page + sizeof(char),(char*)unset, sizeof(char));
             ixFileHandle.writePage(root,page);
         }
     }  */
    free(returnedChild);
    free(page);
    return 0;
}

RC IndexManager::compareInt(const void *entry, const void *recordOnPage) {

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

int IndexManager::compareReal(const void *entry,const void *recordOnPage){
    float entryKey, recordKey;
    int  entrySlotNum, entryPageNum;
    int  recordSlotNum, recordPageNum;
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
RC IndexManager::compareVarChar(const void *entry, const void *recordOnPage){
    std::string entryKey, recordKey;
    int length;
    int  entrySlotNum, entryPageNum;
    int  recordSlotNum, recordPageNum;

    memcpy(&length,(char *)entry, sizeof(int));
    memcpy(&entryKey,(char *)entry+sizeof(int), length);
    memcpy(&entryPageNum,(char *)entry + sizeof(int) + length, sizeof(int));
    memcpy(&entrySlotNum,(char *)entry + 2 * sizeof(int) + length, sizeof(int));


    memcpy(&length,(char *)recordOnPage, sizeof(int));
    memcpy(&recordKey,(char *)recordOnPage + sizeof(int), length);
    memcpy(&recordPageNum,(char *)recordOnPage + sizeof(int) + length, sizeof(int));
    memcpy(&recordSlotNum,(char *)recordOnPage + 2 * sizeof(int) + length, sizeof(int));
    if(entryKey < recordKey)
        return -1;
    else if(entryKey == recordKey && entryPageNum < recordPageNum)
        return -1;
    else if(entryKey == recordKey && entryPageNum == recordPageNum && entrySlotNum < recordSlotNum)
        return -1;
    return 1;

}




void IndexManager::addToPage(void *page, const void *newKey,const Attribute &attribute) {

    //search for location
    int numSlots = getSlotOnPage(page);
    // start of record offset
    int offset = 10;

    if(attribute.type == TypeInt)
    {
        for(int i=0;i<numSlots;i++)
        {
            void* temp = malloc(12);
            memcpy((char*)temp,(char*)page + offset,12);
            int compare = compareInt(newKey,temp);
            free(temp);

            if(compare > 0)
            {
                offset += (2* sizeof(int));
                continue;
            }
            // if newKey is less than the record on page. Insert newEntry at current offset.
            if(compare < 0)
            {
                break;
            }
        }

        int space = getSpaceOnPage(page);
        int recPtrEnd = PAGE_SIZE - space;
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
    else if(attribute.type == TypeReal)
    {
        for(int i=0;i<numSlots;i++)
        {
            void* temp = malloc(12);
            memcpy((char*)temp,(char*)page + offset,12);
            int compare = compareReal(newKey,temp);
            free(temp);

            if(compare > 0)
            {
                offset += (2* sizeof(int));
                continue;
            }
            // if newKey is less than the record on page. Insert newEntry at current offset.
            if(compare < 0)
            {
                break;
            }
        }

        int space = getSpaceOnPage(page);
        int recPtrEnd = PAGE_SIZE - space;
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
        for(int i=0;i<numSlots;i++)
        {
            int length;
            memcpy((char*)&length,(char*)page + offset, sizeof(int));
            void* temp = malloc(12+length);
            memcpy((char*)temp,(char*)page + offset,12+length);
            int compare = compareVarChar(newKey,temp);
            free(temp);

            if(compare > 0)
            {
                offset += (2* sizeof(int));
                continue;
            }
            // if newKey is less than the record on page. Insert newEntry at current offset.
            if(compare < 0)
            {
                break;
            }
        }

        int space = getSpaceOnPage(page);
        int recPtrEnd = PAGE_SIZE - space;
        int shift = recPtrEnd - offset;
        //offset is at the point of insert.

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
    int shiftSize = (PAGE_SIZE - freeSpace) - offset;
    int newSlot = shiftSize / 12;
    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);
    int newSpace = PAGE_SIZE - 10 - 4 - shiftSize;
    setSpaceOnPage(newPage,newSpace);
    setSlotOnPage(newPage,newSlot);

    // Copying sibling pointer from curr page to new page
    int currSiblingPtr;
    memcpy((char*)&currSiblingPtr,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
    memcpy((char*)newPage + PAGE_SIZE - sizeof(int),(char*)&currSiblingPtr, sizeof(int));
    ixFileHandle.appendPage(newPage);
    int newPageNum = ixFileHandle.getNumberOfPages();

    //updating curr Page sibling pointer to new page
    memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&newPageNum, sizeof(int));
    freeSpace = PAGE_SIZE - offset;
    setSpaceOnPage(page,freeSpace);
    setSlotOnPage(page,170);

    return newPageNum;
}

int IndexManager::splitLeafVarchar(IXFileHandle &ixFileHandle, void *page, void *newPage) {

    int slots = getSlotOnPage(page);
    int offset = 10;
    int length,ptr;
    for(int i=1;i<=slots;i++)
    {
        if(offset > (PAGE_SIZE/2))
        {
            ptr = i;
            break;
        }
        memcpy((char*)&length,(char*)page + offset, sizeof(int));
        offset += (length + 12);
    }
    int shiftSize = PAGE_SIZE - getSpaceOnPage(page) - offset;
    memcpy((char*)newPage + 10,(char*)page + offset,shiftSize);
    setSpaceOnPage(newPage,PAGE_SIZE - 10 - 4 - shiftSize);
    setSlotOnPage(newPage,slots - ptr);

    setSpaceOnPage(page,getSpaceOnPage(page) + shiftSize);
    setSlotOnPage(page,ptr);

    // Copying sibling pointer from curr page to new page
    int currSiblingPtr;
    memcpy((char*)&currSiblingPtr,(char*)page + PAGE_SIZE - sizeof(int), sizeof(int));
    memcpy((char*)newPage + PAGE_SIZE - sizeof(int),(char*)&currSiblingPtr, sizeof(int));
    ixFileHandle.appendPage(newPage);
    int newPageNum = ixFileHandle.getNumberOfPages();

    //updating curr Page sibling pointer to new page
    memcpy((char*)page + PAGE_SIZE - sizeof(int),(char*)&newPageNum, sizeof(int));

    return newPageNum;
}

int IndexManager::findPtrToInsert(const Attribute &attribute, const void *page, const void *newKey) {

    int slotNum = getSlotOnPage(page);
    int offset;
    int ptr = -1, slot = -1;
    if(attribute.type == TypeInt)
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
    else if (attribute.type == TypeReal)
    {
        offset = 10;
        void* temp = malloc(12);
        for(int i = 1;i <= slotNum;i++)
        {
            memcpy((char*)temp,(char*)page + offset + sizeof(int), 12);
            int compare = compareReal(newKey,temp);
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
            if(compare < 0)
            {
                memcpy((char*)&ptr,(char*)page + offset, sizeof(int));
                slot = i;
                break;
            } else if (compare > 0)
            {
                offset += (12 + length);
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

    int numSlots = getSlotOnPage(page);

    if(attribute.type == TypeInt)
    {
        int offset = 10;
        for(int i=0;i<numSlots;i++)
        {
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));
            if(temp == x)
            {
                break;
            }
            offset += 12;
        }
        offset += sizeof(int);

        int shiftSize = PAGE_SIZE - getSpaceOnPage(page) - offset;
        memmove((char*)page + offset + lenRec + sizeof(int),(char*)page + offset,shiftSize);
        memcpy((char*)page + offset,(char*)newChild,lenRec);
        memcpy((char*)page + offset + lenRec,(char*)&y, sizeof(int));
        return;
    } else if (attribute.type == TypeReal)
    {
        int offset = 10;
        for(int i=0;i<numSlots;i++)
        {
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));
            if(temp == x)
            {
                break;
            }
            offset += 12;
        }
        offset += sizeof(int);

        int shiftSize = PAGE_SIZE - getSpaceOnPage(page) - offset;
        memmove((char*)page + offset + lenRec + sizeof(int),(char*)page + offset,shiftSize);
        memcpy((char*)page + offset,(char*)newChild,lenRec);
        memcpy((char*)page + offset + lenRec,(char*)&y, sizeof(int));
        return;
    }
    else if (attribute.type == TypeVarChar)
    {
        int offset = 10;
        for(int i=0;i<numSlots;i++)
        {
            int temp;
            memcpy((char*)&temp,(char*)page + offset, sizeof(int));
            if(temp == x)
            {
                break;
            }

            int length;
            memcpy((char*)&length,(char*)page+offset + sizeof(int), sizeof(int));
            offset += (length + 12);
        }

        int shiftSize = PAGE_SIZE - getSpaceOnPage(page) - offset;
        memmove((char*)page + offset + lenRec + sizeof(int),(char*)page + offset,shiftSize);
        memcpy((char*)page + offset,(char*)newChild,lenRec);
        memcpy((char*)page + offset + lenRec,(char*)&y, sizeof(int));
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
            addToInterPage(page,attribute,newKey,x,y,12);
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,12);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - 12;
            memmove((char*)newPage + 10,(char*)newPage + 10 + 12,shiftSize);
        }
        else if(compare2 > 0)
        {
            addToInterPage(newPage,attribute,newKey,x,y,12);
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,12);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - 12;
            memmove((char*)newPage + 10,(char*)newPage + 10 + 12,shiftSize);
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

        memcpy((char*)&len1,(char*)newPage + 10, sizeof(int));
        void* firstRecOnNewPage = malloc(len1 + 12);


        int compare1 = compareVarChar(newKey,lastRecOnCurrPage);
        int compare2 = compareVarChar(newKey,firstRecOnNewPage);

        int newKeyLength;
        memcpy((char*)&newKeyLength,(char*)newKey, sizeof(int));
        newKeyLength += 12;

        if(compare1 < 0)
        {
            addToInterPage(page,attribute,newKey,x,y,newKeyLength);
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,len1);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - len1;
            memmove((char*)newPage + 10,(char*)newPage + 10 + len1,shiftSize);
            length = len1;
        }
        else if(compare2 > 0)
        {
            addToInterPage(newPage,attribute,newKey,x,y,newKeyLength);
            memcpy((char*)rootKey,(char*)firstRecOnNewPage,len1);
            int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - len1;
            memmove((char*)newPage + 10,(char*)newPage + 10 + len1,shiftSize);
            length = len1;
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

    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readPage(currPage,page);

    if(isRoot(page))
    {
        int numSlots = getSlotOnPage(page);
        if(numSlots == 0)
        {
            if(attribute.type == TypeReal || attribute.type == TypeInt)
            {
                int ptr = -1;
                memcpy((char*)page + 10,(char*)&ptr, sizeof(int));
                memcpy((char*)page + 10 + sizeof(int),(char*)newKey, 12);
                memcpy((char*)page + 10 + sizeof(int) + 12,(char*)&ptr, sizeof(int));
                int space = getSpaceOnPage(page);
                setSpaceOnPage(page,space - 12);
                setSlotOnPage(page,1);
                ixFileHandle.writePage(currPage,page);
            }else if(attribute.type == TypeVarChar)
            {
                int ptr = -1;
                memcpy((char*)page + 10,(char*)&ptr, sizeof(int));
                int length;
                memcpy((char*)&length,(char*)newKey, sizeof(int));
                memcpy((char*)page + 10 + sizeof(int),(char*)newKey, length+ 12);
                memcpy((char*)page + 10 + sizeof(int) + length + 12,(char*)&ptr, sizeof(int));
                int space = getSpaceOnPage(page);
                setSpaceOnPage(page,space - length - 12);
                setSlotOnPage(page,1);
                ixFileHandle.writePage(currPage,page);
            }
            free(page);
            return;
        }
        else if(numSlots == 1)
        {
            void* newLeafPage1 = malloc(PAGE_SIZE);
            void* newLeafPage2 = malloc(PAGE_SIZE);
            newLeafPage(newLeafPage1);
            newLeafPage(newLeafPage2);

            ixFileHandle.appendPage(newLeafPage1);
            ixFileHandle.appendPage(newLeafPage2);

            int pageNum = ixFileHandle.getNumberOfPages();
            int pageNum1= pageNum - 2;
            memcpy((char*)page+10,(char*)&pageNum1, sizeof(int));
            int pageNum2 = pageNum - 1;

            if(attribute.type == TypeInt || attribute.type == TypeReal)
            {
                memcpy((char*)page + 10 + 4 + 12,(char*)&pageNum2, sizeof(int));

                void* currKey = malloc(12);
                memcpy((char*)currKey,(char*)page + 10 + 4,12);
                addToPage(newLeafPage2,currKey,attribute);

                int compare = compareInt(newKey,currKey);
                if(compare < 0)
                {
                    addToPage(newLeafPage1,newKey,attribute);
                    ixFileHandle.writePage(pageNum1,newLeafPage1);
                } else
                {
                    addToPage(newLeafPage2,newKey,attribute);
                }
                ixFileHandle.writePage(pageNum2,newLeafPage2);
            }
            else if(attribute.type == TypeVarChar)
            {
                int length;
                memcpy((char*)length,(char*)page + 10 + sizeof(int), sizeof(int));
                memcpy((char*)page + 10 + 4 + length + 12,(char*)&pageNum2, sizeof(int));

                void* currKey = malloc(length + 12);
                memcpy((char*)page + 10 + 4,(char*)currKey,length + 12);
                addToPage(newLeafPage2,currKey,attribute);

                int compare = compareVarChar(newKey,currKey);
                if(compare < 0)
                {
                    addToPage(newLeafPage1,newKey,attribute);
                    ixFileHandle.writePage(pageNum1,newLeafPage1);
                } else
                {
                    addToPage(newLeafPage2,newKey,attribute);
                }
                ixFileHandle.writePage(pageNum2,newLeafPage2);
            }
            free(newLeafPage1);
            free(newLeafPage2);
            free(page);
            return;
        }
        else
        {
            int newPageToInsert = findPtrToInsert(attribute,page,newKey);
            void* newChild = malloc(PAGE_SIZE);
            int x , y , lenRec;
            insertIntoPage(ixFileHandle,attribute,newPageToInsert,newKey,newChild,x,y,lenRec);

            if(lenRec != -1) {
                if (getSpaceOnPage(page) > lenRec) {
                    addToInterPage(page, attribute, returnedChild, n1, n2, lenRec);
                } else {
                    void *newPage = malloc(PAGE_SIZE);
                    int newPageNum = splitInterVarchar(ixFileHandle, page, newPage);

                    void* rootKey = malloc(PAGE_SIZE);
                    int rootKeyLength = findPushUpKey(page,newPage,newKey,rootKey,attribute,x,y);

                    void* insertKey = malloc(rootKeyLength + 8);
                    memcpy((char*)insertKey,(char*)&currPage, sizeof(int));
                    memcpy((char*)insertKey + sizeof(int),(char*)rootKey,rootKeyLength);
                    memcpy((char*)insertKey + sizeof(int) + rootKeyLength,(char*)&newPageNum, sizeof(int));
                    setRootPage(ixFileHandle,insertKey,rootKeyLength);

                    //Unsetting root notation in current page ::  Root -> Inter
                    char unset = '0';
                    memcpy((char *) page + sizeof(char), (char *)&unset, sizeof(char));
                    ixFileHandle.writePage(currPage, page);
                    ixFileHandle.writePage(newPageNum,newPage);
                    free(rootKey);
                    free(newPage);
                }
            }
            free(newChild);
        }
    }
    else if(isLeaf(page))
    {
        int length = getLengthOfEntry(newKey,attribute);
        if(isSpaceAvailable(page,length))
        {
            addToPage(page,newKey,attribute);
            free(page);
            return;
        } else
        {
            if(attribute.type == TypeInt)
            {
                void* newPage = malloc(PAGE_SIZE);
                newLeafPage(newPage);

                int newPageNum = splitLeaf(ixFileHandle, page,newPage);

                n1 = currPage;
                n2 = newPageNum;
                lengthToRet = 12;

                int offset = (170 * 3 * sizeof(int)) + 10;
                // Inserting the new entry into either of these pages
                void* lastRecOnCurrPage = malloc(12);
                memcpy((char*)lastRecOnCurrPage,(char*)page + offset - (3* sizeof(int)),12);

                void* firstRecOnNewPage = malloc(12);
                memcpy((char*)firstRecOnNewPage,(char*)newPage + 10,12);

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
                // Verify for bugs
            }
            else if(attribute.type == TypeReal)
            {
                void* newPage = malloc(PAGE_SIZE);
                newLeafPage(newPage);

                int newPageNum = splitLeaf(ixFileHandle, page,newPage);

                n1 = currPage;
                n2 = newPageNum;
                lengthToRet = 12;

                int offset = (170 * 3 * sizeof(int)) + 10;
                // Inserting the new entry into either of these pages
                void* lastRecOnCurrPage = malloc(12);
                memcpy((char*)lastRecOnCurrPage,(char*)page + offset - (3* sizeof(int)),12);

                void* firstRecOnNewPage = malloc(12);
                memcpy((char*)firstRecOnNewPage,(char*)newPage + 10,12);

                int compare1 = compareReal(newKey,lastRecOnCurrPage);
                int compare2 = compareReal(newKey,firstRecOnNewPage);

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

                int newPageNum = splitLeafVarchar(ixFileHandle,page,newPage);
                n1 = currPage;
                n2 = newPageNum;

                int offset = 10; int length;

                // To get the start of the last record
                for(int i=1;i<getSlotOnPage(page);i++)
                {
                    memcpy((char*)&length, (char*)page + offset, sizeof(int));
                    offset += (length + 12);
                }

                memcpy((char*)&length, (char*)page + offset, sizeof(int));
                void* lastRecOnCurrPage = malloc(length + 12);
                memcpy(lastRecOnCurrPage,(char*)page + offset, length + 12);

                memcpy((char*)&length,(char*)newPage+10, sizeof(int));
                void* firstRecOnNewPage = malloc(length + 12);
                memcpy(firstRecOnNewPage,(char*)newPage + 10,length + 12);

                int compare1 = compareVarChar(newKey,lastRecOnCurrPage);
                int compare2 = compareVarChar(newKey,firstRecOnNewPage);

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
        int x , y , lenRec;
        insertIntoPage(ixFileHandle,attribute,newPageToInsert,newKey,newChild,x,y,lenRec);

        if(newChild == nullptr)
        {
            free(newChild);
            return;
        } else
        {
            if(isSpaceAvailable(page,lenRec))
            {
                addToInterPage(page,attribute,newChild,x,y,lenRec);
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

                    n1 = currPage; n2 = newPageNum;

                    int offset = PAGE_SIZE - getSpaceOnPage(page) - sizeof(int) - 12;
                    void* lastRecOnCurrPage = malloc(12);
                    memcpy((char*)lastRecOnCurrPage,(char*)page + offset,12);

                    void* firstRecOnNewPage = malloc(12);
                    memcpy((char*)firstRecOnNewPage,(char*)newPage + 10 + sizeof(int),12);

                    int compare1 = compareInt(newChild,lastRecOnCurrPage);
                    int compare2 = compareInt(newChild,firstRecOnNewPage);

                    if(compare1 < 0)
                    {
                        addToInterPage(page,attribute,newChild,x,y,lenRec);
                        memcpy((char*)returnedChild,(char*)firstRecOnNewPage,12);
                        lengthToRet = 12;
                    }
                    else if(compare2 > 0)
                    {
                        addToInterPage(newPage,attribute,newChild,x,y,lenRec);
                        memcpy((char*)returnedChild,(char*)firstRecOnNewPage,12);
                        lengthToRet = 12;
                    }
                    else if(compare1 > 0 && compare2 < 0)
                    {
                        //This means first pointer in new page is x.

                        int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - sizeof(int);
                        //shift all records in new page
                        memmove((char*)newPage + 10 + sizeof(int) + lenRec + sizeof(int),(char*)newPage + 10 + sizeof(int) , shiftSize);
                        memcpy((char*)newPage + 10 + sizeof(int),(char*)newChild,lenRec);
                        memcpy((char*)newPage + 10 + sizeof(int) + lenRec,(char*)&y, sizeof(int));
                        memcpy(returnedChild,newChild,12);
                        ixFileHandle.writePage(newPageNum,newPage);
                    }

                    free(lastRecOnCurrPage);
                    free(firstRecOnNewPage);

                }
                else if(attribute.type == TypeVarChar)
                {
                    int newPageNum = splitInterVarchar(ixFileHandle,page,newPage);

                    n1 = currPage; n2 = newPageNum;

                    int offset = 10;
                    int numSlots = getSlotOnPage(page);
                    offset += sizeof(int);
                    for(int i=1;i<numSlots;i++)
                    {
                        int temp;
                        memcpy((char*)&temp,(char*)page + offset, sizeof(int));
                        offset += (temp + 12);
                    }

                    int length1;
                    memcpy((char*)&length1,(char*)page + offset, sizeof(int));
                    void* lastRecOnCurrPage = malloc(length1 + 12);
                    memcpy((char*)lastRecOnCurrPage,(char*)page + offset,length1 + 12);


                    memcpy((char*)&length1,(char*)newPage + 10 + sizeof(int), sizeof(int));
                    void* firstRecOnNewPage = malloc(length1 + 12);
                    memcpy((char*)firstRecOnNewPage,(char*)newPage + 10 + sizeof(int),length1 + 12);

                    int compare1 = compareInt(newChild,lastRecOnCurrPage);
                    int compare2 = compareInt(newChild,firstRecOnNewPage);

                    int startPtr,endPtr;
                    memcpy((char*)&startPtr,(char*)newPage + 10, sizeof(int));
                    memcpy((char*)&endPtr,(char*)newPage + 10 + sizeof(int) + lenRec + 12, sizeof(int));

                    if(compare1 < 0)
                    {
                        addToInterPage(page,attribute,newChild,n1,n2,lenRec);

                        memcpy((char*)returnedChild,(char*)firstRecOnNewPage,length1 + 12);
                        lengthToRet = length1 + 12;
                    }
                    else if(compare2 > 0)
                    {
                        addToInterPage(newPage,attribute,newChild,n1,n2,lenRec);

                        memcpy((char*)returnedChild,(char*)firstRecOnNewPage,length1 + 12);
                        lengthToRet = length1 + 12;
                    }
                    else if(compare1 > 0 && compare2 < 0)
                    {
                        //This means first pointer in new page is x.

                        int shiftSize = PAGE_SIZE - getSpaceOnPage(newPage) - 10 - sizeof(int);
                        //shift all records in new page
                        memmove((char*)newPage + 10 + sizeof(int) + lenRec + sizeof(int),(char*)newPage + 10 + sizeof(int) , shiftSize);
                        memcpy((char*)newPage + 10 + sizeof(int),(char*)newChild,lenRec + 12);
                        memcpy((char*)newPage + 10 + sizeof(int) + lenRec,(char*)&y, sizeof(int));
                        ixFileHandle.writePage(newPageNum,newPage);

                        memcpy((char*)returnedChild,(char*)newChild,length1 + 12);
                        lengthToRet = lenRec;
                    }

                    free(lastRecOnCurrPage);
                    free(firstRecOnNewPage);
                }

                free(newPage);
            }
        }
        free(newChild);
    }
    free(page);
    return;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {

int headerOffset = 10;
    RID nextRid;
    void *nextkey = malloc(200);
    int scanRes;
    IX_ScanIterator ix_ScanIterator;
    scanRes = scan(ixFileHandle, attribute, key, key, true, true, ix_ScanIterator);
    while(ix_ScanIterator.getNextEntry( nextRid, nextkey)){
        if(rid.pageNum == nextRid.pageNum && rid.slotNum == nextRid.slotNum){

            //get the leaf page num -->
            int leafPageNum;
            int numRec, freeSpace;

            void *pageData = malloc(PAGE_SIZE);
            ixFileHandle.readPage(leafPageNum, pageData);

            memcpy(&numRec, (char*) pageData + headerOffset - sizeof(int), sizeof(int));
            memcpy(&freeSpace, (char*)pageData + headerOffset - 2* sizeof(int), sizeof(int));
            



        }


    }

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
void IndexManager::printCurrentNode(IXFileHandle &ixFileHandle, const Attribute &attribute, int pageNum, int newNode) const
{

    //read the page and get the number of records on that page


   // std::cout<<"Page NUM"<<pageNum<<std::endl;
    void *pageData = malloc(PAGE_SIZE);
    ixFileHandle.readPage(pageNum, pageData);
    int headerOffset = 10, numOfKeysCount = 0;
    numOfKeysCount = getSlotOnPage(pageData);
    std::cout<<"key count  is "<<numOfKeysCount<<std::endl;

    //getting the type of the current node
    bool isleaf;
    isleaf = isLeaf(pageData);
    std::cout<<"{";

    //check if it's the root in which case print the start of the json string
    if(pageNum == getRootPage(ixFileHandle) && !isleaf){
        std::cout << "\n";
    }
    std::cout << "\"keys\": [";


    //std::cout<<"the number of records are "<<numOfKeysCount<<std::endl;

    RID currRID;
    //if leaf node print the data too

    if(isleaf){
        switch(attribute.type){
            case TypeInt:
                for (int i = 0; i < numOfKeysCount; i++)
                {
                    int key;

                    //header 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + headerOffset + sizeof(int)+ 3*i*sizeof(int), sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset +  2*sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    memcpy(&currRID.slotNum, (char*)pageData + headerOffset + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));

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
                for (int i = 0; i < numOfKeysCount; i++)
                {
                    float key;

                    //header + P0 + 3*i*sizeof(int)
                    memcpy(&key, (char*)pageData + headerOffset + 3*i*sizeof(int), sizeof(int));
                    memcpy(&currRID.pageNum, (char*)pageData + headerOffset +  sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    memcpy(&currRID.slotNum, (char*)pageData + headerOffset + 2* sizeof(int) + 3* sizeof(int)*i, sizeof(int));


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
                for (int i = 0; i < numOfKeysCount; i++) {

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
                if(numOfKeysCount != 0)
                {
                    for (int i = 0; i < numOfKeysCount; i++) //to key_m
                    {
                        int key;

                        //header + P0 + 4*i*sizeof(int)
                        memcpy(&key, (char*)pageData + headerOffset  + sizeof(int) + 4*i*sizeof(int), sizeof(int));
                        if (i > 0)
                            std::cout << ", ";
                        std::cout << "\"" << key << "\"";
                    }
                    // Print m+1 children
                    std::cout << "],\n\"children\": [\n";
                    int pageID;
                    memcpy(&pageID, (char*)pageData + headerOffset, sizeof(int));

                    if (pageID >= 0)
                        printCurrentNode(ixFileHandle, attribute, pageID, newNode);

                    for (int i = 0; i < numOfKeysCount; i++){
                        if (i > 0)
                            std::cout << ",\n";


                        memcpy(&pageID, (char*)pageData + headerOffset + sizeof(int) + 3* sizeof(int)*(i+1), sizeof(int));
                      //  std::cout<<"page number"<<pageNum<<std::endl;
                        if (pageID >= 0)
                            printCurrentNode(ixFileHandle, attribute, pageID, newNode);
                        else
                            std::cout << "{\"keys\": [\"\"]}"; // Empty child
                    }
                }
                break;
            case TypeReal:

                for (int i = 0; i < numOfKeysCount; i++) // key_2 to key_m
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
                for (int i = 0; i < numOfKeysCount; i++)
                {
                    if (i > 0)
                        std::cout << ",\n";
                    int pageNum;
                    memcpy((char*)pageNum, (char*)pageData + headerOffset + 3* sizeof(int) + 3* sizeof(int)*i, sizeof(int));
                    if (pageNum >=0)
                        printCurrentNode(ixFileHandle, attribute, pageNum, newNode);
                    else
                        std::cout << "{\"keys\": [\"\"]}"; // Empty child
                }


                break;

            case TypeVarChar:
                int offset = 0;
                std::vector<int> keyLengthVec;
                for (int i = 0; i < numOfKeysCount; i++) {
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
                for(int i = 0; i <= numOfKeysCount; i++) // child_1 to child_m
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
    if (getRootPage(ixFileHandle) == pageNum && !isleaf)
        std:: cout << "\n";
    std::cout << "}";


}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const
{

    //check if a B+ tree exists
/*
    void* page = malloc(PAGE_SIZE);

    ixFileHandle.readPage(0,page);
    int temp;
    //memcpy(&temp, (char*) page +26, sizeof(int));
    std::cout<<"total pages"<<ixFileHandle.getNumberOfPages()<<std::endl;
    std::cout<<"total number of slots"<<getSlotOnPage(page)<<std::endl;

    std::cout<<"temp is"<<temp<<std::endl;
*/
    int rootpageNum = getRootPage(ixFileHandle);
    int newNode = 0;
    if(rootpageNum < 0) {
        std::cout<<"B+ tree doesn't exist"<<std::endl;
        return;
    }
   // std::cout<<"PageNum :"<<rootpageNum<<std::endl;
    // Start with root -> pageNum -> 0
   printCurrentNode(ixFileHandle, attribute, rootpageNum, newNode);
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

    int readCounter;
    file->seekg(sizeof(int) + sizeof(int),std::ios_base::beg);
    file->read((char*)&readCounter, sizeof(int));
    readCounter++;
    file->seekp(sizeof(int) + sizeof(int),std::ios_base::beg);
    file->write((char*)&readCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);
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
