#include "Arduino.h"
#include "EEPROM.h"

#include <sensor.h>
#include <timer.h>
#include <actuator.h>

#ifndef brewcontroller_h
#define brewcontroller_h

/*
Estrutura da memória:

Completa:
[seção de configurações 0-10][Seção de rampas 11-1022][código de fim da receita 15-1023]

Configurações:
[pino do sensor principal 0][pino do atuador principal 1]...
ps: provavelmente não vou implementar os dois primeiros, mas os endereços ficam reservados

Rampa:
[código início de rampa 1 byte][tempo em minutos 1 byte][temperatura do mosto 1 byte]...
[tolerancia 1 byte][identificação se há mais processos ou não 1 byte][n de processos extras 1 byte]...
[pino do sensor][valor de referência][tolerância][pino do atuador]
Se o identificador dizer que não há mais processos, a rampa acaba ali e começa no próximo endereço,
do contrário, continua até encontrar o código de início da próxima rampa

Códigos numéricos
Negativo: [código 1 byte][número 1 ou mais bytes]
Número maior que 249 e decimais: [código 1 byte][vezes de 250 1 byte][restante 0 ou 1 byte]...
[decimal de 00 até 99 0 ou 1 byte][código]
exemplo: 250 = [cdg][1][cdg]
         350 = [cdg][1][100][cdg]
		 567 = [cdg][2][67][cdg]
		 1,5 = [cdg][0][1][50][cdg]
ps: não pretendo implementar esses, mas fica aqui caso necessite deles
*/

typedef struct {
	int sensor_pin;
	int actuator_pin;
	float ref_value;
	float tolerance;
} ControlProcess;

typedef struct {
	int position;
	int duration;
	float temp;
	float tolerance;
	int extra_procs;
} Slope;

class BrewController {
public:
	BrewController();
	BrewController(Timer *timer, int main_sensor_pin, Sensor *main_sensor, int main_actuator_pin, Actuator *main_actuator);
	~BrewController();

	//controle do processo
	boolean start(boolean restart = false); //saída verdadeira se não ocorrerem erros
	boolean stop(); //idem
	boolean reset(); //idem
	boolean activate(int output_pin); //ativa uma saída manualmente, o processo deve estar parado ou resetado
	boolean deactivate(int output_pin);
	boolean run(); //verifica as entradas e atua sobre as saídas de forma automática

	//manipulação de receitas e rampas
	//position inicia em 0
	boolean setSlope(int position, unsigned int duration, float moist_temp, float tolerance); //verdadeiro se tudo ocorrer bem
	boolean addProc2Slope(int position, int input_pin, int output_pin, float ref_value, float tolerance); //idem
	//remove um processo que bate com as entradas, retorna falso se houverem erros
	boolean rmvProc2Slope(int position, int input_pin, int output_pin, float ref_value, float tolerance);
	void resetSlope(int position, boolean reset_all); //reseta uma rampa para valores padrão, podendo eliminar procs extras ou não
	void resetAllSlopes(boolean reset_procs); //reseta todas as rampas para valores padrão
	void removeSlope(int position);
	void removeAllSlopes();
	float getSlopeTemp(int position);
	float getSlopeTolerance(int position);
	int getCurrentSlopeNumber();
	int getNumberOfSlopes(); //número total de rampas
	float getCurrentSlopeTemp(); //provalmente não será necessaŕio dados os métodos acima
	Slope getSlope(int position); //retorna a rampa
	int getProcsNum(int position); //retorna o número de processos de uma rampa
	ControlProcess getControlProcess(int slope_position, int proc_position);

	//manipulação de sensores e atuadores
	boolean addSensor(int pin, Sensor *sensor); //verdadeiro se tudo ir bem
	boolean addActuator(int pin, Actuator *actuator); //idem
	boolean clear(int pin); //idem, e limpa todos os pinos com excessão dos dois principais de entrada e saída
	boolean isPinInUse(int pin); //verdadeiro se o pino é usado como entrada ou saída
	float getSensorReading(int pin); //retorna menos infinito como código de erros
	boolean isActuatorOn(int pin); //returna verdadeiro se um atuador está ativo e falso caso contrário or se o pino é um sensor
	void printDeviceMatrix();
	int getDeviceType(int pin); //retorna 0 para sensores, 1 para atuadores e -1 para pinos sem uso
	int getDevice(int pin); //retorna um inteiro com o ponteiro para o dispositivo ou -1 para pinos sem uso

	//Outros métodos (cronômetro, parte pública da memória, etc)
	float getTimeLeft(); //em minutos
	unsigned int getCurrentSlopeDuration(); //em minutos
	unsigned int getSlopeDuration(int position); //em minutos
	unsigned int getMemoryLeft(); //em bytes
	void clearAllMemory(); //limpa e reseta a memória
	unsigned int getStatus(); //status do controlador no código interno
		
private:

	//escreve um número na memória e retorna o endereço logo após o número escrito retorna -1 em caso de erro
	int _writeToMemory(int addr, float number); 
	float _readFromMemory(int addr); //lê um número da memória, decodificando se necessário
	void _clearMemory(boolean clear_config = true);
	void _activateActuator(int pin);
	void _deactivateActuator(int pin);
	int _indexOfPin(int pin); //retorna o índice de um pino na matriz _devices
	void _setConfig(); //faz a configuração com os ítens da memória
	void _setErrorState();
	int _getAddrOfSlope(int position);
	int _getPosOfSlopeAddr(int addr);
	void _resetSlope(int slope_addr, boolean reset_all);
	boolean _moveMemTail(int current_addr, int new_addr); //moves a block of memory from current_addr to the end into new_addr
	int _calcMemSize(float number); //calcula o espaço necessaŕio para escrever um número na memória
	int _getEndAddr();
	void _deactivateAllActuators();
	boolean _nextSlope(boolean first_slope = false); //faz a passagem de rampas ou termina tudo se for a última

	//constantes
	#define _MIN_INF -3.4028235E38 //minus infinity
	#define _MAX_DEVICE_NUM 6 /*Número máximo de sensores e atuadores disponível, 
									 lembrar de casar com a declaração da matriz de dispositivos*/
	
	#define _MEMORY_SIZE (EEPROM.length()) //tamanho da memória em bytes
	#define _CONF_END 10 //endereço do fim da seção de configurações na memória (último endereço)
	#define _SLOPE_START_ID 252 //código de identificação do início de uma rampa na memória
	#define _EXTRA_PROCS_ID 253 //código que diz se haverão mais sensores que o principal
	#define _NO_EXTRA_PROCS_ID 254 //código que diz que não haverão mais sensores que o principal
	#define _RECIPE_END_ID 255 //código de identificação do fim da receita na memória
	
	#define _PIN_COL 0
	#define _TYPE_COL 1
	#define _DEV_COL 2
	
	#define _REST_STATE 0 //State of being on, but not brewing
	#define _BREW_STATE 1 //In brewing and not stopped
	#define _STOP_BREW_STATE 2 //In brewing, but the process was stopped
	#define _ERROR_STATE 3 //An error ocurred and the controller is inoperand until turned off
	
	#define _STR2TIME 1
	#define _STR2TEMP 2
	#define _STR2TOL 3
	#define _STR2PROCID 4
	#define _STR2PROCNUM 5
	
	#define _PRPN2REFVAL 1
	#define _PRPN2TOL 2
	#define _PRPN2ACT 3
	
	#define _MIN_SLOPE_SIZE 5
	#define _XTPROC_SIZE 4

	//variáveis
	int _end_addr; //endereco do fim da receita na memória
	int _current_slope_addr; //endereço do início da rampa atual

	//timer, sensores e atuadores e pinos
	unsigned int _status = _REST_STATE; //status of the controller
	Timer *_timer; //temporizador
	/*
	  matriz que reune entradas e saídas do controlador. Seu formato é:
	  pino | 0 para entrada e 1 para saída | ponteiro pro o sensor ou atuador
	  Por primeiro vem os pinos digitais, ignorando-se o 0 e 1 por serem RX e TX e
	  depois vem os analógicos.
	*/
	int _devices[_MAX_DEVICE_NUM][3]; 
	//PS: Como não vou usar muitos pinos, resolvi reduzir o tamanho da matriz para 6 pinos possíveis, dois
	//dos quais são analógicos
	//PS2: Eu queria ter um nome melhor...
	
	int _main_sensor_index; //índice do sensor principal na matriz _devices
	int _main_actuator_index; //índice do atuador principal na matriz _devices
};

#endif
