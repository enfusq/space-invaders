#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#define F_CPU 8000000UL
#define BAUD 38400
#define UBRR_VALUE ((F_CPU/(8UL * BAUD)) - 1UL)

#define ROWS 20
#define COLUMNS 40
#define MAX_ENEMIES 10

#define ENEMY ((unsigned char)'W')
#define PLAYER ((unsigned char)'^')
#define BULLET ((unsigned char)'*')

unsigned char game_field[ROWS][COLUMNS];

volatile uint8_t ovf_count;
uint8_t enemy_move_count;
unsigned char enemy_move_direction = 'r'; //l = left; r = right

typedef struct {
    uint8_t row;
    uint8_t column;
} Position;

Position player = {ROWS - 1, COLUMNS / 2};
Position cursor = {0, 0};
Position enemies[] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4},
                        {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}};

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

int uart_putchar(char c, FILE *stream) {
    uart_transmit(c);
    return 0;
}

void uart_transmit_string(const char *str) {
    while (*str) {
        uart_transmit(*str);
        str++;
    }
}

void overwrite_position(unsigned char new_char, Position position) {
    int row_difference = position.row - cursor.row;
    int column_difference = position.column - cursor.column;

    //Checks if the cursor should move forward or backward/left or right
    //Updates current cursor position
    if (row_difference < 0) {
        printf("\x1b[%dA", -row_difference);
    } else if (row_difference > 0) {
        printf("\x1b[%dB", row_difference);
    }

    if (column_difference < 0) {
        printf("\x1b[%dD", -column_difference);
    } else if(column_difference > 0) {
        printf("\x1b[%dC", column_difference);
    }

    printf("%c", new_char);
    cursor = position;
}

void move_enemies_left(void) {
    enemy_move_count++;

    Position p1 = enemies[0];
    p1.column--;
    Position p2 = enemies[5];
    p2.column--;

    overwrite_position(' ', enemies[4]);
    overwrite_position(ENEMY, p1);

    overwrite_position(' ', enemies[9]);
    overwrite_position(ENEMY, p2);

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].column--;
    }
    
}

void move_enemies_right(void) {
    enemy_move_count++;

    Position p1 = enemies[4];
    p1.column++;
    Position p2 = enemies[9];
    p2.column++;

    overwrite_position(' ', enemies[0]);
    overwrite_position(ENEMY, p1);

    overwrite_position(' ', enemies[5]);
    overwrite_position(ENEMY, p2);

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].column++;
    }
    
}

void set_initial_game_field(unsigned char game_field[ROWS][COLUMNS]) {
    for (uint8_t i = 0; i < ROWS; i++) {
        for (uint8_t j = 0; j < COLUMNS; j++) {
            if (i <= 1 && j <= 4) {
                //Sets initial enemy positions
                game_field[i][j] = ENEMY;
            } else if (i == player.row && j == player.column) {
                //Sets inital player position
                game_field[i][j] = PLAYER;
            } else {
                game_field[i][j] = (unsigned char)' ';
            }
        }
        
    }
    
}

void render_initial_game_field(unsigned char game_field[ROWS][COLUMNS]) {
    for (uint8_t i = 0; i < COLUMNS + 2; i++)
    {
        uart_transmit('#');
    }
    uart_transmit('\r');
    uart_transmit('\n');
    
    for (uint8_t i = 0; i < ROWS; i++) {
        printf("#%.40s#\r\n", game_field[i]);
    }

    for (uint8_t i = 0; i < COLUMNS + 2; i++)
    {
        uart_transmit('#');
    }
    
    //Set correct cursor position (game_field[0][0])
    uart_transmit_string("\x1b[20A");
    uart_transmit_string("\x1b[41D");
}

FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main(void) {
    uart_init();
    stdout = &uart_output;
    set_initial_game_field(game_field);
    render_initial_game_field(game_field);

    timer_init();
    sei();

    while(1) {
        if (ovf_count >= 60) {
            ovf_count = 0;
            if (enemy_move_count >= 35) {
                enemy_move_count = 0;
                enemy_move_direction = (enemy_move_direction == 'l') ? 'r' : 'l';
            }

            if (enemy_move_direction == 'l') {
                move_enemies_left();
            } else if (enemy_move_direction == 'r') {
                move_enemies_right();
            }
            // DDRD |= (1 << 4);

            // PORTD ^= (1 << 4);   
        }
    }
}

ISR(TIMER0_OVF_vect) {
    ovf_count++;
}