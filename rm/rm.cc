#include "rm.h"
#include<math.h>
RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager() = default;

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::initTables(std::vector<Attribute> &recordDescriptor) {
    Attribute attr;
    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    Attribute attr2;
    attr.name = "table-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    recordDescriptor.push_back(attr);

    // Attribute attr3;
    attr.name = "file-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    recordDescriptor.push_back(attr);

    //Attribute attr4;
    attr.name = "version";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    return 0;
}

RC RelationManager::initColumns(std::vector<Attribute> &recordDescriptor) {

    Attribute attr;
    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    //Attribute attr2;
    attr.name = "column-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    recordDescriptor.push_back(attr);

    // Attribute attr3;
    attr.name = "column-type";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    //Attribute attr4;
    attr.name = "column-length";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    //Attribute attr5;
    attr.name = "column-position";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

    //Attribute attr6;
    attr.name = "version";
    attr.type = TypeInt;
    attr.length = 4;
    recordDescriptor.push_back(attr);

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

    int nullBitLength = 1;
    char* bitInfo = new char[nullBitLength];
    memset(bitInfo,0,nullBitLength);
    // Null bit add
    memcpy((char*)record,bitInfo,nullBitLength);
    delete[] bitInfo;

    // Table - ID : 1
    memcpy((char*)record + nullBitLength,(char*)&temp, sizeof(int));
    // Table-name : 6TABLES
    memcpy((char*)record + nullBitLength + sizeof(int),(char*)&stringLength, sizeof(int));
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)),tempString,stringLength);
    //File-name : 6TABLES
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength,(char*)&stringLength, sizeof(int));
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength + sizeof(int),tempString,stringLength);
    //Version : 1
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength + sizeof(int) + stringLength,(char*)&temp, sizeof(int));

    //std::cout<<(2 * sizeof(int)) + stringLength + sizeof(int) + stringLength<<std::endl;
    return 0;
}

RC RelationManager::initColumnRecord(void *record) {

    int temp = 2, version = 1;

    char* tempString = "COLUMNS";
    int stringLength = 7;

    int nullBitLength = 1;
    char* bitInfo = new char[nullBitLength];
    memset(bitInfo,0,nullBitLength);
    // Null bit add
    memcpy((char*)record ,bitInfo,nullBitLength);
    delete[] bitInfo;

    // Table - ID : 2
    memcpy((char*)record + nullBitLength,(char*)&temp, sizeof(int));
    // Table-name : 7COLUMNS
    memcpy((char*)record + nullBitLength + sizeof(int),(char*)&stringLength, sizeof(int));
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)),tempString,stringLength);
    //File-name : 7COLUMNS
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength,(char*)&stringLength, sizeof(int));
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength + sizeof(int),tempString,stringLength);
    //Version : 1
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)) + stringLength + sizeof(int) + stringLength,(char*)&version, sizeof(int));

    return 0;
}

int RelationManager::prepareColumnRecord(int tableId, Attribute attribute, void *record, int pos) {
    std::string tempString = attribute.name;
    int length = tempString.length();

    int nullBitLength = 1;
    char* bitInfo = new char[nullBitLength];
    memset(bitInfo,0,nullBitLength);
    // Null bit add
    memcpy((char*)record ,bitInfo,nullBitLength);
    delete[] bitInfo;

    // Table - ID
    memcpy((char*)record + nullBitLength,(char*)&tableId, sizeof(int));

    // Column - name
    memcpy((char*)record + nullBitLength + sizeof(int),(char*)&length, sizeof(int));
    memcpy((char*)record + nullBitLength + (2* sizeof(int)),(char*)tempString.c_str(),length);

    // Column - Type
    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length, (char*)&attribute.type, sizeof(int));
    // Column - length
    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length + sizeof(int), (char*)&attribute.length, sizeof(int));
    // Column - position
    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length + (2 * sizeof(int)), (char*)&pos, sizeof(int));
    int version = 1;
    // Version
    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length + (3 * sizeof(int)), (char*)&version, sizeof(int));

    int offset = nullBitLength + (2* sizeof(int)) + length + (4 * sizeof(int));
    return offset;
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
    void* tableRecord = malloc(30);
    initTableRecord(tableRecord);
    RID rid;
    recordBasedFileManager.insertRecord(fileHandle,recordDescriptor,tableRecord,rid);
    free(tableRecord);
    // 4 + 4 + 7 + 4 + 7 + 4
    void* columnRecord = malloc(32);
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
    void* record = malloc(45);
    RID colRID;
    for(int i = 0; i < recordDescriptor.size(); i++)
    {
        int size = prepareColumnRecord(1,recordDescriptor[i],record,i+1);
        void* temp = malloc(size);
        memcpy(temp,record,size);
        recordBasedFileManager.insertRecord(fileHandle,recordDescriptor1,temp,colRID);
        free(temp);
    }

    for(int i = 0; i < recordDescriptor1.size(); i++)
    {
        int size = prepareColumnRecord(2,recordDescriptor1[i],record,i+1);
        void* temp = malloc(size);
        memcpy(temp,record,size);
        recordBasedFileManager.insertRecord(fileHandle,recordDescriptor1,temp,colRID);
        free(temp);
    }
    free(record);
    recordBasedFileManager.closeFile(fileHandle);
    //END of COLUMNS section

    //std::cout<< "create catalog done"<<std::endl;
    return 0;
}

RC RelationManager::deleteCatalog() {
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();

    recordBasedFileManager.destroyFile("COLUMNS");
    recordBasedFileManager.destroyFile("TABLES");
    return 0;
}

int RelationManager::findNextId(FileHandle &fileHandle,const std::vector<Attribute>& recordDescriptor) {

    int total = fileHandle.getNumberOfPages();
    void* buffer = malloc(PAGE_SIZE);

    fileHandle.readPage(total-1,buffer);
    int offset = PAGE_SIZE - (2 * sizeof(int));

    int slotPointer,test;
    memcpy((char*)&test,(char*) buffer + offset , sizeof(int));
    memcpy((char*)&slotPointer,(char*) buffer + offset + sizeof(int), sizeof(int));

    std::cout<<"findNextId"<<std::endl;
    std::cout<<"  "<<test<<"\t"<<slotPointer<<std::endl;

    int recordOffset,length;
    memcpy((char*)&recordOffset,(char*)buffer + slotPointer, sizeof(int));
    memcpy((char*)&length,(char*)buffer + slotPointer+ sizeof(int), sizeof(int));

    std::cout<<"  "<<recordOffset<<"\t"<<length<<std::endl;
    int tableIdOffset, tableId;
    memcpy((char*)&tableIdOffset,(char*)buffer + recordOffset + sizeof(int), sizeof(int));

    memcpy((char*)&tableId,(char*)buffer + tableIdOffset + recordOffset - sizeof(int), sizeof(int));

    std::cout<<"  "<<tableIdOffset<<"\t"<<tableId<<std::endl;
    free(buffer);
    return tableId;
}

RC RelationManager::prepareTableRecord(const int tableId, const std::string &tableName, const std::string &fileName,
                                       const int version,void* record) {

    int nullBitLength = 1;
    char* bitInfo = new char[nullBitLength];
    memset(bitInfo,0,nullBitLength);
    // Null bit add
    memcpy((char*)record ,bitInfo,nullBitLength);
    delete[] bitInfo;

    int length = tableName.length();
    memcpy((char*)record + nullBitLength,(char*)&tableId, sizeof(int));
    memcpy((char*)record + nullBitLength + sizeof(int),(char*)&length, sizeof(int));
    memcpy((char*)record + nullBitLength + (2 * sizeof(int)),tableName.c_str(),length);

    int fileLength = fileName.length();
    memcpy((char*) record + nullBitLength + (2* sizeof(int)) + length, (char*)&fileLength, sizeof(int));
    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length + sizeof(int),fileName.c_str(), fileLength);

    memcpy((char*)record + nullBitLength + (2* sizeof(int)) + length + sizeof(int) + fileLength,(char*)&version,
           sizeof(int));

    return 0;

}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {

    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.createFile(tableName);

    FileHandle fileHandle;
    recordBasedFileManager.openFile("TABLES",fileHandle);
    std::vector<Attribute> recordDescriptor;
    initTables(recordDescriptor);

    int tableId = findNextId(fileHandle,recordDescriptor);
    tableId += 1;

    std::cout<<"Table ID :"<<tableId<<std::endl;
    int length = tableName.length();
    int size = sizeof(char) + (4 * sizeof(int)) + (2 * length);
    void* record = malloc(size);
    prepareTableRecord(tableId,tableName,tableName,1,record);

    RID rid;
    recordBasedFileManager.insertRecord(fileHandle,recordDescriptor,record,rid);
    recordBasedFileManager.closeFile(fileHandle);
    free(record);

    recordBasedFileManager.openFile("COLUMNS",fileHandle);
    std::vector<Attribute> recordDescriptor1;
    initColumns(recordDescriptor1);
    for(int i=0;i< attrs.size();i++)
    {
        int length = attrs[i].name.length();
        length += (6 * sizeof(int)) + sizeof(char);
        void* colRecord = malloc(length);
        prepareColumnRecord(tableId,attrs[i],colRecord,(i+1));
        recordBasedFileManager.insertRecord(fileHandle,recordDescriptor1,colRecord,rid);
        free(colRecord);
    }

    recordBasedFileManager.closeFile(fileHandle);
    return 0;
}

RC RelationManager::deleteTable(const std::string &tableName) {
    return -1;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {

    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    FileHandle tableHandler, columnHandler;
    RBFM_ScanIterator tableScan,columnScan;
    std::vector<Attribute> tableDescriptor, columnDescriptor;
    initTables(tableDescriptor);
    initColumns(columnDescriptor);


    //table scan
    int tableSize = tableName.length()+sizeof(int), tableID;
    void *val = malloc(tableSize);
    int len = tableName.length();
    RID rid;
    std::vector<std::string> tblid_attr;
    std::cout<<"length of table ::"<<len<<std::endl;
    char* tempString = new char[len+1];
    memcpy((char*)tempString,(char*)tableName.c_str(),len);
    tempString[len]='\0';
    std::cout<<"Table name :::"<<tempString<<std::endl;

    int MAX_RC_SIZE = PAGE_SIZE - (3 * sizeof(int));
    void* scanResult_tbl = malloc(MAX_RC_SIZE);
    // std::cout<<size;
    memcpy((char*)val, &len, sizeof(int));
    memcpy((char*)val + sizeof(int), (char*)tableName.c_str(), len);


    // std::cout<<tableName.c_str();
    std::string attr = "table-id";
    tblid_attr.push_back(attr);
    recordBasedFileManager.openFile("TABLES", tableHandler);
    recordBasedFileManager.scan(tableHandler,tableDescriptor,"table-name",EQ_OP, val, tblid_attr,tableScan);

    while(tableScan.getNextRecord(rid,scanResult_tbl) != RBFM_EOF){
        memcpy(&tableID, (char *)scanResult_tbl + 1,sizeof(int));
        break;
    }

    std::cout<<"Table ID : "<<tableID<<std::endl;

    //freeing the used pointers

    free(val);
    free(scanResult_tbl);
    tableScan.close();
    recordBasedFileManager.closeFile(tableHandler);

//column scan

//column attributes push_back
    std::vector<std::string> col_attributes;
    std::string col_attr1 = "column-name";
    std::string col_attr2 = "column-type";
    std::string col_attr3 = "column-length";
    col_attributes.push_back(col_attr1);
    col_attributes.push_back(col_attr2);
    col_attributes.push_back(col_attr3);
    recordBasedFileManager.openFile("COLUMNS", columnHandler);

    void *val1 = malloc(sizeof(int));
    memcpy((char*)val1, &tableID, sizeof(int));

    //call scan function
    recordBasedFileManager.scan(columnHandler,columnDescriptor,"table-id",EQ_OP, val1, col_attributes,columnScan);
    void* scanResult_col = malloc(PAGE_SIZE);
    char *nullFieldsIndicator;
    while(columnScan.getNextRecord(rid,scanResult_col) != RBFM_EOF){

        // get the attributedesc

        Attribute current_col_attr, columnDesc;
        bool nullinfo;
        int offset = ceil((double) col_attributes.size() / 8);
        nullFieldsIndicator = (char *) malloc(offset);
        memcpy(nullFieldsIndicator, (char *)scanResult_col, offset);

        for(int i = 0 ; i < columnDescriptor.size(); i++) {
            columnDesc = columnDescriptor[i];
            nullinfo = nullFieldsIndicator[i/8] & (1 << (7 - i%8));
            if(!nullinfo){
                switch(columnDesc.type){
                    case TypeInt:
                        int tempInt;
                        memcpy(&tempInt,(char *)scanResult_col + offset, columnDesc.length);
                        offset += columnDesc.length;
                        current_col_attr.length = tempInt;
                        break;
                    case TypeReal:
                        float tempFloat;
                        memcpy(&tempFloat,(char *)scanResult_col + offset, columnDesc.length) ;
                        offset += columnDesc.length;
                        current_col_attr.length = tempFloat;
                        break;
                    case TypeVarChar:
                        int length;
                        memcpy(&length,(char *)scanResult_col + offset, sizeof(int));
                        offset += sizeof(int);
                        std::string tmp;
                        memcpy(&tmp, (char*)scanResult_col + offset, length);
                        offset += length;
                        current_col_attr.name = tmp;
                        break;
                }

            }
        }
        attrs.push_back(current_col_attr);

    }
    free(nullFieldsIndicator);
    free(val1);
    free(scanResult_col);
    columnScan.close();
    recordBasedFileManager.closeFile(columnHandler);
    return 0;
}


RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {


    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
    int res = recordBasedFileManager.insertRecord(fileHandle,recordDescriptor,data, rid);
    recordBasedFileManager.closeFile(fileHandle);

    return res;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
    int res = recordBasedFileManager.deleteRecord(fileHandle,recordDescriptor, rid);
    recordBasedFileManager.closeFile(fileHandle);

    return res;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {

    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
    int res = recordBasedFileManager.updateRecord(fileHandle,recordDescriptor, data, rid);
    recordBasedFileManager.closeFile(fileHandle);

    return res;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {

    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
    int res = recordBasedFileManager.readRecord(fileHandle,recordDescriptor, rid, data);
    recordBasedFileManager.closeFile(fileHandle);

    return res;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {

    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.printRecord(attrs, data);
    return 0;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
    int res = recordBasedFileManager.readAttribute(fileHandle,recordDescriptor, rid, attributeName, data);
    recordBasedFileManager.closeFile(fileHandle);
    return res;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {

    FileHandle fileHandle;
    RecordBasedFileManager& recordBasedFileManager = RecordBasedFileManager::instance();
    recordBasedFileManager.openFile(tableName, fileHandle);
    std::vector<Attribute> recordDescriptor;
    getAttributes(tableName,recordDescriptor);
//    int res = recordBasedFileManager.scan(fileHandle,recordDescriptor,conditionAttribute,compOp,value, attributeNames, rm_ScanIterator);
    // rm_scaniterator to rbfm_scan_iterator
    recordBasedFileManager.closeFile(fileHandle);


    //left to complete this function
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