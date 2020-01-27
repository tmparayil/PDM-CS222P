#include "rm.h"

RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager() = default;

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::initTables(std::vector<Attribute> &recordDescriptor) {
    Attribute attr1;
    attr1.name = "table-id";
    attr1.type = TypeInt;
    attr1.length = 4;
    recordDescriptor.push_back(attr1);

    Attribute attr2;
    attr2.name = "table-name";
    attr2.type = TypeVarChar;
    attr2.length = 50;
    recordDescriptor.push_back(attr2);

    Attribute attr3;
    attr3.name = "file-name";
    attr3.type = TypeVarChar;
    attr3.length = 50;
    recordDescriptor.push_back(attr3);

    Attribute attr4;
    attr4.name = "version";
    attr4.type = TypeInt;
    attr4.length = 4;
    recordDescriptor.push_back(attr4);

    return 0;
}

RC RelationManager::initColumns(std::vector<Attribute> &recordDescriptor) {

    Attribute attr1;
    attr1.name = "table-id";
    attr1.type = TypeInt;
    attr1.length = 4;
    recordDescriptor.push_back(attr1);

    Attribute attr2;
    attr2.name = "column-name";
    attr2.type = TypeVarChar;
    attr2.length = 50;
    recordDescriptor.push_back(attr2);

    Attribute attr3;
    attr3.name = "column-type";
    attr3.type = TypeInt;
    attr3.length = 4;
    recordDescriptor.push_back(attr3);

    Attribute attr4;
    attr4.name = "column-length";
    attr4.type = TypeInt;
    attr4.length = 4;
    recordDescriptor.push_back(attr4);

    Attribute attr5;
    attr5.name = "column-position";
    attr5.type = TypeInt;
    attr5.length = 4;
    recordDescriptor.push_back(attr5);

    Attribute attr6;
    attr6.name = "version";
    attr6.type = TypeInt;
    attr6.length = 4;
    recordDescriptor.push_back(attr6);

    /* If needed later for extra credit -> Can use this for the NULL scenario
     *
    Attribute attr7;
    attr7.name = "flag";
    attr7.type = TypeInt;
    attr7.length = 4;
    recordDescriptor.push_back(attr7);
     */
}

RC RelationManager::initTableRecord(void *record) {

    int temp = 1;
    //length -> 6 : String TABLES
    char* tempString = "TABLES";
    int stringLength = 6;

    // Table - ID : 1
    memcpy((char*)record,(char*)&temp, sizeof(int));
    // Table-name : 6TABLES
    memcpy((char*)record + sizeof(int),(char*)&stringLength, sizeof(int));
    memcpy((char*)record + (2 * sizeof(int)),tempString,stringLength);
    //File-name : 6TABLES
    memcpy((char*)record + (2 * sizeof(int)) + stringLength,(char*)&stringLength, sizeof(int));
    memcpy((char*)record + (2 * sizeof(int)) + stringLength + sizeof(int),tempString,stringLength);
    //Version : 1
    memcpy((char*)record + (2 * sizeof(int)) + stringLength + sizeof(int) + stringLength,(char*)&temp, sizeof(int));
    return 0;
}

RC RelationManager::initColumnRecord(void *record) {

    int temp = 2, version = 1;

    char* tempString = "COLUMNS";
    int stringLength = 7;

    // Table - ID : 2
    memcpy((char*)record,(char*)&temp, sizeof(int));
    // Table-name : 7COLUMNS
    memcpy((char*)record + sizeof(int),(char*)&stringLength, sizeof(int));
    memcpy((char*)record + (2 * sizeof(int)),tempString,stringLength);
    //File-name : 7COLUMNS
    memcpy((char*)record + (2 * sizeof(int)) + stringLength,(char*)&stringLength, sizeof(int));
    memcpy((char*)record + (2 * sizeof(int)) + stringLength + sizeof(int),tempString,stringLength);
    //Version : 1
    memcpy((char*)record + (2 * sizeof(int)) + stringLength + sizeof(int) + stringLength,(char*)&version, sizeof(int));

    return 0;
}

RC RelationManager::prepareColumnRecord(int tableId, Attribute attribute, void *record, int pos) {
    // Table - ID
    memcpy((char*)record,(char*)tableId, sizeof(int));

    std::string tempString = attribute.name;
    int length = tempString.length();
    // Column - name
    memcpy((char*)record + sizeof(int),(char*)&length, sizeof(int));
    memcpy((char*)record + (2* sizeof(int)),(char*)tempString.c_str(),length);
    // Column - Type
    memcpy((char*)record + (2* sizeof(int)) + length, (char*)&attribute.type, sizeof(int));
    // Column - length
    memcpy((char*)record + (2* sizeof(int)) + length + sizeof(int), (char*)&attribute.length, sizeof(int));
    // Column - position
    memcpy((char*)record + (2* sizeof(int)) + length + (2 * sizeof(int)), (char*)&pos, sizeof(int));
    int version = 1;
    // Version
    memcpy((char*)record + (2* sizeof(int)) + length + (3 * sizeof(int)), (char*)&version, sizeof(int));

    return 0;
}

RC RelationManager::createCatalog() {

    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.createFile("TABLES");
    recordBasedFileManager.createFile("COLUMNS");
    FileHandle fileHandle;

    // TABLES section
    recordBasedFileManager.openFile("TABLES",fileHandle);
    std::vector<Attribute> recordDescriptor;
    initTables(recordDescriptor);
    // 4 + 4 + 6 + 4 + 6 + 4
    void* tableRecord = malloc(28);
    initTableRecord(tableRecord);
    RID rid;
    recordBasedFileManager.insertRecord(fileHandle,recordDescriptor,tableRecord,rid);
    free(tableRecord);
    // 4 + 4 + 7 + 4 + 7 + 4
    void* columnRecord = malloc(30);
    initColumnRecord(columnRecord);
    RID rid1;
    recordBasedFileManager.insertRecord(fileHandle,recordDescriptor,columnRecord,rid1);
    free(columnRecord);
    recordBasedFileManager.closeFile(fileHandle);
    // END of TABLES section

    // COLUMNS section
    recordBasedFileManager.openFile("COLUMNS",fileHandle);
    std::vector<Attribute> recordDescriptor1;
    initColumns(recordDescriptor1);
    // 4 + 4 + 15 + 4 + 4 + 4 + 4
    void* record = malloc(39);
    RID colRID;
    for(int i = 0; i < recordDescriptor.size(); i++)
    {
        prepareColumnRecord(1,recordDescriptor[i],record,i+1);
        recordBasedFileManager.insertRecord(fileHandle,recordDescriptor1,record,colRID);
    }

    for(int i = 0; i < recordDescriptor1.size(); i++)
    {
        prepareColumnRecord(2,recordDescriptor1[i],record,i+1);
        recordBasedFileManager.insertRecord(fileHandle,recordDescriptor1,record,colRID);
    }
    free(record);
    recordBasedFileManager.closeFile(fileHandle);
    //END of COLUMNS section

    return 0;
}

RC RelationManager::deleteCatalog() {
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();

    recordBasedFileManager.destroyFile("COLUMNS");
    recordBasedFileManager.destroyFile("TABLES");
    return 0;
}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
    return -1;
}

RC RelationManager::deleteTable(const std::string &tableName) {
    return -1;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
    return -1;
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
    return -1;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    return -1;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
    return -1;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
    return -1;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {
    return -1;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    return -1;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {
    return -1;
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}



