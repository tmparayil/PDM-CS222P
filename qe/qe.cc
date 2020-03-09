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
            // Comparison between two attributes section
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
    this->input->getAttributes(attrs);
}

RC Project::getNextTuple(void *data) {

    std::vector<Attribute> recordDescriptor;
    Project::getAttributes(recordDescriptor);

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
        std::cout<<"Handle null in getAttribute"<<std::endl;
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
        float minVar = MAXFLOAT;float max = FLT_MIN; float sum = 0;
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