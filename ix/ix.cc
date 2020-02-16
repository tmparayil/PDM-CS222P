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

void initialise(std::fstream &file)
{
    void* buffer = malloc(PAGE_SIZE);
    int root = 0; // At the start, root page is Page 0
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


/**
 *
 * insertEntry(page,compositeKey,returnedChild)
 *
 *
 * @param ixFileHandle
 * @param attribute
 * @param key
 * @param rid
 * @return
 */

int IndexManager::getRootPage(IXFileHandle &ixFileHandle) {
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
        memcpy((char*)page + 10,(char*)entry,length);
    }

    ixFileHandle.appendPage(page);

    int currPageNum = ixFileHandle.getNumberOfPages();
    setRootInHidden(ixFileHandle,currPageNum);
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

bool IndexManager::validRootPage(const void *page) {

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[0] == 'I' && checkArr[1] == 'L') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool IndexManager::isLeaf(const void *page) {

    char* checkArr = new char[2];
    memcpy((char*)checkArr,(char*)page, 2*sizeof(char));

    if(checkArr[1] == 'L') {
        delete[] checkArr;
        return true;
    }

    delete[] checkArr;
    return false;
}

bool IndexManager::isInter(const void *page) {

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

int IndexManager::getSlotOnPage(const void *page) {
    int slot;
    memcpy((char*)&slot,(char*)page + (2* sizeof(char)) + sizeof(int), sizeof(int));
    return slot;
}


void IndexManager::setSpaceOnPage(const void *page,int space) {
    memcpy((char*)page + (2* sizeof(char)),(char*)&space, sizeof(int));
    return;
}

void IndexManager::setSlotOnPage(const void *page,int slot) {
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
    if(totalPages == 0)
    {
        setRootPage(ixFileHandle,NULL,0);
    }

    void* page = malloc(PAGE_SIZE);
    int root = getRootPage(ixFileHandle);
    ixFileHandle.readPage(root,page);

    if(!validRootPage(page))
        return -1;

    void* returnedChild = malloc(PAGE_SIZE);
    int n1 = -1 , n2 = -1;
    int length = -1;

    if(attribute.type == TypeInt || attribute.type == TypeReal)
    {
        void* newKey = malloc(12); // sizeof(int) + RID size (2 * sizeof(int))
        memcpy((char*)newKey,key, sizeof(int));
        memcpy((char*)newKey + sizeof(int),(char*)rid.pageNum, sizeof(int));
        memcpy((char*)newKey + (2* sizeof(int)),(char*)rid.slotNum, sizeof(int));

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

    // If returnedChild is not Null , new root has to be created.
    if(returnedChild != nullptr)
    {
        void* newChild = malloc( (2*sizeof(int)) + length);
        memcpy((char*)newChild,(char*)&n1, sizeof(int));
        memcpy((char*)newChild + sizeof(int),(char*)returnedChild,length);
        memcpy((char*)newChild + sizeof(int) + length,(char*)&n2, sizeof(int));
        setRootPage(ixFileHandle,newChild, (2*sizeof(int)) + length );

        //Unsetting leaf notation in root page
        char unset = '0';
        memcpy((char*)page + sizeof(char),(char*)unset, sizeof(char));
        ixFileHandle.writePage(root,page);
    }

    free(page);
    return 0;
}

int IndexManager::compareInt(const void *entry, const void *recordOnPage) {

    return 0;
}

int IndexManager::compareReal(const void *entry, const void *recordOnPage) {

    return 0;
}

int IndexManager::compareVarChar(const void *entry, const void *recordOnPage) {
    return 0;
}


void IndexManager::addToPage(void *page, const void *newKey,const Attribute &attribute) {

    //search for location
    int numSlots = getSlotOnPage(page);
    // start of record offset
    int offset = 10;
    offset+= sizeof(int);

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
            void* temp = malloc(12);
            memcpy((char*)temp,(char*)page + offset,12);
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

//Actual insert function
void IndexManager::insertIntoPage(IXFileHandle &ixFileHandle, const Attribute &attribute,int currPage,
                                  const void *newKey, void *returnedChild, int &n1, int &n2, int &length) {
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readPage(currPage,page);

    if(isLeaf(page))
    {
        int length = getLengthOfEntry(newKey,attribute);
        if(isSpaceAvailable(page,length))
        {
            addToPage(page,newKey,attribute);
        } else
        {
            if(attribute.type == TypeInt)
            {
                void* newPage = malloc(PAGE_SIZE);
                newLeafPage(newPage);

                int newPageNum = splitLeaf(ixFileHandle, page,newPage);

                n1 = currPage;
                n2 = newPageNum;
                length = 12;

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
                length = 12;

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
                    ixFileHandle.writePage(currPage,page);
                }
                else if(compare2 > 0)
                {
                    addToPage(newPage,newKey,attribute);
                    memcpy(returnedChild,firstRecOnNewPage,length + 12);
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
                    ixFileHandle.writePage(newPageNum,newPage);
                }
                free(lastRecOnCurrPage);
                free(firstRecOnNewPage);
                free(newPage);
                free(page);
                return;
            }
        }
    } else if(isInter(page))
    {

    }

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

unsigned int IXFileHandle::getNumberOfPages() {
    int pages;
    file->seekg(sizeof(int),std::ios_base::beg);
    file->read((char*)&pages, sizeof(int));
    file->seekg(0,std::ios_base::beg);
    return pages;
}

RC IXFileHandle::readPage(PageNum pageNum, void *data) {
    if(!check_file_stream())
        return -1;

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

RC IXFileHandle::writePage(PageNum pageNum, const void *data) {
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

