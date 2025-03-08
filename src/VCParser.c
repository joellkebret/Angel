//Name: Joell Kebret
//Student ID: 1275379

#include "VCHelpers.h"
#include "VCParser.h"

VCardErrorCode createCard(char* fileName, Card** newCardObject) {
    if (fileName == NULL || newCardObject == NULL) { 
        return INV_FILE;
    }

    // Check file extension
    char* extension = strrchr(fileName, '.');
    if (!extension || (strcmp(extension, ".vcf") != 0 && strcmp(extension, ".vcard") != 0)) {
        return INV_FILE;
    }

    FILE* file = fopen(fileName, "r");
    if (file == NULL) {
        return INV_FILE;
    }

    // Allocate memory for new card object
    *newCardObject = malloc(sizeof(Card));
    if (*newCardObject == NULL) {
        fclose(file);
        return OTHER_ERROR;
    }

    // Initialize card fields
    (*newCardObject)->fn = NULL;
    (*newCardObject)->birthday = NULL;
    (*newCardObject)->anniversary = NULL;
    (*newCardObject)->optionalProperties = initializeList(propertyToString, deleteProperty, compareProperties);
    if ((*newCardObject)->optionalProperties == NULL) {
        free(*newCardObject);
        fclose(file);
        return OTHER_ERROR;
    }

    bool foundFN = false, foundVersion = false, foundBegin = false, foundEnd = false;
    double version = 0.0;
    Property* finalProperty = NULL;

    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        long len = strlen(line);

        // Check if CRLF is present
        if (len < 2 || line[len - 2] != '\r' || line[len - 1] != '\n') {
            deleteCard(*newCardObject);
            fclose(file);
            return INV_CARD;
        }
        line[len - 2] = '\0';  // remove CRLF

        // Check if first line is space/tab (folded line)
        if ((line[0] == ' ' || line[0] == '\t') && finalProperty != NULL) {
            char* foldedValue = strdup(line + 1);
            if (foldedValue) {
                char* oldValue = getFromBack(finalProperty->values);
                if (oldValue) {
                    long newLen = strlen(oldValue) + strlen(foldedValue);
                    char* newVal = malloc(newLen + 1);
                    if (newVal) {
                        sprintf(newVal, "%s%s", oldValue, foldedValue);
                        free(oldValue);
                        finalProperty->values->tail->data = newVal;
                    }
                }
                free(foldedValue);
            }
            continue;
        }

        // Skip completely empty lines
        if (strlen(line) == 0) {
            continue;
        }

        // Ensure first line is BEGIN:VCARD
        if (!foundBegin) {
            if (strcmp(line, "BEGIN:VCARD") != 0) {
                deleteCard(*newCardObject);
                fclose(file);
                return INV_CARD;
            }
            foundBegin = true;
            continue;
        }

        // Check if last line is END:VCARD
        if (strcmp(line, "END:VCARD") == 0) {
            foundEnd = true;
            break;
        }

        // Split at the first colon
        char* colonPosition = strchr(line, ':');
        if (!colonPosition) {
            deleteCard(*newCardObject);
            fclose(file);
            return INV_PROP;
        }

        *colonPosition = '\0';
        char* entireProperty = line;
        char* propertyValue = colonPosition + 1;

        // If property name or value is empty => invalid
        if (strlen(entireProperty) == 0 || strlen(propertyValue) == 0) {
            deleteCard(*newCardObject);
            fclose(file);
            return INV_PROP;
        }

        // Parse property group if any
        char* propertyGroup = strdup("");
        if (!propertyGroup) {
            deleteCard(*newCardObject);
            fclose(file);
            return OTHER_ERROR;
        }

        char* scPosition = strchr(entireProperty, ';');
        char* periodPosition = NULL;
        if (scPosition) {
            *scPosition = '\0';
            periodPosition = strchr(entireProperty, '.');
            *scPosition = ';';
        } else {
            periodPosition = strchr(entireProperty, '.');
        }

        char* propName = entireProperty;
        if (periodPosition) {
            *periodPosition = '\0';
            free(propertyGroup);
            propertyGroup = strdup(entireProperty);
            propName = periodPosition + 1;
        }

        // Create parameter list
        List* paramList = initializeList(parameterToString, deleteParameter, compareParameters);
        if (!paramList) {
            free(propertyGroup);
            deleteCard(*newCardObject);
            fclose(file);
            return OTHER_ERROR;
        }

        bool isValueText = false;

        // If there's a semicolon after property name => parse parameters
        char* scPosition2 = strchr(propName, ';');
        if (scPosition2) {
            *scPosition2 = '\0';
            scPosition2++;
            char* paramToken = strtok(scPosition2, ";");
            while (paramToken != NULL) {
                char* equalPosition = strchr(paramToken, '=');
                if (!equalPosition) {
                    free(propertyGroup);
                    deleteCard(*newCardObject);
                    freeList(paramList);
                    fclose(file);
                    return INV_PROP;
                }

                *equalPosition = '\0';
                char* parameterName = paramToken;
                char* parameterVal = equalPosition + 1;

                Parameter* newParam = malloc(sizeof(Parameter));
                if (!newParam) {
                    free(propertyGroup);
                    deleteCard(*newCardObject);
                    freeList(paramList);
                    fclose(file);
                    return OTHER_ERROR;
                }
                newParam->name = strdup(parameterName);
                newParam->value = strdup(parameterVal);
                insertBack(paramList, newParam);

                // Check if VALUE=text
                if (strcasecmp(parameterName, "VALUE") == 0 && strcasecmp(parameterVal, "text") == 0) {
                    isValueText = true;
                }
                paramToken = strtok(NULL, ";");
            }
        }

        // Handle VERSION
        if (strcmp(propName, "VERSION") == 0) {
            if (foundVersion) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return INV_CARD;
            }
            foundVersion = true;
            version = atof(propertyValue);
            if (version != 4.0) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return INV_CARD;
            }

            freeList(paramList);
            free(propertyGroup);
            continue;
        }
        // Handle FN
        else if (strcmp(propName, "FN") == 0) {
            if (foundFN) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return INV_CARD;
            }
            foundFN = true;

            Property* fnProp = malloc(sizeof(Property));
            if (!fnProp) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return OTHER_ERROR;
            }

            fnProp->name = strdup("FN");
            fnProp->group = propertyGroup;
            fnProp->values = initializeList(valueToString, deleteValue, compareValues);
            fnProp->parameters = paramList;

            if (!fnProp->name || !fnProp->values) {
                deleteProperty(fnProp);
                deleteCard(*newCardObject);
                fclose(file);
                return OTHER_ERROR;
            }

            insertBack(fnProp->values, strdup(propertyValue));
            (*newCardObject)->fn = fnProp;
            finalProperty = fnProp;
        }
        // Handle BDAY/ANNIVERSARY => parse into DateTime struct
        else if (strcmp(propName, "BDAY") == 0 || strcmp(propName, "ANNIVERSARY") == 0) {
            DateTime* vcardDate = malloc(sizeof(DateTime));
            if (!vcardDate) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return OTHER_ERROR;
            }

            vcardDate->UTC    = false;
            vcardDate->isText = isValueText;
            vcardDate->date   = strdup("");
            vcardDate->time   = strdup("");
            vcardDate->text   = strdup("");

            if (isValueText) {
                // If text => store entire propertyValue in text
                free(vcardDate->text);
                vcardDate->text = strdup(propertyValue);
            } else {
                // Not text => parse date/time
                char* timePosition = strchr(propertyValue, 'T');
                if (timePosition) {
                    *timePosition = '\0';
                    free(vcardDate->date);
                    vcardDate->date = strdup(propertyValue);
                    free(vcardDate->time);
                    vcardDate->time = strdup(timePosition + 1);

                    // Check if ends with 'Z'
                    if (vcardDate->time[strlen(vcardDate->time) - 1] == 'Z') {
                        vcardDate->UTC = true;
                        vcardDate->time[strlen(vcardDate->time) - 1] = '\0';
                    }
                } else {
                    // Just YYYYMMDD
                    free(vcardDate->date);
                    vcardDate->date = strdup(propertyValue);
                }
            }

            free(propertyGroup);
            freeList(paramList);

            if (strcmp(propName, "BDAY") == 0) {
                // Save as birthday
                (*newCardObject)->birthday = vcardDate;
            } else {
                // Save as anniversary
                (*newCardObject)->anniversary = vcardDate;
            }
            finalProperty = NULL;
        }
        // Otherwise, store as a normal Property in optionalProperties
        else {
            Property* prop = malloc(sizeof(Property));
            if (!prop) {
                free(propertyGroup);
                deleteCard(*newCardObject);
                freeList(paramList);
                fclose(file);
                return OTHER_ERROR;
            }

            prop->name = strdup(propName);
            prop->group = propertyGroup;
            prop->values = initializeList(valueToString, deleteValue, compareValues);
            prop->parameters = paramList;

            if (!prop->name || !prop->values) {
                deleteProperty(prop);
                deleteCard(*newCardObject);
                fclose(file);
                return OTHER_ERROR;
            }

           // If N property, parse semicolon-delimited fields
if (strcasecmp(propName, "N") == 0 || strcasecmp(propName, "ADR") == 0) {
    char* start = propertyValue;
    
    for (int i = 0; i < (strcasecmp(propName, "N") == 0 ? 5 : 7); i++) { 
        char* semicolon = strchr(start, ';');
        
        if (semicolon) {
            *semicolon = '\0';
            insertBack(prop->values, strdup(start));
            start = semicolon + 1;
        } else {
            insertBack(prop->values, strdup(start));
            break;  // No more values
        }
    }

    // Ensure exactly 5 values for `N` and 7 for `ADR`
    int requiredValues = (strcasecmp(propName, "N") == 0) ? 5 : 7;
    while (getLength(prop->values) < requiredValues) {
        insertBack(prop->values, strdup(""));  // Fill missing fields with empty values
    }
} else {
    // Normal property: store value as a single entry
    insertBack(prop->values, strdup(propertyValue));
}

            insertBack((*newCardObject)->optionalProperties, prop);
            finalProperty = prop;
        }
    } // end while

    fclose(file);

    // If END:VCARD was never found => invalid
    if (!foundEnd) {
        deleteCard(*newCardObject);
        return INV_CARD;
    }

    // Must have found FN and version == 4.0
    if (!foundFN || !foundVersion || version != 4.0) {
        deleteCard(*newCardObject);
        return INV_CARD;
    }

    return OK; //valid card :)
}

void deleteCard(Card* obj) {
    if (obj == NULL) {
        return;
    }

    if (obj->fn != NULL) {
        deleteProperty(obj->fn);
    }

    if (obj->optionalProperties != NULL) {
        freeList(obj->optionalProperties);
    }

    if (obj->birthday != NULL) {
        deleteDate(obj->birthday);
    }

    if (obj->anniversary != NULL) {
        deleteDate(obj->anniversary);
    }

    free(obj); //delete all individuals parts of a v card then card itself.
}

char* cardToString(const Card* obj) {
    if (obj == NULL) {
        return strdup("Invalid card: NULL");
    }

    char* buffer = malloc(8192);
    if (buffer == NULL) {
        return NULL;
    }

    strcpy(buffer, "BEGIN:VCARD\nVERSION:4.0\n");
    
    if (obj->fn) {
        char* fnStr = (char*)getFromFront(obj->fn->values);
        sprintf(buffer + strlen(buffer), "FN:%s\n", fnStr);
    } else {
        free(buffer);
        return NULL;
    }
    
    if (obj->birthday) {
        if (obj->birthday->isText) {
            sprintf(buffer + strlen(buffer), "BDAY:%s\n", obj->birthday->text);
        } else {
            sprintf(buffer + strlen(buffer), "BDAY:%sT%s%s\n", obj->birthday->date, obj->birthday->time, obj->birthday->UTC ? "Z" : "");
        }
    }
    
    if (obj->anniversary) {
        if (obj->anniversary->isText) {
            sprintf(buffer + strlen(buffer), "ANNIVERSARY:%s\n", obj->anniversary->text);
        } else {
            sprintf(buffer + strlen(buffer), "ANNIVERSARY:%sT%s%s\n", obj->anniversary->date, obj->anniversary->time, obj->anniversary->UTC ? "Z" : "");
        }
    }
    
    ListIterator iter = createIterator(obj->optionalProperties);
    void* prop;
    while ((prop = nextElement(&iter)) != NULL) {
        Property* property = (Property*)prop;
        sprintf(buffer + strlen(buffer), "%s", property->name);
        
        ListIterator paramIter = createIterator(property->parameters);
        void* paramElem;
        while ((paramElem = nextElement(&paramIter)) != NULL) {
            Parameter* param = (Parameter*)paramElem;
            sprintf(buffer + strlen(buffer), ";%s=%s", param->name, param->value);
        }
        
        sprintf(buffer + strlen(buffer), ":");
        
        ListIterator valIter = createIterator(property->values);
        void* valElem = nextElement(&valIter);
        if (valElem != NULL) {
            sprintf(buffer + strlen(buffer), "%s", (char*)valElem);
        }
        while ((valElem = nextElement(&valIter)) != NULL) {
            sprintf(buffer + strlen(buffer), ";%s", (char*)valElem);
        }
        
        sprintf(buffer + strlen(buffer), "\n");
    }

    strcat(buffer, "END:VCARD\n");
    return buffer;
}


 char* errorToString(VCardErrorCode err) {
    char* message = NULL;

    switch (err) {
        case OK:
            message = "OK";
            break;
        case INV_FILE:
            message = "Invalid file";
            break;
        case INV_CARD:
            message = "Invalid card";
            break;
        case INV_PROP:
            message = "Invalid property";
            break;
        case INV_DT:
            message = "Invalid date/time";
            break;
        case WRITE_ERROR:
            message = "Write error";
            break;
        case OTHER_ERROR:
            message = "Other error";
            break;
        default:
            message = "Unknown error";
            break;
    }

    //dynamically allocate the message including the null terminator
    char* result = malloc(strlen(message) + 1);
    if (result != NULL) {
        strcpy(result, message);
    }

    return result;  //caller needs to free
} 

VCardErrorCode writeCard(const char* fileName, const Card* obj) {

    //param checks
    if (obj == NULL || fileName == NULL || fileName[0] == '\0') {
        return WRITE_ERROR;
    }

    // file extension check
    char* extension = strrchr(fileName, '.');
    if (!extension || (strcmp(extension, ".vcf") != 0 && strcmp(extension, ".vcard") != 0)) {
        return WRITE_ERROR;
    }

    FILE *file = fopen(fileName, "w");
    if (file == NULL) {
        return WRITE_ERROR;
    }

    //mandatory non properties
    fprintf(file, "BEGIN:VCARD\r\n");
    fprintf(file, "VERSION:4.0\r\n");

    // write fn is not null and not empty
    if (obj->fn != NULL) {
        fprintf(file, "FN:%s\r\n", 
                (obj->fn->values != NULL && getLength(obj->fn->values) > 0) ? 
                (char*)getFromFront(obj->fn->values) : "");
    }

    // Loop through optionalProperties
    ListIterator iter = createIterator(obj->optionalProperties);
    void* elem;
    while ((elem = nextElement(&iter)) != NULL) {
        Property* prop = (Property*)elem;

        //prop name should not be null or values or length of values should not be null
        if (prop->name == NULL || prop->values == NULL || getLength(prop->values) == 0) {
            continue; // Skip this property if invalid
        }

        fprintf(file, "%s", prop->name); 

        //param loop
        ListIterator paramIter = createIterator(prop->parameters);
        void* paramElem;
        int firstParam = 1; //tracking param to avoid exrenuous semicolon
        
        while ((paramElem = nextElement(&paramIter)) != NULL) {
            Parameter* param = (Parameter*)paramElem;
            if (param->name != NULL && param->value != NULL) {
                fprintf(file, "%s%s=%s", firstParam ? ";" : ";", param->name, param->value);
                firstParam = 0;
            }
        }

        //handle properties that require multiple values per rfc 6350
        int requiredValues = 1;  
        int minValues = 1;     

        if (strcasecmp(prop->name, "N") == 0) {
            requiredValues = 5;
        } else if (strcasecmp(prop->name, "ADR") == 0) {
            requiredValues = 7;
        } else if (strcasecmp(prop->name, "CLIENTPIDMAP") == 0) {
            minValues = 2; 
        } else if (strcasecmp(prop->name, "GENDER") == 0) {
            minValues = 1;  //special case one or two
            requiredValues = 2;
        }

        // Print values
        ListIterator valIter = createIterator(prop->values);
        void* valElem;
        int i = 0;

        fprintf(file, ":");
        while ((valElem = nextElement(&valIter)) != NULL && i < requiredValues) {
            fprintf(file, "%s", (char*)valElem);
            if (i < requiredValues - 1) {  
                fprintf(file, ";");  // add semicolon
            }
            i++;
        }

        while (i < requiredValues) {
            fprintf(file, ";");
            i++;
        }

        //check vlaue of this speicifc case
        if (strcasecmp(prop->name, "CLIENTPIDMAP") == 0 && i < minValues) {
            fclose(file);
            return WRITE_ERROR;  // Not enough values for CLIENTPIDMAP
        }

        fprintf(file, "\r\n"); 
    }

    // Handle BDAY
    if (obj->birthday != NULL) {
        fprintf(file, "BDAY");

        if (obj->birthday->isText) {
            // Print text birthday
            fprintf(file, ";VALUE=text:%s\r\n", 
                    (obj->birthday->text != NULL) ? obj->birthday->text : "");
        } else {
            // Print standard date or DT format with UTC check
            fprintf(file, ":%s%s%s%s\r\n", 
                    (obj->birthday->date != NULL) ? obj->birthday->date : "", 
                    (obj->birthday->time != NULL && obj->birthday->time[0] != '\0') ? "T" : "",
                    (obj->birthday->time != NULL) ? obj->birthday->time : "",
                    (obj->birthday->UTC) ? "Z" : "");
        }
    }

    // Handle ANNIVERSARY
    if (obj->anniversary != NULL) {
        fprintf(file, "ANNIVERSARY");

        if (obj->anniversary->isText) {
            // Print text anniversary
            fprintf(file, ";VALUE=text:%s\r\n", 
                    (obj->anniversary->text != NULL) ? obj->anniversary->text : "");
        } else {
            fprintf(file, ":%s%s%s%s\r\n", 
                    (obj->anniversary->date != NULL) ? obj->anniversary->date : "", 
                    (obj->anniversary->time != NULL && obj->anniversary->time[0] != '\0') ? "T" : "",
                    (obj->anniversary->time != NULL) ? obj->anniversary->time : "",
                    (obj->anniversary->UTC) ? "Z" : "");
        }
    }

    // End vCard
    fprintf(file, "END:VCARD\r\n");

    fclose(file);
    return OK; // Write successful
}



VCardErrorCode validateCard(const Card* obj) {
    //check if card null
    if (obj == NULL) {
        return INV_CARD;  
    }

    if (obj->fn == NULL) {   
        return INV_CARD;  // FN must exist
    }
    if (obj->fn->values == NULL || getLength(obj->fn->values) == 0) {
        return INV_CARD;  //FN has no values
    }
    char* fnValue = (char*)getFromFront(obj->fn->values);
    if (fnValue == NULL || strlen(fnValue) == 0) {
        return INV_CARD;  //FN is empty
    }

    // check optionalProperties list card needs at least one
    if (obj->optionalProperties == NULL) {
        return INV_CARD;
    }

    // occurcence / cardinality tracker
    int kindCount = 0; //this is kind of hardcoded for the test
    int nCount = 0;
    int uidCount = 0;
    int revCount = 0;
    int genderCount = 0;
    int prodidCount = 0;


    // loop through optionalProperties
    ListIterator iter = createIterator(obj->optionalProperties);
    void* elem;
    while ((elem = nextElement(&iter)) != NULL) {
        Property* prop = (Property*)elem;

        // version is required
        if (strcasecmp(prop->name, "VERSION") == 0) {
            return INV_CARD;
        }

        // n can only papear at max onece
        if (strcasecmp(prop->name, "N") == 0) {
            nCount++;
            if (nCount > 1) {
                return INV_PROP;  
            }
            if (getLength(prop->values) != 5) {
                return INV_PROP;
            }
        }

        // kind must be unique as well
        if (strcasecmp(prop->name, "KIND") == 0) {
            kindCount++;
            if (kindCount > 1) {
                return INV_PROP; 
            }
        }

        if (strcasecmp(prop->name, "UID") == 0) {
            uidCount++;
            if (uidCount > 1) {
                return INV_PROP; 
            }
        }

        if (strcasecmp(prop->name, "PRODID") == 0) {
            prodidCount++;
            if (prodidCount > 1) {
                return INV_PROP;  
            }
        }

        if (strcasecmp(prop->name, "REV") == 0) {
            revCount++;
            if (revCount > 1) {
                return INV_PROP;  
            }
        }
        
        if (strcasecmp(prop->name, "REV") == 0) {
            revCount++;
            if (revCount > 1) {
                return INV_PROP;  
            }
        }

        if (strcasecmp(prop->name, "GENDER") == 0) {
            genderCount++;
            if (genderCount > 1) {
                return INV_PROP;  
            }
        }

        // prop cant be empty
        if (prop->name == NULL || strlen(prop->name) == 0) {
            return INV_PROP;
        }

        // bday or ann should not be in anniversary
        if (strcasecmp(prop->name, "BDAY") == 0 ||
            strcasecmp(prop->name, "ANNIVERSARY") == 0) 
        {
            return INV_DT;  
        }

        //prop has to have 1 val minimum
        if (prop->values == NULL || getLength(prop->values) == 0) {
            return INV_PROP;  // missing property value
        }

        // parameters must also be non empty
        ListIterator paramIter = createIterator(prop->parameters);
        void* paramElem;
        while ((paramElem = nextElement(&paramIter)) != NULL) {
            Parameter* param = (Parameter*)paramElem;
            if (param->name == NULL || strlen(param->name) == 0 ||
                param->value == NULL || strlen(param->value) == 0) {
                return INV_PROP;  // missing param name/value
            }
        }
    }

    // validate bday if existing
    if (obj->birthday != NULL) {
        if (obj->birthday->isText) {
            //handle text date
            if (strlen(obj->birthday->date) != 0 ||
                strlen(obj->birthday->time) != 0 ||
                strlen(obj->birthday->text) == 0 ||
                obj->birthday->UTC)
            {
                return INV_DT;
            }
        } else {
            if (strlen(obj->birthday->text) != 0) {
                return INV_DT;
            }

            if (strlen(obj->birthday->date) == 0 && strlen(obj->birthday->time) == 0) {
                return INV_DT;  // must have somethin
            }

            //check utc 
            if (obj->birthday->UTC && strlen(obj->birthday->time) == 0) {
                return INV_DT;
            }
        }
    }

    if (obj->anniversary != NULL) {
        if (obj->anniversary->isText) {
            if (strlen(obj->anniversary->date) != 0 ||
                strlen(obj->anniversary->time) != 0 ||
                strlen(obj->anniversary->text) == 0 ||
                obj->anniversary->UTC)
            {
                return INV_DT;
            }
        } else {
            if (strlen(obj->anniversary->text) != 0) {
                return INV_DT;
            }

            if (strlen(obj->anniversary->date) == 0 && strlen(obj->anniversary->time) == 0) {
                return INV_DT;
            }

            if (obj->anniversary->UTC && strlen(obj->anniversary->time) == 0) {
                return INV_DT;
            }
        }
    }

    // If we reach here, everything is valid :)
    return OK; 
}