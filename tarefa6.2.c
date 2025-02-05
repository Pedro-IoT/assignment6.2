#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

// Configuração dos pinos I²C
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

const uint LED = 12;            // Pino do LED conectado
const uint16_t PERIODO = 2000;   // Período do PWM (valor máximo do contador)
const float DIVISOR_PWM = 16.0; // Divisor fracional do clock para o PWM
const uint16_t PASSO_LED = 100;  // Passo de incremento/decremento para o duty cycle do LED
uint16_t led_level = 100;       // Nível inicial do PWM (duty cycle)

#define BUZZER_PIN 21

int espera = 100;

// Mario theme melody and durations (simplified)
const float melody[] = {
    659, 659, 659, 523, 659, 784, 0,
    392, 0, 523, 392, 0, 330, 0,
    440, 494, 466, 440, 392, 659, 784,
    880, 0, 698, 784, 0, 659, 0,
    523, 587, 494, 0
};

const float noteDurations[] = {
    8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8
};

#define TEMPO 200
#define QUARTER_NOTE (60000.0f / TEMPO)  // ms per quarter note
#define NUM_NOTES (sizeof(melody) / sizeof(melody[0]))

// Definição dos pinos usados para o joystick e LEDs
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Pino de leitura do botão do joystick

const int LED_B = 13;                    // Pino para controle do LED azul via PWM
const int LED_R = 11;                    // Pino para controle do LED vermelho via PWM
const uint16_t PERIOD = 4096;            // Período do PWM (valor máximo do contador)
uint16_t led_b_level, led_r_level = 100; // Inicialização dos níveis de PWM para os LEDs
uint slice_led_b, slice_led_r;           // Variáveis para armazenar os slices de PWM correspondentes aos LEDs

// Prototipação das funções
void display_message(uint8_t *ssd, struct render_area *frame_area, char *lines[], uint num_lines);
bool botao_esta_pressionado(uint pin);
void setup_joystick();
void setup_pwm_led(uint led, uint *slice, uint16_t level);
void setup();
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value);
void setup_pwm();
void setup_pwm2();
void play_note(float freq, float duration);
int WaitWithRead(int timeMS);
void joystick_led(uint8_t *ssd, struct render_area *frame_area);
void buzzer_pwm(uint8_t *ssd, struct render_area *frame_area);
void pwm_led(uint8_t *ssd, struct render_area *frame_area);
void menu(int counter, uint8_t *ssd, struct render_area *frame_area);

// Função para exibir mensagens no display
void display_message(uint8_t *ssd, struct render_area *frame_area, char *lines[], uint num_lines) {
    memset(ssd, 0, ssd1306_buffer_length);  // Limpa o buffer do display
    int y = 0;
    for (uint i = 0; i < num_lines; i++) {
        ssd1306_draw_string(ssd, 5, y, lines[i]);
        y += 8; // Incrementa a altura para a próxima linha
    }
    render_on_display(ssd, frame_area);  // Atualiza o display com o conteúdo renderizado
}

// Função para verificar se o botão foi pressionado
bool botao_esta_pressionado(uint pin) {
    if (!gpio_get(pin)) {  // Verifica se o botão está pressionado
        sleep_ms(50);  // Debounce para estabilizar
        if (!gpio_get(pin)) {
            return true;  // Retorna verdadeiro se o botão estiver pressionado
        }
    }
    return false;
}

// Função para configurar o joystick (pinos de leitura e ADC)
void setup_joystick()
{
  // Inicializa o ADC e os pinos de entrada analógica
  adc_init();         // Inicializa o módulo ADC
  adc_gpio_init(VRX); // Configura o pino VRX (eixo X) para entrada ADC
  adc_gpio_init(VRY); // Configura o pino VRY (eixo Y) para entrada ADC

  // Inicializa o pino do botão do joystick
  gpio_init(SW);             // Inicializa o pino do botão
  gpio_set_dir(SW, GPIO_IN); // Configura o pino do botão como entrada
  gpio_pull_up(SW);          // Ativa o pull-up no pino do botão para evitar flutuações
}

// Função para configurar o PWM de um LED (genérica para azul e vermelho)
void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
  gpio_set_function(led, GPIO_FUNC_PWM); // Configura o pino do LED como saída PWM
  *slice = pwm_gpio_to_slice_num(led);   // Obtém o slice do PWM associado ao pino do LED
  pwm_set_clkdiv(*slice, DIVISOR_PWM);   // Define o divisor de clock do PWM
  pwm_set_wrap(*slice, PERIOD);          // Configura o valor máximo do contador (período do PWM)
  pwm_set_gpio_level(led, level);        // Define o nível inicial do PWM para o LED
  pwm_set_enabled(*slice, true);         // Habilita o PWM no slice correspondente ao LED
}

// Função de configuração geral
void setup()
{
  setup_joystick();                                // Chama a função de configuração do joystick
  setup_pwm_led(LED_B, &slice_led_b, led_b_level); // Configura o PWM para o LED azul
  setup_pwm_led(LED_R, &slice_led_r, led_r_level); // Configura o PWM para o LED vermelho
}

// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
  // Leitura do valor do eixo X do joystick
  adc_select_input(ADC_CHANNEL_0); // Seleciona o canal ADC para o eixo X
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vrx_value = adc_read();         // Lê o valor do eixo X (0-4095)

  // Leitura do valor do eixo Y do joystick
  adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vry_value = adc_read();         // Lê o valor do eixo Y (0-4095)
}

void setup_pwm()
{
    uint slice;
    gpio_set_function(LED, GPIO_FUNC_PWM); // Configura o pino do LED para função PWM
    slice = pwm_gpio_to_slice_num(LED);    // Obtém o slice do PWM associado ao pino do LED
    pwm_set_clkdiv(slice, DIVISOR_PWM);    // Define o divisor de clock do PWM
    pwm_set_wrap(slice, PERIODO);           // Configura o valor máximo do contador (período do PWM)
    pwm_set_gpio_level(LED, led_level);    // Define o nível inicial do PWM para o pino do LED
    pwm_set_enabled(slice, true);          // Habilita o PWM no slice correspondente
}

void setup_pwm2() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);  // 1MHz clock
    pwm_config_set_wrap(&config, 0);
    pwm_init(slice_num, &config, true);
}

void play_note(float freq, float duration) {
    if (freq == 0) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        sleep_ms(duration);
        return;
    }

    uint32_t period = 1000000 / freq;  // microseconds
    uint32_t wrap = period - 1;
    uint32_t level = wrap / 2;

    pwm_set_wrap(pwm_gpio_to_slice_num(BUZZER_PIN), wrap);
    pwm_set_gpio_level(BUZZER_PIN, level);
    sleep_ms(duration);
    
    // Brief silence between notes
    pwm_set_gpio_level(BUZZER_PIN, 0);
    sleep_ms(10);
}

// Função para aguardar o botão ou tempo limite
int WaitWithRead(int timeMS) {
    for (int i = 0; i < timeMS; i += 100) {
        if (botao_esta_pressionado(SW)) {
            return 1;  // Retorna 1 se o botão for pressionado
        }
        sleep_ms(100);
    }
    return 0;  // Retorna 0 se o tempo esgotar sem o botão ser pressionado
}

void joystick_led(uint8_t *ssd, struct render_area *frame_area){
    uint16_t vrx_value, vry_value, sw_value; // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão
    printf("Joystick-PWM\n");                // Exibe uma mensagem inicial via porta serial
    char *message[] = {
            "Para sair", 
            "pressione o ",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
    // Loop principal
    while (1)
    {
        joystick_read_axis(&vrx_value, &vry_value); // Lê os valores dos eixos do joystick
        // Ajusta os níveis PWM dos LEDs de acordo com os valores do joystick
        pwm_set_gpio_level(LED_B, vrx_value); // Ajusta o brilho do LED azul com o valor do eixo X
        pwm_set_gpio_level(LED_R, vry_value); // Ajusta o brilho do LED vermelho com o valor do eixo Y

        // Pequeno delay antes da próxima leitura
        sleep_ms(100); // Espera 100 ms antes de repetir o ciclo
        if(botao_esta_pressionado(SW)) break;
    }
}

void buzzer_pwm(uint8_t *ssd, struct render_area *frame_area){
    setup_pwm2();
    char *message[] = {
            "Para sair", 
            "pressione o ",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
    while (1) {
        for (int i = 0; i < NUM_NOTES; i++) {
            if(botao_esta_pressionado(SW)) break;
            float noteDuration = QUARTER_NOTE * (4.0f / noteDurations[i]);
            play_note(melody[i], noteDuration);
        }
        if(botao_esta_pressionado(SW)) break;
        sleep_ms(1000);  // Pause before repeating
    }
}

void pwm_led(uint8_t *ssd, struct render_area *frame_area){
    uint up_down = 1; // Variável para controlar se o nível do LED aumenta ou diminui
    setup_pwm();      // Configura o PWM
    char *message[] = {
            "Para sair", 
            "pressione o ",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
    while (1){
        pwm_set_gpio_level(LED, led_level); // Define o nível atual do PWM (duty cycle)
        int botao_pressionado = WaitWithRead(espera);
        if(botao_pressionado) break;
        if (up_down){
            led_level += PASSO_LED; // Incrementa o nível do LED
            if (led_level >= PERIOD)
                up_down = 0; // Muda direção para diminuir quando atingir o período máximo
        }
        else{
            led_level -= PASSO_LED; // Decrementa o nível do LED
            if (led_level <= PASSO_LED)
                up_down = 1; // Muda direção para aumentar quando atingir o mínimo
        }
    }
}

void menu(int counter, uint8_t *ssd, struct render_area *frame_area){
    if(counter == 0){
        char *message[] = {
            "Bem-Vindo", 
            "Use o joystick", 
            "verticalmente",
            "para navegar"
        };
        display_message(ssd, frame_area, message, count_of(message));
    }
    else if(counter == 1){
        char *message[] = {
            "Para controlar o", 
            "led com joystick", 
            "pressione o ",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
        
    }
    else if(counter == 2){
        char *message[] = {
            "Para ouvir", 
            "o tema do mario", 
            "pressione o",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
    }
    else if(counter == 3){
        char *message[] = {
            "Para ver o", 
            "led azul variar", 
            "pressione o ",
            "joystick"
        };
        display_message(ssd, frame_area, message, count_of(message));
    }
}

int main(){
    // Inicialização do hardware
    stdio_init_all();
    setup();
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display
    ssd1306_init();

    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    uint16_t vrx_value, vry_value, sw_value;
    int contador  = 0;

    while(1){
        menu(contador, ssd, &frame_area);
        pwm_set_gpio_level(LED_B, 0);
        pwm_set_gpio_level(LED_R, 0);
        pwm_set_gpio_level(LED, 0);
        joystick_read_axis(&vrx_value, &vry_value); // Lê os valores dos eixos do joystick
        if(vrx_value < 1000){
            contador++;
            if(contador > 3) contador--;
            menu(contador, ssd, &frame_area);
        }
        if(vrx_value > 3000){
            contador--;
            if(contador < 0) contador++;
            menu(contador, ssd, &frame_area);
        }
        switch(contador){
            case 1:
                if(botao_esta_pressionado(SW)) joystick_led(ssd, &frame_area);
                break;
            case 2:
                if(botao_esta_pressionado(SW)) buzzer_pwm(ssd, &frame_area);
                break;
            case 3:
                if(botao_esta_pressionado(SW)) pwm_led(ssd, &frame_area);
                break;
            default: printf("usuario ainda está no menu inicial\n");
        }
        sleep_ms(100);
        }
}
