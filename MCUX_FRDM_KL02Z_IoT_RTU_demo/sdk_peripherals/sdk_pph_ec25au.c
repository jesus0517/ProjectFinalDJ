
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "stdio.h"
#include "stdlib.h"
#include "sdk_pph_ec25au.h"
#include "sdk_mdlw_leds.h"



/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct _estado_fsm{
	uint8_t anterior;
	uint8_t actual;
}estado_fsm_t;

enum _ec25_lista_ordendes{
	kORDEN_NO_HAY_EN_EJECUCION=0,
	kORDEN_ENVIAR_MENSAJE_DE_TEXTO,
	ENVIAR_MENSAJE_MQTT,
};

#define EC25_BYTES_EN_BUFFER	100
#define BUFFER_MQTT 200
/*******************************************************************************
 * Private Prototypes
 ******************************************************************************/
void ec25BorrarBufferRX(void);

/*******************************************************************************
 * External vars
 ******************************************************************************/
//extern uint8_t ec25_mensaje_de_texto[]="Hola desde EC25";
/*******************************************************************************
 * Local vars
 ******************************************************************************/
char mensaje_mqtt[BUFFER_MQTT];
uint8_t ec25_buffer_tx[EC25_BYTES_EN_BUFFER];		//almacena los datos a enviar al modem
uint8_t ec25_buffer_rx[EC25_BYTES_EN_BUFFER];		//almacena las datos que provienen del modem
uint8_t ec25_index_buffer_rx = 0;				//apuntador de buffer de datos

estado_fsm_t ec25_fsm;	//almacena estado actual de la maquina de estados modem EC25

uint32_t ec25_timeout;	//almacena las veces que han llamado la fsm del EC25 y estimar tiempo

uint8_t ec25_comando_en_ejecucion;	//almacena ultimo comando enviado para ser ejecutado

//Listado de comando AT disponibles para ser enviados al modem Quectel
const char *ec25_comandos_at[] = {
		    "AT",			//comprueba disponibilidad de dispositivo
			"ATI",			//consulta informaci??n del modem
			"AT+CPIN?",		//consulta estado de la simcard
			"AT+QCFG=\"nwscanmode\",0,1",                           // configura las bandas
			"AT+QCFG=\"band\",0, 800005A,0",                        //configuracion de bandas
			"AT+QCSQ",
			"AT+CREG?",		                                        //consulta estado de la red celular y tecnolog??a usada en red celular
			"AT+CGREG?",
			"AT+CEREG?",
			"AT+CGDCONT=1,\"IP\",\"web.colombiamovil.com.co\"",     //configuracion de APN
			"AT+QIACT=1",
			"AT+QIACT?",
			"AT+QMTOPEN=0,\"20.42.115.242\",1883",					// direccion ip del servidor
			"AT+QMTCONN=0,\"modem\"[,\"guest\",\"guest\"]",								// suscripcion a
			"AT+QMTPUB=0,1,1,0,\"1/sensor\"",
			"MENSAJE MQTT", 		//MENSAJE & CTRL+Z
			"AT+CFUN=0",
			"AT+CFUN=1",
			"AT+CSQ", //consulta calidad de la se??al RSSI

	};

//Lista de respuestas a cada comando AT
const char  *ec25_repuestas_at[]={
		        "OK",		//AT
				"EC25",		//ATI
				"READY",	//AT+CPIN?
				"OK",		//AT+QCFG=\"nwscanmode\",0,1
				"OK",		//AT+QCFG=\"band\",0, 800005A,0
				"\"WCDMA\"",
				"0,1",		//AT+CREG? = GSM,REGISTERED
				"0,1",
				"0,1",
				"OK",		//AT+CGDCONT=1,\"IP\",\"internet.movistar.com.co\"
				"OK",
				"1,1,1",	//AT+QIACT?
				"QMTOPEN: 0,0",		//AT+QMTOPEN=0,\"142.93.88.99\",1883
				"QMTCONN: 0,0,0",		//AT+QMTCONN=0,\"LAB1\"
				">",		//AT+QMTPUB=0,0,0,0,\"LAB1\"
				"OK",		//MENSAJE & CTRL+Z
				"pdpdeact",
				"OK",
				"+CSQ:",		//AT+CSQ

};


/*******************************************************************************
 * Private Source Code
 ******************************************************************************/
//------------------------------------
void ec25BorrarBufferRX(void){
	uint8_t i;

	//LLenar de ceros buffer que va a recibir datos provenientes del modem
	for(i=0;i<EC25_BYTES_EN_BUFFER;i++){
		ec25_buffer_rx[i]=0;
	}

	//borra apuntador de posicion donde se va a almacenar el proximo dato
	//Reset al apuntador
	ec25_index_buffer_rx=0;
}
//------------------------------------
void ec25BorrarBufferTX(void){
	uint8_t i;

	//LLenar de ceros buffer que va a recibir datos provenientes del modem
	for(i=0;i<EC25_BYTES_EN_BUFFER;i++){
		ec25_buffer_tx[i]=0;
	}
}
//------------------------------------
void waytTimeModem(void) {
	uint32_t tiempo = 0xFFFF;
	do {
		tiempo--;
	} while (tiempo != 0x0000);
}
//------------------------------------
void ec25EnviarComandoAT(uint8_t comando){
	char comando_at[EC25_BYTES_EN_BUFFER];
	sprintf(comando_at,"%s\r\n", ec25_comandos_at[comando]);	//Prepara en buffercomandoa enviar al modem
	uart0ImprimirMensaje((uint8_t *)(&comando_at[0]),strlen(comando_at));	//Envia comando AT indicado
}


//------------------------------------
status_t ec25ProcesarRespuestaAT(uint8_t comando){
	status_t resultado_procesamiento;	//variable que almacenar?? el resultado del procesamiento
	uint8_t *puntero_ok=0;	//variable temporal que ser?? usada para buscar respuesta

	switch(ec25_fsm.anterior){
	case kFSM_ENVIANDO_AT:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),(char*) (ec25_repuestas_at[kAT])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}

		break;
	case kFSM_ENVIANDO_ATI:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kATI])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;
	case kFSM_ENVIANDO_CPIN:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CPIN])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QCFG_CONFIG1:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
		    	(char*) (ec25_repuestas_at[kAT_QCFG_CONFIG1])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
	    	printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QCFG_CONFIG2:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QCFG_CONFIG2])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QCSQ:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QCSQ])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_CREG:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CREG])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;
	case kFSM_ENVIANDO_CGREG:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CGREG])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
	    break;

	case kFSM_ENVIANDO_CEREG:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CEREG])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
	    break;

	case kFSM_ENVIANDO_CGDCONT:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CGDCONT])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
	    break;

	case kFSM_ENVIANDO_QIACT_1:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QIACT_1])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("eRROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QIACT:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QIACT])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QMTOPEN:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QMTOPEN])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR DE CONEXION MQTT\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kFSM_ENVIANDO_QMTCONN:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_QMTCONN])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;

	case kSFM_PUBLICA_TEM_HUM:
			//Busca palabra EC25 en buffer rx de quectel
			puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
					(char*) (ec25_repuestas_at[kAT_QMTPUB_T_H])));

			if(puntero_ok!=0x00){
				printf("OK\r\n");
				resultado_procesamiento=kStatus_Success;
			}else{
				printf("ERROR\r\n");
				resultado_procesamiento=kStatus_Fail;
			}
			break;

	case kFSM_ENVIA_MSJ_MQTT:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_MQTT_MSJ_T_H])));

		if(puntero_ok!=0x00){
			printf("OK\r\n");
			resultado_procesamiento=kStatus_Success;
		}else{
			printf("ERROR\r\n");
			resultado_procesamiento=kStatus_Fail;
		}
		break;


	case kFSM_ENVIANDO_CSQ:
		//Busca palabra EC25 en buffer rx de quectel
		puntero_ok = (uint8_t*) (strstr((char*) (&ec25_buffer_rx[0]),
				(char*) (ec25_repuestas_at[kAT_CSQ])));

		if(puntero_ok!=0x00){
			//la respuesta a AT+CSQ incluye dos parametros RSSI,BER
			//el valor de interes para la aplicaci??n es RSSI
			char string_data[4];
			int8_t rssi;	// received signal strength
			uint8_t i;		//usada para borrar los daot s

			//borra buffer que va a almacenar string
			for(i=0;i<sizeof(string_data);i++){
				string_data[i]=0;
			}

			memcpy(&string_data[0],puntero_ok+5, 3);	//copia los bytes que corresponden al RSSI (3 digitos)
			rssi=(int8_t)atoi(&string_data[0]);	//convierte string a entero

			if((rssi>EC25_RSSI_MINIMO_ACEPTADO)&&(rssi!=99)){
				resultado_procesamiento=kStatus_Success;
				printf("OK\r\n");
			}else{
				resultado_procesamiento=kStatus_OutOfRange;
				printf("FUERA DE RANGO\r\n");
			}
		}else{
			resultado_procesamiento=kStatus_Fail;
			printf("ERROR\r\n");
		}
	break;

	default:
		//para evitar bloqueos, si se le especifica un comando incorrecto, genera error
		resultado_procesamiento=kStatus_Fail;
		break;
	}
	return(resultado_procesamiento);
}
/*******************************************************************************
 * Public Source Code
 ******************************************************************************/
status_t ec25Inicializacion(void){
	ec25_fsm.anterior=kFSM_INICIO;	//reinicia estado anterios
	ec25_fsm.actual=kFSM_INICIO;	//reinicia estado actual
	ec25_timeout=0;	//borra contador de tiempo
	ec25BorrarBufferTX();	//borrar buffer de transmisi??n
	ec25BorrarBufferRX();	//borra buffer de recepci??n
	ec25_comando_en_ejecucion=kORDEN_NO_HAY_EN_EJECUCION;	//Borra orden en ejecucion actual
	return(kStatus_Success);
}

status_t IniciodelPrograma(void){
	//ec25_fsm.anterior=kFSM_ENVIANDO_AT;
	ec25_timeout=0;	//borra contador de tiempo
	ec25BorrarBufferTX();	//borrar buffer de transmisi??n
	ec25BorrarBufferRX();	//borra buffer de recepci??n
	ec25_comando_en_ejecucion=ENVIAR_MENSAJE_MQTT;	//almacena cual es la orden que debe cumplir la FSM
	ec25_fsm.actual=kFSM_ENVIANDO_AT;	//inicia la FSM a rtabajar desde el primer comando AT
	return(kStatus_Success);

}

//------------------------------------
status_t ec25EnviarMensajeDeTexto(uint8_t *mensaje, uint8_t size_mensaje ){
	memcpy(&ec25_buffer_tx[0],mensaje, size_mensaje);	//copia mensaje a enviar en buffer TX del EC25
	ec25_comando_en_ejecucion=kORDEN_ENVIAR_MENSAJE_DE_TEXTO;	//almacena cual es la orden que debe cumplir la FSM
	ec25_fsm.actual=kFSM_ENVIANDO_AT;	//inicia la FSM a rtabajar desde el primer comando AT
	return(kStatus_Success);
}


void ImprimirMensaje(void){

	uart0ImprimirMensaje((&mensaje_mqtt),strlen(mensaje_mqtt));

}


status_t Lectura_sensor(float valor_temperatura, float valor_humedad){


	sprintf(mensaje_mqtt,"temperatura,%.2f,humedad,%.2f,\r\n %c",valor_temperatura,valor_humedad, 0x1A);


	return(kStatus_Success);
}


//------------------------------------
uint8_t ec25Polling(void){
	status_t resultado;
	uint8_t nuevo_byte_uart;

	switch (ec25_fsm.actual) {
	case kFSM_INICIO:
		//En este estado no hace nada y est?? a la espera de iniciar una nueva orden
		break;

	case kFSM_RESULTADO_ERROR:

	    break;

	case kFSM_RESULTADO_EXITOSO:
		//Se queda en este estado y solo se sale cuando se utilice la funci??n ec25Inicializacion();
		break;

	case kFSM_RESULTADO_ERROR_RSSI:
		//Se queda en este estado y solo se sale cuando se utilice la funci??n ec25Inicializacion();
		break;

	case kFSM_ENVIANDO_AT:
		printf("Enviando AT:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT);	//Envia comando AT
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_ATI:
		printf("Enviando ATI:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kATI);	//Envia comando AT
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CPIN:
		printf("Enviando CPIN?:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CPIN);	//Envia comando AT+CPIN?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QCFG_CONFIG1:
		printf("Enviando AT+QCFG=\"nwscanmode\",0,1:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QCFG_CONFIG1);	//Envia comando AT+CREG?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QCFG_CONFIG2:
		printf("Enviando AT+QCFG=\"band\",0, 800005A,0:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QCFG_CONFIG2);	//Envia comando AT+CREG?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QCSQ:
		printf("Enviando AT+QCSQ:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QCSQ);	//Envia comando AT+CSQ
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CREG:
		printf("Enviando AT+CREG:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CREG);	//Envia comando AT+CREG?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CGREG:
		printf("Enviando AT+CGREG:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CGREG);	//Envia comando AT+CREG?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CEREG:
		printf("Enviando AT+CEREG:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CEREG);	//Envia comando AT+CREG?
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CGDCONT:
		printf("Enviando AT+CGDCONT=1,\"IP\",\"web.colombiamovil.com.co\":");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CGDCONT);	//Envia comando AT+CMGS="3003564960"
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QIACT_1:
		printf("Enviando AT+QIACT=1:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QIACT_1);	//Envia comando AT+CMGS="3003564960"
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QIACT:
		printf("Enviando AT+QIACT?:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QIACT);	//Envia comando AT+CMGS="3003564960"
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QMTOPEN:
		printf("Enviando AT+QMTOPEN=0,\"20.84.105.98\",1883:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QMTOPEN);	//Envia comando AT+CMGS="3003564960"
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_QMTCONN:
		printf("Enviando AT+QMTCONN=0,\"modem\"[,\"guest\",\"guest\"]:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QMTCONN);	//Envia comando AT+CMGS="3003564960"
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kSFM_PUBLICA_TEM_HUM:
		printf("Enviando AT+QMTPUB=0,1,1,0,\"1/temperatura\":");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_QMTPUB_T_H);
		//ec25EnviarComandoATMQTT(kAT_QMTPUB_TEMP);
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIA_MSJ_MQTT:
		printf("Enviando temperatura:");
		ImprimirMensaje();
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ENVIANDO_CSQ:
		printf("Enviando AT+CSQ:");
		ec25BorrarBufferRX();	//limpia buffer para recibir datos de quectel
		ec25EnviarComandoAT(kAT_CSQ);
		ec25_fsm.anterior = ec25_fsm.actual;		//almacena el estado actual
		ec25_fsm.actual = kFSM_ESPERANDO_RESPUESTA;	//avanza a esperar respuesta del modem
		ec25_timeout = 0;	//reset a contador de tiempo
		break;

	case kFSM_ESPERANDO_RESPUESTA:
		ec25_timeout++;	//incrementa contador de tiempo

		//Busca si llegaron nuevos datos desde modem mientras esperaba
		while (uart0CuantosDatosHayEnBuffer() > 0) {
			resultado = uart0LeerByteDesdeBuffer(&nuevo_byte_uart);
			//si fueron encontrados nuevos bytes en UART entonces los mueve al buffer del EC25
			if (resultado == kStatus_Success) {
				//reinicia contador de tiempo
				ec25_timeout=0;
				//almacena dato en buffer rx de quectel
				ec25_buffer_rx[ec25_index_buffer_rx] = nuevo_byte_uart;
				//incrementa apuntador de datos en buffer de quectel
				ec25_index_buffer_rx++;
			}
		}

		//pregunta si el tiempo de espera supera el configurado
		if (ec25_timeout > EC25_TIEMPO_MAXIMO_ESPERA) {
			ec25_fsm.actual = kFSM_PROCESANDO_RESPUESTA;
		}
		break;

	case kFSM_PROCESANDO_RESPUESTA:
		//procesa respuesta dependiendo de cual comando AT se le hab??a enviado al modem
		resultado = ec25ProcesarRespuestaAT(ec25_fsm.anterior);
		//Si la respuesta al comando AT es correcta (kStatus_Success), avanza al siguiente resultado
		switch (resultado) {
		case kStatus_Success:
			//el siguiente estado depende del estado anterior
			switch (ec25_fsm.anterior) {
			case kFSM_ENVIANDO_AT:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_ATI;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_ATI:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el resultado actual
				ec25_fsm.actual = kFSM_ENVIANDO_CPIN;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_CPIN:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QCFG_CONFIG1;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QCFG_CONFIG1:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el resultado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QCFG_CONFIG2;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QCFG_CONFIG2:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el resultado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QCSQ;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QCSQ:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el resultado actual
				ec25_fsm.actual = kFSM_ENVIANDO_CREG;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_CREG:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_CGREG;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_CGREG:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_CEREG;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_CEREG:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_CGDCONT;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_CGDCONT:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el resultado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QIACT_1;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QIACT_1:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QIACT;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QIACT:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QMTOPEN;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QMTOPEN:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_QMTCONN;//avanza a enviar nuevo comando al modem

				break;

			case kFSM_ENVIANDO_QMTCONN:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
 			    ec25_fsm.actual = kSFM_PUBLICA_TEM_HUM;//avanza a enviar nuevo comando al modem
				break;

			case kSFM_PUBLICA_TEM_HUM:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
			    ec25_fsm.actual = kFSM_ENVIA_MSJ_MQTT;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIA_MSJ_MQTT:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kSFM_PUBLICA_TEM_HUM;//avanza a enviar nuevo comando al modem
				ec25BorrarBufferTX();
				break;

			case kFSM_ENVIANDO_CSQ:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
				ec25_fsm.actual = kFSM_ENVIANDO_AT;//avanza a enviar nuevo comando al modem
				break;

			case kFSM_ENVIANDO_QLTS_2:
				ec25_fsm.anterior = ec25_fsm.actual;//almacena el estado actual
			    ec25_fsm.actual = kFSM_ENVIANDO_AT;//avanza a enviar nuevo comando al modem
			    break;

			default:
				//para evitar bloqueos, marca error de proceso en caso de detectar un estado ilegal
				ec25_fsm.actual = kFSM_RESULTADO_ERROR;	//se queda en resultado de error
				break;
			}
			break;

		case kStatus_OutOfRange:
			//si la respuesta del analisis es fuera de rango, indica que el RSSI no se cumple
			ec25_fsm.actual = kFSM_RESULTADO_ERROR_RSSI;//se queda en resultado de error
			break;

		default:
			//Si la respuesta es incorrecta, se queda en resultado de error
			//No se cambia (ec25_fsm.anterior) para mantener en que comando AT fue que se gener?? error

			ec25_fsm.actual = kFSM_RESULTADO_ERROR;

			break;
		}
	}
	return(ec25_fsm.actual);
}
