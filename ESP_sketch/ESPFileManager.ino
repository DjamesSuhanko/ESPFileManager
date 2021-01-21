// use this include if  compiling in vscode with platformIO plugin #include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include <ESP8266WiFi.h>
#include <string.h>

#define SSID   "SuhankoFamily"
#define PASSWD "fsjmr112"

uint8_t is_overflow        = 0;
const int overflow_limit   = 250;
char msg[overflow_limit]   = {0};

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    File root = fs.open(dirname,"r");
    if(!root){
        Serial.println("#Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("#Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.println(file.name());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char * path){
    File file = fs.open(path, "r");
    if(!file || file.isDirectory()){
        Serial.println("#Failed to open file for reading");
        return;
    }

    Serial.write('#');
    while(file.available()){
        Serial.write(file.read());
    }
    Serial.write('$');
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    File file = fs.open(path, "w");
    if(!file){
        Serial.println("#Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("#File written");
    } else {
        Serial.println("#Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    File file = fs.open(path, "a");
    if(!file){
        Serial.println("#Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("#Message appended");
    } else {
        Serial.println("#Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    if (fs.rename(path1, path2)) {
        Serial.println("#File renamed");
    } else {
        Serial.println("#Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    if(fs.remove(path)){
        Serial.println("#File deleted");
    } else {
        Serial.println("#Delete failed");
    }
}

void fileManager(){
    memset(msg,0,sizeof(msg)); //zera o array a cada ciclo
    is_overflow = 0;           //zera o interador de limite de leitura
    while (Serial.available() > 0){
        msg[is_overflow] = Serial.read();
        //se exceder o limite do overflow_limit, interrompe a leitura
        if (is_overflow == overflow_limit){
            break;
        }
        is_overflow++;
    }

    if (msg[0] == '^'){
        //verificado inicio, procura pelo fim
        if (String(msg).lastIndexOf("$") == -1){
            return;
        }

        //só chega aqui se início (^) e fim ($) de mensagens forem encontrados
        uint8_t first_delimiter = String(msg).indexOf("-");
        uint8_t end_of_line     = String(msg).indexOf("$");

        String filename = "/" + String(msg).substring(1,first_delimiter);
    
        //1 - ler do arquivo
        if (msg[first_delimiter+1] == 'r'){
            readFile(LittleFS, filename.c_str());
            //Serial.println(msg);
        }
        //2 - ler todos os arquivos
        else if (msg[first_delimiter+1] == 'l'){
            listDir(LittleFS,"/",0);
        }
        //3 - criar arquivo
        else if (msg[first_delimiter+1] == 'w'){
            String msg_to_write = String(msg).substring(first_delimiter+3,end_of_line);
            writeFile(LittleFS,filename.c_str(),msg_to_write.c_str());
        }
        //4 - concatenar a um arquivo existente
        else if (msg[first_delimiter+1] == 'a'){
            String msg_to_write = String(msg).substring(first_delimiter+3,end_of_line);
            appendFile(LittleFS,filename.c_str(),msg_to_write.c_str());
        }
        //5 - excluir arquivo
        else if (msg[first_delimiter+1] == 'd'){
            deleteFile(LittleFS, filename.c_str());
        }
        //6 - renomear arquivo
        else if (msg[first_delimiter+1] == 'm'){
            String new_name = String(msg).substring(first_delimiter+3,end_of_line);
            renameFile(LittleFS,filename.c_str(),new_name.c_str());
        }
    }
}

void setup(){
    LittleFSConfig cfg;
    cfg.setAutoFormat(true);
    LittleFS.setConfig(cfg);

    WiFi.begin(SSID,PASSWD);
    while (WiFi.status() != WL_CONNECTED){delay(100);}

    Serial.begin(9600);
    delay(3000);
    if(!LittleFS.begin()){
        Serial.println("#LittleFS Mount Failed");
        return;
    }
    
    /*
    listDir(LittleFS, "/", 0);
    writeFile(LittleFS, "/hello.txt", "Hello ");
    appendFile(LittleFS, "/hello.txt", "World!\n");
    readFile(LittleFS, "/hello.txt");
    deleteFile(LittleFS, "/foo.txt");
    renameFile(LittleFS, "/hello.txt", "/foo.txt");
    readFile(LittleFS, "/foo.txt");
    */
}

void loop(){
    fileManager();
    delay(1000);
}
