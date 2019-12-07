#include <stdlib.h>
#include "dberror.h"
#include "expr.h"
#include "buffer_mgr.h"
#include "record_mgr.h"
#include "storage_mgr.h"
#include "tables.h"
#include "test_helper.h"

#define ASSERT_EQUALS_RECORDS(_l,_r, schema, message)			\
  do {									\
    Record *_lR = _l;   						 \
    Record *_rR = _r;                                                    \
    ASSERT_TRUE(memcmp(_lR->data,_rR->data,getRecordSize(schema)) == 0, message); \
    int i;							\
    for(i = 0; i < schema->numAttr; i++)				\
      {										\
        Value *lVal, *rVal;          	                                   \
		char *lSer, *rSer; 					\
        getAttr(_lR, schema, i, &lVal); 	                                 \
        getAttr(_rR, schema, i, &rVal);                                  \
		lSer = serializeValue(lVal); \
		rSer = serializeValue(rVal); \
        ASSERT_EQUALS_STRING(lSer, rSer, "attr same");	\
		free(lVal); \
		free(rVal); \
		free(lSer); \
		free(rSer); \
      }									\
  } while(0)

// test methods
static void testMenuBasedUserInterface(void);

// struct for test records
typedef struct TestRecord {
	int a;
	char *b;
	int c;
} TestRecord;

// helper methods
Record *testRecord(Schema *schema, int a, char *b, int c);
Schema *testSchema(void);
Record *fromTestRecord(Schema *schema, TestRecord in);

// test name
char *testName;
char *userTableName;
// main method
int main(void) {
	testName = "";

	testMenuBasedUserInterface();

	return 0;
}

// ************************************************************ 
void testMenuBasedUserInterface(void) {

	printf("-----------------------------\n");
	printf("        WELCOME                     \n");
	printf("------------------------------\n");
	testName = "test menu-based user interface";
	int option = 0;
	do {
		printf("Please select a number from the below options...\n");
		printf("1. Create a new table \n2. Insert records into the table \n3. Update a record in the table \n4. Delete the table \n5. Exit\n");
		scanf("%d", &option);
		
		RM_TableData *table = (RM_TableData *) malloc(sizeof(RM_TableData));
		BM_BufferPool *bm;
		Schema *schema; 
		Record *record;
		RID *recordRids;
		
		TEST_CHECK(initRecordManager(NULL));
		
		switch (option) {
    		case 1: {
    			printf("Enter the table name \n");
    			char *table_name = malloc(20);
    			scanf("%s", &(*table_name));
    			schema = testSchema();
    			userTableName = table_name;
    			TEST_CHECK(createTable(userTableName, schema));
    			TEST_CHECK(openTable(table, userTableName));
    			TEST_CHECK(closeTable(table));
    			
    			bm = (BM_BufferPool *) table->mgmtData;
    			userTableName = table_name;
    			printf("Table %s created\n", userTableName);
			}
			TEST_DONE();
			break;
			
    		case 2: {
    			int numInserts;
    			
    			printf("Enter the number of records to insert \n");
    			scanf("%d", &numInserts);
    			
    			TEST_CHECK(openTable(table, userTableName));
    			
    			TestRecord inserts[numInserts];
    			table->mgmtData = (BM_BufferPool *)bm;
    			for (int counter = 0; counter < numInserts; counter++) {
    				printf("Enter data for Row [%d], Column 1 \n", counter);
    				scanf("%d", &inserts[counter].a);
    				
    				inserts[counter].b = malloc(10);
    				char *charData = malloc(10);
    				printf("Enter data for Row [%d], Column 2 \n", counter);
    				scanf("%s", &(*charData));
    				strcpy(inserts[counter].b, charData);
    				
    				printf("Enter data for Row [%d], Column 3 \n", counter);
    				scanf("%d", &inserts[counter].c);
    			}
    
    			Record *r;
    			RID *rids;
    			rids = (RID *) malloc(sizeof(RID) * numInserts);
    			// insert rows into table
    			for (int counter = 0; counter < numInserts; counter++) {
    				r = fromTestRecord(schema, inserts[counter]);
    				TEST_CHECK(insertRecord(table, r));
    				rids[counter] = r->id;
    			}
    
    			// randomly retrieve records from the table and compare to inserted ones
    			for (int counter = 0; counter < numInserts*5; counter++) {
    				int pos = rand() % numInserts;
    				RID rid = rids[pos];
    				TEST_CHECK(getRecord(table, rid, r));
    
    				ASSERT_EQUALS_RECORDS(fromTestRecord(schema, inserts[pos]), r,schema, "compare records");
    			}
    			recordRids = rids; record = r;
    			
    			TEST_CHECK(closeTable(table));
    			
    			printf("%d record(s) inserted into the table %s \n", numInserts, userTableName);
    		}
    		TEST_DONE();
			break;
			
			case 3: {
    			int numUpdates;
    			
    			printf("Enter the number of records to update \n");
    			scanf("%d", &numUpdates);
    			
    			TEST_CHECK(openTable(table, userTableName));
    			
    			TestRecord updates[numUpdates];
    			table->mgmtData = (BM_BufferPool *)bm;
    			for (int counter = 0; counter < numUpdates; counter++) {
    			    printf("Enter data for Row [%d], Column 1 \n", counter);
    				scanf("%d", &updates[counter].a);
    				
    				updates[counter].b = malloc(10);
    				char *charData = malloc(10);
    				printf("Enter data for Row [%d], Column 2 \n", counter);
    				scanf("%s", &(*charData));
    				strcpy(updates[counter].b, charData);
    				
    				printf("Enter data for Row [%d], Column 3 \n", counter);
    				scanf("%d", &updates[counter].c);
    			}
    			
                for(int counter = 0; counter < numUpdates; counter++) {
                    record = fromTestRecord(schema, updates[counter]);
                    record->id = recordRids[counter];
                    TEST_CHECK(updateRecord(table, record)); 
                }
                
    			TEST_CHECK(closeTable(table));
    			
    			printf("%d record(s) updated in the table %s \n", numUpdates, userTableName);
    
    		}
            TEST_DONE();
			break;
			
    		case 4: {
    			TEST_CHECK(deleteTable(userTableName));
    			printf("Table %s deleted \n", userTableName);
    		}
			break;
    			
    		case 5: {
    		    printf("Exiting... !!\n");
    		    TEST_CHECK(shutdownRecordManager());
    			TEST_DONE();
    			exit(0);
    		}
			break;
    		
    		default:
		    	printf("Invalid option. Please try again\n");
    			exit(0);
		}
	} while (option < 6);
}

Schema *
testSchema(void) {
	printf("Enter the schema details. Default number of columns are 3.\n");
	Schema *result;

	char *names[3]; // = { "a", "b", "c" };
	DataType dt[3]; // = { DT_INT, DT_STRING, DT_INT };
	int type[3];
	for (int counter = 0; counter < 3; counter++) {
		names[counter] = malloc(20);
		printf("Enter Column Name \n");
		scanf("%s", &(*names[counter]));
		
		printf("Enter Column Data Type [0 = INT ; 1 = STRING ; 2= FLOAT ; 3 = BOOLEAN] \n");
		scanf("%d", &type[counter]);
		dt[counter] = type[counter];
	}

	int sizes[] = { 0, 4, 0 };
	int keys[] = { 0 };
	
	char **cpNames = (char **) malloc(sizeof(char*) * 3);
	DataType *cpDt = (DataType *) malloc(sizeof(DataType) * 3);
	int *cpSizes = (int *) malloc(sizeof(int) * 3);
	int *cpKeys = (int *) malloc(sizeof(int));

	for (int counter = 0; counter < 3; counter++) {
		cpNames[counter] = (char *) malloc(2);
		strcpy(cpNames[counter], names[counter]);
	}
	
	memcpy(cpDt, dt, sizeof(DataType) * 3);
	memcpy(cpSizes, sizes, sizeof(int) * 3);
	memcpy(cpKeys, keys, sizeof(int));

	result = createSchema(3, cpNames, cpDt, cpSizes, 1, cpKeys);

	return result;
}

Record *
fromTestRecord(Schema *schema, TestRecord in) {
	return testRecord(schema, in.a, in.b, in.c);
}

Record *
testRecord(Schema *schema, int a, char *b, int c) {
	Value *value;
	Record *result1;
	TEST_CHECK(createRecord(&result1, schema));
	MAKE_VALUE(value, DT_INT, a);
	TEST_CHECK(setAttr(result1, schema, 0, value));
	freeVal(value);
	MAKE_STRING_VALUE(value, b);
	TEST_CHECK(setAttr(result1, schema, 1, value));
	freeVal(value);
	MAKE_VALUE(value, DT_INT, c);
	TEST_CHECK(setAttr(result1, schema, 2, value));
	freeVal(value);

	return result1;
}