#if (SUBCORE != 3)  // SubCoreの指定
#error "Core selection is wrong!!"
#endif

#include <GS2200Hal.h>
#include <GS2200AtCmd.h>
#include <TelitWiFi.h>
#include "config.h"
#include <MP.h>
#include <vector>

#define  CONSOLE_BAUDRATE  2000000

// ここに受信したデータが入る
extern uint8_t ESCBuffer[];
extern uint32_t ESCBufferCnt;
/////////////////////////////////////////////////////////////////////////
char* pc_msg;
#define MSGLEN      200
#define MY_MSGID    10
struct MyPacket {
  volatile int status; /* 0:ready, 1:busy */
  char message[MSGLEN];
};
MyPacket packet;

TelitWiFi gs2200;
TWIFI_Params gsparams;
/////////////////////////////////////////////////////////////////////////
void setup() {

  /* initialize digital pin LED_BUILTIN as an output. */
  pinMode(LED0, OUTPUT);
  digitalWrite( LED0, LOW );   // turn the LED off (LOW is the voltage level)
  Serial.begin( CONSOLE_BAUDRATE ); // talk to PC

  /* Initialize SPI access of GS2200 */
  Init_GS2200_SPI_type(iS110B_TypeC);//typeCに変更

  /* Initialize AT Command Library Buffer */
    gsparams.mode = ATCMD_MODE_LIMITED_AP;//アクセスポイントモード
  //gsparams.mode = ATCMD_MODE_STATION;//ステーションモード
  gsparams.psave = ATCMD_PSAVE_DEFAULT;
  if( gs2200.begin( gsparams ) ){
    ConsoleLog( "GS2200 Initilization Fails" );
    while(1);
  }

    /* GS2200 runs as AP *///アクセスポイントモード
    if( gs2200.activate_ap( AP_SSID, PASSPHRASE, AP_CHANNEL ) ){
      ConsoleLog( "WiFi Network Fails" );
      while(1);
    }
    /* GS2200 Association to AP *///ステーションモード
  /*if( gs2200.activate_station( AP_SSID, PASSPHRASE ) ){
    ConsoleLog( "Association Fails" );
    while(1);
  }*/

  digitalWrite( LED0, HIGH ); // turn on LED

    memset(&packet, 0, sizeof(packet));
    int      ret3;
    ret3 = MP.begin();
    MP.RecvTimeout(MP_RECV_BLOCKING);  
    //　起動エラー
    if (ret3 < 0) 
        printf("SubCore3 start error = %d\n", ret3);
    delay(50);
}

// the loop function runs over and over again forever
void loop() 
{
  ATCMD_RESP_E resp;
  char server_cid = 0, remote_cid=0;
  ATCMD_NetworkStatus networkStatus;
  uint32_t timer=0;


  resp = ATCMD_RESP_UNMATCH;
  ConsoleLog( "Start TCP Server");
  
  resp = AtCmd_NSTCP( TCPSRVR_PORT, &server_cid);
  if (resp != ATCMD_RESP_OK) {
    ConsoleLog( "No Connect!" );
    delay(2000);
    return;
  }

  if (server_cid == ATCMD_INVALID_CID) {
    ConsoleLog( "No CID!" );
    delay(2000);
    return;
  }
    
  while( 1 )
  {
    ConsoleLog( "Waiting for TCP Client");

    if( ATCMD_RESP_TCP_SERVER_CONNECT != WaitForTCPConnection( &remote_cid, 5000 ) ){
      continue;
    }

    ConsoleLog( "TCP Client Connected");
    // Prepare for the next chunck of incoming data
    WiFi_InitESCBuffer();
    
    // Start the echo server
      int loopcnt=0;
    while( loopcnt++<100000 ){
      while( Get_GPIO37Status() ){
        resp = AtCmd_RecvResponse();
        loopcnt=0;
        if( ATCMD_RESP_BULK_DATA_RX == resp ){
          if( Check_CID( remote_cid ) )
          {
            //ConsolePrintf( "Received : %s\r\n", ESCBuffer+1 );   
                  pc_msg = ESCBuffer+1;
                  printf("pc_msg : %s\n",pc_msg);
            int    ret3;
            if (packet.status == 0) 
            {
              packet.status = 1;
              snprintf(packet.message, MSGLEN, "%s",pc_msg);
              ret3 = MP.Send(MY_MSGID, &packet);
              if (ret3 < 0) {
                printf("MP.Send error = %d\n", ret3);
              }
            }
            if( ATCMD_RESP_OK != AtCmd_SendBulkData( remote_cid, ESCBuffer+1, ESCBufferCnt-1 ) )
            {
              // Data is not sent, we need to re-send the data
                      ConsolePrintf( "Sent Error : %s\r\n", ESCBuffer+1 );
              delay(10);
            }
          }
          WiFi_InitESCBuffer();
        }
        
      }
    }    
  }
}
