## 1. Basic information
- Team #: 7
- Github Repo Link: https://github.com/UCI-Chenli-teaching/cs222p-winter20-team-7.git
- Student 1 UCI NetID: tparayil
- Student 1 Name: Thomas Parayil
- Student 2 UCI NetID :  shahap1
- Student 2 Name : Aneri Shah


## 2. Catalog information about Index
##### Show your catalog information about an index (tables, columns).

We have a file named 'Indices' that is used to keep track of all the index files that have been created. The Indices file is created in the createCatalog function and deleted in the deleteCatalog function. We have the table-id, column-name and the corresponding file-name stored as part of the records in the Indices file. 


## 3. Block Nested Loop Join (If you have implemented this feature)
##### Describe how your block nested loop join works (especially, how you manage the given buffers.)


In Block Nested Loop join, we load the left block with the size number of pages * PAGE_SIZE. After that we look for a match in the right data and if there's a match we push that to a queue storing void* elements and the size of the record in another queue of integers. We obtain the results that fit in a block of memory and store it in our queue. At the start of each call of getNextTuple for BNL join, we check if the queue is empty or not. If it is empty, we continue with the next left block, else we return the front of queue and pop it after returning.If there are no left or right entries, we return QE_EOF. The resultant data is a combination of the null bits of the left tuple and the right tuple followed by the left and right tuple's data. The getAttributes function returns the left recordDescriptor followed by the right recordDescriptor that describes the data of getNextTuple.


## 4. Index Nested Loop Join (If you have implemented this feature)
##### Describe how your grace hash join works.

For the INL join, we mantain two flags - leftOver and innerLeftOver that tracks if there are any elements that are still to be compared and if the right iterator reached the EOF in the previous run or not. We compare the left tuples with the right tuples to obtain a match. When we get a match, we change the values of the corresponding flags and return the resultant data. If there are no left or right entries left to be compared and we haven't found a match, we return QE_EOF. The resultant data is a combination of the null bits of the left tuple and the right tuple followed by the left and right tuple's data.The getAttributes function returns the left recordDescriptor followed by the right recordDescriptor that describes the data of getNextTuple.



## 5. Grace Hash Join (If you have implemented this feature)
##### Describe how your grace hash join works (especially, in-memory structure).
Not implemented


## 6. Aggregation
##### Describe how your aggregation (basic, group-based hash) works.
We read the records for the input iterator passed for the Aggregate and get the corresponding value sto compute the count, max, min, avg, sum for the attribute.We continue this until we have next tuples left to be read for the iterator. We maintain a checkflag to denote if we have a tuple to return and if the checkflag is set, we return QE_EOF. We have implemented the groupby extension for the Aggregate function as explained below.

Groupby extension:
For groupby, we group by attribute and maintain an in memory hash table i.e hashmap to store the corresponding results.  The value obtained is the key and the computed values are inserted as vector for the key. Depending on the type of key, we add the result to intMap(typeInt), floatMap(typeReal), strMap(typeVarChar)
After doing thus, we check the size of the hashmap and return the element at the front and erase it after returning. If the size of the hashmap is 0, we return QE_EOF.





## 7. Implementation Detail
##### Have you added your own source file (.cc or .h)?
No

##### Have you implemented any optional features? Then, describe them here.

We have implemented groupby for the aggregation function as explained above.



##### Other implementation details:

We add the indices into the corresponding index files for each attribute of a table. We thus maintain the catalogs for the relation manager through the Tables and Columns and the Index manager through the Indices file. The Index manager is tied to the relation manager and we are able to query the resulting interface to obtain the required information. While calling the insertTuple for the relation manager, we add the corresponding record to the index file and make the required deletions when we delete a tuple to maintain consistency. Other functions and their implementations are as explained in the above questions.

## 6. Other (optional)
##### Freely use this section to tell us about things that are related to the project 4, but not related to the other sections (optional)
