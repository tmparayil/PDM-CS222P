#include <sys/attr.h>
#include <cmath>
#include <float.h>
#include "qe.h"

Filter::Filter(Iterator *input, const Condition &condition) {

    this->input = input;
    this->condition = condition;
}

void Filter::getAttributes(std::vector<Attribute> &attrs) const {
    this->input->getAttributes(attrs);
}

RC Filter::getNextTuple(void *data) {

    std::vector<Attribute> recordDescriptor;
    this->input->getAttributes(recordDescriptor);

    int i = 0;
    bool attrCheck = false;
    for(Attribute attr : recordDescriptor)
    {
        if(attr.name == condition.lhsAttr)
        {
            attrCheck = true;
            break;
        }
        i++;
    }

    //Condition attribute is not present in the list of attributes
    if(!attrCheck)
        return -1;

    void* filterData = malloc(PAGE_SIZE);
    while(this->input->getNextTuple(filterData) != QE_EOF) {
        int length = getLengthOfRecord(filterData, recordDescriptor);
        bool checkIfTrue = false;
        //RHS is a value
        if (!this->condition.bRhsIsAttr) {
            if (checkIfTrueWrapper(filterData, this->condition, i,recordDescriptor)) {
                checkIfTrue = true;
            }
            if (checkIfTrue) {
                memcpy((char*)data,(char*)filterData, length);
                free(filterData);
                return 0;
            }
        } else {
            std::cout<<"comparison between 2 attributes"<<std::endl;
        }

    }
    free(filterData);
    return QE_EOF;
}

bool checkIfTrueWrapper(const void* data,const Condition& condition,int ptr,const std::vector<Attribute>& recordDescriptor)
{
    void* value = malloc(PAGE_SIZE);
    getAttributeValue(data,value,ptr,recordDescriptor);
    if(checkCondition(value,condition))
        return true;
    else
        return false;
}

bool checkCondition(const void* value,const Condition& condition)
{
    if(condition.rhsValue.type == TypeInt)
    {
        int val1 , val2;
        memcpy((char*)&val1,(char*)value, sizeof(int));
        memcpy((char*)&val2,(char*)condition.rhsValue.data, sizeof(int));
        if(checkConditionInt(val1,val2,condition.op))
            return true;
        else
            return false;
    }
    else if(condition.rhsValue.type == TypeReal)
    {
        float val1 , val2;
        memcpy((char*)&val1,(char*)value, sizeof(int));
        memcpy((char*)&val2,(char*)condition.rhsValue.data, sizeof(int));
        if(checkConditionFloat(val1,val2,condition.op))
            return true;
        else
            return false;
    }
    else if(condition.rhsValue.type == TypeVarChar)
    {
        int recordLength;
        memcpy((char*)&recordLength,(char*)value, sizeof(int));
        char* tempString = new char[recordLength+1];
        memcpy(tempString,(char*)value + sizeof(int), recordLength);
        tempString[recordLength] = '\0';
        int length;
        memcpy((char*)&length,(char*)condition.rhsValue.data, sizeof(int));
        char* compareString = new char[length+1];
        memcpy(compareString,(char*)condition.rhsValue.data + sizeof(int),length);
        compareString[length] = '\0';

        if(checkConditionChar(tempString,compareString,condition.op))
        {
            delete[] tempString;
            delete[] compareString;
            return true;
        }
        else
        {
            delete[] tempString;
            delete[] compareString;
            return false;
        }
    }
}


Project::Project(Iterator *input,const std::vector<std::string>& attrNames)
{
    this->input = input;
    this->attrNames = attrNames;
}

void Project::getAttributes(std::vector<Attribute> &attrs) const {
    std::vector<Attribute> temp;
    this->input->getAttributes(temp);
    for(std::string attrName : this->attrNames)
    {
        for(Attribute attr: temp)
        {
            if(attr.name == attrName)
            {
                attrs.push_back(attr);
            }
        }
    }
}

RC Project::getNextTuple(void *data) {

    std::vector<Attribute> recordDescriptor;
    this->input->getAttributes(recordDescriptor);

    int i = 0;
    std::vector<int> attrPos;

    for(std::string attrName : this->attrNames)
    {
        for(int j = 0; j< recordDescriptor.size();j++)
        {
            if(recordDescriptor[j].name == attrName)
            {
                i++;
                attrPos.push_back(j);
            }
        }
    }

    //All mapping attributes are not present in the record Descriptor
    if(i != this->attrNames.size())
        return -1;

    void* rawData = malloc(PAGE_SIZE);
    while(this->input->getNextTuple(rawData) != QE_EOF)
    {
        mappingRecord(recordDescriptor,rawData,data,attrPos);
        free(rawData);
        return 0;
    }

    free(rawData);
    return QE_EOF;
}

RC mappingRecord(const std::vector<Attribute>& recordDescriptor, const void *record, void *data, const std::vector<int>& attrPos)
{
    bool debugFlag = false;
    int nullInfo = ceil((double) attrPos.size() / CHAR_BIT);
    char* bitInfo = new char[nullInfo];
    memset(bitInfo, 0 , nullInfo);

    int nullBitInfo = ceil((double) recordDescriptor.size() / 8);
    bool checkNull = false;

    //offset of data
    int offset = nullInfo;

    char* nullFields = (char*)malloc(nullBitInfo);
    memcpy(nullFields,(char*)record,nullBitInfo);

    if(debugFlag)
    {
        std::cout<<std::endl;
        std::cout<<"QE mapping record()"<<std::endl;
        std::cout<<"----------------"<<std::endl;
        std::cout<<"Null info : "<<nullInfo<<std::endl;
    }

    for(int i=0;i<attrPos.size();i++)
    {
        int index = attrPos[i];
        checkNull = nullFields[index/8] & (1 << (7 - index % 8));

        if(checkNull)
        {
            int bitShift = CHAR_BIT - 1 - i%CHAR_BIT;
            bitInfo[i/CHAR_BIT] = bitInfo[i / CHAR_BIT] | (1 << (bitShift));
            continue;
        } else
        {
            //if not null, call getAttributeValue(const void* data,void* value,int ptr,const std::vector<Attribute>& recordDescriptor)
            void* value = malloc(PAGE_SIZE);

            getAttributeValue(record,value,index,recordDescriptor);
            Attribute attr = recordDescriptor[index];

            if(attr.type == TypeReal || attr.type == TypeInt)
            {
                memcpy((char*)data + offset,(char*)value, sizeof(int));
                offset += sizeof(int);
            }
            else if(attr.type == TypeVarChar)
            {
                int length;
                memcpy((char*)&length,(char*)value, sizeof(int));
                memcpy((char*)data + offset,(char*)value,length + sizeof(int));
                offset += length + sizeof(int);
            }
            free(value);
        }
    }

    memcpy((char*)data,(char*)bitInfo,nullInfo);
    free(nullFields);
    delete[] bitInfo;
    return 0;
}

bool checkConditionInt(int recordValue, int compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return (recordValue == compareValue);
    else if(comparisonOperator == LT_OP)
        return (recordValue < compareValue);
    else if(comparisonOperator == LE_OP)
        return (recordValue <= compareValue);
    else if(comparisonOperator == GT_OP)
        return (recordValue > compareValue);
    else if(comparisonOperator == GE_OP)
        return  (recordValue >= compareValue);
    else if(comparisonOperator == NE_OP)
        return (recordValue != compareValue);
}

bool checkConditionFloat(float recordValue, float compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return (recordValue == compareValue);
    else if(comparisonOperator == LT_OP)
        return (recordValue < compareValue);
    else if(comparisonOperator == LE_OP)
        return (recordValue <= compareValue);
    else if(comparisonOperator == GT_OP)
        return (recordValue > compareValue);
    else if(comparisonOperator == GE_OP)
        return  (recordValue >= compareValue);
    else if(comparisonOperator == NE_OP)
        return (recordValue != compareValue);
}

bool checkConditionChar(char *recordValue, char *compareValue, CompOp comparisonOperator) {
    if(comparisonOperator == EQ_OP)
        return strcmp(recordValue,compareValue) == 0;
    else if(comparisonOperator == LT_OP)
        return strcmp(recordValue,compareValue) < 0;
    else if(comparisonOperator == LE_OP)
        return strcmp(recordValue,compareValue) <= 0;
    else if(comparisonOperator == GT_OP)
        return strcmp(recordValue,compareValue) > 0;
    else if(comparisonOperator == GE_OP)
        return strcmp(recordValue,compareValue) >= 0;
    else if(comparisonOperator == NE_OP)
        return strcmp(recordValue,compareValue) != 0;
}




void getAttributeValueBlock(const void* data,void* value,int ptr,const std::vector<Attribute>& recordDescriptor, int blockOffset)
{
    int nullInfo = ceil((double) recordDescriptor.size() / 8);
    bool checkNull = false;

    char* nullFields = (char*)malloc(nullInfo);
    memcpy(nullFields,(char*)data,nullInfo);

    int offset = blockOffset;
    offset += nullInfo;

    for(int i=0; i < ptr;i++)
    {
        checkNull = nullFields[i/8] & (1 << (7 - i % 8));
        if(!checkNull)
        {
            switch (recordDescriptor[i].type) {
                case TypeInt:
                case TypeReal:
                    offset += recordDescriptor[i].length;
                    break;
                case TypeVarChar:
                    int temp = 0;
                    memcpy((char*)&temp, (char *) data + offset, sizeof(int));
                    offset += temp + sizeof(int);
                    break;
            }
        }
    }

    checkNull = nullFields[ptr/8] & (1 << (7 - ptr % 8));

    if(checkNull)
    {
        //the attribute required is null
//        std::cout<<"Handle null in getAttribute"<<std::endl;
    }

    if(recordDescriptor[ptr].type == TypeReal || recordDescriptor[ptr].type == TypeInt)
    {
        int temp = 0;
        memcpy((char*)value,(char*)data + offset, sizeof(int));

        memcpy(&temp,(char*)data + offset, sizeof(int));

    }
    else if(recordDescriptor[ptr].type == TypeVarChar)
    {
        int length;
        memcpy((char*)&length, (char*)data + offset, sizeof(int));
        memcpy((char*)value,(char*)data + offset,length + sizeof(int));
    }
    free(nullFields);
    return;
}

void getAttributeValue(const void* data,void* value,int ptr,const std::vector<Attribute>& recordDescriptor)
{
    int nullInfo = ceil((double) recordDescriptor.size() / 8);
    bool checkNull = false;

    char* nullFields = (char*)malloc(nullInfo);
    memcpy(nullFields,(char*)data,nullInfo);

    int offset = 0;
    offset += nullInfo;

    for(int i=0; i < ptr;i++)
    {
        checkNull = nullFields[i/8] & (1 << (7 - i % 8));
        if(!checkNull)
        {
            switch (recordDescriptor[i].type) {
                case TypeInt:
                case TypeReal:
                    offset += recordDescriptor[i].length;
                    break;
                case TypeVarChar:
                    int temp = 0;
                    memcpy((char*)&temp, (char *) data + offset, sizeof(int));
                    offset += temp + sizeof(int);
                    break;
            }
        }
    }

    checkNull = nullFields[ptr/8] & (1 << (7 - ptr % 8));

    if(checkNull)
    {
        //the attribute required is null
//        std::cout<<"Handle null in getAttribute"<<std::endl;
    }

    if(recordDescriptor[ptr].type == TypeReal || recordDescriptor[ptr].type == TypeInt)
    {
        memcpy((char*)value,(char*)data + offset, sizeof(int));
    }
    else if(recordDescriptor[ptr].type == TypeVarChar)
    {
        int length;
        memcpy((char*)&length, (char*)data + offset, sizeof(int));
        memcpy((char*)value,(char*)data + offset,length + sizeof(int));
    }
    free(nullFields);
    return;
}

int getLengthOfRecord(const void* data,std::vector<Attribute>& recordDescriptor)
{
    bool checknull;
    uint32_t RecSize = 0;
    uint32_t nullinfosize = ceil((double) recordDescriptor.size() / 8);
    u_int32_t bytesforNullIndic = ceil((double) recordDescriptor.size() / CHAR_BIT);

    RecSize += bytesforNullIndic;

    unsigned char *null_Fields_Indicator = (unsigned char *) malloc(nullinfosize);
    memcpy(null_Fields_Indicator, (char *) data, nullinfosize);

    for (int i = 0; i < recordDescriptor.size(); i++) {
        checknull = null_Fields_Indicator[i / 8] & (1 << (7 - i % 8));
        if (!checknull) {
            switch (recordDescriptor[i].type) {
                case TypeInt:
                case TypeReal:
                    RecSize += recordDescriptor[i].length;
                    break;
                case TypeVarChar:
                    int temp = 0;
                    memcpy((char*)&temp, (char *) data + RecSize, sizeof(int));
                    RecSize += temp + sizeof(int);
                    break;
            }
        }
    }
    free(null_Fields_Indicator);
    return RecSize;
}

Aggregate::Aggregate(Iterator* input, const Attribute &aggAttr,AggregateOp op)
{
    this->input = input;
    this->attribute = aggAttr;
    this->op = op;
    this->checkFlag = false;
    this->groupFlag = false;
}

Aggregate::Aggregate(Iterator *input,const Attribute &aggAttr,const Attribute &groupAttr,AggregateOp op)
{
    this->input = input;
    this->attribute = aggAttr;
    this->op = op;
    this->groupAttr = groupAttr;
    this->groupFlag = true;
    this->checkFlag = false;
}

void Aggregate::getAttributes(std::vector<Attribute> &attrs) const
{
    this->input->getAttributes(attrs);
}

RC Aggregate::getNextTuple(void *data) {

    std::vector<Attribute> recordDescriptor;
    Aggregate::getAttributes(recordDescriptor);

    if(this->groupFlag)
    {
        bool attrCheck = false,attrCheck1 = false;
        int ptr1,ptr2;
        for(int i = 0;i < recordDescriptor.size();i++)
        {
            if(recordDescriptor[i].name == this->attribute.name)
            {
                attrCheck = true;
                ptr1 = i;
            }
            if(recordDescriptor[i].name == this->groupAttr.name)
            {
                attrCheck1 = true;
                ptr2 = i;
            }
        }

        if(!attrCheck && !attrCheck1)
            return -1;

        int x = 0;
        float minVar = FLT_MAX;float max = FLT_MIN; float sum = 0;
        float count = 0;
        void* record = malloc(PAGE_SIZE);
        while(this->input->getNextTuple(record) != QE_EOF)
        {
            this->checkFlag = true;
            void* temp1 = malloc(PAGE_SIZE);
            getAttributeValue(record,temp1,ptr1,recordDescriptor);

            void* temp2 = malloc(PAGE_SIZE);
            getAttributeValue(record,temp2,ptr2,recordDescriptor);
            float currValue;

            if(recordDescriptor[ptr1].type == TypeInt)
            {
                int holder;
                memcpy((char*)&holder,(char*)temp2, sizeof(int));
                currValue = (float)holder;
                sum += currValue;
                count++;
                if(minVar > currValue)
                    minVar = currValue;
                if(max < currValue)
                    max = currValue;
            }
            else if(recordDescriptor[ptr1].type == TypeReal)
            {
                memcpy((char*)&currValue,(char*)temp2, sizeof(int));
                sum += currValue;
                count++;
                if(minVar > currValue)
                    minVar = currValue;
                if(max < currValue)
                    max = currValue;
            }

            if(this->groupAttr.type == TypeInt)
            {
                int temp;
                memcpy((char*)&temp,(char*)temp2, sizeof(int));

                if(this->intMap.find(temp) == this->intMap.end()) {
                    std::vector<float> tempVector = {{currValue,currValue,1,currValue,currValue}};
                    this->intMap[temp] = tempVector;
                } else {
                    minVar = this->intMap[temp][0];
                    max = this->intMap[temp][1];
                    count = this->intMap[temp][2];
                    sum = this->intMap[temp][3];

                    if(minVar > currValue)
                        minVar = currValue;
                    if(max < currValue)
                        max = currValue;
                    count++;
                    sum+= currValue;

                    std::vector<float> tempVector = {{minVar,max,count,sum,currValue}};
                    this->intMap[temp] = tempVector;
                }
            }
            else if(this->groupAttr.type == TypeReal)
            {
                float temp;
                memcpy((char*)&temp,(char*)temp2 + sizeof(char), sizeof(int));

                if(this->floatMap.find(temp) == this->floatMap.end()) {
                    std::vector<float> tempVector = {{currValue,currValue,1,currValue,currValue}};
                    this->floatMap[temp] = tempVector;
                } else {
                    minVar = this->floatMap[temp][0];
                    max = this->floatMap[temp][1];
                    count = this->floatMap[temp][2];
                    sum = this->floatMap[temp][3];

                    if(minVar > currValue)
                        minVar = currValue;
                    if(max < currValue)
                        max = currValue;
                    count++;
                    sum+= currValue;

                    std::vector<float> tempVector = {{minVar,max,count,sum,currValue}};
                    this->floatMap[temp] = tempVector;
                }
            }
            else if(this->groupAttr.type == TypeVarChar)
            {
                int length;
                memcpy((char*)&length,(char*)temp2 + sizeof(char), sizeof(int));
                std::string temp;
                memcpy((char*)&temp,(char*)temp2 + sizeof(char),length + sizeof(int));

                if(this->strMap.find(temp) == this->strMap.end()) {
                    std::vector<float> tempVector = {{currValue,currValue,1,currValue,currValue}};
                    this->strMap[temp] = tempVector;
                } else {
                    minVar = this->strMap[temp][0];
                    max = this->strMap[temp][1];
                    count = this->strMap[temp][2];
                    sum = this->strMap[temp][3];

                    if(minVar > currValue)
                        minVar = currValue;
                    if(max < currValue)
                        max = currValue;
                    count++;
                    sum+= currValue;

                    std::vector<float> tempVector = {{minVar,max,count,sum,currValue}};
                    this->strMap[temp] = tempVector;
                }
            }
        }

        free(record);

        if(!this->checkFlag)
            return QE_EOF;

        if(this->groupAttr.type == TypeInt)
        {
            if(this->intMap.size() == 0)
                return QE_EOF;

            auto itr = this->intMap.begin();
            memcpy((char*)data,(char*)&x, sizeof(char));
            memcpy((char*)data + sizeof(char),(char*)&itr->first, sizeof(int));

            if(this->op == MIN){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[0], sizeof(int));
            } else if (this->op == MAX){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[1], sizeof(int));
            }else if(this->op == COUNT) {
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[2], sizeof(int));
            }else if(this->op == SUM){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[3], sizeof(int));
            }else if(this->op == AVG){
                float avg = itr->second[3] / itr->second[2];
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&avg, sizeof(int));
            }

            this->intMap.erase(itr->first);
            return 0;
        }
        else if(this->groupAttr.type == TypeReal)
        {
            if(this->floatMap.size() == 0)
                return QE_EOF;

            auto itr = this->floatMap.begin();
            memcpy((char*)data,(char*)&x, sizeof(char));
            memcpy((char*)data + sizeof(char),(char*)&itr->first, sizeof(int));

            if(this->op == MIN){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[0], sizeof(int));
            } else if (this->op == MAX){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[1], sizeof(int));
            }else if(this->op == COUNT) {
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[2], sizeof(int));
            }else if(this->op == SUM){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[3], sizeof(int));
            }else if(this->op == AVG){
                float avg = itr->second[3] / itr->second[2];
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&avg, sizeof(int));
            }

            this->floatMap.erase(itr->first);
            return 0;
        }
        else if(this->groupAttr.type == TypeVarChar)
        {
            if(this->strMap.size() == 0)
                return QE_EOF;

            auto itr = this->strMap.begin();
            memcpy((char*)data,(char*)&x, sizeof(char));
            int length = itr->first.length();
            memcpy((char*)data + sizeof(char),(char*)&length, sizeof(int));
            memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->first, length);

            if(this->op == MIN){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[0], sizeof(int));
            } else if (this->op == MAX){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[1], sizeof(int));
            }else if(this->op == COUNT) {
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[2], sizeof(int));
            }else if(this->op == SUM){
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&itr->second[3], sizeof(int));
            }else if(this->op == AVG){
                float avg = itr->second[3] / itr->second[2];
                memcpy((char*)data + sizeof(char) + sizeof(int),(char*)&avg, sizeof(int));
            }
            this->strMap.erase(itr->first);
            return 0;
        }
        return 0;
    }
    else {
        bool attrCheck = false;
        int ptr=-1;
        for(int i = 0;i < recordDescriptor.size();i++)
        {
            if(recordDescriptor[i].name == this->attribute.name)
            {
                attrCheck = true;
                ptr = i;
                break;
            }
        }

        if(!attrCheck)
            return -1;

        float sum = 0 ; float min = MAXFLOAT , max = FLT_MIN;
        int x = 0; float count = 0;
        void* record = malloc(PAGE_SIZE);
        while(this->input->getNextTuple(record) != QE_EOF)
        {
            this->checkFlag = true;
            void* temp = malloc(PAGE_SIZE);
            getAttributeValue(record,temp,ptr,recordDescriptor);

            if(recordDescriptor[ptr].type == TypeInt)
            {
                int currValue;
                memcpy((char*)&currValue,(char*)temp, sizeof(int));
                sum += currValue;
                count++;
                if(min > currValue)
                    min = currValue;
                if(max < currValue)
                    max = currValue;
            }
            else if(recordDescriptor[ptr].type == TypeReal)
            {
                float currValue;
                memcpy((char*)&currValue,(char*)temp, sizeof(int));
                sum += currValue;
                count++;
                if(min > currValue)
                    min = currValue;
                if(max < currValue)
                    max = currValue;
            }
        }

        free(record);
        if(!this->checkFlag)
            return QE_EOF;

        float avg = sum / count;

        memcpy((char*)data,(char*)&x, sizeof(char));

        if(this->op == MIN)
        {
            memcpy((char*)data + sizeof(char),(char*)&min, sizeof(int));
        }
        else if(this->op == MAX) {
            memcpy((char*)data + sizeof(char),(char*)&max, sizeof(int));
        }
        else if(this->op == SUM) {
            memcpy((char*)data + sizeof(char),(char*)&sum, sizeof(int));
        }
        else if(this->op == COUNT){
            memcpy((char*)data + sizeof(char),(char*)&count, sizeof(int));
        }
        else if(this->op == AVG){
            memcpy((char*)data + sizeof(char),(char*)&avg, sizeof(int));
        }

        this->checkFlag = false;
        return 0;
    }

}

int checkIfValidAttribute(std::string attrName,const std::vector<Attribute>& recordDescriptor)
{
    int check = -1;
    for(int i = 0; i<recordDescriptor.size();i++)
    {
        if(recordDescriptor[i].name == attrName)
        {
            check = i;
            break;
        }
    }
    return check;
}


INLJoin::INLJoin(Iterator *leftIn,IndexScan *rightIn,const Condition &condition){
    this->leftIn = leftIn;
    this->rightIn = rightIn;
    this->condition = condition;
    this->leftOver = true;
    this->innerLeftOver = false;
}

void INLJoin::getAttributes(std::vector<Attribute> &attrs) const {

    std::vector<Attribute> temp;
    this->leftIn->getAttributes(temp);
    for(Attribute attr: temp)
    {
        attrs.push_back(attr);
    }

    this->rightIn->getAttributes(temp);
    for(Attribute attr: temp)
    {
        attrs.push_back(attr);
    }
}
RC BNLJoin::getNextTuple(void *data){
    std::vector<Attribute> recordDescriptorLeft;
    std::vector<Attribute> recordDescriptorRight;
    this->leftIn->getAttributes(recordDescriptorLeft);
    this->rightIn->getAttributes(recordDescriptorRight);

    int nullInfo1 = ceil((double) recordDescriptorLeft.size() / CHAR_BIT);
    std::vector<Attribute> recordDescriptor;
    BNLJoin::getAttributes(recordDescriptor);
    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);


    int ptr1 = checkIfValidAttribute(condition.lhsAttr,recordDescriptorLeft);
    int ptr2 = checkIfValidAttribute(condition.rhsAttr,recordDescriptorRight);
    if( ptr1 == -1 || ptr2 == -1)
        return -1;

    void *leftrec = malloc(PAGE_SIZE);

    while(this -> leftIn -> getNextTuple(leftrec) != QE_EOF){
        std::cout<<"inside while "<<std::endl;
        int count = 0;
        void* leftBlock = malloc(PAGE_SIZE*numPages);

        //get the left block
        std::vector<int> recordSizeInBlock;
        int recSize = getLengthOfRecord(leftrec, recordDescriptorLeft);
       // std::cout<<"rec size is "<<recSize<<std::endl;

        memcpy((char*) leftBlock + count, leftrec, recSize);
        count += recSize;
        recordSizeInBlock.push_back(recSize);

        //get the left block
        while(count <= numPages*PAGE_SIZE){
            if(this -> leftIn -> getNextTuple(leftrec) != QE_EOF) {
                int recSize = getLengthOfRecord(leftrec, recordDescriptorLeft);
                memcpy((char *) leftBlock + count, leftrec, recSize);
                count += recSize;
                //     std::cout<<"rec size in loop is "<<recSize<<std::endl;
                recordSizeInBlock.push_back(recSize);
            } else
                break;
        }

        free(leftrec);
        //std::cout<<"left block stored "<<std::endl;
        //the left block of size numPages*PAGE_SIZE done

        if(recordDescriptorLeft[ptr1].type == TypeInt) {

            if(!this->innerLeftOver) {
                std::cout<<"ghjkl"<<std::endl;
                this->rightIn->setIterator();
            }

            void* currRecord = malloc(PAGE_SIZE);
            while(this->rightIn->getNextTuple(currRecord) != QE_EOF) {

                //compare x2 with records in x1
                int currcount = 0;
                int i = 0;
                while (i < recordSizeInBlock.size()) {
                    void *intValue = malloc(sizeof(int));
                    void *record = malloc(recordSizeInBlock[i]);
                    memcpy((char*)record, (char*) leftBlock + currcount, recordSizeInBlock[i]);
                    getAttributeValue(record, intValue, ptr1, recordDescriptorLeft);

                    int length2 = getLengthOfRecord(currRecord, recordDescriptorRight);
                    int nullInfo2 = ceil((double) recordDescriptorRight.size() / CHAR_BIT);

                    void *newIntValue = malloc(sizeof(int));
                    getAttributeValue(currRecord,newIntValue,ptr2,recordDescriptorRight);

                    int x1, x2;
                    memcpy((char *) &x1, (char *) intValue, sizeof(int));
                    memcpy((char *) &x2, (char *) newIntValue, sizeof(int));
            //   std::cout<<x1<<" , "<<x2<<std::endl;

                    if (x1 == x2) {
                        void *finalBit = malloc(nullInfo);

                        combineNullBits(record, currRecord, nullInfo1, nullInfo2, finalBit, recordDescriptorLeft.size(),
                                        recordDescriptorRight.size(), nullInfo);
                        memcpy((char*)data, (char*)finalBit, nullInfo);
                        memcpy((char *) data + nullInfo, (char *) record + nullInfo1, recordSizeInBlock[i] - nullInfo1);

                        memcpy((char *) data + nullInfo + recordSizeInBlock[i] - nullInfo1, (char *) currRecord + nullInfo2,
                               length2 - nullInfo2);
                        free(newIntValue);
                        free(intValue);
                        free(currRecord);
                        this->innerLeftOver = true;
                        free(record);
                        return 0;

                    }
                    currcount += recordSizeInBlock[i];
                    i++;
                    free(record);
                }
               // this->entriesLeft = false;
               // this->innerLeftOver = false;
                free(currRecord);

            }
            this->innerLeftOver = false;
            continue;

        }
       else  if(recordDescriptorLeft[ptr1].type == TypeReal) {

            if(!this->innerLeftOver) {
                this->rightIn->setIterator();
            }

            void* currRecord = malloc(PAGE_SIZE);
            while(this->rightIn->getNextTuple(currRecord) != QE_EOF) {

                //compare x2 with records in x1
                int currcount = 0;
                int i = 0;
                while (currcount <= numPages * PAGE_SIZE) {
                    void *floatValue = malloc(sizeof(int));
                    void *record = malloc(recordSizeInBlock[i]);
                    memcpy((char*) leftBlock + currcount, (char*)record, recordSizeInBlock[i]);

                    getAttributeValue(record, floatValue, ptr1, recordDescriptorLeft);
                    int length2 = getLengthOfRecord(currRecord, recordDescriptorRight);
                    int nullInfo2 = ceil((double) recordDescriptorRight.size() / CHAR_BIT);

                    void *newFloatValue = malloc(sizeof(int));

                    float x1, x2;
                    memcpy((char *) &x1, (char *) floatValue, sizeof(int));
                    memcpy((char *) &x2, (char *) newFloatValue, sizeof(int));
                //     std::cout<<x1<<" , "<<x2<<std::endl;

                    if (x1 == x2) {
                        void *finalBit = malloc(nullInfo);

                        combineNullBits(record, currRecord, nullInfo1, nullInfo2, finalBit, recordDescriptorLeft.size(),
                                        recordDescriptorRight.size(), nullInfo);
                        memcpy((char*)data, (char*)finalBit, nullInfo);
                        memcpy((char *) data + nullInfo, (char *) record + nullInfo1, recordSizeInBlock[i] - nullInfo1);
                        memcpy((char *) data + nullInfo + recordSizeInBlock[i] - nullInfo1, (char *) currRecord + nullInfo2,
                               length2 - nullInfo2);

                        this->innerLeftOver = true;
                        free(newFloatValue);
                        free(floatValue);
                        free(currRecord);
                        free(record);
                        return 0;

                    }
                    currcount += recordSizeInBlock[i];
                    i++;
                    free(record);
                }
                free(currRecord);


            }
            this->innerLeftOver = false;
            continue;


        }
       free(leftBlock);
    }




}
RC INLJoin::getNextTuple(void *data) {

    std::vector<Attribute> recordDescriptor1;
    std::vector<Attribute> recordDescriptor2;

    std::vector<Attribute> recordDescriptor;
    INLJoin::getAttributes(recordDescriptor);

    int nullInfo = ceil((double) recordDescriptor.size() / CHAR_BIT);

    this->leftIn->getAttributes(recordDescriptor1);
    this->rightIn->getAttributes(recordDescriptor2);

    int nullInfo1 = ceil((double) recordDescriptor1.size() / CHAR_BIT);

    int ptr1 = checkIfValidAttribute(condition.lhsAttr,recordDescriptor1);
    int ptr2 = checkIfValidAttribute(condition.rhsAttr,recordDescriptor2);

    if( ptr1 == -1 || ptr2 == -1)
        return -1;

    void* record = malloc(PAGE_SIZE);
    while(this->leftOver && this->leftIn->getNextTuple(record) != QE_EOF)
    {


        int length1 = getLengthOfRecord(record,recordDescriptor1);

        if(recordDescriptor1[ptr1].type == TypeInt)
        {
            void* intValue = malloc(sizeof(int));
            getAttributeValue(record,intValue,ptr1,recordDescriptor1);

            if(!this->innerLeftOver)
                this->rightIn->setIterator(nullptr, nullptr,false,false);

            void* currRecord = malloc(PAGE_SIZE);
            while(this->rightIn->getNextTuple(currRecord) != QE_EOF)
            {
                int length2 = getLengthOfRecord(currRecord,recordDescriptor2);
                int nullInfo2 = ceil((double) recordDescriptor2.size() / CHAR_BIT);

                void* newIntValue = malloc(sizeof(int));
                getAttributeValue(currRecord,newIntValue,ptr2,recordDescriptor2);

                int x1 , x2;
                memcpy((char*)&x1,(char*)intValue, sizeof(int));
                memcpy((char*)&x2,(char*)newIntValue,sizeof(int));
//                std::cout<<x1<<" , "<<x2<<std::endl;

                if(x1 == x2)
                {
                    void* finalBit = malloc(nullInfo);
                    combineNullBits(record,currRecord,nullInfo1,nullInfo2,finalBit,recordDescriptor1.size(),recordDescriptor2.size(),nullInfo);
                    memcpy((char*)data + nullInfo,(char*)record + nullInfo1,length1 - nullInfo1);
                    memcpy((char*)data + nullInfo + length1 - nullInfo1,(char*)currRecord + nullInfo2,length2 - nullInfo2);
                    this->leftOver = true;
                    this->innerLeftOver = true;
                    free(newIntValue);
                    free(intValue);
                    free(currRecord);
                    free(record);
                    return 0;
                }
            }
            this->leftOver = false;
            this->innerLeftOver = false;
            free(currRecord);
            free(intValue);
        }
        if(recordDescriptor1[ptr1].type == TypeReal)
        {
            void* realValue = malloc(sizeof(int));
            getAttributeValue(record,realValue,ptr1,recordDescriptor1);

            if(!this->innerLeftOver)
                this->rightIn->setIterator(nullptr, nullptr, false,false);

            void* currRecord = malloc(PAGE_SIZE);
            while(this->rightIn->getNextTuple(currRecord) != QE_EOF)
            {
                int length2 = getLengthOfRecord(currRecord,recordDescriptor2);
                int nullInfo2 = ceil((double) recordDescriptor2.size() / CHAR_BIT);

                void* newRealValue = malloc(sizeof(int));
                getAttributeValue(currRecord,newRealValue,ptr2,recordDescriptor2);

                float x1 , x2;
                memcpy((char*)&x1,(char*)realValue, sizeof(int));
                memcpy((char*)&x2,(char*)newRealValue,sizeof(int));
//                std::cout<<x1<<" , "<<x2<<std::endl;

                if(x1 == x2)
                {
                    void* finalBit = malloc(nullInfo);
                    combineNullBits(record,currRecord,nullInfo1,nullInfo2,finalBit,recordDescriptor1.size(),recordDescriptor2.size(),nullInfo);
                    memcpy((char*)data,(char*)finalBit,nullInfo);
                    memcpy((char*)data + nullInfo,(char*)record + nullInfo1,length1 - nullInfo1);
                    memcpy((char*)data + nullInfo + length1 - nullInfo1,(char*)currRecord + nullInfo2,length2 - nullInfo2);
                    this->leftOver = true;
                    this->innerLeftOver = true;
                    free(newRealValue);
                    free(currRecord);
                    free(record);
                    return 0;
                }
                free(newRealValue);
            }
            this->leftOver = false;
            this->innerLeftOver = false;
            free(realValue);
            free(currRecord);
        }
    }
    free(record);
    return QE_EOF;
}


void combineNullBits(const void* record,const void* currRecord,int nullInfo1,int nullInfo2,void* finalBit,int x,int y,int nullInfo)
{
    char* bits = new char[nullInfo];
    memset(bits,0,nullInfo);

    char* nullBit1 = new char[nullInfo1];
    char* nullBit2 = new char[nullInfo2];

    memcpy((char*)nullBit1,(char*)record,nullInfo1);
    memcpy((char*)nullBit2,(char*)currRecord,nullInfo2);

    bool checkNull;
    for(int i=0;i<x;i++)
    {
        checkNull = nullBit1[i/8] & (1 << (7 - i % 8));
        if(checkNull)
        {
            int bitShift = nullInfo - 1 - i%nullInfo;
            bits[i/CHAR_BIT] = bits[i / CHAR_BIT] | (1 << (bitShift));
        }
    }
    for(int i=x;i<x + y;i++)
    {
        checkNull = nullBit2[i/8] & (1 << (7 - i % 8));
        if(checkNull)
        {
            int bitShift = nullInfo - 1 - i%nullInfo;
            bits[i/CHAR_BIT] = bits[i / CHAR_BIT] | (1 << (bitShift));
        }
    }
    memcpy((char*)finalBit,(char*)bits,nullInfo);
    free(nullBit1);
    free(nullBit2);
    free(bits);
    return;


}


BNLJoin::BNLJoin(Iterator *leftIn,            // Iterator of input R
                 TableScan *rightIn,           // TableScan Iterator of input S
                 const Condition &condition,   // Join condition
                 const unsigned numPages       // # of pages that can be loaded into memory,
//   i.e., memory block size (decided by the optimizer)
)
{
    this->leftIn = leftIn;
    this->rightIn = rightIn;
    this->condition = condition;
    this -> numPages = numPages;

}
void BNLJoin::getAttributes(std::vector<Attribute> &attrs) const {

    std::vector<Attribute> temp;
    this->leftIn->getAttributes(temp);
    for(Attribute attr: temp)
    {
        attrs.push_back(attr);
    }

    this->rightIn->getAttributes(temp);
    for(Attribute attr: temp)
    {
        attrs.push_back(attr);
    }
}

void BNLJoin::getStrings(std::string &leftString, std::string &rightString){

    std::string temp = condition.lhsAttr;
    std::cout<<"condition lhs attr: "<<temp<<std::endl;
    int loc;
    loc = temp.find(".");
    leftString = temp.substr(loc+1);

    std::cout<<"left key name "<<leftString<<std::endl;
    temp = condition.rhsAttr;

    std::cout<<"condition rhs attr: "<<temp<<std::endl;
    loc = temp.find(".");
    rightString = temp.substr(loc+1);
    std::cout<<"right key name "<<rightString<<std::endl;

}

int getLength(std::vector<Attribute> attributes){
    int recSize = 0;
    int offset = ceil((double) attributes.size() / CHAR_BIT);
    recSize += offset;
    for(int i = 0; i < attributes.size(); i++){
        if(attributes[i].type == TypeInt || attributes[i].type == TypeReal){
            recSize += attributes[i].length;
        }
        else{
            recSize += (sizeof(int) + attributes[i].length);
        }
        // std::cout<<"rec size "<<recSize<<std::endl;
    }
    return recSize;
}


