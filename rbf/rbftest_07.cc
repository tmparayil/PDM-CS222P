/*#include "pfm.h"
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
*/
#include "pfm.h"
#include "rbfm.h"
#include "test_util.h"

int RBFTest_private_4(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. Create a File - test_private_4a
    // 2. Create a File - test_private_4b
    // 3. Open test_private_4a
    // 4. Open test_private_4b
    // 5. Insert 50000 records into test_private_4a
    // 6. Insert 50000 records into test_private_4b
    // 7. Close test_private_4a
    // 8. Close test_private_4b
    std::cout << std::endl << "***** In RBF Test Case Private 4 ****" << std::endl;

    RC rc;
    std::string fileName = "test_private_4a";
    std::string fileName2 = "test_private_4b";

    // Create a file named "test_private_4a"
    rc = rbfm.createFile(fileName);
    assert(rc == success && "Creating a file should not fail.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating a file should not fail.");

    // Create a file named "test_private_4b"
    rc = rbfm.createFile(fileName2);
    assert(rc == success && "Creating a file should not fail.");

    rc = createFileShouldSucceed(fileName2);
    assert(rc == success && "Creating a file should not fail.");

    // Open the file "test_private_4a"
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // Open the file "test_private_4b"
    FileHandle fileHandle2;
    rc = rbfm.openFile(fileName2, fileHandle2);
    assert(rc == success && "Opening the file should not fail.");

    RID rid, rid2;
    void *record = malloc(1000);
    void *record2 = malloc(1000);
    void *returnedData = malloc(1000);
    void *returnedData2 = malloc(1000);
    int numRecords = 50000;
    int batchSize = 1000;

    std::vector<Attribute> recordDescriptorForTwitterUser, recordDescriptorForTweetMessage;

    createRecordDescriptorForTwitterUser(recordDescriptorForTwitterUser);
    createRecordDescriptorForTweetMessage(recordDescriptorForTweetMessage);

    // NULL field indicator

    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptorForTwitterUser.size());
    unsigned char nullsIndicator[nullFieldsIndicatorActualSize];
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    int nullFieldsIndicatorActualSize2 = getActualByteForNullsIndicator(recordDescriptorForTweetMessage.size());
    unsigned char nullsIndicator2[nullFieldsIndicatorActualSize2];
    memset(nullsIndicator2, 0, nullFieldsIndicatorActualSize2);

    std::vector<RID> rids, rids2;
    // Insert 50,000 records into the file - test_private_4a and test_private_4b
    for (int i = 0; i < numRecords / batchSize; i++) {
        for (int j = 0; j < batchSize; j++) {
            memset(record, 0, 1000);
            int size = 0;

            prepareLargeRecordForTwitterUser(recordDescriptorForTwitterUser.size(), nullsIndicator, i * batchSize + j,
                                             record, &size);
            rc = rbfm.insertRecord(fileHandle, recordDescriptorForTwitterUser, record, rid);
            assert(rc == success && "Inserting a record for the file #1 should not fail.");
            rids.push_back(rid);
        }
        for (int j = 0; j < batchSize; j++) {
            memset(record2, 0, 1000);
            int size2 = 0;

            prepareLargeRecordForTweetMessage(recordDescriptorForTweetMessage.size(), nullsIndicator2,
                                              i * batchSize + j, record2, &size2);

            rc = rbfm.insertRecord(fileHandle2, recordDescriptorForTweetMessage, record2, rid2);
            assert(rc == success && "Inserting a record  for the file #2 should not fail.");

            rids2.push_back(rid2);
        }

        if (i % 5 == 0 && i != 0) {
            std::cout << i << " / " << numRecords / batchSize << " batches (" << numRecords
                      << " records) inserted so far for both files." << std::endl;
        }
    }

    std::cout << "Inserting " << numRecords << " records done for the both files." << std::endl << std::endl;

    // Close the file - test_private_4a
    rc = rbfm.closeFile(fileHandle);
    if (rc != success) {
        return -1;
    }
    assert(rc == success);

    free(record);
    free(returnedData);

    if (rids.size() != numRecords) {
        return -1;
    }

    // Close the file - test_private_4b
    rc = rbfm.closeFile(fileHandle2);
    if (rc != success) {
        return -1;
    }
    assert(rc == success);

    free(record2);
    free(returnedData2);

    if (rids2.size() != numRecords) {
        return -1;
    }

    int fsize = getFileSize(fileName);
    if (fsize <= 0) {
        std::cout << "File Size should not be zero at this moment." << std::endl;
        std::cout << "***** [FAIL] Test Case Private 4 Failed! *****" << std::endl << std::endl;
        return -1;
    }

    fsize = getFileSize(fileName2);
    if (fsize <= 0) {
        std::cout << "File Size should not be zero at this moment." << std::endl;
        std::cout << "***** [FAIL] Test Case Private 4 Failed! *****" << std::endl << std::endl;
        return -1;
    }

    // Write RIDs of test_private_4a to a disk - do not use this code. This is not a page-based operation. For test purpose only.
std::    ofstream ridsFile("test_private_4a_rids", std::ios::out | std::ios::trunc | std::ios::binary);

    if (ridsFile.is_open()) {
        ridsFile.seekp(0, std::ios::beg);
        for (int i = 0; i < numRecords; i++) {
            ridsFile.write(reinterpret_cast<const char *>(&rids.at(i).pageNum), sizeof(unsigned));
            ridsFile.write(reinterpret_cast<const char *>(&rids.at(i).slotNum), sizeof(unsigned));
        }
        ridsFile.close();
        std::cout << std::endl << std::endl;
    }

    // Write RIDs of test_private_4b to a disk - do not use this code. This is not a page-based operation. For test purpose only.
   std:: ofstream ridsFile2("test_private_4b_rids", std::ios::out | std::ios::trunc | std::ios::binary);

    if (ridsFile2.is_open()) {
        ridsFile2.seekp(0, std::ios::beg);
        for (int i = 0; i < numRecords; i++) {
            ridsFile2.write(reinterpret_cast<const char *>(&rids2.at(i).pageNum), sizeof(unsigned));
            ridsFile2.write(reinterpret_cast<const char *>(&rids2.at(i).slotNum), sizeof(unsigned));
        }
        ridsFile2.close();
    }

    std::cout << "***** RBF Test Case Private 4 Finished. The result will be examined. *****" << std::endl << std::endl;

    return 0;
}

int main() {

    remove("test_private_4a");
    remove("test_private_4b");
    remove("test_private_4a_rids");
    remove("test_private_4b_rids");

    return RBFTest_private_4(RecordBasedFileManager::instance());

}