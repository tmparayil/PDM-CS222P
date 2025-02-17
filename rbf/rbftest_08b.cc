/*#include "pfm.h"
#include "rbfm.h"
#include "test_util.h"
#include "rbfm.cc"
#include "pfm.cc"

int RBFTest_8b(RecordBasedFileManager &rbfm) {
    // Functions tested
    // 1. Create Record-Based File
    // 2. Open Record-Based File
    // 3. Insert Record - NULL
    // 4. Read Record
    // 5. Close Record-Based File
    // 6. Destroy Record-Based File
    std::cout << std::endl << "***** In RBF Test Case 08b *****" << std::endl;

    RC rc;
    std::string fileName = "test8b";

    // Create a file named "test8b"
    rc = rbfm.createFile(fileName);
    assert(rc == success && "Creating the file should not fail.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file failed.");

    // Open the file "test8b"
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    RID rid;
    int recordSize = 0;
    void *record = malloc(100);
    void *returnedData = malloc(100);

    std::vector<Attribute> recordDescriptor;
    createRecordDescriptor(recordDescriptor);

    // NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    // Setting the age & salary fields value as null
    nullsIndicator[0] = 80; // 01010000

    // Insert a record into a file
    prepareRecord(recordDescriptor.size(), nullsIndicator, 8, "Anteater", NULL, 177.8, NULL, record, &recordSize);
    std::cout << std::endl << "Inserting Data:" << std::endl;
    rbfm.printRecord(recordDescriptor, record);

    rc = rbfm.insertRecord(fileHandle, recordDescriptor, record, rid);
    assert(rc == success && "Inserting a record should not fail.");

    // Given the rid, read the record from file
    rc = rbfm.readRecord(fileHandle, recordDescriptor, rid, returnedData);
    assert(rc == success && "Reading a record should not fail.");

    // The salary field should not be printed
    std::cout << std::endl << "Returned Data:" << std::endl;
    rbfm.printRecord(recordDescriptor, returnedData);

    // Compare whether the two memory blocks are the same
    if (memcmp(record, returnedData, recordSize) != 0) {
        std::cout << "[FAIL] Test Case 8b Failed!" << std::endl << std::endl;
        free(nullsIndicator);
        free(record);
        free(returnedData);
        return -1;
    }

    std::cout << std::endl;

    // Close the file "test8b"
    rc = rbfm.closeFile(fileHandle);
    assert(rc == success && "Closing the file should not fail.");

    // Destroy File
    rc = rbfm.destroyFile(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    rc = destroyFileShouldSucceed(fileName);
    assert(rc == success && "Destroying the file should not fail.");
    free(nullsIndicator);
    free(record);
    free(returnedData);

    std::cout << "RBF Test Case 08b Finished! The result will be examined." << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    // PagedFileManager *pfm = PagedFileManager::instance();

    // To test the functionality of the record-based file manager 
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    remove("test8b");

    return RBFTest_8b(rbfm);
}
*/
#include "pfm.h"
#include "test_util.h"

int RBFTest_private_6(PagedFileManager &pfm) {
    // Functions Tested:
    // 1. Open File
    // 2. Append Page
    // 3. Close File
    // 4. Open File again
    // 5. Get Number Of Pages
    // 6. Get Counter Values
    // 7. Close File
    std::cout << std::endl << "***** In RBF Test Case Private 6 *****" << std::endl;

    RC rc;
    std::string fileName = "test_private_6";

    unsigned readPageCount = 0;
    unsigned writePageCount = 0;
    unsigned appendPageCount = 0;
    unsigned readPageCount1 = 0;
    unsigned writePageCount1 = 0;
    unsigned appendPageCount1 = 0;

    rc = pfm.createFile(fileName);
    assert(rc == success && "Creating the file should not fail.");

    // Open the file "test_private_6"
    FileHandle fileHandle;
    rc = pfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // Collect before counters
    rc = fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case p5 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }

    // Append the first page read the first page write the first page append the second page
    void *data = malloc(PAGE_SIZE);
    void *read_buffer = malloc(PAGE_SIZE);
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 96 + 30;
    }
    rc = fileHandle.appendPage(data);
    assert(rc == success && "Appending a page should not fail.");
    rc = fileHandle.readPage(0, read_buffer);
    assert(rc == success && "Reading a page should not fail.");
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 96 + 30;
    }
    rc = fileHandle.writePage(0, data);
    assert(rc == success && "Writing a page should not fail.");
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 96 + 30;
    }
    rc = fileHandle.appendPage(data);
    assert(rc == success && "Appending a page should not fail.");

    // collect after counters
    rc = fileHandle.collectCounterValues(readPageCount1, writePageCount1, appendPageCount1);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case 13 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }
    std::cout << "before:R W A - " << readPageCount << " " << writePageCount << " " << appendPageCount
              << " after:R W A - " << readPageCount1 << " " << writePageCount1 << " " << appendPageCount1 << std::endl;
    assert((readPageCount1 - readPageCount == 1 || readPageCount1 - readPageCount == 2) &&
           "Read counter should be correct.");
    assert(writePageCount1 - writePageCount == 1 && "Write counter should be correct.");
    assert((appendPageCount1 - appendPageCount == 2 || appendPageCount1 - appendPageCount == 3) &&
           "Append counter should be correct.");


    // Get the number of pages
    unsigned count = fileHandle.getNumberOfPages();
    assert(count == (unsigned) 2 && "The count should be one at this moment.");

    // Close the file "test_private_6"
    rc = pfm.closeFile(fileHandle);

    assert(rc == success && "Closing the file should not fail.");

    std::cout << "REOPEN FILE" << std::endl;
    // Open the file "test_private_6"
    FileHandle fileHandle2;
    rc = pfm.openFile(fileName, fileHandle2);

    assert(rc == success && "Open the file should not fail.");

    // collect after counters
    rc = fileHandle2.collectCounterValues(readPageCount, writePageCount, appendPageCount);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case p5 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }
    std::cout << "after reopen: R W A - " << readPageCount << " " << writePageCount << " " << appendPageCount
              << std::endl;
    assert((readPageCount - readPageCount1 <= 2) && "Read counter should be correct.");
    assert((writePageCount - writePageCount1 <= 2) && "Write counter should be correct.");
    assert((appendPageCount - appendPageCount1 == 0) && "Append counter should be correct.");

    rc = pfm.closeFile(fileHandle2);

    assert(rc == success && "Closing the file should not fail.");

    int fsize = getFileSize(fileName);
    if (fsize <= 0) {
        std::cout << "File Size should not be zero at this moment." << std::endl;
        std::cout << "***** [FAIL] RBF Test Case Private 6 Failed! *****" << std::endl << std::endl;
        return -1;
    }

    rc = pfm.destroyFile(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    free(data);
    free(read_buffer);

    std::cout << "RBF Test Case Private 6 Finished! The result will be examined." << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    PagedFileManager &pfm = PagedFileManager::instance();
    remove("test_private_6");
    return RBFTest_private_6(pfm);
}