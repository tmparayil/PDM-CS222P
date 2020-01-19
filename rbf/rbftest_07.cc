#include "pfm.h"
#include "test_util.h"
#include "pfm.cc"

int RBFTest_7(PagedFileManager &pfm) {
    // Functions Tested:
    // 1. Create File
    // 2. Open File
    // 3. Append Page
    // 4. Get Number Of Pages
    // 5. Read Page
    // 6. Write Page
    // 7. Close File
    // 8. Destroy File
    std::cout << std::endl << "***** In RBF Test Case 07 *****" << std::endl;

    RC rc;
    std::string fileName = "test7";

    unsigned readPageCount = 0;
    unsigned writePageCount = 0;
    unsigned appendPageCount = 0;
    unsigned readPageCount1 = 0;
    unsigned writePageCount1 = 0;
    unsigned appendPageCount1 = 0;

    // Create the file named "test7"
    rc = pfm.createFile(fileName);
    assert(rc == success && "Creating the file should not fail.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file failed.");

    // Open the file "test7"
    FileHandle fileHandle;
    rc = pfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // collect before counters
    rc = fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case 7 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }

    // Append 100 pages
    void *data = malloc(PAGE_SIZE);
    for (unsigned j = 0; j < 100; j++) {
        for (unsigned i = 0; i < PAGE_SIZE; i++) {
            *((char *) data + i) = i % (j + 1) + 30;
        }
        rc = fileHandle.appendPage(data);
        assert(rc == success && "Appending a page should not fail.");
    }
    std::cout << "100 Pages have been successfully appended!" << std::endl;

    // collect after counters
    rc = fileHandle.collectCounterValues(readPageCount1, writePageCount1, appendPageCount1);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case 7 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }
    std::cout << "before:R W A - " << readPageCount << " " << writePageCount << " " << appendPageCount
              << " after:R W A - "
              << readPageCount1 << " " << writePageCount1 << " " << appendPageCount1 << std::endl;
    assert(appendPageCount1 > appendPageCount && "The appendPageCount should have been increased.");

    // Get the number of pages
    unsigned count = fileHandle.getNumberOfPages();
    assert(count == (unsigned) 100 && "The count should be 100 at this moment.");

    // Read the 87th page and check integrity
    void *buffer = malloc(PAGE_SIZE);
    rc = fileHandle.readPage(86, buffer);
    assert(rc == success && "Reading a page should not fail.");

    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 87 + 30;
    }
    rc = memcmp(buffer, data, PAGE_SIZE);
    assert(rc == success && "Checking the integrity of a page should not fail.");
    std::cout << "The data in 87th page is correct!" << std::endl;

    // Update the 87th page
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 60 + 30;
    }
    rc = fileHandle.writePage(86, data);
    assert(rc == success && "Writing a page should not fail.");

    // Read the 87th page and check integrity
    rc = fileHandle.readPage(86, buffer);
    assert(rc == success && "Reading a page should not fail.");

    rc = memcmp(buffer, data, PAGE_SIZE);
    assert(rc == success && "Checking the integrity of a page should not fail.");

    // Close the file "test7"
    rc = pfm.closeFile(fileHandle);
    assert(rc == success && "Closing the file should not fail.");

    // Destroy the file
    rc = pfm.destroyFile(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    rc = destroyFileShouldSucceed(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    free(data);
    free(buffer);

    std::cout << "RBF Test Case 07 Finished! The result will be examined." << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    PagedFileManager &pfm = PagedFileManager::instance();

    remove("test7");

    return RBFTest_7(pfm);
}
