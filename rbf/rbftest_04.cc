/*#include "pfm.h"
#include "test_util.h"
#include "pfm.cc"

int RBFTest_4(PagedFileManager &pfm) {
    // Functions Tested:
    // 1. Open File
    // 2. Append Page
    // 3. Get Number Of Pages
    // 4. Get Counter Values
    // 5. Close File
    std::cout << std::endl << "***** In RBF Test Case 04 *****" << std::endl;

    RC rc;
    std::string fileName = "test3";

    unsigned readPageCount = 0;
    unsigned writePageCount = 0;
    unsigned appendPageCount = 0;
    unsigned readPageCount1 = 0;
    unsigned writePageCount1 = 0;
    unsigned appendPageCount1 = 0;

    // Open the file "test3"
    FileHandle fileHandle;
    rc = pfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // Collect before counters
    rc = fileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case 4 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }

    // Append the first page
    void *data = malloc(PAGE_SIZE);
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        *((char *) data + i) = i % 96 + 30;
    }
    rc = fileHandle.appendPage(data);
    assert(rc == success && "Appending a page should not fail.");

    // collect after counters
    rc = fileHandle.collectCounterValues(readPageCount1, writePageCount1, appendPageCount1);
    if (rc != success) {
        std::cout << "[FAIL] collectCounterValues() failed. Test Case 4 failed." << std::endl;
        pfm.closeFile(fileHandle);
        return -1;
    }
    std::cout << "before:R W A - " << readPageCount << " " << writePageCount << " " << appendPageCount
              << " after:R W A - "
              << readPageCount1 << " " << writePageCount1 << " " << appendPageCount1 << std::endl;
    assert(appendPageCount1 > appendPageCount && "The appendPageCount should have been increased.");

    // Get the number of pages
    unsigned count = fileHandle.getNumberOfPages();
    assert(count == (unsigned) 1 && "The count should be one at this moment.");

    // Close the file "test3"
    rc = pfm.closeFile(fileHandle);
    assert(rc == success && "Closing the file should not fail.");

    free(data);

    std::cout << "RBF Test Case 04 Finished! The result will be examined." << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    PagedFileManager &pfm = PagedFileManager::instance();

    return RBFTest_4(pfm);
}
*/
#include "pfm.h"
#include "rbfm.h"
#include "test_util.h"

int RBFTest_private_2c(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. Create File - RBFM
    // 2. Open File
    // 3. insertRecord() - with multiple NULLs
    // 4. Close File
    // 5. Destroy File
    std::cout << "***** In RBF Test Case Private 2c *****" << std::endl;

    RC rc;
    std::string fileName = "test_private_2c";

    // Create a file named "test_private_2c"
    rc = rbfm.createFile(fileName);
    assert(rc == success && "Creating the file should not fail.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file should not fail.");

    // Open the file "test_private_2c"
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    RID rid;
    int recordSize = 0;
    void *record = malloc(2000);
    void *returnedData = malloc(2000);

    std::vector<Attribute> recordDescriptor;
    createLargeRecordDescriptor3(recordDescriptor);

    // NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    unsigned char nullsIndicator[nullFieldsIndicatorActualSize];
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);
    memset(record, 0, 2000);

    // Setting the following bytes as NULL
    // The entire byte representation is: 100011011000001111001000
    //                                    123456789012345678901234
    nullsIndicator[0] = 157; // 10011101
    nullsIndicator[1] = 130; // 10000010
    nullsIndicator[2] = 75;  // 01001011

    // Insert a record into a file
    prepareLargeRecord3(recordDescriptor.size(), nullsIndicator, 8, record, &recordSize);

    // Values of attr0, attr13, and attr20 should be NULL.
    std::cout << std::endl << "Data to be inserted:" << std::endl;
    rbfm.printRecord(recordDescriptor, record);

    rc = rbfm.insertRecord(fileHandle, recordDescriptor, record, rid);
    assert(rc == success && "Inserting a record should not fail.");

    // Given the rid, read the record from file
    rc = rbfm.readRecord(fileHandle, recordDescriptor, rid, returnedData);
    assert(rc == success && "Reading a record should not fail.");

    // Values of multiple fields should be NULL.
    std::cout << std::endl << "Returned Data:" << std::endl;
    rbfm.printRecord(recordDescriptor, returnedData);

    // Compare whether the two memory blocks are the same
    if (memcmp(record, returnedData, recordSize) != 0) {
        std::cout << "***** [FAIL] Test Case Private 2c Failed! *****" << std::endl << std::endl;
        free(record);
        free(returnedData);
        rbfm.closeFile(fileHandle);
        rbfm.destroyFile(fileName);
        return -1;
    }

    // Close the file "test_private_2c"
    rc = rbfm.closeFile(fileHandle);
    assert(rc == success && "Closing the file should not fail.");

    int fsize = getFileSize(fileName);
    if (fsize <= 0) {
        std::cout << "File Size should not be zero at this moment." << std::endl;
        return -1;
    }

    // Destroy File
    rc = rbfm.destroyFile(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    rc = destroyFileShouldSucceed(fileName);
    assert(rc == success && "Destroying the file should not fail.");

    free(record);
    free(returnedData);

    std::cout << "***** RBF Test Case Private 2c Finished. The result will be examined! *****" << std::endl << std::endl;

    return 0;
}

int main() {

    remove("test_private_2c");

    return RBFTest_private_2c(RecordBasedFileManager::instance());

}