#include "pfm.h"
#include <iostream>
#include <fstream>

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
//        std::cout<< " File exists" << std::endl;
        return true;
    }
    return false;
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

    if(fileHandle.getFile() != nullptr)
    {
        std::cout<<"Already file stream is open with this handle"<<std::endl;
        return -1;
    }

    std::fstream f(fileName);
    if(f.good()) {
//        std::cout<<"Setting file stream to FileHandle"<<std::endl;
        fileHandle.setFile(&f);
        return 0;
    }

    return -1;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {

    if((fileHandle.getFile()) != nullptr)
    {
        fileHandle.setFile(nullptr);
        return 0;
    }

    std::cout<<"No file stream is open with this handle"<<std::endl;
    return -1;
}

FileHandle::FileHandle() {
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    file;
}

FileHandle::~FileHandle() = default;

void FileHandle::setFile(std::fstream* stream) {
    file = stream;
}

std::fstream* FileHandle::getFile() {
    return file;
}

bool FileHandle::check_file_stream()
{
    if(file == nullptr)
    {
        std::cout<<"No stream exists to read"<<std::endl;
        return -1;
    }

    return 0;
}

RC FileHandle::readPage(PageNum pageNum, void *data) {

    if(!check_file_stream())
        return -1;

    file->read(static_cast<char *>(data), PAGE_SIZE);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {

    if(!check_file_stream())
        return -1;

    file->seekg(0,file->end);
    int length = file->tellg();
    if(pageNum * PAGE_SIZE > length)
        return -1;
    else
    {
        int offset = pageNum * PAGE_SIZE;
        file->seekg(offset,file->beg);
        file->write(static_cast<const char *>(data), PAGE_SIZE);
        writePageCounter++;
        return 0;
    }
}

RC FileHandle::appendPage(const void *data) {

    if(!check_file_stream())
        return -1;

    file->seekg(0,file->end);
    int length = file->tellg();
    int numOfPages = length / PAGE_SIZE;
    int incompletePageSize = length % PAGE_SIZE;

    if(incompletePageSize == 0)
    {
        file->seekg(numOfPages*PAGE_SIZE,file->beg);
        file->write(static_cast<const char *>(data), PAGE_SIZE);
    } else
    {
        file->seekg((numOfPages+1)*PAGE_SIZE,file->beg);
        file->write(static_cast<const char *>(data), PAGE_SIZE);
    }
    appendPageCounter++;
    return 0;
}

unsigned FileHandle::getNumberOfPages() {

    file->seekg(0,file->end);
    int length = file->tellg();
    
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    return -1;
}