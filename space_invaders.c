#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#define BAUD 57600
#define UBRR_VALUE (F_CPU/(8 * BAUD)) - 1

#define WIDTH 20
#define HEIGHT 20

unsigned char game_field[HEIGHT][WIDTH];

ISR(TIMER0_OVF_vect) {
    ovf_count++ 
    if (ovf_count >= 2) {
        ovf_count = 0;
        

    }
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

void set_initial_game_field(unsigned char game_field[HEIGHT][WIDTH]) {
    for (int i = 0; i < HEIGHT - 1; i++) {
        for (int j = 0; j < WIDTH - 1; j++) {
            if (i <= 1 && j <= 4) {
                //Sets initial enemy positions
                game_field[i][j] = "#";
            } else if (i == HEIGHT - 1 && j == 10) {
                //Sets inital player position
                game_field[i][j] = "^";
            } else {
                game_field[i][j] = "";
            }
        }
        
    }
    
}

int main(void) {
    uart_init();
    set_initial_game_field(game_field);
    render_inital_game_field(game_field);

    timer_init();
    sei();
}