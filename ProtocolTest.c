/** 
 * @file ProtocolTest.c
 * @date 2023/7/17~2023/7/26
 * @author Minho Shin (smh9800@g.skku.edu)
 * @version 0.0.0.1
 * @brief Fastech 프로그램의 Protocol Test 구현을 위한 프로그램
 * @details C언어와 GTK3(라즈비안(데비안11) 호환을 위해서), GLADE(UI XML->.glade파일) 사용
 * Ethernet 부분(Ezi Servo Plus-E 모델용)만 구현, 
 * 라이브러리로 분리할 만한 기본 함수, GUI프로그램 구현 함수가 섞인 상태
 * @warning 동작 시 예외처리가 제대로 안되어있으니 정확한 절차로만 작동시킬것
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "ReturnCodes_Define.h"



/************************************************************************************************************************************
 ********************************나중에 라이브러리로 뺄 FASTECH 라이브러리와 같은 기능의 함수*************************************************
 ************************************************************************************************************************************/
 
typedef uint8_t BYTE;

#define BUFFER_SIZE 258
#define PORT 3001 //UDP GUI

int client_socket;
struct sockaddr_in server_addr;

static BYTE header, sync_no, frame_type, data;
static BYTE buffer[BUFFER_SIZE];

char *protocol;

bool FAS_Connect(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID);
bool FAS_ConnectTCP(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID);

void FAS_Close(int iBdID);

int FAS_ServoEnable(int iBdID, bool bOnOff);
int FAS_MoveOriginSingleAxis(int iBdID);
int FAS_MoveStop(int iBdID);

/************************************************************************************************************************************
 ***************************GUI 프로그램의 버튼 등 구성요소들에서 사용하는 callback등 여러 함수***********************************************
 ************************************************************************************************************************************/
static void on_button_connect_clicked(GtkButton *button, gpointer user_data);
static void on_button_send_clicked(GtkButton *button, gpointer user_data);

static void on_combo_protocol_changed(GtkComboBoxText *combo_text, gpointer user_data);
static void on_combo_command_changed(GtkComboBox *combo_id, gpointer user_data);
static void on_combo_data1_changed(GtkComboBox *combo_id, gpointer user_data);

static void on_check_autosync_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_check_fastech_toggled(GtkToggleButton *togglebutton, gpointer user_data);

/************************************************************************************************************************************
 ******************************************************* 편의상 만든 함수 **************************************************************
 ************************************************************************************************************************************/
 
GtkWidget *text_sendbuffer;
GtkWidget *text_monitor1;
GtkWidget *text_monitor2;
GtkTextBuffer *sendbuffer_buffer;
GtkTextBuffer *monitor1_buffer;
GtkTextBuffer *monitor2_buffer;
 
void print_buffer(uint8_t *array, size_t size);
void library_interface();
char *command_interface();
char *FMM_interface(FMM_ERROR error);
char* array_to_string(const unsigned char *array, int size);

 /**@brief Main 함수*/
int main(int argc, char *argv[]) {
    GtkBuilder *builder;
    GtkWidget *window;
    GObject *button;
    GtkComboBoxText *combo_text;
    GtkComboBox* combo_id;
    GObject *checkbox;
    GError *error = NULL;
    
    srand(time(NULL));

    header = 0xAA;
    sync_no = (BYTE)(rand() % 256);
    
    // GTK 초기화
    gtk_init(&argc, &argv);

    // GtkBuilder 생성
    builder = gtk_builder_new();

    // XML file에서 UI불러오기
    if (!gtk_builder_add_from_file(builder, "ProtocolTest.glade", &error)) {
        g_printerr("Error loading UI file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    // Window 생성
    window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_widget_show_all(window);
    
    GtkStack *stk2 = GTK_STACK(gtk_builder_get_object(builder, "stk2"));
    
    text_monitor1 = GTK_WIDGET(gtk_builder_get_object(builder, "text_monitor1"));
    monitor1_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_monitor1));
    text_monitor2 = GTK_WIDGET(gtk_builder_get_object(builder, "text_monitor2"));
    monitor2_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_monitor2));
    text_sendbuffer = GTK_WIDGET(gtk_builder_get_object(builder, "text_sendbuffer"));
    sendbuffer_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_sendbuffer));
    
    // callback 함수 연결, user_data를 빌더로 사용함
    button = gtk_builder_get_object(builder, "button_connect");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_connect_clicked), builder);
    button = gtk_builder_get_object(builder, "button_send");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_send_clicked), builder);
    
    combo_text = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_protocol"));
    g_signal_connect(combo_text, "changed", G_CALLBACK(on_combo_protocol_changed), NULL);
    
    combo_id = GTK_COMBO_BOX(gtk_builder_get_object(builder, "combo_command"));
    g_signal_connect(combo_id, "changed", G_CALLBACK(on_combo_command_changed), stk2);
    combo_id = GTK_COMBO_BOX(gtk_builder_get_object(builder, "combo_data1"));
    g_signal_connect(combo_id, "changed", G_CALLBACK(on_combo_data1_changed), NULL);


    checkbox = gtk_builder_get_object(builder, "check_autosync");
    g_signal_connect(checkbox, "toggled", G_CALLBACK(on_check_autosync_toggled), NULL);
    checkbox = gtk_builder_get_object(builder, "check_fastech");
    g_signal_connect(checkbox, "toggled", G_CALLBACK(on_check_fastech_toggled), NULL);

    // Start the GTK main loop
    gtk_main();

    return 0;
}

/************************************************************************************************************************************
 ********************************GUI 프로그램의 버튼 등 구성요소들에서 사용하는 callback함수*************************************************
 ************************************************************************************************************************************/
 
 /**@brief Connect버튼의 callback*/
static void on_button_connect_clicked(GtkButton *button, gpointer user_data) {
    BYTE sb1, sb2, sb3, sb4;
    
    // Get the GtkBuilder object passed as user data
    GtkBuilder *builder = GTK_BUILDER(user_data);

    // Get the entry widget by its ID
    GtkEntry *entry_ip = GTK_ENTRY(gtk_builder_get_object(builder, "entry_ip"));

    // Get the entered text from the entry
    const char *ip_text = gtk_entry_get_text(entry_ip);

    // Check if the IP is valid (For a simple example, let's assume it's valid if it's not empty)
    if (g_strcmp0(ip_text, "") != 0) {
        g_print("IP: %s\n", ip_text);

        // Parse and store IP address in BYTE format
        char *ip_copy = g_strdup(ip_text); // 문자열 복사
        char *token, *saveptr;

        for (int i = 0; i < 4; i++) {
            token = strtok_r(i == 0 ? ip_copy : NULL, ".", &saveptr);
            if (token == NULL) {
                g_print("Invalid IP format: %s\n", ip_text);
                g_free(ip_copy);
                return;
            }
            BYTE byte_val = atoi(token);
            switch (i) {
                case 0:
                    sb1 = byte_val;
                    break;
                case 1:
                    sb2 = byte_val;
                    break;
                case 2:
                    sb3 = byte_val;
                    break;
                case 3:
                    sb4 = byte_val;
                    break;
            }
        }
        g_print("Parsed IP: %d.%d.%d.%d\n", sb1, sb2, sb3, sb4);
        g_free(ip_copy);
    }
    else {
            g_print("Please enter a valid IP.\n");
    }
        
    if(strcmp(protocol, "TCP") == 0){
        FAS_ConnectTCP(sb1, sb2, sb3, sb4, 0);
        g_print("Selected Protocol: %s\n", protocol);
    }
    else if(strcmp(protocol, "UDP") == 0){
        FAS_Connect(sb1, sb2, sb3, sb4, 0);
        g_print("Selected Protocol: %s\n", protocol);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }
}

 /**@brief Send버튼의 callback*/
static void on_button_send_clicked(GtkButton *button, gpointer user_data){
    sync_no++;
    
    library_interface();
    
    int send_result = sendto(client_socket, buffer, buffer[1] + 2, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)); 
    if (send_result < 0) {
        perror("sendto failed");
    }
    while(1){
        ssize_t received_bytes = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (received_bytes < 0) {
            perror("recvfrom failed");
            continue;
        }

        // Print the received data in hexadecimal format
        printf("Server: ");
        for (ssize_t i = 0; i < received_bytes; i++) {
            printf("%02x ", (BYTE)buffer[i]);
        }
        printf("\n");
        FMM_ERROR errorCode = buffer[5];
        char *errorMsg = FMM_interface(errorCode);
        
        char *response_text = array_to_string(buffer, received_bytes);
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(monitor1_buffer, &iter);
        gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
        gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
        gtk_text_buffer_insert(monitor1_buffer, &iter, response_text, -1);
        char *command = command_interface();
        gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "[RECEIVE]", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "RESPONSE : ", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, errorMsg, -1);
        
        break;
    }
    memset(&buffer, 0, sizeof(buffer));
}

 /**@brief TCP/UDP 프로토콜 선택 콤보박스의 callback*/
static void on_combo_protocol_changed(GtkComboBoxText *combo_text, gpointer user_data) {
    protocol = gtk_combo_box_text_get_active_text(combo_text);
    if (protocol != NULL) {
        g_print("Selected Protocol: %s\n", protocol);
    }
}

 /**@brief 명령어 콤보박스 combo_command의 callback*/
static void on_combo_command_changed(GtkComboBox *combo_id, gpointer user_data) {
    const gchar *selected_id = gtk_combo_box_get_active_id(combo_id);
    
    if (selected_id != NULL) {
        GtkStack *stk2 = GTK_STACK(user_data);

        // 선택한 옵션에 따라 보여지는 페이지를 변경
        if (g_strcmp0(selected_id, "0x2A") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page1");
        } 
        else if (g_strcmp0(selected_id, "0x31") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page0");
        }
        else if (g_strcmp0(selected_id, "0x33") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page0");
        }
        else if (g_strcmp0(selected_id, "0x37") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page2");
        }
    }
    
    if (selected_id  != NULL) {
        g_print("Selected Command: %s\n", selected_id);
        char* endptr;
        unsigned long int value = strtoul(selected_id, &endptr, 16);
        if (*endptr == '\0' && value <= UINT8_MAX) {
            frame_type = (uint8_t)value;
        } else {
            g_print("Invalid input: %s\n", selected_id);
        }
    } else {
        g_print("No item selected.\n");
    }
    g_print("Converted Frame: %X \n", frame_type);
}

 /**@brief 명령어 콤보박스 combo_data1의 callback*/
static void on_combo_data1_changed(GtkComboBox *combo_id, gpointer user_data) {
    const gchar *selected_id = gtk_combo_box_get_active_id(combo_id);

    if (selected_id != NULL) {
        g_print("Selected Data: %s\n", selected_id);
        char* endptr;
        unsigned long int value = strtoul(selected_id, &endptr, 16);
        if (*endptr == '\0' && value <= UINT8_MAX) {
            data = (uint8_t)value;
        } else {
            g_print("Invalid input: %s\n", selected_id);
        }
    } else {
        g_print("No item selected.\n");
    }
    g_print("Converted Data: %X \n", data);
}

 /**@brief AutoSync 체크박스의 callback*/
static void on_check_autosync_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    gboolean is_checked = gtk_toggle_button_get_active(togglebutton);
    if (is_checked) {
        sync_no = (BYTE)(rand() % 256);
        g_print("Auto Sync Enabled Sync No: %X \n",sync_no);
    } else {
        sync_no = 0x00;
        g_print("Auto Sync Disabled Sync No: %X \n",sync_no);
    }
}

 /**@brief FASTECH 프로토콜 체크박스의 callback*/
static void on_check_fastech_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    gboolean is_checked = gtk_toggle_button_get_active(togglebutton);
    if (is_checked) {
        header = 0xAA;
        g_print("FASTECH Protocol header: %X \n",header);
    } else {
        header = 0x00;
        g_print("USER Protocol header: %X \n",header);
    }
}


/************************************************************************************************************************************
 ********************************나중에 라이브러리로 뺄 FASTECH 라이브러리와 같은 기능의 함수*************************************************
 ************************************************************************************************************************************/
 
 /**@brief UDP 연결 시 사용
  * @param BYTE sb1,sb2,sb3,sb4 IPv4주소 입력 시 각 자리
  * @param int iBdID 드라이브 ID
  * @return boolean 성공시 TRUE 실패시 FALSE*/
bool FAS_Connect(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID){
    char SERVER_IP[16]; //최대 길이 가정 "xxx.xxx.xxx.xxx\0" 
    snprintf(SERVER_IP, sizeof(SERVER_IP), "%u.%u.%u.%u", sb1, sb2, sb3, sb4);
    
    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    else{
        g_print("Socket created\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported\n");
        exit(EXIT_FAILURE);
    }
    else{
        g_print("Valid address\n");
    }
}

 /**@brief TCP 연결 시 사용
  * @param BYTE sb1,sb2,sb3,sb4 IPv4주소 입력 시 각 자리
  * @param int iBdID 드라이브 ID
  * @return boolean 성공시 TRUE 실패시 FALSE*/
bool FAS_ConnectTCP(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID){
    char SERVER_IP[16]; //최대 길이 가정 "xxx.xxx.xxx.xxx\0" 
    snprintf(SERVER_IP, sizeof(SERVER_IP), "%u.%u.%u.%u", sb1, sb2, sb3, sb4);
    
    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    else{
        g_print("Socket created\n");
    }

    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    else{
        g_print("Connection Success\n");
    }
}

 /**@brief 연결 해제 시 사용
  * @param int iBdID 드라이브 ID */
void FAS_Close(int iBdID){
    close(client_socket);
}

 /**@brief Servo의 상태를 ON/OFF
  * @param int iBdID 드라이브 ID 
  * @param bool bOnOff Enable/Disable
  * @return 명령이 수행된 정보*/
int FAS_ServoEnable(int iBdID, bool bOnOff){
    buffer[0] = header; buffer[1] = 0x04; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type; buffer[5] = data;
}

 /**@brief Servo를 천천히 멈추는 기능
  * @param int iBdID 드라이브 ID
  * @return 명령이 수행된 정보*/
int FAS_MoveStop(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
}

 /**@brief 시스템의 원점을 찾는 기능?
  * @param int iBdID 드라이브 ID
  * @return 명령이 수행된 정보*/
int FAS_MoveOriginSingleAxis(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
}


/************************************************************************************************************************************
 ******************************************************* 편의상 만든 함수 **************************************************************
 ************************************************************************************************************************************/
 
 /**@brief 명령전달에 쓰는 버퍼 내용을 터미널에 일단 보여주는 함수*/
 void print_buffer(uint8_t *array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", array[i]);
    }
    printf("\n");
}

/**@brief  배열을 문자열로 변환하는 함수*/
char* array_to_string(const uint8_t *array, int size) {
    char *str = g_strdup_printf("%02X", array[0]); // 첫 번째 요소는 따로 처리

    char *temp;
    for (int i = 1; i < size; i++) { // 첫 번째 요소 이후의 요소들 처리
        temp = g_strdup_printf("%s %02X", str, array[i]);
        g_free(str);
        str = temp;
    }

    return str;
}

 /**@brief 함수들을 찾아가게하는 인터페이스 용도 함수*/
void library_interface(){
    switch(frame_type)
    {
        case 0x2A:
            FAS_ServoEnable(0, 0);
            break;
        case 0x31:
            FAS_MoveStop(0);
            break;
        case 0x33:
            FAS_MoveOriginSingleAxis(0);
            break;
    }
    size_t data_size = buffer[1] + 2;
    print_buffer(buffer, data_size);
    
    char *text = array_to_string(buffer, buffer[1] + 2);
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
    gtk_text_buffer_set_text(monitor1_buffer, text, -1);
    
    char *command = command_interface();
    gtk_text_buffer_set_text(monitor2_buffer, "[SEND]", -1);
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
    gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
    gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
    gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
    gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
}

 /**@brief 각 명령어의 함수 이름을 찾아가는 인터페이스 용도 함수*/
char *command_interface(){
    switch(frame_type)
    {
        case 0x2A:
            return "FAS_ServoEnable";
        case 0x31:
            return "FAS_MoveStop";
        case 0x33:
            return "FAS_MoveOriginSingleAxis";
        default:
            return "Transfer Fail";
    }
}

 /**@brief 통신상태에서 출력할 내용을 찾아가는 인터페이스 용도 함수*/
char* FMM_interface(FMM_ERROR error) {
    switch (error) {
        case FMM_OK:
            return "FMM_OK";
        case FMM_NOT_OPEN:
            return "FMM_NOT_OPEN";
        case FMM_INVALID_PORT_NUM:
            return "FMM_INVALID_PORT_NUM";
        case FMM_INVALID_SLAVE_NUM:
            return "FMM_INVALID_SLAVE_NUM";
        case FMC_DISCONNECTED:
            return "FMC_DISCONNECTED";
        case FMC_TIMEOUT_ERROR:
            return "FMC_TIMEOUT_ERROR";
        case FMC_CRCFAILED_ERROR:
            return "FMC_CRCFAILED_ERROR";
        case FMC_RECVPACKET_ERROR:
            return "FMC_RECVPACKET_ERROR";
        case FMM_POSTABLE_ERROR:
            return "FMM_POSTABLE_ERROR";
        case FMP_FRAMETYPEERROR:
            return "FMP_FRAMETYPEERROR";
        case FMP_DATAERROR:
            return "FMP_DATAERROR";
        case FMP_PACKETERROR:
            return "FMP_PACKETERROR";
        case FMP_RUNFAIL:
            return "FMP_RUNFAIL";
        case FMP_RESETFAIL:
            return "FMP_RESETFAIL";
        case FMP_SERVOONFAIL1:
            return "FMP_SERVOONFAIL1";
        case FMP_SERVOONFAIL2:
            return "FMP_SERVOONFAIL2";
        case FMP_SERVOONFAIL3:
            return "FMP_SERVOONFAIL3";
        case FMP_SERVOOFF_FAIL:
            return "FMP_SERVOOFF_FAIL";
        case FMP_ROMACCESS:
            return "FMP_ROMACCESS";
        case FMP_PACKETCRCERROR:
            return "FMP_PACKETCRCERROR";
        case FMM_UNKNOWN_ERROR:
            return "FMM_UNKNOWN_ERROR";
        default:
            return "Unknown error";
    }
}
