#include "pfm.h"
#include <iostream>

PagedFileManager &PagedFileManager::instance() {
    static PagedFileManager _pf_manager = PagedFileManager();
    return _pf_manager;
}

PagedFileManager::PagedFileManager() = default;

PagedFileManager::~PagedFileManager() = default;

PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

bool file_exists(const std::string &fileName)
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
    int temp = HEADER_VAL;
    int recordNum = 0;
    int readCount = 0;
    int writeCount = 0;
    int appendCount = 1;

    // First time creation of hidden page
    memcpy((char*)buffer,(char*)&temp,HEADER_SIZE);
    memcpy((char*)buffer + HEADER_SIZE,(char*)&recordNum, sizeof(int));
    memcpy((char*) buffer + HEADER_SIZE + sizeof(int),(char*)&readCount, sizeof(int));
    memcpy((char*)buffer + HEADER_SIZE + 2 * sizeof(int),(char*)&writeCount, sizeof(int));
    memcpy((char*)buffer + HEADER_SIZE + 3 * sizeof(int),(char*)&appendCount, sizeof(int));

    file.seekp(0,std::ios_base::beg);
    file.write((char*)buffer,PAGE_SIZE);
    file.seekp(0,std::ios_base::beg);
    free(buffer);
}

RC PagedFileManager::createFile(const std::string &fileName) {

    if(file_exists(fileName))
        return -1;

    std::fstream newFile(fileName,std::ios::out);
    if(!newFile)
    {
        std::cout<<"Error in creating the file"<<std::endl;
        return -1;
    }

    initialise(newFile);
    newFile.close();
    return 0;
}

RC PagedFileManager::destroyFile(const std::string &fileName) {

    if(!file_exists(fileName))
        return -1;

    if(std::remove(fileName.c_str()) != 0)
        return -1;

    return 0;
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {

    if(file_exists(fileName)) {
        fileHandle.setFile(const_cast<std::string &>(fileName));
        return 0;
    }

    return -1;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {

    if((fileHandle.getFile()) != nullptr)
    {
        fileHandle.closeFile();
        return 0;
    }

    std::cout<<"No file stream is open with this handle"<<std::endl;
    return -1;
}

FileHandle::FileHandle() {
    file;
}

FileHandle::~FileHandle() = default;

void FileHandle::setFile(std::string& fileName) {
    file = new std::fstream(fileName);
}

void FileHandle::closeFile()
{
    file->close();
    delete(file);
}

std::fstream* FileHandle::getFile() {
    return file;
}

bool FileHandle::check_file_stream()
{
    if(!file->good())
        return false;

    return true;
}

RC FileHandle::readPage(PageNum pageNum, void *data) {

    if(!check_file_stream())
        return -1;

    if(pageNum >= FileHandle::getNumberOfPages())
    {
        return -1;
    }

    int offset = (pageNum + 1) * PAGE_SIZE;
    file->seekg(offset,std::ios_base::beg);
    file->read((char*)data, PAGE_SIZE);
    file->seekg(0,std::ios_base::beg);

    int readCounter;
    file->seekg(HEADER_SIZE + sizeof(int),std::ios_base::beg);
    file->read((char*)&readCounter, sizeof(int));
    readCounter++;
    file->seekp(HEADER_SIZE + sizeof(int),std::ios_base::beg);
    file->write((char*)&readCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {

    if (!check_file_stream())
        return -1;

    if (pageNum >= FileHandle::getNumberOfPages())
        return -1;

    int offset = (pageNum + 1) * PAGE_SIZE;
    file->seekp(offset, std::ios_base::beg);
    file->write((char *)data, PAGE_SIZE);
    file->seekp(0, std::ios_base::beg);

    int writeCounter;
    file->seekg(HEADER_SIZE + 2 * sizeof(int),std::ios_base::beg);
    file->read((char*)&writeCounter, sizeof(int));
    writeCounter++;
    file->seekp(HEADER_SIZE + 2 * sizeof(int),std::ios_base::beg);
    file->write((char*)&writeCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);
    return 0;
}

RC FileHandle::appendPage(const void *data) {

    if(!check_file_stream())
        return -1;

    file->seekp(0,std::ios_base::end);
    file->write((char*)data, PAGE_SIZE);
    file->seekp(0,std::ios_base::beg);

    //Update total pages as well
    int totalPages = 0;
    int appendCounter;

    file->seekg(HEADER_SIZE,std::ios_base::beg);
    file->read((char*)&totalPages, sizeof(int));
    file->seekg(HEADER_SIZE + 3 * sizeof(int),std::ios_base::beg);
    file->read((char*)&appendCounter, sizeof(int));
    totalPages++;
    appendCounter++;
    file->seekp(HEADER_SIZE,std::ios_base::beg);
    file->write((char*)&totalPages, sizeof(int));
    file->seekp(HEADER_SIZE + 3 * sizeof(int),std::ios_base::beg);
    file->write((char*)&appendCounter, sizeof(int));
    file->seekp(0,std::ios_base::beg);

    return 0;
}

unsigned FileHandle::getNumberOfPages() {

    int pages;
    file->seekg(HEADER_SIZE,std::ios_base::beg);
    file->read((char*)&pages, sizeof(int));
    file->seekg(0,std::ios_base::beg);

    return pages;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {

    void* buffer = malloc(PAGE_SIZE);
    file->seekg(0,std::ios_base::beg);
    file->read((char*)buffer,PAGE_SIZE);

    int readPage , writePage , appendPage;

    memcpy((char*)&readPage,(char*)buffer + HEADER_SIZE + sizeof(int), sizeof(int));
    memcpy((char*)&writePage,(char*)buffer + HEADER_SIZE + 2 * sizeof(int), sizeof(int));
    memcpy((char*)&appendPage,(char*)buffer + HEADER_SIZE + 3 * sizeof(int), sizeof(int));

    readPageCount = readPage;
    writePageCount = writePage;
    appendPageCount = appendPage;

    memcpy((char*)buffer + HEADER_SIZE + sizeof(int),(char*)&readPage, sizeof(int));
    memcpy((char*)buffer + HEADER_SIZE + 2 * sizeof(int),(char*)&writePage, sizeof(int));
    memcpy((char*)buffer + HEADER_SIZE + 3 * sizeof(int),(char*)&appendPage, sizeof(int));

    file->seekp(0,std::ios_base::beg);
    file->write((char*)buffer,PAGE_SIZE);
    file->seekp(0,std::ios_base::beg);

    free(buffer);
    return 0;
}
