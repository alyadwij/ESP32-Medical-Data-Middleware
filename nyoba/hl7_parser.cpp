#include "hl7_parser.h"
#include <string.h>

bool split_string(char *str, const char *delimiter, char fields[][MAX_STR], int *field_count) {
    if (!str || !delimiter || !fields || !field_count) {
        Serial.println("split_string: Invalid input");
        return false;
    }
    int i = 0;
    int pos = 0;
    int len = strlen(str);
    char *start = str;

    while (pos <= len && i < 10) {
        if (str[pos] == delimiter[0] || str[pos] == '\0') {
            int field_len = pos - (start - str);
            if (field_len > 0 && field_len < MAX_STR) {
                strncpy(fields[i], start, field_len);
                fields[i][field_len] = '\0';
                i++;
            }
            start = str + pos + 1;
            if (str[pos] == '\0') break;
        }
        pos++;
    }
    *field_count = i;
    return i > 0;
}

bool parse_hl7_message(const char *message, int len, Metadata *meta, Parameter *params, int *param_count) {
    if (!message || !meta || !params || !param_count || len <= 0 || len > BUFFER_SIZE) {
        Serial.println("parse_hl7_message: Invalid input");
        return false;
    }

    *param_count = 0;
    meta->alat[0] = '\0';
    meta->waktu[0] = '\0';
    meta->id[0] = '\0';

    Serial.print("Parsing HL7 message, length: ");
    Serial.println(len);

    // Clean the message
    char cleaned_message[BUFFER_SIZE] = {0};
    int cleaned_len = 0;
    int start_offset = (message[0] == 0x0B) ? 1 : 0;
    for (int i = start_offset; i < len && cleaned_len < BUFFER_SIZE - 1; i++) {
        if (i < len - 2 && message[i] == 0x1C && message[i + 1] == 0x0D) {
            continue;
        }
        cleaned_message[cleaned_len++] = message[i];
    }
    cleaned_message[cleaned_len] = '\0';
    Serial.print("Cleaned message length: ");
    Serial.println(cleaned_len);

    // Process each line
    char line[MAX_STR] = {0};
    int start = 0;
    for (int i = 0; i < cleaned_len; i++) {
        if (cleaned_message[i] == '\r' || cleaned_message[i] == '\n' || i == cleaned_len - 1) {
            int line_len = (i == cleaned_len - 1) ? i - start + 1 : i - start;
            if (line_len > 0 && line_len < MAX_STR) {
                strncpy(line, cleaned_message + start, line_len);
                line[line_len] = '\0';
                Serial.print("Processing line: ");
                Serial.println(line);

                // Split line into fields
                char fields[10][MAX_STR] = {0};
                int field_count = 0;
                if (!split_string(line, "|", fields, &field_count)) {
                    Serial.println("Failed to split line");
                    continue;
                }

                Serial.print("Field count: ");
                Serial.println(field_count);
                if (field_count > 0) {
                    Serial.print("Fields: [0]=");
                    Serial.print(fields[0]);
                    if (field_count > 1) Serial.print(", [1]="), Serial.print(fields[1]);
                    if (field_count > 2) Serial.print(", [2]="), Serial.print(fields[2]);
                    if (field_count > 3) Serial.print(", [3]="), Serial.print(fields[3]);
                    Serial.println();
                }

                // Process MSH segment
                if (field_count > 0 && strcmp(fields[0], "MSH") == 0) {
                    Serial.print("MSH detected, field_count: ");
                    Serial.println(field_count);
                    if (field_count > 3 && fields[3][0]) {
                        strncpy(meta->alat, fields[3], MAX_STR - 1);
                        meta->alat[MAX_STR - 1] = '\0';
                    } else {
                        strcpy(meta->alat, "Unknown");
                    }
                    if (field_count > 2 && fields[2][0]) {
                        strncat(meta->alat, " ", MAX_STR - strlen(meta->alat) - 1);
                        strncat(meta->alat, fields[2], MAX_STR - strlen(meta->alat) - 1);
                        Serial.print("Appended fields[2], alat: ");
                        Serial.println(meta->alat);
                    }
                }
                // Process OBR segment
                else if (field_count > 0 && strcmp(fields[0], "OBR") == 0) {
                    if (field_count > 2 && fields[2][0]) {
                        strncpy(meta->id, fields[2], MAX_STR - 1);
                        meta->id[MAX_STR - 1] = '\0';
                    }
                    if (field_count > 4 && strlen(fields[4]) >= 14) {
                        char waktu_raw[15] = {0};
                        strncpy(waktu_raw, fields[4], 14);
                        waktu_raw[14] = '\0';
                        snprintf(meta->waktu, MAX_STR, "%c%c%c%c-%c%c-%c%c,%c%c:%c%c:%c%c",
                                 waktu_raw[0], waktu_raw[1], waktu_raw[2], waktu_raw[3],
                                 waktu_raw[4], waktu_raw[5], waktu_raw[6], waktu_raw[7],
                                 waktu_raw[8], waktu_raw[9], waktu_raw[10], waktu_raw[11],
                                 waktu_raw[12], waktu_raw[13]);
                        Serial.print("OBR - ID: ");
                        Serial.print(meta->id);
                        Serial.print(", Waktu: ");
                        Serial.println(meta->waktu);
                    }
                }
                // Process OBX segment
                else if (field_count > 0 && strcmp(fields[0], "OBX") == 0 && *param_count < MAX_PARAM) {
                    int obx_index = (field_count > 1 && fields[1][0]) ? atoi(fields[1]) - 1 : -1;
                    if (obx_index >= 5 && field_count > 4 && fields[4][0]) {
                        char param_name[MAX_STR] = {0};
                        char *caret_pos = strchr(fields[3], '^');
                        if (caret_pos) {
                            char *second_caret = strchr(caret_pos + 1, '^');
                            if (second_caret) {
                                int len = second_caret - (caret_pos + 1);
                                if (len > 0 && len < MAX_STR) {
                                    strncpy(param_name, caret_pos + 1, len);
                                    param_name[len] = '\0';
                                }
                            } else {
                                strncpy(param_name, caret_pos + 1, MAX_STR - 1);
                                param_name[MAX_STR - 1] = '\0';
                            }
                        } else {
                            strncpy(param_name, fields[3], MAX_STR - 1);
                            param_name[MAX_STR - 1] = '\0';
                        }
                        strncpy(params[*param_count].name, param_name, MAX_STR - 1);
                        params[*param_count].name[MAX_STR - 1] = '\0';
                        strncpy(params[*param_count].value, fields[4], MAX_STR - 1);
                        params[*param_count].value[MAX_STR - 1] = '\0';
                        Serial.print("Stored parameter ");
                        Serial.print(*param_count);
                        Serial.print(": ");
                        Serial.print(params[*param_count].name);
                        Serial.print(" = ");
                        Serial.println(params[*param_count].value);
                        (*param_count)++;
                    }
                }
            } else if (line_len >= MAX_STR) {
                Serial.print("Line too long, skipping: ");
                Serial.println(line_len);
            }
            start = i + 1;
        }
    }
    Serial.print("Total parameters stored: ");
    Serial.println(*param_count);
    return true;
}