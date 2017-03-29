#define BUFFER_SIZE 512 // file reader buffer capacity
#define MAXSIZE 100 //stack capacity
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libxml/parser.h>
#include <unistd.h>
#include <json/json.h>

/* FUNCTION PROTOTYPES */
/* converts from csv */
void csvToXml();
void csvToJson();
/* converts from xml */
void xmlToCsv();
void xmlCsv(xmlNode *node, char* prefix, bool first_line);
void xmlToJson();
void xmlToJsonWrite(xmlNode *a_node, bool flag);
/* converts from json */
void jsonToXml();
void jsonToXmlWrite(json_object *j_obj, xmlNode *parent);
void jsonToCsv();
void jsonToCsvWrite(json_object *j_obj, bool first_line);
/* stack operations */
void push(json_object *j_obj);
json_object *pop(void),*peek(void);
/* misc functions */
int ftruncate(int fd, off_t length);
void removeCharacters(int length, FILE *file);
char *addString(char *s1, char *s2);

/* GLOBAL VARIABLES */
char input[50], output[50], format[5]; // -i -o -t vars
bool input_found, format_found, output_found, compress = false, converted = true;
FILE *csv_doc; // csv file to be read and write
xmlDoc *xml_doc; // xml file to be read and write
json_object *j_obj;
/* stack declaration */
struct stack {
    int stk[MAXSIZE];
    int top;
};
typedef struct stack STACK;
STACK s;

/* cli operations and function calls */
int main(int argc, char **argv){
    //Check for arguments, if not enough then terminate
    if(argc == 1)
    {
        printf("You haven't entered any arguments. Program will exit");
        return -1;
    }

    //Check arguments if they match
    int i;

    input_found = false;
    format_found = false;
    output_found = false;

    for(i=0; i<argc; i++){
        if(strcasecmp(argv[i],"-i") == 0){
            if(argc==(i+1) || (argv[i+1][0]=='-')){
                printf("You put the flag '-i' but did not enter a file path. Program will exit.");
                return -1;
            }
            else if(input_found==true){
                printf("You attempted to enter multiple inputs. Program will exit.");
                return -1;
            }
            else{
                strcpy(input, argv[i+1]);
                input_found = true;
            }
        }
        else if(strcasecmp(argv[i],"-o")==0 && (argv[i+1][0]!='-')){
            if(argc==i+1){
                printf("You put the flag '-o' but did not enter a file path. Program will exit.");
                return -1;
            }
            else if(output_found == true){
                printf("You attempted to enter multiple outputs. Program will exit.");
                return -1;
            }
            else{
                output_found = true;
                strcpy(output, argv[i+1]);
            }
        }
        else if(strcasecmp(argv[i],"-t")==0 && (argv[i+1][0]!='-')){
            if(argc==i+1){
                printf("You put the flag '-t' but did not enter a format. Program will exit.");
                return -1;
            }
            else if(format_found == true){
                printf("You attempted to enter multiple formats. Program will exit.");
                return -1;
            }
            else{
                format_found = true;
                strcpy(format, argv[i+1]);
            }
        }
        else if(strcasecmp(argv[i],"-true") == 0){
            compress = true;
        }
    }

    // Check if the user entered input and format at least.
    if(!(format_found && input_found)){
        printf("Either input or format is missing. Program will exit.");
        return -1;
    }

    // Not xml but compressed mode
    if(compress && strcasecmp(format,"xml")!=0){
        printf("Warning! Compress mode only works for xml conversions.\n");
    }
    // Get the input format from input directory
    int len = strlen(input);
    const char *last_four = &input[len-4];

    // Determine file formats

    //Convert from XML
    if(strcasecmp(last_four,".xml") == 0){
        if(strcasecmp(format,"csv") == 0){
            xmlToCsv();
        }
        else if(strcasecmp(format,"json") == 0){
            xmlToJson();
        }
    }
        //Convert from CSV
    else if(strcasecmp(last_four,".csv") == 0){
        if(strcasecmp(format,"xml") == 0){
            csvToXml();
        }
        else if(strcasecmp(format,"json") == 0){
            csvToJson();
        }
    }
        //Convert from JSON
    else if(strcasecmp(last_four,"json") == 0){
        if(strcasecmp(format,"xml") == 0){
            jsonToXml();
        }
        else if(strcasecmp(format,"csv") == 0){
            jsonToCsv();
        }
    }
    else{
        printf("Invalid input format.\n");
        return -1;
    }
    // final info message
    if(converted)
        printf("File '%s' is converted from %s to %s as '%s'.",input,last_four,format,(!output_found ? "output":output));
    else
        printf("File '%s' could not be converted.",input);

    return 0;
}

/* XML -> JSON */
void xmlToJson(){
    // XML Init
    xmlNode *xml_root = NULL;
    // JSON file root
    json_object *j_root = json_object_new_object();
    FILE *json_doc = fopen("output.json","w");

    xml_doc = xmlReadFile(input, NULL, 256);
    if (xml_doc == NULL)
    {
        printf("Couldn't read XML file.");
    }
    else
    {
        xml_root = xmlDocGetRootElement(xml_doc);
        push(j_root);
        xmlToJsonWrite(xml_root, false);
        json_object_to_file(output_found ? output:"output.json",j_root);	// save file
        fclose(json_doc);
    }
}
void xmlToJsonWrite(xmlNode *xml_parent, bool flag) {
    printf("Parent for %s is %d\n", xml_parent->name, &s.stk[s.top]);
    bool isArray; // currently array or obj?
    // check for attributes
    if(xml_parent->properties){
        xmlAttr* attribute = xml_parent->properties;
        while(attribute && attribute->name && attribute->children)
        {
            xmlChar* value = xmlNodeListGetString(xml_parent->doc, attribute->children, 1);
            json_object *j_new_string = json_object_new_string((char *)value);
            xmlFree(value);
            if (!flag)
                json_object_object_add(peek(),attribute->name, j_new_string);
            else
                json_object_array_add(peek(), j_new_string);
            attribute = attribute->next;
        }
    }

    // traversing children
    if(xml_parent->children!=NULL){
        if(xml_parent->children->type==1){
            if(xml_parent->children->prev==NULL){
                if(xml_parent->children->next!=NULL
                    && strcmp(xml_parent->children->next->name,xml_parent->children->name)==0)
                {
                    push(json_object_new_array());
                    printf("Array created from %s at %d\n", xml_parent->name, &s.stk[s.top]);
                    xmlToJsonWrite(xml_parent->children, true);
                    isArray = true;
                }
                else if(xml_parent->children->next==NULL
                        || strcmp(xml_parent->children->next->name,xml_parent->children->name)!=0){
                    push(json_object_new_object());
                    xmlToJsonWrite(xml_parent->children, false);
                    isArray = false;
                }
            }
        }
        else if(xml_parent->children->type==3){
            json_object *j_new_string = json_object_new_string(xml_parent->children->content);
            if(!flag) {
                json_object_object_add(peek(), xml_parent->name, j_new_string);
            }
            else {
                json_object_array_add(peek(), j_new_string);
            }
        }
    }

    // traversing siblings
    if(xml_parent->next!=NULL){
        xmlToJsonWrite(xml_parent->next, flag);
    } else if(xml_parent->parent->parent!=NULL){
        printf("Popping %d\n", &s.stk[s.top]);
        json_object *tmp = pop();
        if (json_object_get_type(peek()) == json_type_array){
            printf("Pushing %d as array\n", &s.stk[s.top]);
            json_object_array_add(peek(),tmp);
        }
        else if(json_object_get_type(peek()) == json_type_object) {
            printf("Pushing %d as obj\n", &s.stk[s.top]);
            json_object_object_add(peek(), xml_parent->parent->name, tmp);
        }
    }
}
/* XML -> CSV */
char *first_tag;
void xmlToCsv(){
    // read xml file
    xml_doc = xmlReadFile(input, NULL, 256); // 256 is option for parse
    xmlNodePtr xml_root = NULL;
    if(xml_doc == NULL){
        printf("Couldn't read XML file.\n");
    }
    else{
        // create file for csv
        csv_doc = fopen((output_found ? output:"output.csv"),"w+");

        // init XML
        xml_root = xmlDocGetRootElement(xml_doc);
        xmlNode *xml_node = NULL, *xml_parent;
        xml_node = xml_root->children;

        // Write tags to first line
        xmlCsv(xml_root,"", true);
        removeCharacters(1,csv_doc);
        fputc('\n',csv_doc);

        rewind(csv_doc);
        char first_line[BUFFER_SIZE];
        fgets(first_line, BUFFER_SIZE, csv_doc);
        fclose(csv_doc);

        // re-write first line
        csv_doc = fopen((output_found ? output:"output.csv"),"w+");
        fputs(first_line, csv_doc);

        //write values
        xmlCsv(xml_root,"", false);
        removeCharacters(1,csv_doc);

        // free memory
        fclose(csv_doc);
        xmlFreeDoc(xml_doc);
        xmlCleanupParser();
        xmlMemoryDump();
    }
}
void xmlCsv(xmlNode *node, char* prefix, bool first_line){
    bool flag = false;
    //children
    if(node->children!=NULL) {
        if(node->children->type==1){
            xmlCsv(node->children, "", first_line);
        }
        else if(node->children->type==3){
            if(!first_line){
                fputs(node->children->content, csv_doc);
                fputc(',', csv_doc);
            }
            else if(first_line && !(node->prev!=NULL && strcmp(node->prev->name,node->name)==0)){
                fputs(addString(prefix,node->name), csv_doc);
                fputc(',', csv_doc);
            }
        }
    }
    //attributes
    if(node->type==1 && node && node->properties){
        xmlAttr* attribute = node->properties;
        while(attribute && attribute->name && attribute->children)
        {
            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
            if(!first_line){
                fputs(value, csv_doc);
                fputc(',', csv_doc);
            }
            else{
                fputs(addString(node->name,addString("_",attribute->name)), csv_doc);
                fputc(',', csv_doc);
            }
            xmlFree(value);
            attribute = attribute->next;

        }
    }

    if(node->next!=NULL){

        if(node->parent!=NULL && xmlDocGetRootElement(xml_doc)==node->parent) {
            removeCharacters(1, csv_doc);
            fputc('\n', csv_doc);
        }

        xmlCsv(node->next, prefix, first_line);
    }
}
/* JSON -> XML */
void jsonToXml(){
    // Initiate a xml file
    xml_doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNode *xml_root = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(xml_doc, xml_root);

    // Read json file to a json object
    json_object *j_obj = json_object_from_file(input);

    // Parse the JSON file
    if(j_obj == NULL) {
        printf("Couldn't read JSON file.\n");
    }
    else {
        jsonToXmlWrite(j_obj, xml_root);
        //Save the xml file
        xmlSaveFormatFileEnc(output_found == true ? output : ("output.xml"), xml_doc, "UTF-8", 1);
        xmlFreeDoc(xml_doc);
        xmlCleanupParser();
        xmlMemoryDump();
    }
}
void jsonToXmlWrite(json_object *j_obj, xmlNode *parent){
    json_object *j_value;
    xmlNodePtr xml_parent = parent;
    int j_len,i;
    enum json_type type;
    char temp[BUFFER_SIZE];
    json_object_object_foreach(j_obj, key, val) {
        type = json_object_get_type(val);

        switch (type) {
            case json_type_object:
                xml_parent = xmlNewChild(parent, NULL, BAD_CAST key, NULL); // if it is an object, parse and store the parent
                jsonToXmlWrite(val, xml_parent);
                break;

            case json_type_array:
                j_len = json_object_array_length(val);
                // parse each element of json array and create tags for each
                for (i = 0; i < j_len; i++) {
                    xml_parent = xmlNewChild(parent, NULL, BAD_CAST key, NULL);
                    jsonToXmlWrite(json_object_array_get_idx(val, i), xml_parent);
                }
                break;
            case json_type_string:
                // create xml elements only if it is finally a string
                // prop vs child depending on input
                if (compress) {
                    xmlNewProp(parent, BAD_CAST key, BAD_CAST json_object_get_string(val));
                }
                else {
                    xmlNewChild(parent, NULL, BAD_CAST key, BAD_CAST json_object_get_string(val));
                }
                break;
            case json_type_int:
                memset(temp, 0, 255);
                sprintf(temp,"%d", json_object_get_int(val));
                if (compress) {
                    xmlNewProp(parent, BAD_CAST key, BAD_CAST temp);
                }
                else {
                    xmlNewChild(parent, NULL, BAD_CAST key, BAD_CAST temp);
                }
                break;
            case json_type_boolean:
                if (compress) {
                    xmlNewProp(parent, BAD_CAST key, BAD_CAST "true");
                }
                else {
                    xmlNewChild(parent, NULL, BAD_CAST key, BAD_CAST "true");
                }
                break;
            default:
                if (compress) {
                    xmlNewProp(parent, BAD_CAST key, BAD_CAST "null");
                }
                else {
                    xmlNewChild(parent, NULL, BAD_CAST key, BAD_CAST "null");
                }

        }
    }
}
/* JSON -> CSV */
void jsonToCsv(){
    // Initiate a csv file
    csv_doc = fopen((output_found ? output:"output.csv"),"w");

    // Read json file to a json object
    json_object *j_obj = json_object_from_file(input);

    jsonToCsvWrite(j_obj, true);
    fputc('\n',csv_doc);
    jsonToCsvWrite(j_obj, false);
    fclose(csv_doc);
}
void jsonToCsvWrite(json_object *j_obj, bool first_line){
    json_object *j_value;
    enum json_type type;
    json_object *jarray;
    // json reading loop
    json_object_object_foreach(j_obj,key,val){
        type = json_object_get_type(val);
        switch (type) {
            case json_type_object:
                j_value = json_object_object_get(j_obj,key);
                jsonToCsvWrite(j_value, first_line);
                break;
            case json_type_array:
                jarray = j_obj;
                if(key) {
                    jarray = json_object_object_get(j_obj, key);
                }
                int arraylen = json_object_array_length(jarray);
                int i;
                json_object * jvalue;

                for (i=0; i< arraylen; i++){
                    jvalue = json_object_array_get_idx(jarray, i);
                    enum json_type type2;
                    json_object_object_foreach(jvalue, key, val){
                        if(first_line && i==0){
                            fputs(key, csv_doc);
                            fputc(',', csv_doc);
                        }
                        else if(!first_line) {
                            fputs(json_object_get_string(val),csv_doc);
                            fputc(',',csv_doc);

                        }
                    }
                    removeCharacters(1,csv_doc);
                    fputc('\n',csv_doc);
                }

                break;
            default:
                if (!first_line)
                    fputs(json_object_get_string(val),csv_doc);
                else
                    fputs(key,csv_doc);
                fputc(',',csv_doc);
                break;
        }
    }
}
/* CSV -> JSON */
void csvToJson(){
    char buf[BUFFER_SIZE]; // string to process read lines
    csv_doc = fopen(input,"r"); // open and read csv file
    // Is file being read successfully?
    if(csv_doc == NULL){
        printf("Couldn't read CSV file.\n");
    }
    else {
        json_object *j_root = json_object_new_object();
        char temp[BUFFER_SIZE]; // store first line of csv
        char tags[50][50], contents[500][500]; // store tags in the xml and the contents
        int row_len=0,tag_len=0,i,j=0,k=0;
        strcpy(temp,fgets(buf, BUFFER_SIZE,csv_doc)); // read first line and assign to temp

        // split and assign tags to tags array
        for(i = 0; i<strlen(temp); i++){
            if((temp[i] == ',') || (i == strlen(temp)-1)){
                tag_len++;
                j++;
                k=0;
            }
            else{
                tags[j][k] = temp[i];
                k++;
            }
        }

        // get content and count row length
        while(feof(csv_doc)==0){
            fgets(contents[row_len],BUFFER_SIZE,csv_doc);
            contents[row_len][strlen(contents[row_len])-1] = '\0'; // removes '\n'
            row_len++;
        }
        row_len = row_len - 1;
        char *token, *s=",";
        json_object * j_array = json_object_new_array();

        for(i=0; i<row_len; i++){
            json_object * j_parent = json_object_new_object();
            for(j=0; j<tag_len; j++) {
                if (j == 0) { // for initiating strtok function

                    token = strtok(contents[i], s);
                    //printf("token is %s\n",token);
                    if(token==NULL) {
                        printf("CSV format is invalid.\n");
                        converted = false;
                        break;
                    }
                    json_object *j_val = json_object_new_string(token);
                    json_object_object_add(j_parent, tags[j], j_val);
                } else {

                    token = strtok(NULL, s);
                    //printf("token is %s\n",token);
                    if(token==NULL) {
                        printf("CSV format is invalid.\n");
                        converted = false;
                        break;
                    }
                    json_object *j_val = json_object_new_string(token);
                    json_object_object_add(j_parent, tags[j], j_val);

                }

            }
            json_object_array_add(j_array,j_parent);
        }
        json_object_object_add(j_root, "root", j_array);
        fclose(csv_doc);
        if(converted)
            json_object_to_file(output_found ? output:"output.json",j_root);
    }
}
/* CSV -> XML */
void csvToXml(){
    csv_doc = NULL; // csv file read from disk
    char buf[BUFFER_SIZE]; // string to process read lines
    csv_doc = fopen(input,"r"); // open and read csv file

    // Is file being read successfully?
    if(csv_doc == NULL){
        printf("Couldn't read CSV file.\n");
    }
    else{
        // Get the number of tags to be created
        int tag_len=0, row_len=0,j=0,k=0,i;
        char temp[BUFFER_SIZE]; // store first line of csv
        char tags[50][50], contents[500][500]; // store tags in the xml and the contents

        strcpy(temp,fgets(buf, BUFFER_SIZE,csv_doc)); // read first line and assign to temp

        // split and assign tags to tags array
        for(i = 0; i<strlen(temp); i++){
            if((temp[i] == ',') || (i == strlen(temp)-1)){
                tag_len++;
                j++;
                k=0;
            }
            else{
                tags[j][k] = temp[i];
                k++;
            }
        }

        // get content and count row length
        while(feof(csv_doc)==0){
            fgets(contents[row_len],BUFFER_SIZE,csv_doc);
            contents[row_len][strlen(contents[row_len])-1] = '\0'; // removes '\n'
            row_len++;
        }
        row_len = row_len - 1;

        // Create XML file if CSV is read
        xml_doc = NULL;
        xmlNodePtr xml_root = NULL, xml_node = NULL, xml_parent = NULL;

        // assign root and initiate XML
        xml_doc = xmlNewDoc(BAD_CAST "1.0");
        xml_root = xmlNewNode(NULL, BAD_CAST "root");
        xmlDocSetRootElement(xml_doc, xml_root);

        // create elements for XML
        for(i = 0; i<row_len; i++){
            xml_parent = xmlNewChild(xml_root, NULL, BAD_CAST "parent", NULL); // create a node as "parent" which doesnt exist in csv
            const char s[2] = ","; // split regex
            char *token; // store next split element


            for(j=0; j<tag_len; j++){
                if(j==0){ // for initiating strtok function
                    token = strtok(contents[i], s);
                    if(token==NULL) {
                        printf("CSV format is invalid.\n");
                        converted = false;
                        break;
                    }
                    if(compress)
                        xmlNewProp(xml_parent, BAD_CAST tags[j], BAD_CAST token);
                    else
                        xmlNewChild(xml_parent, NULL, tags[j], BAD_CAST token);
                }
                else{
                    token = strtok(NULL, s);
                    if(token==NULL) {
                        printf("CSV format is invalid.\n");
                        converted = false;
                        break;
                    }
                    if(compress)
                        xmlNewProp(xml_parent, BAD_CAST tags[j], BAD_CAST token);
                    else
                        xmlNewChild(xml_parent, NULL, tags[j], BAD_CAST token);
                }
            }


        }
        if(converted)
            xmlSaveFormatFileEnc(output_found == true ? output:("output.xml"), xml_doc, "UTF-8", 1);

        // free memory
        fclose(csv_doc);
        xmlFreeDoc(xml_doc);
        xmlCleanupParser();
        xmlMemoryDump();
    }
}
/* STACK OPERATIONS */
/*  Function to add an element to the stack */
void push (json_object *j_obj) {
    if (s.top == (MAXSIZE - 1))
    {
        printf ("Stack is Full\n");
        return;
    }
    else
    {
        s.top = s.top + 1;
       // printf("Pushing address %d Top: %d\n",&j_obj,s.top);
        s.stk[s.top] = j_obj;
        return;
    }

}
/*  Function to delete an element from the stack */
json_object *pop (void){
    json_object *j_obj;
    if (s.top == - 1)
    {
        printf ("Stack is Empty\n");
        return (s.top);
    }
    else
    {
        //printf("Popping address %d Top: %d\n",&s.stk[s.top],s.top);
        j_obj = s.stk[s.top];
        s.top = s.top - 1;
    }
    return(j_obj);
}
/* Function to see top element */
json_object *peek(void){
    if(s.top!=-1)
        return s.stk[s.top];
    else
        return NULL;
}

/* MISC */
/* concat 2 strings */
char *addString(char *s1, char *s2){
    char *s3 = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(s3,s1);
    strcat(s3,s2);
    return s3;
}
/* remove n character from end of file (for csv commas) */
void removeCharacters(int length, FILE *file){
    fseeko(file,-length,SEEK_END);
    int position = ftello(file);
    ftruncate(fileno(file), position);
}