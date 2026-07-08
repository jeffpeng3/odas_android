#include <general/interface.h>

interface_obj * interface_construct() {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_undefined;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) NULL;

    return obj;

}

interface_obj * interface_construct_blackhole() {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_blackhole;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) NULL;

    return obj;

}

interface_obj * interface_construct_file(const char * fileName) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_file;
    obj->fileName = (char *) malloc(sizeof(char) * (strlen(fileName)+1));
    strcpy(obj->fileName, fileName);
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) NULL;

    return obj;

}

interface_obj * interface_construct_socket(const char * ip, const unsigned int port) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_socket;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) malloc(sizeof(char) * (strlen(ip)+1));
    strcpy(obj->ip, ip);
    obj->port = port;
    obj->deviceName = (char *) NULL;

    return obj;        

}

interface_obj * interface_construct_pulseaudio(const char * sourceName) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_pulseaudio;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) malloc(sizeof(char) * (strlen(sourceName)+1));
    strcpy(obj->deviceName, sourceName);

    return obj;        

}

interface_obj * interface_construct_soundcard(const unsigned int card, const unsigned int device) {

    char * deviceName = (char *) malloc(sizeof(char) * 1024);

    sprintf(deviceName, "hw:%u,%u", card, device);

    interface_obj * obj = interface_construct_soundcard_by_name(deviceName);

    free((void* ) deviceName);

    return obj;

}

interface_obj * interface_construct_soundcard_by_name(char * deviceName) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_soundcard;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) malloc(sizeof(char) * (strlen(deviceName)+1));
    strcpy(obj->deviceName, deviceName);

    return obj;

}

interface_obj * interface_construct_terminal(void) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_terminal;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) NULL;

    return obj;

}

interface_obj * interface_construct_aaudio(void) {

    interface_obj * obj;

    obj = (interface_obj *) malloc(sizeof(interface_obj));

    obj->type = interface_aaudio;
    obj->fileName = (char *) NULL;
    obj->ip = (char *) NULL;
    obj->port = 0;
    obj->deviceName = (char *) NULL;

    return obj;

}

interface_obj * interface_clone(const interface_obj * obj) {

    interface_obj * clone;

    clone = interface_construct();

    clone->type = obj->type;        

    if (obj->type == interface_file) {
        
        clone->fileName = (char *) malloc(sizeof(char) * (strlen(obj->fileName) + 1));
        strcpy(clone->fileName, obj->fileName);

    }

    if (obj->type == interface_socket) {
        
        clone->ip = (char *) malloc(sizeof(char) * (strlen(obj->ip) + 1));
        strcpy(clone->ip, obj->ip);
        clone->port = obj->port;

    }

    if (obj->type == interface_soundcard || obj->type == interface_pulseaudio) {
        
        clone->deviceName = (char *) malloc(sizeof(char) * (strlen(obj->deviceName) + 1));
        strcpy(clone->deviceName, obj->deviceName);

    }

    return clone;

}

void interface_destroy(interface_obj * obj) {

    if (obj->fileName != NULL) {
        free((void *) obj->fileName);
    }

    if (obj->deviceName != NULL) {
        free((void *) obj->deviceName);
    }

    if (obj->ip != NULL) {
        free((void *) obj->ip);
    }

    free((void *) obj);

}

void interface_printf(const interface_obj * obj) {

    if (obj != NULL) {

        switch(obj->type) {

            case interface_blackhole:
                printf("type = blackhole\n");
            break;

            case interface_file:
                printf("type = file, fileName = %s\n",obj->fileName);
            break;

            case interface_socket:
                printf("type = socket, ip = %s, port = %u\n",obj->ip,obj->port);
            break;

            case interface_soundcard:
                printf("type = soundcard_name, devicename = %s\n",obj->deviceName);
            break;

            case interface_pulseaudio:
                printf("type = pulseaudio, devicename = %s\n", obj->deviceName);
            break;

            case interface_terminal:
                printf("type = terminal\n");
            break;

            case interface_aaudio:
                printf("type = aaudio\n");
            break;

            default:
                printf("type = undefined\n");
            break;

        }

    }
    else {

        printf("(null)\n");

    }

}
