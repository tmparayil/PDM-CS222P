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
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
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

    int offset = pageNum * PAGE_SIZE;
    file->seekg(offset,file->beg);
    file->read((char*)data, PAGE_SIZE);
    file->seekg(0,file->beg);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {

    if (!check_file_stream())
        return -1;

    if (pageNum >= getNumberOfPages())
        return -1;

    int offset = pageNum * PAGE_SIZE;
    file->seekp(offset, std::ios_base::beg);
    file->write((char *)data, PAGE_SIZE);
    file->seekp(0, std::ios_base::beg);
    writePageCounter++;
    return 0;
}

RC FileHandle::appendPage(const void *data) {

    if(!check_file_stream())
        return -1;

    file->seekp(0,std::ios_base::end);
    file->write((char*)data, PAGE_SIZE);
    file->seekp(0,std::ios_base::beg);
    appendPageCounter++;
    return 0;
}

unsigned FileHandle::getNumberOfPages() {

    file->seekg(0,std::ios_base::end);
    int length = file->tellg();
    file->seekg(0,std::ios_base::beg);
    int pages = length / PAGE_SIZE;
    return pages;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {

    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
    return 0;
}