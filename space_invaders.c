#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#define BAUD 38400
#define UBRR_VALUE ((F_CPU/(8UL * BAUD)) - 1UL)

#define WIDTH 40
#define HEIGHT 20

unsigned char game_field[HEIGHT][WIDTH];
uint8_t ovf_count;

typedef struct {
    uint8_t x;
    uint8_t y;
} Position;

Position player = {WIDTH / 2, HEIGHT - 1};
Position cursor = {0, 0};

ISR(TIMER0_OVF_vect) {
    ovf_count++;
    if (ovf_count >= 6) {
        ovf_count = 0;
        
        move_enemies();
    }
}

void move_enemies(void) {
    
}

void uart_init(void) {
    UBRR0H = (unsigned char)(UBRR_VALUE >> 8);
    UBRR0L = (unsigned char)(UBRR_VALUE & 0xFF);

    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void timer_init(void) {
    TCNT0 = 0;
    TCCR0A = 0x00;
    TIMSK0 = (1 << TOIE0);
    TCCR0B = (1 << CS02); 
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

void uart_transmit_string(const char *str) {
    while (*str) {
        uart_transmit(*str);
        str++;
    }
}

void set_initial_game_field(unsigned char game_field[HEIGHT][WIDTH]) {
    for (uint8_t i = 0; i < HEIGHT; i++) {
        for (uint8_t j = 0; j < WIDTH; j++) {
            if (i <= 1 && j <= 4) {
                //Sets initial enemy positions
                game_field[i][j] = (unsigned char)'W';
            } else if (i == player.y && j == player.x) {
                //Sets inital player position
                game_field[i][j] = (unsigned char)'^';
            } else {
                game_field[i][j] = (unsigned char)' ';
            }
        }
        
    }
    
}

void render_initial_game_field(unsigned char game_field[HEIGHT][WIDTH]) {
    for (uint8_t i = 0; i < WIDTH + 2; i++)
    {
        uart_transmit('#');
    }
    uart_transmit('\r');
    uart_transmit('\n');
    
    for (uint8_t i = 0; i < HEIGHT; i++) {
        uart_transmit('#');
        for (uint8_t j = 0; j < WIDTH; j++) {
            uart_transmit(game_field[i][j]);
        }
        uart_transmit('#');
        uart_transmit('\r');
        uart_transmit('\n');
    }

    for (uint8_t i = 0; i < WIDTH + 2; i++)
    {
        uart_transmit('#');
    }
    uart_transmit('\r');
    uart_transmit('\n');
    
}

int main(void) {
    uart_init();
    set_initial_game_field(game_field);
    render_initial_game_field(game_field);

    timer_init();
    sei();
}