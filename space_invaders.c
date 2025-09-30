#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#define F_CPU 8000000UL
#define BAUD 38400
#define UBRR_VALUE ((F_CPU/(8UL * BAUD)) - 1UL)

#define ROWS 30
#define COLUMNS 60
#define MAX_ENEMIES 20

#define ENEMY ((unsigned char)'W')
#define PLAYER ((unsigned char)'^')
#define BULLET ((unsigned char)'*')


uint8_t enemy_move_count;
unsigned char enemy_move_direction = 'r'; //l = left; r = right
unsigned char player_move_buffer; 

volatile uint8_t player_ticks;
volatile uint8_t enemy_ticks;

uint8_t rendering = 0;

typedef struct {
    uint8_t row;
    uint8_t column;
} Position;

Position player = {ROWS - 1, COLUMNS / 2};
Position cursor = {0, 0};
Position enemies[MAX_ENEMIES];

void uart_init(void) {
    UBRR0H = (unsigned char)(UBRR_VALUE >> 8);
    UBRR0L = (unsigned char)(UBRR_VALUE & 0xFF);

    UCSR0A = (1 << U2X0);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
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

void generate_enemy_positions(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES / 2; i++) {
        enemies[i].row = 0;
        enemies[i].column = i;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES / 2; i++) {
        enemies[i + (MAX_ENEMIES / 2)].row = 1;
        enemies[i + (MAX_ENEMIES / 2)].column = i;
    }
    
}

void overwrite_position(unsigned char new_char, Position position) {
    int row_difference = position.row - cursor.row;
    int column_difference = position.column - cursor.column;

    //Checks if the cursor should move forward or backward/left or right
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

    //Writes a new character and moves back to the argument position
    printf("%c\x1b[1D", new_char);
    //Updates cursor position
    cursor = position;
}

void move_enemies_left(void) {
    enemy_move_count++;

    Position p1 = enemies[0];
    p1.column--;
    Position p2 = enemies[MAX_ENEMIES / 2];
    p2.column--;

    overwrite_position(' ', enemies[MAX_ENEMIES / 2 - 1]);
    overwrite_position(ENEMY, p1);

    overwrite_position(' ', enemies[MAX_ENEMIES - 1]);
    overwrite_position(ENEMY, p2);

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].column--;
    }
    
}

void move_enemies_right(void) {
    enemy_move_count++;

    Position p1 = enemies[MAX_ENEMIES / 2 - 1];
    p1.column++;
    Position p2 = enemies[MAX_ENEMIES - 1];
    p2.column++;

    overwrite_position(' ', enemies[0]);
    overwrite_position(ENEMY, p1);

    overwrite_position(' ', enemies[MAX_ENEMIES / 2]);
    overwrite_position(ENEMY, p2);

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].column++;
    }
    
}

void move_enemies(void) {
    if (rendering == 1) {
        return;
    }

    rendering = 1;
    //Logic to check in which direction enemies should move
    if (enemy_move_count >= (COLUMNS - MAX_ENEMIES / 2)) {
        enemy_move_count = 0;
        enemy_move_direction = (enemy_move_direction == 'l') ? 'r' : 'l';
    }

    if (enemy_move_direction == 'l') {
        move_enemies_left();
    } else if (enemy_move_direction == 'r') {
        move_enemies_right();
    }
    rendering = 0;
}

void move_player(void) {
    if (rendering == 1) {
        return;
    }

    rendering = 1;
    if (player_move_buffer == 'a' && !((player.column - 1) < 0)) {
        Position new_player_pos = player;
        new_player_pos.column--;
        overwrite_position(PLAYER, new_player_pos);
        overwrite_position(' ', player);

        cursor = player;
        player = new_player_pos;
        player_move_buffer = ' ';
    }

    if (player_move_buffer == 'd' && !((player.column + 1) > (COLUMNS - 1))) {
        Position new_player_pos = player;
        new_player_pos.column++;
        overwrite_position(PLAYER, new_player_pos);
        overwrite_position(' ', player);

        cursor = player;
        player = new_player_pos;
        player_move_buffer = ' ';
    }

    rendering = 0;
}

void render_game_field(void) {
    for (uint8_t i = 0; i < COLUMNS + 2; i++) {
        uart_transmit('#');
    }
    uart_transmit_string("\r\n");

    for (uint8_t i = 0; i < ROWS; i++) {
        uart_transmit('#');
        for (uint8_t j = 0; j < COLUMNS; j++) {
            if (i <= 1 && j <= (MAX_ENEMIES / 2 - 1)) {
                uart_transmit(ENEMY);
            } else if (i == player.row && j == player.column) {
                uart_transmit(PLAYER);
            } else {
                uart_transmit(' ');
            }
        }
        
        uart_transmit_string("#\r\n");
    }

    for (uint8_t i = 0; i < COLUMNS + 2; i++) {
        uart_transmit('#');
    }
    
    
    //Set correct cursor position {0, 0}
    printf("\x1b[%dA", ROWS);
    printf("\x1b[%dD", (COLUMNS + 1));
}

FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main(void) {
    uart_init();
    stdout = &uart_output;
    printf("\x1b[?25l");
    printf("\x1b[1D");
    printf(" "); //Hides cursor and deletes garbage character
    printf("\x1b[1D");
    generate_enemy_positions();
    render_game_field();

    timer_init();
    sei();

    while(1) {
        if (player_ticks >= 2) {
            player_ticks = 0;
            move_player();
        } else if (enemy_ticks >= 6) {
            enemy_ticks = 0;
            move_enemies();
        }    
    }
}

ISR(TIMER0_OVF_vect) {
    player_ticks++;
    enemy_ticks++;
}

ISR(USART_RX_vect) {
    player_move_buffer = UDR0;
}