/*! @file : MCUX_FRDM_KL02Z_IoT_RTU_demo.c
 *
 * @version 1.0.0
 * @date    8/01/2021
 * @brief   Funcion principal main
 * @details
 *			v0.1 dato recibido por puerto COM es contestado en forma de ECO
 *			v0.2 dato recibido por puerto COM realiza operaciones especiales
 *					A/a=invierte estado de LED conectado en PTB10
 *					v=apaga LED conectado en PTB7
 *					V=enciende LED conectado en PTB7
 *					r=apaga LED conectado en PTB6
 *			v0.3 nuevo comando por puerto serial para prueba de MMA8451Q
 *					M=detecta acelerometro MM8451Q en bus I2C0
 *
 *
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL02Z4.h"
#include "fsl_debug_console.h"


#include "sdk_hal_uart0.h"
#include "sdk_hal_gpio.h"
#include "sdk_hal_i2c0.h"
#include "sdk_hal_i2c1.h"
#include "sdk_hal_adc.h"
#include "sdk_hal_lptmr0.h"

#include "sdk_mdlw_leds.h"
#include "sdk_pph_mma8451Q.h"
#include "sdk_pph_ec25au.h"
#include "sdk_pph_bme280.h"
#include "sdk_pph_sht3x.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define HABILITAR_MODEM_EC25		1
#define HABILITAR_SENSOR_BME280		1
#define HABILITAR_SENSOR_MMA8451Q	1
#define HABILITAR_ENTRADA_ADC_PTB8	1
#define HABILITAR_SENSOR_SHT3X		1

#define HABILITAR_TLPTMR0			1



/*******************************************************************************
 * Private Prototypes
 ******************************************************************************/


/*******************************************************************************
 * External vars
 ******************************************************************************/


/*******************************************************************************
 * Local vars
 ******************************************************************************/


/*******************************************************************************
 * Private Source Code
 ******************************************************************************/
void waytTime(void) {
//	uint32_t tiempo = 0x1FFFF;
//	do {
//		tiempo--;
//	} while (tiempo != 0x0000);


	//espera a que hayan ocurrido por lo menos 100ms interrupciones
	while (lptmr0GetTimeValue() < 100){
	}
	lptmr0SetTimeValue(0);		//Reset de variable contador de interrupciones
}

/*
 * @brief   Application entry point.
 */
int main(void) {
	uint8_t ec25_mensaje_de_texto[]="Hola desde EC25";
	uint8_t ec25_estado_actual;
	uint8_t ec25_detectado=0;

	uint32_t adc_dato;
	uint8_t adc_base_de_tiempo=0;

	mma8451_data_t	mma8451Q_datos;
	uint8_t mma8451Q_detectado=0;
	uint8_t mma8451Q_base_de_tiempo=0;

	bme280_data_t bme280_datos;
	uint8_t bme280_detectado=0;
	uint8_t bme280_base_de_tiempo=0;

	sht3x_data_t sht3x_datos;
	uint8_t sht3x_detectado=0;
	uint8_t sht3x_base_de_tiempo=0;

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    BOARD_InitDebugConsole();
#endif

    printf("Inicializa UART0:");
    //inicializa puerto UART0 y solo avanza si es exitoso el proceso
    if(uart0Inicializar(115200)!=kStatus_Success){	//115200bps
    	printf("Error");
    	return 0 ;
    };
    printf("OK\r\n");

    printf("Inicializa I2C0:");
    //inicializa puerto I2C0 y solo avanza si es exitoso el proceso
    if(i2c0MasterInit(100000)!=kStatus_Success){	//100kbps
    	printf("Error");
    	return 0 ;
    }
    printf("OK\r\n");

    printf("Inicializa I2C1:");
    //inicializa puerto I2C1 y solo avanza si es exitoso el proceso
    if(i2c1MasterInit(100000)!=kStatus_Success){	//100kbps
    	printf("Error");
    	return 0 ;
    }
    printf("OK\r\n");

#if HABILITAR_ENTRADA_ADC_PTB8
    //Inicializa conversor analogo a digital
    //Se debe usar  PinsTools para configurar los pines que van a ser analogicos
    printf("Inicializa ADC:");
    if(adcInit()!=kStatus_Success){
    	printf("Error");
    	return 0 ;
    }
    printf("OK\r\n");
#endif

#if HABILITAR_SENSOR_MMA8451Q
    printf("Detectando MMA8451Q:");
    //LLamado a funcion que identifica acelerometro MMA8451Q
    if (mma8451QWhoAmI() == kStatus_Success){
    	printf("OK\r\n");
    	(void)mma8451QInit();	//inicializa acelerometro MMA8451Q
    	mma8451Q_detectado=1;	//activa bandera que indica (SI HAY MM8451Q)
    }
#endif

#if HABILITAR_SENSOR_SHT3X
    printf("Detectando SHT3X:");
    //LLamado a funcion que identifica sensor SHT3X
    if(sht3xInit()== kStatus_Success){
    	sht3x_detectado=1;
    	printf("OK\r\n");
    }
#endif

#if HABILITAR_SENSOR_BME280
    printf("Detectando BME280:");
    //LLamado a funcion que identifica sensor BME280
    if (bme280WhoAmI() == kStatus_Success){
    	printf("OK\r\n");
    	(void)bme280Init();	//inicializa sensor bme280
    	bme280_detectado=1;	//activa bandera que indica (SI HAY BME280)
    }
#endif

#if HABILITAR_MODEM_EC25
    //Inicializa todas las funciones necesarias para trabajar con el modem EC25
    printf("Inicializa modem EC25\r\n");
    ec25Inicializacion();

    //Configura FSM de modem para enviar mensaje de texto
    printf("Enviando mensaje de texto por modem EC25\r\n");
    ec25EnviarMensajeDeTexto(&ec25_mensaje_de_texto[0], sizeof(ec25_mensaje_de_texto));
#endif

#if HABILITAR_TLPTMR0
    //Inicializa todas las funciones necesarias para trabajar con el modem EC25
    printf("Inicializa low power timer 0\r\n");

    lptmr0Init();
#endif

	//Ciclo infinito encendiendo y apagando led verde
	//inicia SUPERLOOP
    while(1) {
    	waytTime();		//base de tiempo fija aproximadamente 200ms


#if HABILITAR_ENTRADA_ADC_PTB8
    	adc_base_de_tiempo++;//incrementa base de tiempo para tomar una lectura ADC
    	if(adc_base_de_tiempo>10){	// >10 equivale aproximadamente a 2s
    		adc_base_de_tiempo=0;	//reinicia contador de tiempo
    		adcTomarCaptura(PTB8_ADC0_SE11_CH14, &adc_dato);	//inicia lectura por ADC y guarda en variable adc_dato
    		printf("ADC ->");
    		printf("PTB8:0x%X ",adc_dato);	//imprime resultado ADC
    		printf("\r\n");	//Imprime cambio de linea
    	}
#endif

#if HABILITAR_SENSOR_MMA8451Q
    	if(mma8451Q_detectado==1){	//Solo hace esto si preciamente fue detectado el acelerometro con el mma8451QWhoAmI();
        	mma8451Q_base_de_tiempo++; //incrementa base de tiempo para tomar dato acelerometro
        	if(mma8451Q_base_de_tiempo>10){	//	>10 equivale aproximadamente a 2s
        		mma8451Q_base_de_tiempo=0;	//reinicia contador de tiempo
        		if(mma8451QReadAccel(&mma8451Q_datos)==kStatus_Success){	//toma lectura de ejes X,Y,Z
        			printf("MMA8451Q ->");
        			printf("Accel_X:0x%X ",mma8451Q_datos.x_value);	//imprime aceleración X
        			printf("Accel_Y:0x%X ",mma8451Q_datos.y_value);	//imprime aceleración Y
        			printf("Accel_Z:0x%X ",mma8451Q_datos.z_value);	//imprime aceleración Z
        			printf("\r\n");	//Imprime cambio de linea
        		}
        	}
    	}
#endif

#if HABILITAR_SENSOR_BME280
    	if(bme280_detectado==1){
    		bme280_base_de_tiempo++;	//incrementa base de tiempo para tomar dato bme280
    		if(bme280_base_de_tiempo>10){	//	>10 equivale aproximadamente a 2s
    			bme280_base_de_tiempo=0; //reinicia contador de tiempo
    			if(bme280ReadData(&bme280_datos)==kStatus_Success){	//toma lectura humedad, presion, temperatura
        			printf("BME280 ->");
    				printf("temperatura:0x%X ",bme280_datos.temperatura);	//imprime temperatura sin procesar
        			printf("humedad:0x%X ",bme280_datos.humedad);	//imprime humedad sin procesar
        			printf("presion:0x%X ",bme280_datos.presion);	//imprime presion sin procesar
        			printf("\r\n");	//Imprime cambio de linea
    			}
    		}
    	}
#endif

#if HABILITAR_SENSOR_SHT3X
    	if(sht3x_detectado==1){
    		sht3x_base_de_tiempo++; //incrementa base de tiempo para tomar dato sensor SHT3X
			if(sht3x_base_de_tiempo>10){//	>10 equivale aproximadamente a 2s
				sht3x_base_de_tiempo=0; //reinicia contador de tiempo
	    		if (sht3xReadData(&sht3x_datos) == kStatus_Success) {//toma lectura humedad, temperatura
	    			printf("SHT3X ->");
	    			printf("temperatura:0x%X ",sht3x_datos.temperatura);	//imprime temperatura sin procesar
	    			printf("CRC8_t:0x%X ",sht3x_datos.crc_temperatura);	//imprime CRC8 de temperatura
        			printf("humedad:0x%X ",sht3x_datos.humedad);	//imprime humedad sin procesar
        			printf("CRC8_h:0x%X ",sht3x_datos.crc_humedad);	//imprime CRC8 de temperatura
        			printf("\r\n");	//Imprime cambio de linea
	    		}
			}
    	}
#endif

#if HABILITAR_MODEM_EC25
    	ec25_estado_actual = ec25Polling();	//actualiza maquina de estados encargada de avanzar en el proceso interno del MODEM
											//retorna el estado actual de la FSM

		switch(ec25_estado_actual){			//controla color de los LEDs dependiendo de estado modemEC25
    	case kFSM_RESULTADO_ERROR:
    		toggleLedRojo();
    		apagarLedVerde();
    		apagarLedAzul();
    		break;

    	case kFSM_RESULTADO_EXITOSO:
    		apagarLedRojo();
    		toggleLedVerde();
    		apagarLedAzul();
    		break;

    	case kFSM_RESULTADO_ERROR_RSSI:
    		toggleLedRojo();
    		apagarLedVerde();
    		toggleLedAzul();
    		break;

    	default:
    		apagarLedRojo();
    		apagarLedVerde();
    		toggleLedAzul();
    		break;
    	}
#endif
    }
    return 0 ;
}
